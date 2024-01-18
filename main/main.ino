/*
 * Example
 *
 * If you encounter any issues:
 * - check the readme.md at https://github.com/sinricpro/esp8266-esp32-sdk/blob/master/README.md
 * - ensure all dependent libraries are installed
 * - see https://github.com/sinricpro/esp8266-esp32-sdk/blob/master/README.md#arduinoide
 * - see https://github.com/sinricpro/esp8266-esp32-sdk/blob/master/README.md#dependencies
 * - open serial monitor and check whats happening
 * - check full user documentation at https://sinricpro.github.io/esp8266-esp32-sdk
 * - visit https://github.com/sinricpro/esp8266-esp32-sdk/issues and check for existing issues or open a new one
 */

 // Custom devices requires SinricPro ESP8266/ESP32 SDK 2.9.6 or later

// Uncomment the following line to enable serial debug output
//#define ENABLE_DEBUG

/* Fill-in your Template ID (only if using Blynk.Cloud) */
#define BLYNK_TEMPLATE_ID "TMPL34xHrxouP"
#define BLYNK_TEMPLATE_NAME "Water Level Monitoring With ESP8266"
#define BLYNK_AUTH_TOKEN "P8Hwq-6EJlu8ge90TbW1lI-tCidvK1CY"

#ifdef ENABLE_DEBUG
  #define DEBUG_ESP_PORT Serial
  #define NODEBUG_WEBSOCKETS
  #define NDEBUG
#endif

#include <Arduino.h>
#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
  #include <ESPAsyncWebServer.h>//#include "ESPAsyncWebSrv.h"
  #include <WebSerialLite.h>
#endif
#ifdef ESP32
  #include <WiFi.h>
#endif

#include <AsyncElegantOTA.h>

#include <SinricPro.h>
#include "WaterLevelIndicator.h"
#include <Adafruit_SSD1306.h>
#include <BlynkSimpleEsp8266.h>

#define APP_KEY    "ad04dc1c-be73-4cc7-922f-cf4c131265ef"
#define APP_SECRET "1b7bb02f-d326-4219-b3f3-b22bc82d6601-52a58116-fb53-4e43-95a0-b11bb1c2acb9"
#define DEVICE_ID  "65a559e4ccc93539a124099b"

#define SSID       "Home_Network"//"JioFi3_5CC044_Asish"//"Home_Network"
#define PASS       "NetworkAdmin"//"Aishani2018"//"NetworkAdmin"

#define BAUD_RATE  115200

#define EVENT_WAIT_TIME   30000 // send event every 30 seconds

#if defined(ESP8266)
  const int trigPin = 12; //D6
  const int echoPin = 14; //D5
#elif defined(ESP32) 
  const int trigPin = 5;
  const int echoPin = 18;
#elif defined(ARDUINO_ARCH_RP2040)
  const int trigPin = 15;
  const int echoPin = 14;
#endif

#define wifiLed    D0
#define BuzzerPin  D3
#define VPIN_WATER_LEVEL    V1 ////for blynk app
#define VPIN_DISTANCE    V2 //for blynk app

char auth[] = BLYNK_AUTH_TOKEN;

const int EMPTY_TANK_HEIGHT = 90; // Height when the tank is empty - dhalai top 104, dhalai bottom - 92
const int FULL_TANK_HEIGHT  =  20; // Height when the tank is full - tank bottom to balloon - 81, dhalai top to balloon 10 //waterproof sensor min measure distance 20cm

WaterLevelIndicator &waterLevelIndicator = SinricPro[DEVICE_ID];

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Create AsyncWebServer object on port 80
// Set your access point network credentials
const char* ap_ssid = "ESP8266-Basement-Access-Point";
const char* ap_password = "123456789";
AsyncWebServer server(80);

long duration;
float distanceInCm; 
int waterLevelAsPer;
int lastWaterLevelAsPer;
float lastDistanceInCm;

// RangeController
void updateRangeValue(int value) {
  waterLevelIndicator.sendRangeValueEvent("rangeInstance1", value);
}

// PushNotificationController
void sendPushNotification(String notification) {
  waterLevelIndicator.sendPushNotification(notification);
}

long getDuration() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(15);

  digitalWrite(trigPin, LOW);
  return pulseIn(echoPin, HIGH);
}

void handleSensor() {
  if (SinricPro.isConnected() == false) {
    Serial.printf("SinricPro is disconnected. Please check!\r\n");
    WebSerial.println("SinricPro is disconnected. Please check!");
  }

  if(Blynk.connected() == false) {
    Serial.printf("Blynk is disconnected. Please check!\r\n");
    WebSerial.println("Blynk is disconnected. Please check!");
  }

  static unsigned long last_millis;
  unsigned long        current_millis = millis();
  if (last_millis && current_millis - last_millis < EVENT_WAIT_TIME) return; // Wait untill 30 secs
  last_millis = current_millis;

  int measurementsCounter = 30;
  duration = 0;
  long pulseDuration = 0;
  for (int i = 0; i < measurementsCounter; i++) {
    pulseDuration = getDuration();
    Serial.printf("pulseDuration ==>: %d\r\n", pulseDuration);
    WebSerial.printf("pulseDuration ==>: %d\r\n", pulseDuration);
    duration += pulseDuration;
    delay(100);
  }

  duration = duration/measurementsCounter;

  Serial.printf("duration read: %d..\r\n", duration); 
  WebSerial.printf("duration read: %d..\r\n", duration); 
  distanceInCm = duration / 29 / 2;
  
  if(distanceInCm <= 0) { 
    Serial.printf("Invalid reading: %d..\r\n", distanceInCm); 
    WebSerial.printf("Invalid reading: %d..\r\n", distanceInCm); 
    return;
  }
  
  if(lastDistanceInCm == distanceInCm) { 
    Serial.printf("Water level did not changed. do nothing...!\r\n");
    WebSerial.println("Water level did not changed. do nothing...!");
    return;
  }
  
  int change = abs(lastDistanceInCm - distanceInCm);
  if(change < 2) {
    Serial.println("Too small change in water level (waves?). Ignore...");
    WebSerial.println("Too small change in water level (waves?). Ignore...");
    return;
  }
  
  lastDistanceInCm = distanceInCm;
  waterLevelAsPer = map((int)distanceInCm ,EMPTY_TANK_HEIGHT, FULL_TANK_HEIGHT, 0, 100); 
  waterLevelAsPer = constrain(waterLevelAsPer, 1, 100);
  
  Serial.printf("Distance (cm): %f. %d%%\r\n", distanceInCm, waterLevelAsPer);
  WebSerial.printf("Distance (cm): %f. %d%%\r\n", distanceInCm, waterLevelAsPer);
  
  /* Update distance on server */
  Blynk.virtualWrite(VPIN_DISTANCE, distanceInCm);
  displayData();

  /* only play buzzer when water level changes in percentage */
  if(lastWaterLevelAsPer != waterLevelAsPer) {
    /* Update water level on server */  
    updateRangeValue(waterLevelAsPer);  
    Blynk.virtualWrite(VPIN_WATER_LEVEL, waterLevelAsPer);

  /* Send a push notification if the water level is too low! */
    if(waterLevelAsPer < 5) { 
      sendPushNotification("Water level is too low!");
      controlBuzzer(300);
      delay(100);
      controlBuzzer(300);
      delay(100);
      controlBuzzer(300);
      delay(100);
    } 

    /* when water level reaches 100 %*/
    if(waterLevelAsPer >= 100) { 
      controlBuzzer(700);
      delay(400);
      controlBuzzer(700);
      delay(400);
      controlBuzzer(700);
      delay(400);
    } 
  } 
  else {
    Serial.printf("Water level did not changed in Percentage. do nothing...!\r\n");
    WebSerial.printf("Water level did not changed in Percentage. do nothing...!\r\n");
  }  
}

void controlBuzzer(int duration){
  digitalWrite(BuzzerPin, HIGH);
  Serial.println(" BuzzerPin HIT");
  WebSerial.println(" BuzzerPin HIT");
  delay(duration);
  digitalWrite(BuzzerPin, LOW);
} 

/********* 
 * Setup *
 *********/

 void setupHttpServer(){
  server.on("/waterLevelAsPer", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("waterLevelAsPer api endpoint hit…");
    request->send_P(200, "text/plain", String(waterLevelAsPer).c_str());
  });

  server.on("/distance", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("test api endpoint hit…");
    request->send_P(200, "text/plain",  String(distanceInCm).c_str());
  });

  server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("distance api endpoint hit…");
    request->send_P(200, "text/plain", "This is test message");
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I'm water tank sensor server. You can upload new compiled binary (sketch -> export compiled binary menu) using ip/update url on the browser.");
  });

  AsyncElegantOTA.begin(&server);    // Start AsyncElegantOTA
  // WebSerial is accessible at "<IP Address>/webserial" in browser
  WebSerial.begin(&server);
  /* Attach Message Callback */
  WebSerial.onMessage(recvMsg);
  // Start server
  server.begin();
 }

void setupSensor() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void setupSinricPro() {
  SinricPro.onConnected([]{ Serial.printf("[SinricPro]: Connected\r\n"); WebSerial.println("[SinricPro]: Connected"); });
  SinricPro.onDisconnected([]{ Serial.printf("[SinricPro]: Disconnected\r\n"); WebSerial.println("[SinricPro]: Connected"); });
  SinricPro.begin(APP_KEY, APP_SECRET);
}

void setupNodeMCU(){
  pinMode(wifiLed, OUTPUT);
  pinMode(BuzzerPin, OUTPUT);
}

void setupVariables(){
  lastWaterLevelAsPer = 0;
  lastDistanceInCm = 0;
}

void setupDisplay(){
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    WebSerial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(1000);  
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.clearDisplay();
}

void displayData(){
  display.clearDisplay();
  display.setTextSize(3);
  if(waterLevelAsPer >= 100){
    display.setCursor(0,0);
  } else if (waterLevelAsPer <= 9) {
    display.setCursor(30,0);
  } else {
    display.setCursor(20,0);
  }  
  display.print(waterLevelAsPer);
  display.print(" ");
  display.print("%");
  display.setTextSize(1);
  display.print(" FULL");
  display.setTextSize(1);
  display.setCursor(10,25);
  display.print("Distance: ");
  display.print(distanceInCm);
  display.print(" ");
  display.print("cm");
  display.display();
}

BlynkTimer timer;
void setupBlynk(){
  timer.setInterval(60000L, checkBlynkStatus); // check if Blynk server is connected every 1 mins
  Blynk.config(auth);
  checkBlynkStatus();
}

void setupWiFi() {
  #if defined(ESP8266)
    WiFi.setSleepMode(WIFI_NONE_SLEEP); 
    WiFi.setAutoReconnect(true);
  #elif defined(ESP32)
    WiFi.setSleep(false); 
    WiFi.setAutoReconnect(true);
  #endif

  WiFi.mode(WIFI_AP_STA);
  // Setting the ESP as an access point
  Serial.print("[WiFi]: Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  if(WiFi.softAP(ap_ssid,ap_password,1,false)==true)
  {
    Serial.print("[WiFi]: Access Point is Created with SSID: ");
    Serial.println(ap_ssid);
    Serial.print("[WiFi]: Access Point IP: ");
    Serial.println(WiFi.softAPIP());
  }
  else
  {
    Serial.println("[WiFi]: Unable to Create Access Point");
  }

  WiFi.begin(SSID, PASS);
  Serial.printf("[WiFi]: Connecting to %s", SSID);
  WebSerial.printf("[WiFi]: Connecting to %s", SSID);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf(".");
    WebSerial.printf(".");
    delay(250);
  }

  Serial.println("");
  Serial.print("[WiFi]: Connected to ");
  Serial.println(SSID);
  Serial.print("[WiFi]: IP address: ");
  Serial.println(WiFi.localIP());

  WebSerial.println("");
  WebSerial.print("[WiFi]: Connected to ");
  WebSerial.println(SSID);
  WebSerial.print("[WiFi]: IP address: ");
  WebSerial.println(WiFi.localIP());
}

void checkBlynkStatus() { // called every 3 seconds by SimpleTimer

  bool isconnected = Blynk.connected();
  if (isconnected == false) {
    Serial.println("[Blynk]: Not Connected");
    WebSerial.println("[Blynk]: Not Connected");
    digitalWrite(wifiLed, HIGH);
  }
  if (isconnected == true) {
    digitalWrite(wifiLed, LOW);
    Serial.println("[Blynk]: Connected");
    WebSerial.println("[Blynk]: Connected");
  }
}

/* Message callback of WebSerial */
void recvMsg(uint8_t *data, size_t len){
  WebSerial.println("Received Data...");
  String d = "";
  for(int i=0; i < len; i++){
    d += char(data[i]);
  }
  WebSerial.println(d);
}

void setup() {
  Serial.begin(BAUD_RATE);
  setupVariables();
  setupSensor();
  setupNodeMCU();
  setupWiFi();
  setupHttpServer();
  setupSinricPro();
  setupBlynk();
  setupDisplay();
}

BLYNK_CONNECTED() {
  Blynk.syncVirtual(VPIN_WATER_LEVEL);
  Blynk.syncVirtual(VPIN_DISTANCE);
}

/********
 * Loop *
 ********/

void loop() {
  handleSensor();
  SinricPro.handle();
  Blynk.run();
  timer.run(); // Initiates SimpleTimer
  AsyncElegantOTA.loop();
}