#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <SPI.h>
#include <LoRa.h>
#define DHTPIN A13
#define DHTTYPE DHT11z
//---------------------------------------- LoRa Pin / GPIO configuration.
#define nss 5
#define rst 14
#define lora_pin 2
//----------------------------------------
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

String Incoming = ""; //LoRa incoming
String Message = "";  //LoRa message
byte LocalAddress = 0x01;               //--> address of this device (Master Address).
byte Destination_ESP32_Slave_1 = 0x02;  //--> destination to send to Slave 1 (ESP32).
byte Destination_ESP32_Slave_2 = 0x03;  //--> destination to send to Slave 2 (ESP32).
byte Slv = 0;                       // Variable declaration to count slaves.
// Variable declaration for Millis/Timer.
unsigned long previousMillis_SendMSG = 0;
const long interval_SendMSG = 1000;

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
void sendMessage(String Outgoing, byte Destination) {
  LoRa.beginPacket();             //--> start packet
  LoRa.write(Destination);        //--> add destination address
  LoRa.write(LocalAddress);       //--> add sender address
  LoRa.write(Outgoing.length());  //--> add payload length
  LoRa.print(Outgoing);           //--> add payload
  LoRa.endPacket();               //--> finish packet and send it
}
void onReceive(int packetSize) {
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
}
void setup() {
  Serial.begin(115200);  // 啟動序列埠
  connect_wifi();      // 執行 Wi-Fi 連線
  dht.begin(); //DHT22初始化
  client.setServer(mqttServer, mqttPort);  // 設定 MQTT 經紀人
  //---------------------------------------- Settings and start Lora Ra-02.
  LoRa.setPins(nss, rst, lora_pin);

  Serial.println("Start LoRa init...");
  if (!LoRa.begin(433E6)) {             // initialize ratio at 915 or 433 MHz
    Serial.println("LoRa init failed. Check your connections.");
    while (true);                       // if failed, do nothing
  }
  Serial.println("LoRa init succeeded.");
  //---------------------------------------- 
}

void loop() {
  // if (!client.connected()) {  // 如果未和MQTT經紀人連線
  //   connectMQTT();            // 執行連線
  // }
  // client.loop();  // 一直檢查 • 以便接收訂閱的訊息
  // readDT22();
  // // 若數據有效才送出
  // if (!isnan(humidity) && !isnan(humidity)) { // 改
  //   client.publish(publishTopic_temp, String(temperature).c_str());
  //   client.publish(publishTopic_humidity, String(humidity).c_str());   
  // }
  // delay(3000);  // 延時3秒後 • 再重複一次新的溫度

  unsigned long currentMillis_SendMSG = millis();
  
  if (currentMillis_SendMSG - previousMillis_SendMSG >= interval_SendMSG) {
    previousMillis_SendMSG = currentMillis_SendMSG;

    Slv++;
    if (Slv > 2) Slv = 1;

    Message = "SDS" + String(Slv);

    //::::::::::::::::: Condition for sending message / command data to Slave 1 (ESP32 Slave 1).
    if (Slv == 1) {
      Serial.println();
      Serial.print("Send message to ESP32 Slave " + String(Slv));
      Serial.println(" : " + Message);
      sendMessage(Message, Destination_ESP32_Slave_1);
    }
    //:::::::::::::::::

    //::::::::::::::::: Condition for sending message / command data to Slave 2 (UNO Slave 2).
    if (Slv == 2) {
      Serial.println();
      Serial.print("Send message to ESP32 Slave " + String(Slv));
      Serial.println(" : " + Message);
      sendMessage(Message, Destination_ESP32_Slave_2);
    }
    //:::::::::::::::::
  }
  //----------------------------------------

  //---------------------------------------- parse for a packet, and call onReceive with the result:
  onReceive(LoRa.parsePacket());
  //----------------------------------------
}