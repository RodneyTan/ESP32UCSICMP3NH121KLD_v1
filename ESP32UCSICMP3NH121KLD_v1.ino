// Solar Irradiance Monitoring Station at UCSI University KL Campus
// Station equipped with 10W Solar PV Panel, KLD1210 Charge Controller, 12V 3.2Ah Lead Acid Sealed Battery, 
// Kipp & Zonen CMP3 Pyranometer CMP3 and Nengh NH121 Dry Bulb Ambient Air Temperature Sensor connected to 
// ESP32 through ADS1115 ADC
// Developed By Rodney Tan (PhD)
// Ver 1.00 (Apr 2024)
#include <WiFi.h>
#include "esp_wpa2.h" //wpa2 library for connections to Enterprise networks
#include <C:\Users\rodne\Documents\Arduino\ESP32UCSICMP3\WiFiCredential.h>
#define EAP_ANONYMOUS_IDENTITY EAP_ID
#define EAP_IDENTITY EAP_ID //if connecting from another corporation, use identity@organisation.domain 
#define EAP_PASSWORD EAP_PW //your password
#define SSID SS_ID
int RSSI;

#include "time.h"
const char* ntpServer = "swntp.ucsi.edu.my";
const long  gmtOffset_sec = 28800; //GMT+8:00
const int   daylightOffset_sec = 0;

#include "ADS1X15.h"
ADS1115 ADS(0x48); // default address, ADDR connected to GND
float CMP3;
float NH121;
float Batt_Voltage;

#include "ThingSpeak.h"
unsigned long myChannelNumber = 1234567;
const char * myWriteAPIKey = "xxxxxxxxxx";
WiFiClient client;


void ConnectEnterpriseWiFi() {
  const char* wpa2e_ssid = SSID;
  int wpa2e_count = 0;
  delay(10);
  Serial.println();
  Serial.print("Connecting to network: " + String(wpa2e_ssid));
  WiFi.disconnect(true);  //disconnect form wifi to set new wifi connection
  WiFi.mode(WIFI_STA); //init wifi mode
  WiFi.begin(wpa2e_ssid, WPA2_AUTH_PEAP, EAP_ANONYMOUS_IDENTITY, EAP_IDENTITY, EAP_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    wpa2e_count++;
    if (wpa2e_count >= 60) { //after 30 seconds timeout - reset board
      Serial.println("\nCouldn't get a wifi connection. Restarting...");
      ESP.restart();
    }
  }
  Serial.print("\nConnected to UCSI Wifi with IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Connected Network Signal Strength (RSSI): ");
  Serial.println(WiFi.RSSI());  /*Print WiFi signal strength*/
}

void SyncNTP(){
  // Getting NTP server time
  Serial.print("Getting NTP server time.");

  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)){
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Successfully synced NTP server time to ESP32 local time");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}


void setup(){
  // Setup Serial Port
  Serial.begin(115200);
  Serial.println("ESP32UCSICMP3NH121KLD1210");
  // Connect to Wi-Fi
  ConnectEnterpriseWiFi();
  // Get NTP server time and set EPS32 RTC
  SyncNTP();
  // Setup ADC
  Wire.begin();
  ADS.begin();
  // Setup ThingSpeak
  ThingSpeak.begin(client);
}


void loop(){
  // Check for WiFi connectivity
  if (WiFi.status() != WL_CONNECTED) {
    ConnectEnterpriseWiFi();
  }
  
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  
  // Send data to Thingspeak at per minute rate
  if (timeinfo.tm_sec==0){
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    // Goto to sleep at 8pm
    if (timeinfo.tm_hour==20){
      esp_sleep_enable_timer_wakeup(600 * 60 * 1000000ULL); // Wake up timer in the next 10 hours
      esp_deep_sleep_start();
    }

    // Read ADC for CMP3 with sensitivity of 13.93uV/W/m^2
    ADS.setGain(16);  // Gain 16, Full Scale Range 0.256V, 7.8125uV per bit
    if (ADS.isReady()) {
      CMP3 = ((float)ADS.readADC(1)*7.8125e-6)/13.93e-6;
    }
    // Read ADC for NH121 with output signal of DC 0-2V for -40C to 60C range 
    ADS.setGain(2);  // Gain 2, Full Scale Range 2.048V, 62.5uV per bit
    if (ADS.isReady()) {
      NH121=((((float)ADS.readADC(0)*62.5e-6)/2)*100)-40;
    }
    // Read ADC for Batt Voltage 
    ADS.setGain(1);  // Gain 1, Full Scale Range 4.096V, 125uV per bit
    if (ADS.isReady()) {
      Batt_Voltage=(((float)ADS.readADC(2)*125e-6)*5);
    }
    // Get WiFi Network Strength
    RSSI = WiFi.RSSI();    

    // Set ThingSpeak Field
    ThingSpeak.setField(1, CMP3);
    ThingSpeak.setField(2, NH121);
    ThingSpeak.setField(3, Batt_Voltage);
    ThingSpeak.setField(4, RSSI);
    
    // Write to ThingSpeak
    int StatusCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    Serial.println(StatusCode);
    delay(1000); // prevent loop duplicate write
    
    // Sync RTC with NTP server every 30 mins
    if (timeinfo.tm_min==0 || timeinfo.tm_min==30){
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    }
  }
}
