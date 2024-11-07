#include <Arduino.h>
#if defined(ESP32)
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
  #define DEVICE "ESP32"
#elif defined(ESP8266)
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "ESP8266"
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <WiFi.h>

// Define pins for LEDs and sensors
#define LED_PIN_1 15 
#define LED_PIN_2 19 
#define LED_PIN_3 18 
#define BUZZER_PIN 25  // Pin for the active buzzer
#define PIN_ANALOG_IN 32
#define trigPin 13
#define echoPin 14
#define MAX_DISTANCE 700
float timeOut = MAX_DISTANCE * 60; // Timeout in microseconds
int soundVelocity = 340;  // Speed of sound in m/s (343 m/s at 20°C)

// WiFi credentials
#define WIFI_SSID "WiFi-552E"
#define WIFI_PASSWORD "94046748"

// InfluxDB settings
#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "9-9n09z9o_PYT-pXYNZ3IwYa0I8Ozfk9tih-LZmwfMWVC37RLvdJqf9JrboS5gGcHrO8eKXAFBgpZMe3Ogu_4Q=="
#define INFLUXDB_ORG "932b221d12b2b7e8"
#define INFLUXDB_BUCKET "iotproject"

// Time zone info
#define TZ_INFO "AEST-10AEDT,M10.1.0,M4.1.0/3"

// Declare InfluxDB client and data point
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensor("iotproject");

// Function to measure temperature
float get_temp() {
  int analogValue = analogRead(PIN_ANALOG_IN);
  float voltage = (analogValue / 4095.0) * 3.3;  // Convert to voltage (3.3V reference)
  float tempCelsius = (voltage - 1.1) * 100.0;   // Adjusted for calibration
  return tempCelsius;
}

// Function to measure distance using ultrasonic sensor
float getSonar() {
  unsigned long pingTime;
  float distance;

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  pingTime = pulseIn(echoPin, HIGH, timeOut);

  if (pingTime > 0) {
    distance = (float)pingTime * soundVelocity / 2 / 10000;  // Convert to cm
  } else {
    distance = -1; // Error or no echo
  }

  return distance;
}

// Function to control LED based on distance
void controlLED(float distance) {
  // Turn off all LEDs initially
  digitalWrite(LED_PIN_1, LOW);
  digitalWrite(LED_PIN_2, LOW);
  digitalWrite(LED_PIN_3, LOW);
  
  // Control LED based on distance
  if (distance <= 10) {
    Serial.println("LED Red is on (distance <= 10 cm)");
    digitalWrite(LED_PIN_1, HIGH);
  } else if (distance <= 20) {
    Serial.println("LED Blue is on (distance <= 20 cm)");
    digitalWrite(LED_PIN_2, HIGH);
  } else {
    Serial.println("LED Green is on (distance > 20 cm)");
    digitalWrite(LED_PIN_3, HIGH);
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  pinMode(LED_PIN_1, OUTPUT);
  pinMode(LED_PIN_2, OUTPUT);
  pinMode(LED_PIN_3, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT); // Set the buzzer pin as output
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println("\nConnected to WiFi!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

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
}

void loop() {
  float distance = getSonar();
  float temperature = get_temp();
  int rssi = WiFi.RSSI();

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("RSSI: ");
  Serial.println(rssi);

  // Control LED based on distance
  controlLED(distance);

  // Control buzzer based on RSSI value
  if (rssi <= -55) { //If RSSI is low, activate buzzer
    Serial.println("RSSI is higher than -55, activating buzzer");
    digitalWrite(BUZZER_PIN, HIGH);  // Turn on the buzzer
  } else {
    digitalWrite(BUZZER_PIN, LOW);   // Turn off the buzzer
  }

  // Prepare data point for InfluxDB
  sensor.clearFields();
  
  if (distance >= 0) {
    sensor.addField("sonar", distance);
  } else {
    Serial.println("No sonar reading");
  }

  sensor.addField("temp", temperature);
  sensor.addField("rssi", rssi);  // Keep RSSI as integer to match existing schema

  Serial.print("Writing to InfluxDB: ");
  Serial.println(sensor.toLineProtocol());

  // Check WiFi connection before writing to InfluxDB
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("WiFi Connection Lost");
  }

  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  } else {
    Serial.println("Data successfully written to InfluxDB");
  }

  delay(5000);
}
