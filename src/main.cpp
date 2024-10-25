#include <Arduino.h>
#if defined(ESP32)
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"
#elif defined(ESP8266)
#include <ESP8277WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#define DEVICE "ESP8266"
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#define PIN_ANALOG_IN 32
#define trigPin 13
#define echoPin 14
#define MAX_DISTANCE 700
float timeOut = MAX_DISTANCE * 60; // Timeout in microseconds
int soundVelocity = 340;  // Speed of sound in m/s (343 m/s at 20°C)

// WiFi AP SSID
#define WIFI_SSID "A54"
// WiFi Password
#define WIFI_PASSWORD "8s2d32y5yzur424"

#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "SJ3Ir0aIn_gJJCg9yf8gd0B5hFiA0qFvO4Ivvnay4R20MLI8W_Ta-bebTC3eojupKWWWlq5pWGv5W17Ht88Hcg=="
#define INFLUXDB_ORG "1d418e38cc9b5f3e"
#define INFLUXDB_BUCKET "taraesp32"

// Time zone info
#define TZ_INFO "AEST-10AEDT, M10.1.0,M4.1.0/3"

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io";
// const char *mqtt_broker = "broker.hivemq.com";
const char *topic = "kaisesp32/wifiMeta/rssi";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

String clientID;

WiFiClient espClient;
PubSubClient client(espClient);

void callback(char *topic, byte *payload, unsigned int length);
void reconnect();

// Declare InfluxDB Client Instance with preconfigured InfluxCloud
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

//Declare Data Point
Point sensor("taraesp32");

float get_temp() {
  int analogValue = analogRead(PIN_ANALOG_IN);
  float voltage = (analogValue / 4095.0) * 3.3;  // Convert to voltage (3.3V reference)
  float tempCelsius = (voltage - 0.5) * 100.0;   // Example conversion
  return tempCelsius;
}

float getSonar() {
  unsigned long pingTime;
  float distance;

  // Trigger the sensor by sending a HIGH signal for 10 microseconds
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Measure the time taken for the echo to return
  pingTime = pulseIn(echoPin, HIGH, timeOut); 

  // Calculate distance based on the time and speed of sound
  if (pingTime > 0) {
    distance = (float)pingTime * soundVelocity / 2 / 10000;  // Convert to cm
  } else {
    distance = -1;  // If no echo received, return -1
  }

  return distance;
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);  // Set resolution to 12 bits

  // Setup WiFi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

   Serial.println("Connected to the Wi-Fi network with local IP: ");
  Serial.println(WiFi.localIP());

  clientID = "kaisesp32-";
  clientID += WiFi.macAddress();

  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected()) {
    Serial.printf("The client %s connects to the public MQTT broker\n", clientID.c_str());
    if (client.connect(clientID.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Public EMQX MQTT broker connected");
    } 
    else 
    {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }

  client.subscribe("kaisesp32/msgIn/led_group");

  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB Connection Failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  sensor.addTag("device", DEVICE);
  sensor.addTag("location", "homeoffice");
  sensor.addTag("esp32_id", String(WiFi.BSSIDstr().c_str()));

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void loop() {
  float distance = getSonar();  // Get sonar reading
  float temperature = get_temp();  // Get temperature reading

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");

  // Prepare InfluxDB fields
  sensor.clearFields();  // Clear previous fields
  
  if (distance >= 0) {
    sensor.addField("sonar", distance);  // Add sonar field only if valid
  } else {
    Serial.println("No sonar reading");
  }

  sensor.addField("temp", temperature);  // Add temperature field

  // Write data to InfluxDB
  Serial.print("Writing: ");
  Serial.println(sensor.toLineProtocol());

  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("WiFi Connection Lost");
  }

  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  } else {
    Serial.println("Data successfully written to InfluxDB");
  }

    if (!client.connected()) {
    reconnect();
  }
  client.loop();

  String tmp = String(WiFi.RSSI());
  const char *payload = tmp.c_str();
  if (client.publish(topic, payload)) {
    Serial.println("publish okay");
  } else {
    Serial.println("publish failed");
  }

  delay(2000);
}
