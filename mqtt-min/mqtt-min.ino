//
// Arduino Nano + ESP8266-01 で MQTT の最小テスト
//
// 5秒毎にA0(アナログピン)を読んでbrokerにPub（送信）しながら、Subを待ち受ける
// 届いたSub のメッセージが ”ON”または”OFF”なら、NanoのオンボードのLED（D13）をON-OFFする
// ５秒毎のきっかけはタイマーを使用
// 個別URLやユーザー名パスワードは外出し

// 参考先
//  \file  ESP8266 MQTT Bridge example \author  Tuan PM <tuanpm@live.com>

#include <SoftwareSerial.h>
#include <espduino.h>
#include <mqtt.h>
#include "config.h"  // Wi-FiのSSID、パスワード、MQTTのbrokerURL、MQTTクライアントの設定
#include <MsTimer2.h>  // タイマー割り込み(MsTimer2)


SoftwareSerial debugPort(2, 3); // RX, TX
ESP esp(&Serial, &debugPort, 4);
MQTT mqtt(&esp);
boolean wifiConnected = false;
int i = 0;  // 割り込み回数をカウント


// 
// MsTimer2 割り込み発生時の処理
//
void subscribeFrq(){

  char* data_buf;
  char buf[128];
  char *bufPt;
//  data_buf = "  From ESPduino";
  itoa(i, buf, 10);
  strcat(buf, data_buf);
  bufPt = &(buf[0]);
  mqtt.publish(MQTT_TOPIC_PUB, bufPt);
  debugPort.println(bufPt);

  int a = analogRead(A0);
  itoa(a, buf, 10);
  data_buf = "  AnalogA0";
  strcat(buf, data_buf);
  bufPt = &(buf[0]);
mqtt.publish(MQTT_TOPIC_PUB, bufPt);
debugPort.println(bufPt);
  i++;
}



void wifiCb(void* response)
{
  uint32_t status;
  RESPONSE res(response);

  if(res.getArgc() == 1) {
    res.popArgs((uint8_t*)&status, 4);
    if(status == STATION_GOT_IP) {
      debugPort.println("WIFI CONNECTED");
//      mqtt.connect("72d5w9.messaging.internetofthings.ibmcloud.com", 1883, false);
//      mqtt.connect(MQTT_BROKER_URL, MQTT_BROKER_PORT, false);
      mqtt.connect(MQTT_BROKER_URL, MQTT_BROKER_PORT);
      // true にすると 「client handshake start.」の先が進まない        
      wifiConnected = true;
      //or mqtt.connect("host", 1883); /*without security ssl*/
    } else {
      wifiConnected = false;
      mqtt.disconnect();
      debugPort.println("DISCONNECTED");
    }
    
  }
}


void mqttConnected(void* response)
{
  debugPort.println("Connected");
//  mqtt.subscribe("/topic/0"); //or mqtt.subscribe("topic"); /*with qos = 0*/
mqtt.subscribe(MQTT_TOPIC_SUB);
debugPort.println("PUB/SUB?");

}
void mqttDisconnected(void* response)
{

}

// Subした時の処理
void mqttData(void* response)
{
  RESPONSE res(response);

  debugPort.print("Received: topic=");
  String topic = res.popString();
  debugPort.println(topic);

  debugPort.print("data=");
  String data = res.popString();
  debugPort.println(data);
  if(data == "ON") {
    digitalWrite(13, HIGH);
  }
  if(data == "OFF") {
    digitalWrite(13, LOW);
  }
}

void mqttPublished(void* response)
{

}
void setup() {
  Serial.begin(19200);
  debugPort.begin(19200);
  esp.enable();
  delay(500);
  esp.reset();
  delay(500);
  while(!esp.ready());

  debugPort.println("ARDUINO: setup mqtt client");
//  if(!mqtt.begin("DVES_duino", "admin", "Isb_C4OGD4c3", 120, 1)) {
boolean mqttBegin = false;
mqttBegin = mqtt.begin(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PW, 120, 1);
debugPort.println(mqttBegin);
debugPort.println(MQTT_CLIENT_ID);
debugPort.println(MQTT_USERNAME);
debugPort.println(MQTT_PW);
  
  if(!mqttBegin) {
    debugPort.println("ARDUINO: fail to setup mqtt");
    while(1);
  }

  debugPort.println("ARDUINO: setup mqtt lwt");
  mqtt.lwt("/lwt", "offline", 0, 0); //or mqtt.lwt("/lwt", "offline");

/*setup mqtt events */
  mqtt.connectedCb.attach(&mqttConnected);
  mqtt.disconnectedCb.attach(&mqttDisconnected);
  mqtt.publishedCb.attach(&mqttPublished);
  mqtt.dataCb.attach(&mqttData);

  /*setup wifi*/
  debugPort.println("ARDUINO: setup wifi");
  esp.wifiCb.attach(&wifiCb);

  esp.wifiConnect(AP_SSID,AP_PASSWORD);

  debugPort.println("ARDUINO: system started");

  MsTimer2::set(5000, subscribeFrq); // 5000ms period　←5000ミリ秒おきに関数の実行を指定 
  MsTimer2::start();

}

void loop() {
  esp.process();
  if(wifiConnected) {
  
  }
}
