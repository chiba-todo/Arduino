/*
  MQTT client
  5秒に1回Pub（インターバルは不正確）しながら、Subする。
  SubしたメッセージががON/OFFなら、13番のLED

  Circuit:
  * WiFi shield attached
  * Intel Edison でもOK！ 
  Intel Edison のWi-Fi機能は、Arduinoで使用するときWiFi shield と互換性があるので
  WiFi.h がそのまま使える。
  Wi-Fi接続が確立した後、MQTTの接続を行う
  
  WiFiWebClient.inoの HTTP処理を mqtt_basic.inoの処理で置き換え 
  PubSubClient.h のリファレンス
  http://knolleary.net/arduino-client-for-mqtt/api/
  PubSubClient.h の参考
  http://m2mio.tumblr.com/post/30048662088/a-simple-example-arduino-mqtt-m2mio

*/

#include <SPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
// #include <MsTimer2.h>  // タイマー割り込み(MsTimer2)
// avr/interrupt.h が必要なので、Edison-Arduinoでは使用できない。

#include <Wire.h>
#include "config.h"
#include <SparkFunTSL2561.h>


// I2C-OLED SO1602
#include <I2CLiquidCrystal.h>
/*
http://n.mtng.org/ele/arduino/i2c.html
*/


// HDC1000

// TSL2561
// Create an SFE_TSL2561 object, here called "light":

SFE_TSL2561 light;

// Global variables:

boolean gain;     // Gain setting, 0 = X1, 1 = X16;
unsigned int ms;  // Integration ("shutter") time in milliseconds

// ここまで TSL2561


//char ssid[] = "yourNetwork"; //  your network SSID (name) 
//char pass[] = "secretPassword";    // your network password (use for WPA, or use as key for WEP)
char ssid[] = AP_SSID; //  SSIDを設定する
char pass[] = AP_PASSWORD;    // SSIDに対応するパスワードを設定する
int keyIndex = 0;            // your network key Index number (needed only for WEP)

// MQTTのパラメータ
char server[] = MQTT_BROKER_URL;    // MQTT broker (サーバー) のアドレス（IPまたはサーバーURL）

// ローカル仮想マシンmosquittoがインストールされてマシンが立ち上がっていれば、
// 起動操作などなしに brokerとしてはたらく。
// テスト時、他機のターミナルから発行するコマンド
// subするとき：mosquitto_sub -d -t outTopic
// pubするとき：mosquitto_pub -h 192.168.0.20 -p 1883 -t inTopic -m "ON"


unsigned long timer;    // loop() の時間計測用
int cont = 0;  // 割り込み（loop）回数をカウント
boolean beginLcd = false;    // LCD初期化済みフラグ

int status = WL_IDLE_STATUS;
//

WiFiClient wfclient;   // クライアントのオブジェクト（Wi-Fi）
PubSubClient client(server, MQTT_BROKER_PORT, callback, wfclient);   // クライアントのオブジェクト（MQTT）

// I2C-OLED SO1602
I2CLiquidCrystal lcd(0x3c, (uint8_t)127);
                  //  |             +--- contrast (0-255)
                  //  +-------- I2C ADDR (SA0=L: 0x3c, SA0=H: 0x3d)


// HDC1000


// sub でメッセージが来た時の処理
char message_buff[100];
void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  int i = 0;

  Serial.println("Message arrived:  topic: " + String(topic));
  Serial.println("Length: " + String(length,DEC));
  
  // create character buffer with ending null terminator (string)
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  String msgString = String(message_buff);
  Serial.println("Payload: " + msgString);
  
  String data = msgString;
  Serial.println(data);
  if(data == "ON") {
    digitalWrite(13, HIGH);
  }
  if(data == "OFF") {
    digitalWrite(13, LOW);
  }
  
}

// HDC1000の処理
void configure() {
  Wire.beginTransmission(HDC1000_ADDRESS);
  Wire.write(HDC1000_CONFIGURATION_POINTER);
  Wire.write(HDC1000_CONFIGURE_MSB);
  Wire.write(HDC1000_CONFIGURE_LSB);
  Wire.endTransmission();
}
 
int getManufacturerId() {
  int manufacturerId;
 
  Wire.beginTransmission(HDC1000_ADDRESS);
  Wire.write(HDC1000_MANUFACTURER_ID_POINTER);
  Wire.endTransmission();
  Wire.requestFrom(HDC1000_ADDRESS, 2);
  while (Wire.available() < 2) {
    ;
  }

  manufacturerId = Wire.read() << 8;
  manufacturerId |= Wire.read();

  return manufacturerId;
}
 
void getTemperatureAndHumidity(float *temperature, float *humidity) {
  unsigned int tData, hData;
 
  Wire.beginTransmission(HDC1000_ADDRESS);
  Wire.write(HDC1000_TEMPERATURE_POINTER);
  Wire.endTransmission();
 
  while (digitalRead(HDC1000_RDY_PIN) == HIGH) {
    ;
  }
 
  Wire.requestFrom(HDC1000_ADDRESS, 4);
  while (Wire.available() < 4) {
    ;
  }
 
  tData = Wire.read() << 8;
  tData |= Wire.read();
 
  hData = Wire.read() << 8;
  hData |= Wire.read();
 
  *temperature = tData / 65536.0 * 165.0 - 40.0;
  *humidity = hData / 65536.0 * 100.0;
}
// HDC1000ここまで



void setup() {
  pinMode(13, OUTPUT);
  // Wi-Fi接続
  // このあたりは、Wi-Fiシールド用サンプルのまま
  //Initialize serial and wait for port to open:
  Serial.begin(9600); 
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  
  
  // LCDであいさつ

  if (!beginLcd){
    lcd.begin(16, 2);
    beginLcd = true;
  }
  lcd.setCursor(0, 0);
  lcd.print("Connecting to ...    ");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  
  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present"); 
    // don't continue:
    while(true);
  } 

  String fv = WiFi.firmwareVersion();
  if( fv != "1.1.0" )
    Serial.println("Please upgrade the firmware");
  
  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) { 
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:    
    status = WiFi.begin(ssid, pass);
     // wait 10 seconds for connection:
//    delay(10000);  
  lcd.setCursor(0, 1);
  lcd.print(server);
    delay(5000);          // 10秒は、待ちすぎ？
  } 
  Serial.println("Connected to wifi");
  printWifiStatus();                             // Wi-Fi 接続できた！

// MQTT 接続  
  Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:


  
  if (client.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PW)) {
  // ３つのパラメータは、それぞれ mosquitto_sub/pub の -i -u -P に対応  
    Serial.println("connected to server");
    client.publish(MQTT_TOPIC_PUB,"おはよう！");
    client.subscribe(MQTT_TOPIC_SUB);
  }
//  client.subscribe("inTopic");
// client.loop(); // ここでこの処理をしてもsubできない


// HDC1000
Wire.begin();
  pinMode(HDC1000_RDY_PIN, INPUT);
 
  delay(15); /* Wait for 15ms */
  configure();
 
  Serial.print("Manufacturer ID = 0x");
  Serial.println(getManufacturerId(), HEX);

// HDC1000ここまで


// TSL2561

  Serial.println("TSL2561 example sketch");

  // Initialize the SFE_TSL2561 library

  // 特に指定しなければ、 light.begin() で読みに行く I2Cアドレスは、0x39。
  // 基板のジャンパで設定すれば、0x29 または 0x49 に変更できる。
  // 詳しくは、 https://learn.sparkfun.com/tutorials/getting-started-with-the-tsl2561-luminosity-sensor

  light.begin();

  // ファクトリーIDを取得して確認（取得できなかったらエラー）
  unsigned char ID;
  
  if (light.getID(ID))
  {
    Serial.print("Got factory ID: 0X");
    Serial.print(ID,HEX);
    Serial.println(", should be 0X5X");
  }
  else
  {
    byte error = light.getError();
    printError(error);
  }

  // ゲインは1倍に
  // gain = 0 なら 1倍
  // gain = 0 なら 16倍
  gain = 0;

  // 測定時間は、402ms
  // If time = 0, integration will be 13.7ms
  // If time = 1, integration will be 101ms
  // If time = 2, integration will be 402ms
  // If time = 3, use manual start / stop to perform your own integration

  unsigned char time = 2;

  // setTiming() will set the third parameter (ms) to the
  // requested integration time in ms (this will be useful later):
  
  Serial.println("Set timing...");
  light.setTiming(gain,time,ms);

  // To start taking measurements, power up the sensor:
  
  Serial.println("Powerup...");
  light.setPowerUp();
  
  // The sensor will now gather light during the integration time.
  // After the specified time, you can retrieve the result from the sensor.
  // Once a measurement occurs, another integration period will start.

// TSL2561 ここまで

  lcd.clear();
}

void loop() {

  char buf[128];
  char *bufPt;
  char temperature_char[10]={0};       // Edisonのsprinf では、%f が使えるので、
  char humidity_char[10]={0};          // 文字列変換はしなくてよいが、LCD用に変換する。
  char lux_char[10]={0};

// MQTT接続が切れていた場合、再接続（実施しなくても？）
  if (!client.connected()) {
    client.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PW); 
  // ３つのパラメータは、それぞれ mosquitto_sub/pub の -i -u -P に対応  
    Serial.println("connected to server");

    client.publish(MQTT_TOPIC_PUB,"おはよう！(loop()内)");
    client.subscribe(MQTT_TOPIC_SUB);
  }
// 5秒ごとに実行
  if (millis() > (timer + 5000)) {    // millis()は，Arduinoを起動してからの時間をミリ秒で返す。
    timer = millis();

// HDC1000
    float temperature, humidity;
 
    getTemperatureAndHumidity(&temperature, &humidity);
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.print(" degree, Humidity = ");
  Serial.print(humidity);
  Serial.println("%");
// HDC1000 ここまで

// TSL2561
  double lux;    // Resulting lux value
  boolean good;  // True if neither sensor is saturated

  // Wait between measurements before retrieving the result
  // (You can also configure the sensor to issue an interrupt
  // when measurements are complete)
  
  // This sketch uses the TSL2561's built-in integration timer.
  // You can also perform your own manual integration timing
  // by setting "time" to 3 (manual) in setTiming(),
  // then performing a manualStart() and a manualStop() as in the below
  // commented statements:
  
  // ms = 1000;
  // light.manualStart();
  delay(ms);
  // light.manualStop();
  
  // Once integration is complete, we'll retrieve the data.
  
  // There are two light sensors on the device, one for visible light
  // and one for infrared. Both sensors are needed for lux calculations.
  
  // Retrieve the data from the device:

  unsigned int data0, data1;
  
  if (light.getData(data0,data1))
  {
    // getData() returned true, communication was successful
    
    Serial.print("data0: ");
    Serial.print(data0);
    Serial.print(" data1: ");
    Serial.print(data1);
  
    // To calculate lux, pass all your settings and readings
    // to the getLux() function.
    
    // The getLux() function will return 1 if the calculation
    // was successful, or 0 if one or both of the sensors was
    // saturated (too much light). If this happens, you can
    // reduce the integration time and/or gain.
    // For more information see the hookup guide at: https://learn.sparkfun.com/tutorials/getting-started-with-the-tsl2561-luminosity-sensor
  
//    double lux;    // Resulting lux value
//    boolean good;  // True if neither sensor is saturated
    
    // Perform lux calculation:

    good = light.getLux(gain,ms,data0,data1,lux);
    
    // Print out the results:
  
    Serial.print(" lux: ");
    Serial.print(lux);
    if (good) Serial.println(" (good)"); else Serial.println(" (BAD)");
  }
  else
  {
    // getData() returned false because of an I2C error, inform the user.

    byte error = light.getError();
    printError(error);
  }


// ここまで TSL2561

    // LCD用変換
    sprintf(temperature_char, "%2.2f",temperature);
    sprintf(humidity_char, "%2.2f", humidity);
    sprintf(lux_char, "%3.1lf", lux);

    // MQTT用の電文を作る    
//    sprintf(buf, "{\"d\":{\"Temperature\": %s, \"Humidity\": %s, \"Lux\": %s}}", temperature_char, humidity_char,lux_char);
    sprintf(buf, "{\"d\":{\"Counter\": %d, \"Temperature\": %2.2f, \"Humidity\": %2.2f, \"Lux\": %3.1lf}}", cont, temperature, humidity,lux);
    bufPt = &(buf[0]);
    client.publish(MQTT_TOPIC_PUB,bufPt);

    Serial.print("published:timer:");
    Serial.println(timer);
    Serial.println(bufPt);

      // I2C-OLED SO1602
      if (!beginLcd){
        lcd.begin(16, 2);
        beginLcd = true;
      }
      //
        lcd.setCursor(0, 0);
      //  lcd.home();
        lcd.print("Te:");
        lcd.print(temperature_char);
//        lcd.print(temperature);
        lcd.print(" Hu:");
        lcd.println(humidity_char);
//        lcd.println(humidity);
        lcd.setCursor(0, 1);
// TSL2561
        lcd.print("Lx:");
        lcd.print(lux_char);
//        lcd.print(lux);


    
    cont++;
  }

  client.loop();   // 常時subするのに必要
 
  // if there are incoming bytes available 
  // from the server, read them and print them:
//  while (wfclient.available()) {
//    char c = wfclient.read();
//    Serial.write(c);
//  }

//  // if the server's disconnected, stop the client:
//  if (!wfclient.connected()) {
//    Serial.println();
//    Serial.println("disconnecting from server.");
//    wfclient.stop();
//
//    // do nothing forevermore:
//    while(true);
//  }
}


void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

// TSL2561 用（エラー処理）

void printError(byte error)
  // If there's an I2C error, this function will
  // print out an explanation.
{
  Serial.print("I2C error: ");
  Serial.print(error,DEC);
  Serial.print(", ");
  
  switch(error)
  {
    case 0:
      Serial.println("success");
      break;
    case 1:
      Serial.println("data too long for transmit buffer");
      break;
    case 2:
      Serial.println("received NACK on address (disconnected?)");
      break;
    case 3:
      Serial.println("received NACK on data");
      break;
    case 4:
      Serial.println("other error");
      break;
    default:
      Serial.println("unknown error");
  }
}
