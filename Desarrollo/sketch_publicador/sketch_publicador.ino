#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <BearSSLHelpers.h>
#include "ca_cert.h"
#include "client_cert.h"
#include "private_key.h"

// WiFi settings
const char* ssid = "nombre_red";
const char* password = "contraseña";

// AWS IoT settings
const char* aws_endpoint = "a1mgq13vqqoshs-ats.iot.us-east-1.amazonaws.com";
const int port = 8883;

// NTP settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// Flag to track connection status
bool awsConnected = false;

// Create a secure client
BearSSL::WiFiClientSecure net;
PubSubClient client(net);

//Sensor
const int trigPin = 14;  // TRIG connected to D5
const int echoPin = 13;  // ECHO connected to D7

const int ledPin1 = 16;  // LED connected to D0 LED ROJO
const int ledPin2 = 4;   // LED connected to D2 LED AMARILLO
const int ledPin3 = 2;   // LED connected to D4 LED VERDE

// Timer to track last message received time
unsigned long lastMsgTime = 0;

int proximity() {
  // Clear the trigPin by setting it LOW
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Trigger the sensor by setting the trigPin HIGH for 10 microseconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the time for the echoPin to go HIGH
  long duration = pulseIn(echoPin, HIGH);

  // Calculate the distance
  // Speed of sound wave divided by 2 (there and back)
  int distance = duration * 0.034 / 2;

  // Print the distance on the serial monitor
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  return distance;
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message;
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    message += (char)payload[i];
  }
  Serial.println();

  // Update the last message received time
  lastMsgTime = millis();
  
  // Process the message
  if (String(topic) == "myhome/door/controlled") {
    if (message == "1") {
      digitalWrite(ledPin1, HIGH);  // Turn on red LED
      digitalWrite(ledPin2, LOW);
      digitalWrite(ledPin3, LOW);
    } else if (message == "2") {
      digitalWrite(ledPin1, LOW);
      digitalWrite(ledPin2, HIGH);  // Turn on yellow LED
      digitalWrite(ledPin3, LOW);
    } else if (message == "3") {
      digitalWrite(ledPin1, LOW);
      digitalWrite(ledPin2, LOW);
      digitalWrite(ledPin3, HIGH);  // Turn on green LED
    } else {
      digitalWrite(ledPin1, LOW);
      digitalWrite(ledPin2, LOW);
      digitalWrite(ledPin3, LOW);   // Turn off all LEDs if message is not 1, 2, or 3
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Set the trigPin as an OUTPUT
  pinMode(trigPin, OUTPUT);

  // Set the echoPin as an INPUT
  pinMode(echoPin, INPUT);
  
  // Set the LED pins as OUTPUT
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(ledPin3, OUTPUT);

  setup_wifi();

  // Initialize NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Wait for time to be set
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  Serial.println("Time synchronized");

  // Load certificate and private key
  BearSSL::X509List trustAnchor(ca_cert);
  BearSSL::X509List clientCert(client_cert);
  BearSSL::PrivateKey clientKey(private_key);

  net.setTrustAnchors(&trustAnchor);
  net.setClientRSACert(&clientCert, &clientKey);

  client.setServer(aws_endpoint, port);
  client.setCallback(callback);
  client.setKeepAlive(60);  // Increase keepalive interval to 60 seconds

  // Check and connect to AWS IoT
  if (client.connect("ESP8266Client")) {
    if (!awsConnected) {
      Serial.println("Connected to AWS IoT");
      awsConnected = true; // Set the flag to true
    }
    client.subscribe("myhome/door/controlled");  // Subscribe to the topic
  } else {
    Serial.print("Connection to AWS IoT failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");

    // Print detailed SSL error information
    int ssl_error = net.getLastSSLError();
    Serial.print("Last SSL error code: ");
    Serial.println(ssl_error);
  }
}

void loop() {
  if (!client.connected()) {
    // Reconnect if connection is lost
    if (client.connect("ESP8266Client")) {
      if (!awsConnected) {
        Serial.println("Connected to AWS IoT");
        awsConnected = true; // Set the flag to true
      }
      client.subscribe("myhome/door/controlled");  // Subscribe to the topic
    }
  }

  client.loop();

  // Check if 2 seconds have passed since the last message was received
  if (millis() - lastMsgTime > 2000) {
    digitalWrite(ledPin1, LOW);  // Turn off red LED
    digitalWrite(ledPin2, LOW);  // Turn off yellow LED
    digitalWrite(ledPin3, LOW);  // Turn off green LED
  }

  // Read proximity sensor
  int distance = proximity();

  // Convert distance to string
  char distanceStr[10];
  itoa(distance, distanceStr, 10);

  // Publish the distance
  client.publish("myhome/door/proximity", distanceStr);
  delay(1000); // Wait for 1 second before publishing again
}