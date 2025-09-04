#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include "Adafruit_CCS811.h"
#include <SPI.h>
#include <LoRa.h>
#define DHTPIN A13
//---------------------------------------- LoRa Pin / GPIO configuration.
#define nss 5
#define rst 14
#define lora_pin 2
//----------------------------------------
const char* ssid = "B2-7";       // 無線網路基地台的SSID
const char* password = "00000000";       // 無線網路基地台的密碼
// MQTT部分 • 函式區引用及參數設定
const char *mqttServer = "test.mosquitto.org";  // MQTT網誌
const int mqttPort = 1883;                      // 預設伺機埠號
const char *mqttUser = "";                      // 登入帳號
const char *mqttPassword = "";                  // 登入密碼
const char *publishTopic_temp = "/laizhizhi076/temperature";     // 需要的主題名稱
const char *publishTopic_humidity = "/laizhizhi076/humidity";
const char *publishTopic_CO2 = "/laizhizhi076/CO2_level";
String clientID = "HOLLE_I_just_want_pass";  // 建立的client ID
WiFiClient espClient;  // 建立WiFiClient物件
PubSubClient client(espClient);  // 建立WiFiClient的物件作為其參數 •

String Incoming = "";                   //LoRa incoming
byte LocalAddress = 0x01;               //--> address of this device (Master Address).
byte Destination_ESP32_Slave_1 = 0x02;  //--> destination to send to Slave 1 (ESP32).// Variable declaration for Millis/Timer.
unsigned long previousMillis_SendMSG = 0;

DHT dht(DHTPIN , DHT22);
float temperature, humidity;  // 溫度值,濕度值
Adafruit_CCS811 CO2;          //CO2 SENSOR物件
int dioxide;                //CO2的數值


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
void connected_mqtt() {
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
void publish_mqtt() {
  while (!client.connected()) {
    connected_mqtt();
  }
  // 若數據有效才送出
  if (!isnan(temperature) && !isnan(humidity) && !isnan(dioxide)) {
  client.publish(publishTopic_temp, String(temperature).c_str());
  client.publish(publishTopic_humidity, String(humidity).c_str());
    if(CO2.available()){
    if(!CO2.readData()){
      client.publish(publishTopic_CO2, String(dioxide).c_str()); 
    }
  }
  Serial.println("Data sent to MQTT");
  }
  delay(10000);
}
void set_lora() {
  LoRa.setPins(nss, rst, lora_pin);

  Serial.println("Start LoRa init...");
  if (!LoRa.begin(433E6)) {             // initialize ratio at 915 or 433 MHz
    Serial.println("LoRa init failed. Check your connections.");
    while (true);                       // if failed, do nothing
  }
  Serial.println("LoRa init succeeded.");
}
void readData() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  dioxide = CO2.geteCO2();
} 
void sendMessage(String Outgoing, byte Destination) {
  LoRa.beginPacket();             //--> start packet
  LoRa.write(Destination);        //--> add destination address
  LoRa.write(LocalAddress);       //--> add sender address
  LoRa.write(Outgoing.length());  //--> add payload length
  LoRa.print(Outgoing);           //--> add payload
  LoRa.endPacket();               //--> finish packet and send it 
}
void Lora_send(){
  String merge_to_CSV = String(temperature) + "," + String(humidity) + "," + String(dioxide);
  unsigned long currentMillis_SendMSG = millis();
  if (currentMillis_SendMSG - previousMillis_SendMSG >= 10000) {
    previousMillis_SendMSG = currentMillis_SendMSG;
    //::::::::::::::::: Condition for sending message / command data to Slave 1 (ESP32 Slave 1).
      Serial.println();
      Serial.println("Temperature :" + String(temperature));
      Serial.println("Humidity :" + String(humidity));
      if(CO2.available()){
      if(!CO2.readData()){
        Serial.println("CO2_level :" + String(dioxide));
        sendMessage(merge_to_CSV, Destination_ESP32_Slave_1);
        }
      }
  }
  //----------------------------------------
}
bool compare_rssi(){
  int wifi_rssi = WiFi.RSSI();
  int lora_rssi = LoRa.packetRssi();
  return (wifi_rssi >= lora_rssi) ? 0 : 1;
}
/*void onReceive(int packetSize) {
  if (packetSize == 0) return;  //--> if there's no packet, return

  //---------------------------------------- read packet header bytes:
  int recipient = LoRa.read();        //--> recipient address
  byte sender = LoRa.read();          //--> sender address
  byte incomingLength = LoRa.read();  //--> incoming msg length
  //---------------------------------------- 

  // Clears Incoming variable data.
  Incoming = "";

  //---------------------------------------- Get all incoming data.
  while (LoRa.available()) {
    Incoming += (char)LoRa.read();
  }
  //---------------------------------------- 

  //---------------------------------------- Check length for error.
  if (incomingLength != Incoming.length()) {
    Serial.println("error: message length does not match length");
    return; //--> skip rest of function
  }
  //---------------------------------------- 

  //---------------------------------------- Checks whether the incoming data or message for this device.
  if (recipient != LocalAddress) {
    Serial.println("This message is not for me.");
    return; //--> skip rest of function
  }
  //---------------------------------------- 

  //---------------------------------------- if message is for this device, or broadcast, print details:
  Serial.println();
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + Incoming);
  //Serial.println("RSSI: " + String(LoRa.packetRssi()));
  //Serial.println("Snr: " + String(LoRa.packetSnr()));
  //---------------------------------------- 
}*/
void setup() {
  Serial.begin(9600);   // 啟動序列埠
  connect_wifi();       // 執行 Wi-Fi 連線
  dht.begin();          //DHT22初始化
  CO2.begin(); //啟用CCS811
  client.setServer(mqttServer, mqttPort);  // 設定 MQTT 經紀人
  connected_mqtt();
  // set_lora();
}

void loop() {
  readData(); //測溫溼度
  // Serial.println("Temp: " + String(temperature) + " °C");
  // Serial.println("Humidity: " + String(humidity) + " %");
  // if(CO2.available()){
  //   if(!CO2.readData()){
  //     Serial.println("CO2: " + String(dioxide) + " ppm");
  //   }
  // }
  publish_mqtt();
  // Lora_send();
}