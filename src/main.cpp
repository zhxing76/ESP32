#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <SPI.h>
#include <LoRa.h>
String Incoming = "";
String Message = "";
#define DHTPIN A13
#define DHTTYPE DHT11

const char* ssid = "AndroidAP473E";       // 無線網路基地台的SSID
const char* password = "ZIYU_0111";       // 無線網路基地台的密碼

// MQTT部分 • 函式區引用及參數設定
const char *mqttServer = "test.mosquitto.org";  // MQTT網誌
const int mqttPort = 1883;                      // 預設伺機埠號
const char *mqttUser = "";                      // 登入帳號
const char *mqttPassword = "";                  // 登入密碼
const char *publishTopic_temp = "/laizhizhi076/temperature";     // 需要的主題名稱
const char *publishTopic_humidity = "/laizhizhi076/humidity";

String clientID = "HOLLE_I_just_want_pass";  // 建立的client ID
WiFiClient espClient;  // 建立WiFiClient物件
PubSubClient client(espClient);  // 建立WiFiClient的物件作為其參數 •

DHT dht(A13 , DHT22);
float temperature, humidity;  // 溫度值,濕度值

void connect_wifi() {  // 連線到Wi-Fi基地台
      Serial.print("Connecting");
      Serial.println(ssid);
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("Wi-Fi已連線");
    }
void connectMQTT() {  // 連線到MQTT經紀人
    Serial.println("Attempting MQTT connection...");
    if (client.connect(clientID.c_str(), mqttUser, mqttPassword)) {  // clientID, 帳號, 密碼
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=: ");
      Serial.println(client.state()); //輸出目狀態
      delay(5000);
    }
  }
void readDT22() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
}
void setup() {
  Serial.begin(115200);  // 啟動序列埠
  connect_wifi();      // 執行 Wi-Fi 連線
  dht.begin(); //DHT22初始化
  client.setServer(mqttServer, mqttPort);  // 設定 MQTT 經紀人
}

void loop() {
  if (!client.connected()) {  // 如果未和MQTT經紀人連線
    connectMQTT();            // 執行連線
  }
  client.loop();  // 一直檢查 • 以便接收訂閱的訊息
  readDT22();
  // 若數據有效才送出
  if (!isnan(humidity) && !isnan(humidity)) { // 改
    client.publish(publishTopic_temp, String(temperature).c_str());
    client.publish(publishTopic_humidity, String(humidity).c_str());   
  }


  delay(3000);  // 延時3秒後 • 再重複一次新的溫度
}