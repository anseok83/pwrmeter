// Copyright(c) 2021 Anseok Lee (anseok83_at_gmail_dot_com)
// MIT License
//
// Power meter sensor
// https://github.com/anseok83/pwrmeter

// Reference
// [OpenEnergyMonitor] https://learn.openenergymonitor.org/electricity-monitoring/ct-sensors/interface-with-arduino

// Version 0.0.2 (2021-04-01)

// Configurable parameters: measurement/report intervals
// WifiManager: parameter configuration windows (description displays)


//this needs to be first, or it all crashes and burns...
#include <FS.h>


///////////////////////////////////////////////
// Network device
///////////////////////////////////////////////

// ESP8266 (WiFi module)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WiFiUdp.h>
#include "time_ntp.h"
#include <SocketIOClient.h>

#include <aREST.h>

// Create aREST instance
aREST rest = aREST();

// Create an instance of the server on Port 80
WiFiServer server(80);

// Wifi manager
WiFiManager wifiManager;

// Power measurement interval [ms]
int intervalPwrMeas = 1000;

// Flag for saving data
bool shouldSaveConfig = false;

// Default wifi information
char ssid[20] = "";
char password[20] = "";

//String JSON;
SocketIOClient client;

// Ethernet address
uint8_t  MAC_STA[] = {0, 0, 0, 0, 0, 0};
char macaddr[20] = "";
char localip[20] = "";

///////////////////////////////////////////////
// Data
///////////////////////////////////////////////

// Storage for Measurements
// keep some mem free; allocate remainder
#define KEEP_MEM_FREE 10240

// Measurements
unsigned long ulMeasCount = 0;  // values already measured
unsigned long ulNoMeasValues = 0; // size of array
unsigned long ulMeasDelta_ms;   // distance to next meas time

// Measurement intervals
unsigned long previousMillis = 0;
unsigned long previousPwrMeas = 0;
bool isFirstUpdateCur = true;


///////////////////////////////////////////////
// Sensors
///////////////////////////////////////////////

#include "EmonLib.h"                   // Include Emon Library
EnergyMonitor emon1;                   // Create an instance

// Current power sensor

// PIN for WiFi setup reset
#define TRIGGER_PIN 15

// Pins for power sensors
#define CURRENT_POWER_PIN 5
#define CURRENT_MEASURE_PIN 0

// Windowing (averaging)
#define CURRENT_WINDOW_SIZE  20

//// Variables for current measure

float curValue[CURRENT_WINDOW_SIZE];
int curValueIndex = 0;
float curValueSum, curValueAvg = 0;
int lastCurValueIndex = 0;

// REST variable
float powerMeasure;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


void setup() {
  ////////////////
  // Setup routine
  ////////////////

  // setup serial port
  Serial.begin(115200);
  delay(1);

  // Init variables and expose them to REST API
  rest.variable("powerMeasure", &powerMeasure);

  //  // Function to be exposed

  // Give name & ID to the device (ID should be 6 characters long)
  rest.set_id("1");
  rest.set_name("powme");

  // setup pins
    pinMode(TRIGGER_PIN, INPUT);

  // Uncomment if you reset wifi configuration
//    Serial.print("Reset ESP Board");
//    delay(5000);
//    wifiManager.resetSettings();
//    ESP.reset();

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //fetches ssid and pass from eeprom and tries to connect
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("Powme") ) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  // Ethernet address lookup
  uint8_t* MAC = WiFi.macAddress(MAC_STA);
  for (int i = 0; i < 6; i++) {
    Serial.print(":");
    Serial.print(MAC[i], HEX);
  }
  Serial.println("");
  sprintf(macaddr, "%2X%2X%2X%2X%2X%2X", MAC[0], MAC[1], MAC[2], MAC[3], MAC[4], MAC[5]);
  Serial.print("MacAddrToStr: ");
  Serial.println(macaddr);

  // Local IP
  IPAddress myIp = WiFi.localIP();
  sprintf(localip, "%d.%d.%d.%d", myIp[0], myIp[1], myIp[2], myIp[3]);

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  // Start the server
  server.begin();
  Serial.println("Server started");

  // Setup IO pins
  pinMode(CURRENT_POWER_PIN, OUTPUT);

  emon1.current(CURRENT_MEASURE_PIN, 111.1);
}

void loop() {
  // Start measurement

  // If trigger pin is high, reset wifi configuration
  if ( digitalRead(TRIGGER_PIN) == HIGH ) {
    Serial.print("Reset ESP Board");
    delay(5000);
    wifiManager.resetSettings();
    ESP.reset();
  }

  // current time
  unsigned long currentMillis = millis();

  float instPower;

  // Measurement interval (Pwr sensor)
  if (currentMillis - previousPwrMeas > intervalPwrMeas)
  {
    previousPwrMeas = currentMillis;

    // Calculate Irms only
    double Irms = emon1.calcIrms(1480);  
    
    // Power
    Serial.println(Irms * 230.0);

    // Adjust I to Power value
    // Unit : kW
    // [Calibration]
    // Note: We used 120 ohm as a burden resistor.
    instPower = (Irms * 28.0 / 1000.0 - 0.3);

    // Pwr sensor windowing & averaging
    curValue[curValueIndex] = instPower;
    lastCurValueIndex = curValueIndex;
    curValueIndex = (curValueIndex + 1) % CURRENT_WINDOW_SIZE;

    // only for first update (intialization)
    if (isFirstUpdateCur == true)
    {
      for (int i = 1; i < CURRENT_WINDOW_SIZE; i++)
      {
        curValue[i] = curValue[0];
      }

      isFirstUpdateCur = false;
    }

    curValueSum = 0;
    for (int i = 0; i < CURRENT_WINDOW_SIZE; i++) {
      curValueSum += curValue[i];
    }

    curValueAvg = (curValueSum / CURRENT_WINDOW_SIZE);

    // Update rest value
    powerMeasure = curValueAvg;
  }

//
  // Check the connectivity
  // Wifi
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Wifi connection disconnected!");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }

  // Handle REST calls
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
  while(!client.available()){
    delay(1);
  }
  rest.handle(client);

}
