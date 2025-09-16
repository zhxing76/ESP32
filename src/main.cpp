#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include <SPI.h>
#include <LoRa.h>
#define dht_pin A13
#define nss 5
#define rst 14
#define lora_pin 2

namespace config {                                // WiFi and MQTT configuration
  const char* ssid = "bone";                      // 無線網路基地台的SSID
  const char* password = "09006721180";           // 無線網路基地台的密碼
  // MQTT部分 • 函式區引用及參數設定
  const char *mqttServer = "test.mosquitto.org";  // MQTT網誌
  const int mqttPort = 1883;                      // 預設伺機埠號
  const char *mqttUser = "";                      // 登入帳號
  const char *mqttPassword = "";                  // 登入密碼
  const char *publishTopic_data = "/laizhizhi076/CSV_data";     // 需要的主題名稱
  String clientID = "HOLLE_I_just_want_pass";     // 建立的client ID
  WiFiClient espClient;                           // 建立WiFiClient物件
  PubSubClient client(espClient);                 // 建立WiFiClient的物件作為其參數 •
}
DHT dht(dht_pin , DHT22);
SensirionI2cScd4x scd41_sensor;
struct sensor_data{                               // Structure to hold sensor data
  int16_t temperature;
  int16_t humidity;
  int16_t dioxide;
  uint8_t payload[6];
};
int mode = 0;
int last_mode = -1;
unsigned long previousMillis_MQTT = 0;
unsigned long previousMillis_LoRa = 0;
const unsigned long interval = 6000;  // 6秒間隔，配合SCD41的5秒更新週期

void readData(sensor_data &data) {              // Read temperature, humidity, and CO2 data from SCD41
  uint16_t co2 = 0;
  float temp_scd = 0.0;
  float hum_scd = 0.0;
  
  if (scd41_sensor.readMeasurement(co2, temp_scd, hum_scd) == 0) {
    // 檢查數據是否合理
    if (co2 > 0 && co2 < 4000 && temp_scd > -40 && temp_scd < 85 && hum_scd >= 0 && hum_scd <= 100) {
      // 使用 SCD41 的數據
      data.temperature = temp_scd * 10;  // 轉換為整數（保留一位小數）
      data.humidity = hum_scd * 10;
      data.dioxide = co2;
    } else {
      // 數據不合理
      Serial.println("SCD41 data out of range");
    }
  } /*else {
    // SCD41 讀取失敗，嘗試讀取 DHT22 作為備用
    Serial.print("\nSCD41 not ready, trying DHT22... ");
    
    float temp_dht = dht.readTemperature();
    float hum_dht = dht.readHumidity();
    
    if (!isnan(temp_dht) && !isnan(hum_dht)) {
      data.temperature = temp_dht * 10;
      data.humidity = hum_dht * 10;
      Serial.println("Using DHT22 backup data");
    } else {
      Serial.println("Both sensors failed, using defaults");
      data.temperature = 0;
      data.humidity = 0;
    }
  }*/
}
void connectWiFi() {                            // Connect to Wi-Fi access point
      Serial.print("Connecting to ");
      Serial.println(config::ssid);
      WiFi.begin(config::ssid, config::password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("WiFi connected successfully");
    }
void connectMQTT() {                            // Connect to MQTT broker
    while (!config::client.connected()) {
      Serial.println("Attempting MQTT connection...");
      if (config::client.connect(config::clientID.c_str(), config::mqttUser, config::mqttPassword)) {
        Serial.println("MQTT connected successfully");
      } else {
        Serial.print("MQTT connection failed, rc=");
        Serial.print(config::client.state());
        Serial.println(" try again in 5 seconds");
        delay(5000);
      }
    }    
}
void setupLoRa() {                              // Initialize LoRa
  LoRa.setPins(nss, rst, lora_pin);

  Serial.println("Starting LoRa initialization...");
  if (!LoRa.begin(433E6)) {             // Initialize transmission frequency 915 or 433 MHz
    Serial.println("LoRa initialization failed. Check your connections.");
    while (true);                       // If failed, do nothing
  }
  Serial.println("LoRa initialized successfully");
}
void packPayload(sensor_data &data){            // Pack data into payload format
    data.payload[0] = data.temperature >> 8;    // High byte
    data.payload[1] = data.temperature & 0xFF;  // Low byte
    data.payload[2] = data.humidity >> 8;     
    data.payload[3] = data.humidity & 0xFF;
    data.payload[4] = data.dioxide >> 8;
    data.payload[5] = data.dioxide & 0xFF;
}
void publishMQTT(sensor_data &data) {           // Publish data to MQTT broker
  while (!config::client.connected()) {
    connectMQTT();
  }
  packPayload(data);
  config::client.publish(config::publishTopic_data, data.payload, sizeof(data.payload));
  Serial.print("Data sent to MQTT: ");
  Serial.print(data.temperature / 10.0); Serial.print("°C, ");
  Serial.print(data.humidity / 10.0); Serial.print("%, ");
  Serial.print(data.dioxide); Serial.println("ppm\n");
}
void sendLoRa(sensor_data &data){               // Send data via LoRa
  packPayload(data);  
  LoRa.beginPacket();             
  LoRa.write(data.payload, 6);
  LoRa.endPacket();               
  Serial.println("Data sent via LoRa");
}
void selectUploadMode(void *pvParameters) {
  while(1){
    if (Serial.available()) 
      mode = Serial.parseInt();
  }
}

void setup() {
  Serial.begin(9600);   // Start serial communication 
  Serial.println("=== System Initialization ===");
  connectWiFi();
  dht.begin();
  delay(1000);
  Wire.begin();
  scd41_sensor.begin(Wire, SCD41_I2C_ADDR_62);
  
  scd41_sensor.startPeriodicMeasurement();
  Serial.println("SCD41 sensor initialized");
  // Initialize MQTT
  config::client.setServer(config::mqttServer, config::mqttPort);
  connectMQTT();
  setupLoRa();
  // Start upload mode selection task
  xTaskCreatePinnedToCore(selectUploadMode, "selectUploadMode", 10000, NULL, 1, NULL, 1);
  Serial.println("=== Initialization Complete ===");
  Serial.println("Enter 0 for WiFi upload, 1 for LoRa upload:");
}

void loop() {
  sensor_data data;  
  if (mode != last_mode) {
    Serial.println("Mode switched to: " + String(mode) + (mode==0 ? " (WiFi)":" (LoRa)"));
    last_mode = mode;
  }
  
  unsigned long currentMillis = millis();
  if (mode == 0) {
    if (currentMillis - previousMillis_MQTT >= interval) {
      previousMillis_MQTT = currentMillis;
      readData(data); // Read all sensor data from SCD41 (with DHT22 as backup)
      publishMQTT(data); 
    }
  }else if (mode == 1) {
    if (currentMillis - previousMillis_LoRa >= interval) {
      previousMillis_LoRa = currentMillis;
      readData(data); // Read all sensor data from SCD41 (with DHT22 as backup)
      sendLoRa(data);
    }
  }
}