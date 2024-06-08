import 'dart:async';
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:path_provider/path_provider.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Semáforo en Protoboard',
      theme: ThemeData(
        primarySwatch: Colors.blue,
      ),
      home: ProximityPage(),
    );
  }
}

class ProximityPage extends StatefulWidget {
  @override
  _ProximityPageState createState() => _ProximityPageState();
}

class _ProximityPageState extends State<ProximityPage> {
  String _proximityData = "Esperando datos...";
  Color redColor = Colors.grey;
  Color yellowColor = Colors.grey;
  Color greenColor = Colors.grey;
  late MqttServerClient client;

  @override
  void initState() {
    super.initState();
    _connectToAwsIot();
  }

  Future<void> _connectToAwsIot() async {
    client = MqttServerClient.withPort('a1mgq13vqqoshs-ats.iot.us-east-1.amazonaws.com', 'flutter_client', 8883);

    client.secure = true;
    client.securityContext = SecurityContext.defaultContext;
    final caCert = await _loadAssetToFile('assets/certs/ca.pem', 'ca.pem');
    print('Certificado CA cargado en: ${caCert.path}');
    final cert = await _loadAssetToFile('assets/certs/cert.pem.crt', 'cert.pem.crt');
    print('Certificado cargado en: ${cert.path}');
    final privateKey = await _loadAssetToFile('assets/certs/private.pem.key', 'private.pem.key');
    print('Llave privada cargada en: ${privateKey.path}');
    client.securityContext.setTrustedCertificates(caCert.path);
    client.securityContext.useCertificateChain(cert.path);
    client.securityContext.usePrivateKey(privateKey.path);

    client.setProtocolV311();
    client.logging(on: true);

    final connMessage = MqttConnectMessage()
        .withClientIdentifier('flutter_client')
        .startClean()
        .withWillQos(MqttQos.atLeastOnce);
    client.connectionMessage = connMessage;

    try {
      await client.connect();
      print('Conectado a AWS IoT');
      _subscribeToTopic(client);
    } catch (e) {
      print('Excepción al conectar: $e');
      client.disconnect();
    }
  }

  void _subscribeToTopic(MqttServerClient client) {
    final topic = 'myhome/door/proximity'; // Tema de suscripción correcto
    client.subscribe(topic, MqttQos.atLeastOnce);
    client.updates?.listen((List<MqttReceivedMessage<MqttMessage>> c) {
      final MqttPublishMessage recMess = c[0].payload as MqttPublishMessage;
      final String pt = MqttPublishPayload.bytesToStringAsString(recMess.payload.message);
      setState(() {
        _proximityData = pt;
        _updateTrafficLight(double.tryParse(pt) ?? 0);
      });
      print('Dato recibido del servidor MQTT: $pt');
    });
  }

  void _publishMessage(MqttServerClient client, String topic, String message) {
    final builder = MqttClientPayloadBuilder();
    builder.addString(message);
    client.publishMessage(topic, MqttQos.atLeastOnce, builder.payload!);
  }

  void _updateTrafficLight(double proximity) {
    setState(() {
      redColor = Colors.grey;
      yellowColor = Colors.grey;
      greenColor = Colors.grey;

      if (proximity >= 3 && proximity <= 10) {
        redColor = Colors.red;
        _publishMessage(client, 'myhome/door/controlled', '1');
      } else if (proximity >= 11 && proximity <= 15) {
        yellowColor = Colors.yellow;
        _publishMessage(client, 'myhome/door/controlled', '2');
      } else if (proximity > 15) {
        greenColor = Colors.green;
        _publishMessage(client, 'myhome/door/controlled', '3');
      }
    });
  }

  Future<File> _loadAssetToFile(String assetPath, String fileName) async {
    print('Cargando el archivo $assetPath...');
    final data = await rootBundle.load(assetPath);
    final bytes = data.buffer.asUint8List();
    final directory = await getApplicationDocumentsDirectory();
    final file = File('${directory.path}/$fileName');
    await file.writeAsBytes(bytes);
    print('Archivo cargado en: ${file.path}');
    return file;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Semáforo en Protoboard'),
      ),
      body: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: <Widget>[
          Text('Distancia recibida: $_proximityData'),
          SizedBox(height: 20),
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              buildLed(redColor),
              SizedBox(width: 10),
              buildLed(yellowColor),
              SizedBox(width: 10),
              buildLed(greenColor),
            ],
          ),
        ],
      ),
    );
  }

  Widget buildLed(Color color) {
    return Column(
      children: [
        Container(
          width: 10,
          height: 50,
          color: Colors.grey[800],
        ),
        Container(
          width: 40,
          height: 40,
          decoration: BoxDecoration(
            color: color,
            shape: BoxShape.circle,
            border: Border.all(width: 2, color: Colors.black),
          ),
        ),
        Container(
          width: 10,
          height: 50,
          color: Colors.grey[800],
        ),
      ],
    );
  }
}