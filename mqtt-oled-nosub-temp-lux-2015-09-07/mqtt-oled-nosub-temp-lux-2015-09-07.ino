//
// Arduino Nano + ESP8266-01 で MQTT ～温湿度、照度計（表示器付き）～
//
// この状態ではメモリが逼迫しておりコンパイル時点で動作に問題があるかもしれないと言われる
// よって、Subする部分は割愛
// 
// 個別URLやユーザー名パスワードは外出し


#include "SoftwareSerial.h"
#include <espduino.h>
#include <mqtt.h>
#include "config.h"
#include <Wire.h>


// I2C-OLED SO1602
#include <I2CLiquidCrystal.h>

// HDC1000

// TSL2561
#include "TSL2561.h"

SoftwareSerial debugPort(2, 3); // RX, TX
ESP esp(&Serial, &debugPort, 4);
MQTT mqtt(&esp);

// I2C-OLED SO1602
I2CLiquidCrystal lcd(0x3c, (uint8_t)127);
                  //  |             +--- contrast (0-255)
                  //  +-------- I2C ADDR (SA0=L: 0x3c, SA0=H: 0x3d)

// HDC1000


//// TSL2561
TSL2561 tsl(TSL2561_ADDR_FLOAT); 


boolean wifiConnected = false;
boolean canMqtt = false;
boolean beginLcd = false;
int i = 0;  // 割り込み（loop）回数をカウント

// SparkFun TSL2561
boolean gain;     // Gain setting, 0 = X1, 1 = X16;
unsigned int ms;  // Integration ("shutter") time in milliseconds


void wifiCb(void* response)
{
  uint32_t status;
  RESPONSE res(response);

  if(res.getArgc() == 1) {
    res.popArgs((uint8_t*)&status, 4);
    if(status == STATION_GOT_IP) {
      debugPort.println("WIFI CONNECTED");
      mqtt.connect(MQTT_BROKER_URL, MQTT_BROKER_PORT);
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
debugPort.println("PUB/SUB?");
canMqtt = true;

}
void mqttDisconnected(void* response)
{
  debugPort.println("disConnected");
}

// Subした時の処理 (このスケッチではSubしない)
void mqttData(void* response)
{
//  RESPONSE res(response);
//
//  debugPort.print("Received: topic=");
//  String topic = res.popString();
//  debugPort.println(topic);
//
//  debugPort.print("data=");
//  String data = res.popString();
//  debugPort.println(data);
// 
}

// Pubした時の処理
void mqttPublished(void* response)
{
  debugPort.println("Published");
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
// ここまで




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

// HDC1000
  pinMode(HDC1000_RDY_PIN, INPUT);
 
  delay(15); /* Wait for 15ms */
  configure();
 
  debugPort.print("Manufacturer ID = 0x");
  debugPort.println(getManufacturerId(), HEX);

// HDC1000ここまで

// TSL2561

  if (tsl.begin()) {
    debugPort.println("Found sensor");
  } else {
    debugPort.println("No sensor?");
    while (1);
  }


// ゲイン
    
  // You can change the gain on the fly, to adapt to brighter/dimmer light situations
  tsl.setGain(TSL2561_GAIN_0X);         // set no gain (for bright situtations)
  // tsl.setGain(TSL2561_GAIN_16X);      // set 16x gain (for dim situations)
  
  // Changing the integration time gives you a longer time over which to sense light
  // longer timelines are slower, but are good in very low light situtations!
  tsl.setTiming(TSL2561_INTEGRATIONTIME_13MS);  // shortest integration time (bright light)
  //tsl.setTiming(TSL2561_INTEGRATIONTIME_101MS);  // medium integration time (medium light)
  //tsl.setTiming(TSL2561_INTEGRATIONTIME_402MS);  // longest integration time (dim light)
  
  // Now we're ready to get readings!
// ここまで

}

void loop() {
  char buf[128];
  char *bufPt;
  float Humi;
  float Temp;
  char temperature_char[10]={0};
  char humidity_char[10]={0};
  char lux_char[10]={0};

  esp.process();
  if(wifiConnected) {
    if(canMqtt) {
      debugPort.println(i);

// debugPort.println("Loop in");

// HDC1000
  float temperature, humidity;
 
getTemperatureAndHumidity(&temperature, &humidity);
//  Serial.print("Temperature = ");
//  Serial.print(temperature);
//  Serial.print(" degree, Humidity = ");
//  Serial.print(humidity);
//  Serial.println("%");
// HDC1000 ここまで
//debugPort.println("Loop Temp got");

// TSL2561

//  // Simple data read example. Just read the infrared, fullspecrtrum diode 
//  // or 'visible' (difference between the two) channels.
//  // This can take 13-402 milliseconds! Uncomment whichever of the following you want to read
//  uint16_t x = tsl.getLuminosity(TSL2561_VISIBLE);     
//  //uint16_t x = tsl.getLuminosity(TSL2561_FULLSPECTRUM);
//  //uint16_t x = tsl.getLuminosity(TSL2561_INFRARED);
//  
//  debugPort.println(x, DEC);
//
//  // More advanced data read example. Read 32 bits with top 16 bits IR, bottom 16 bits full spectrum
//  // That way you can do whatever math and comparisons you want!
  uint32_t lum = tsl.getFullLuminosity();
  uint16_t ir, full;
  ir = lum >> 16;
  full = lum & 0xFFFF;
//  debugPort.print("IR: "); debugPort.print(ir);   debugPort.print("\t\t");
//  debugPort.print("Full: "); debugPort.print(full);   debugPort.print("\t");
//  debugPort.print("Visible: "); debugPort.print(full - ir);   debugPort.print("\t");
//  
//  debugPort.print("Lux: "); debugPort.println(tsl.calculateLux(full, ir));
  float Lux = tsl.calculateLux(full, ir);
//

//      delay(5000);
      
      dtostrf(temperature,4,1,temperature_char);
      dtostrf(humidity,4,1,humidity_char);
      dtostrf(Lux,4,1,lux_char);
      sprintf(buf, "{\"d\":{\"Temperature\": %s, \"Humidity\": %s, \"Lux\": %s}}", temperature_char, humidity_char,lux_char);
//      sprintf(buf, "{\"d\":{\"Temperature\": %s, \"Humidity\": %s}}", temperature_char, humidity_char);
      bufPt = &(buf[0]);
      mqtt.publish(MQTT_TOPIC_PUB, bufPt);
      debugPort.println(bufPt);



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
        lcd.print(" Hu:");
        lcd.println(humidity_char);
        lcd.setCursor(0, 1);
// TSL2561
        lcd.print("Lx:");
        lcd.print(lux_char);
  
//      debugPort.println("Delay Start");
    digitalWrite(13, LOW);
      delay(5000);
      i++;
//      debugPort.println("Loop out");
    digitalWrite(13, HIGH);
    }
  }
}
