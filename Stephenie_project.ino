#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <ESPmDNS.h> 
#include "Protocentral_MAX30205.h" // USING YOUR WORKING LIBRARY

// --- Pin Definitions ---
#define I2C_SDA_PIN     8
#define I2C_SCL_PIN     9
#define SPI_MOSI_PIN    7
#define SPI_MISO_PIN    2
#define SPI_SCK_PIN     6
#define SD_CS_PIN       10
#define FSR1_PIN        0  
#define FSR2_PIN        1  

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define FSR_THRESHOLD 500  
#define LOG_INTERVAL 1000  // 1 Second updates

const char* ssid = "Jesus is Lord";
const char* password = "ROBOTICS";
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;

// --- MQTT Topics ---
const char* topic_data = "bonatech/spinalbrace/data"; 
const char* topic_cmd = "bonatech/spinalbrace/commands";
const char* topic_diag = "bonatech/spinalbrace/diag";

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RTC_DS3231 rtc;
SPIClass spi(FSPI);
WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80); 
MAX30205 tempSensor; // Protocentral Object

// --- Global Variables ---
unsigned long lastLogTime = 0;
unsigned long totalWornSeconds = 0; 
char currentFileName[20]; 
float currentTemp = 0.0; 

void setup() {
  Serial.begin(115200);
  delay(2000); 
  
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  
  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.clearDisplay(); display.setTextColor(WHITE); display.setTextSize(1);
  }
  
  rtc.begin();
  if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  
  DateTime now = rtc.now();
  snprintf(currentFileName, sizeof(currentFileName), "/%04d-%02d-%02d.csv", now.year(), now.month(), now.day());

  // Use the exact Protocentral initialization you provided
  if(!tempSensor.scanAvailableSensors()){
    Serial.println("Couldn't find the temperature sensor.");
  } else {
    tempSensor.begin(); 
  }

  spi.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SD_CS_PIN);
  if (SD.begin(SD_CS_PIN, spi)) {
    if (!SD.exists(currentFileName)) {
      File dataFile = SD.open(currentFileName, FILE_WRITE);
      if (dataFile) {
        dataFile.println("Timestamp,FSR1,FSR2,WornTime(s),Temp(C),Status");
        dataFile.close();
      }
    }
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  // Initialize mDNS as brace1.local
  if (!MDNS.begin("brace1")) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started: http://brace1.local");
  }

  server.on("/download", handleDownload);
  server.begin();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
}

void handleDownload() {
  if (server.hasArg("date")) {
    String dateStr = server.arg("date");
    String path = "/" + dateStr + ".csv";
    
    if (SD.exists(path)) {
      File file = SD.open(path, FILE_READ);
      server.sendHeader("Content-Disposition", "attachment; filename=\"" + dateStr + ".csv\"");
      server.streamFile(file, "text/csv");
      file.close();
    } else {
      server.send(404, "text/plain", "ERROR: Offline Log file not found for " + dateStr);
    }
  } else {
    server.send(400, "text/plain", "ERROR: Date parameter missing");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) { msg += (char)payload[i]; }
  
  if (String(topic) == topic_cmd) {
    if (msg.indexOf("run_diagnostics") >= 0) {
      String sdStatus = SD.exists(currentFileName) ? "OK" : "ERROR";
      
      float t = tempSensor.getTemperature();
      if(t < 0.0) t = t + 64.0;
      String tempStatus = (t > 0.0 && t < 100.0) ? "OK" : "ERROR";
      
      int f1 = analogRead(FSR1_PIN);
      int f2 = analogRead(FSR2_PIN);
      
      String diagStr = "{\"sd\":\"" + sdStatus + "\", \"temp\":\"" + tempStatus + "\", \"fsr1\":" + String(f1) + ", \"fsr2\":" + String(f2) + "}";
      client.publish(topic_diag, diagStr.c_str());
    }
  }
}

void reconnectMQTT() {
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    String clientId = "SpinalBrace-" + String(random(0xffff), HEX);
    if(client.connect(clientId.c_str())) {
      client.subscribe(topic_cmd); 
    }
  }
}

void loop() {
  server.handleClient(); 
  if (!client.connected()) reconnectMQTT();
  client.loop(); 

  unsigned long currentMillis = millis();
  
  if (currentMillis - lastLogTime >= LOG_INTERVAL) {
    lastLogTime = currentMillis;
    
    int fsr1Value = analogRead(FSR1_PIN);
    int fsr2Value = analogRead(FSR2_PIN);
    bool isWornCorrectly = (fsr1Value > FSR_THRESHOLD || fsr2Value > FSR_THRESHOLD);
    if(isWornCorrectly) totalWornSeconds++; 

    DateTime now = rtc.now();
    
    char newFileName[20];
    snprintf(newFileName, sizeof(newFileName), "/%04d-%02d-%02d.csv", now.year(), now.month(), now.day());
    if (String(newFileName) != String(currentFileName)) {
      strcpy(currentFileName, newFileName);
      if (!SD.exists(currentFileName)) {
        File dataFile = SD.open(currentFileName, FILE_WRITE);
        if (dataFile) { dataFile.println("Timestamp,FSR1,FSR2,WornTime(s),Temp(C),Status"); dataFile.close(); }
      }
    }

    int hr12 = now.twelveHour(); if (hr12 == 0) hr12 = 12; 
    char timeStr12[15]; snprintf(timeStr12, sizeof(timeStr12), "%02d:%02d:%02d %s", hr12, now.minute(), now.second(), now.isPM() ? "PM" : "AM");
    char fullDateStr[25]; snprintf(fullDateStr, sizeof(fullDateStr), "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    
    // --- EXACT PROTOCENTRAL TEMP LOGIC YOU PROVIDED ---
    currentTemp = tempSensor.getTemperature();
    if (currentTemp < 0.0) {
      currentTemp = currentTemp + 64.0;
    }
    // Filter out wild garbage readings just in case
    if (currentTemp > 100.0) currentTemp = 0.0; 

    String statusStr = isWornCorrectly ? "Compliant" : "Removed";
    
    updateDisplay(timeStr12, isWornCorrectly, totalWornSeconds, currentTemp, (WiFi.status() == WL_CONNECTED));

    File dataFile = SD.open(currentFileName, FILE_APPEND);
    if (dataFile) {
      dataFile.println(String(fullDateStr) + "," + String(fsr1Value) + "," + String(fsr2Value) + "," + String(totalWornSeconds) + "," + String(currentTemp) + "," + statusStr);
      dataFile.close();
    }

    if (client.connected()) {
      String jsonPayload = "{\"timestamp\":\"" + String(fullDateStr) + "\",\"fsr1\":" + String(fsr1Value) + ",\"fsr2\":" + String(fsr2Value) + ",\"worn_time\":" + String(totalWornSeconds) + ",\"temp\":" + String(currentTemp) + ",\"status\":\"" + statusStr + "\"}";
      client.publish(topic_data, jsonPayload.c_str());
    }
  }
}

void updateDisplay(const char* timeStr, bool compliant, unsigned long totalSecs, float temp, bool isOnline) {
  display.clearDisplay(); display.setTextColor(WHITE, BLACK);
  if (isOnline) { display.fillRect(0, 8, 2, 2, WHITE); display.fillRect(3, 6, 2, 4, WHITE); display.fillRect(6, 4, 2, 6, WHITE); display.fillRect(9, 2, 2, 8, WHITE); } 
  else { display.drawLine(0, 2, 10, 10, WHITE); display.drawLine(0, 10, 10, 2, WHITE); }
  
  int16_t x1, y1; uint16_t w, h; display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(SCREEN_WIDTH - w, 0); display.print(timeStr);
  display.drawLine(0, 12, SCREEN_WIDTH, 12, WHITE); display.setCursor(0, 18);
  display.print("Temp: ");
  if (temp == 0.0) display.println("-- C"); else { display.print(temp, 1); display.println(" C"); }
  
  unsigned long hrs = totalSecs / 3600; unsigned long mins = (totalSecs % 3600) / 60; unsigned long secs = totalSecs % 60;
  char timeFmt[25]; snprintf(timeFmt, sizeof(timeFmt), "%02lu h %02lu m %02lu s", hrs, mins, secs); 
  display.setCursor(0, 32); display.print("Worn: "); display.println(timeFmt);

  if(compliant) { display.fillRect(0, 48, SCREEN_WIDTH, 16, WHITE); display.setTextColor(BLACK, WHITE); display.setCursor(30, 52); display.println("COMPLIANT"); } 
  else { display.drawRect(0, 48, SCREEN_WIDTH, 16, WHITE); display.setTextColor(WHITE, BLACK); display.setCursor(35, 52); display.println("REMOVED"); }
  display.display();
}