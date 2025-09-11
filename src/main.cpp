#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include "Adafruit_CCS811.h"
#include <SPI.h>
#include <LoRa.h>
#define DHTPIN A13
#define nss 5
#define rst 14
#define lora_pin 2

const char* ssid = "B2-7";                      // 無線網路基地台的SSID
const char* password = "00000000";              // 無線網路基地台的密碼
// MQTT部分 • 函式區引用及參數設定
const char *mqttServer = "test.mosquitto.org";  // MQTT網誌
const int mqttPort = 1883;                      // 預設伺機埠號
const char *mqttUser = "";                      // 登入帳號
const char *mqttPassword = "";                  // 登入密碼
const char *publishTopic_temp = "/laizhizhi076/CSV_data";     // 需要的主題名稱
String clientID = "HOLLE_I_just_want_pass";  // 建立的client ID
WiFiClient espClient;  // 建立WiFiClient物件
PubSubClient client(espClient);  // 建立WiFiClient的物件作為其參數 •

DHT dht(DHTPIN , DHT22);
float temperature, humidity;  // 溫度值,濕度值
Adafruit_CCS811 CO2;          //CO2 SENSOR物件
int dioxide;                  //CO2的數值

int mode = 0;                 //上傳模式
int last_mode = -1;           //上次的模式
unsigned long previousMillis_MQTT = 0;
unsigned long previousMillis_LoRa = 0;
const unsigned long interval = 2000; // 2 秒
void readData() {       // 讀取溫濕度、二氧化碳濃度數據
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  dioxide = CO2.geteCO2();
}
void connect_wifi() {   // 連線到Wi-Fi基地台
      Serial.print("Connecting");
      Serial.println(ssid);
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("Wi-Fi已連線");
    }
String merge_to_CSV(){  // 合併數據為CSV格式
  //maybe要加上溫溼度是否有效
    if(CO2.available() && !CO2.readData()){  // 確保 CO2 數值有效
    return String(temperature) + "," + String(humidity) + "," + String(dioxide);
  } else {
    Serial.println("CO2 data not ready, skipping LoRa send.");
    return String(temperature) + "," + String(humidity) + ",N/A"; // CO2 數值無效時標記為 N/A
  }
}
void connected_mqtt() { // 連線到MQTT經紀人
    while (!client.connected()) {
      Serial.println("Attempting MQTT connection...");
      if (client.connect(clientID.c_str(), mqttUser, mqttPassword)) {
        Serial.println("connected");
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        delay(5000);
      }
    }    
}
void publish_mqtt() {   // 發佈數據到MQTT經紀人
  while (!client.connected()) {
    connected_mqtt();
  }
  // 若數據有效才送出
  client.publish(publishTopic_temp, merge_to_CSV().c_str());
  Serial.println("Data sent to MQTT");
}
void set_lora() {       // LoRa初始化
  LoRa.setPins(nss, rst, lora_pin);

  Serial.println("Start LoRa init...");
  if (!LoRa.begin(433E6)) {             // initialize ratio at 915 or 433 MHz
    Serial.println("LoRa init failed. Check your connections.");
    while (true);                       // if failed, do nothing
  }
  Serial.println("LoRa init succeeded.");
}
void Lora_send(){       // LoRa發送數據
    LoRa.beginPacket();             
    LoRa.print(merge_to_CSV());  // 發送合併後的CSV數據    
    LoRa.endPacket();               
    Serial.println("LoRa sent!");
}
void check_data(){      // 檢查數據是否有效
  
}
void select_upload(void *pvParameters) {
  while(1){
    if (Serial.available()) 
      mode = Serial.parseInt();
  }
}

void setup() {
  Serial.begin(9600);   // 啟動序列埠
  connect_wifi();       // 執行 Wi-Fi 連線
  dht.begin();          //DHT22初始化
  CO2.begin();          //啟用CCS811
  client.setServer(mqttServer, mqttPort);  // 設定 MQTT 經紀人
  connected_mqtt();
  set_lora();
  xTaskCreatePinnedToCore(select_upload, "select_upload", 10000, NULL, 1, NULL, 1);
  Serial.print("使用wifi上傳請輸入0，使用LoRa上傳請輸入1:\n");
}

void loop() {
  readData(); //測溫溼度
  if (mode != last_mode) {
    Serial.println("模式切換為：" + String(mode) + (mode==0 ? " (WiFi)":" (LoRa)"));
    last_mode = mode;
  }
  unsigned long currentMillis = millis();
  if (mode == 0) {
    if (currentMillis - previousMillis_MQTT >= interval) {
      previousMillis_MQTT = currentMillis;
      publish_mqtt();
    }
  }else if (mode == 1) {
    if (currentMillis - previousMillis_LoRa >= interval) {
      previousMillis_LoRa = currentMillis;
      Lora_send();
    }
  }
  // Serial.println("Temp: " + String(temperature) + " °C");
  // Serial.println("Humidity: " + String(humidity) + " %");
  // if(CO2.available()){
  //   if(!CO2.readData()){
  //     Serial.println("CO2: " + String(dioxide) + " ppm");
  //   }
  // }

}