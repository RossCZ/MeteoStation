#include <ESP8266WiFi.h>
#include <ThingSpeak.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SSD1306.h>

// Wifi Settings
const char* ssid = "NetworkName";
const char* pass = "NetworkPassword";
const unsigned long maxAttempts = 30;
WiFiClient client;

// *** disable Wi-Fi connection *** 
//const unsigned long maxAttempts = 0;

// ThingSpeak Settings
// *** production ***
const unsigned long myChannelNumber = 1234;
const char* myWriteAPIKey = "apiKey1234";
// *** testing ***
//const unsigned long myChannelNumber = 5678
//const char* myWriteAPIKey = "apiKey5678";

// Display settings
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

// Initialize BME280 Sensor
Adafruit_BME280 bme;

// Initilaize DS18B20
#define DSPIN D3
OneWire oneWireDS(DSPIN);
DallasTemperature dsSensor(&oneWireDS);

// Time settings
// ThingSpeak has 15s update limit (use min 20s to be sure) !!!
const unsigned long writeTime = 600 * 1000;
const unsigned long refreshTime = 10 * 1000; // must be less than write time
const unsigned long processTime = 730; // guess of total process time between writes in order to ensure precise interval
const unsigned long infoTime = 1500;
const unsigned long loopFrequency = 1000;

// Variables inicialization
unsigned long refreshStart = 0;
unsigned long writeStart = 0;
bool enforceWrite = false;
bool enforceRefresh = false;

// *** Channel settings ***
// *** channel order ***
// 1 - temperature outside
// 2 - pressure
// 3 - temperature balcony
// 4 - temperature living room
// 5 - temperature bedroom
// 6 - humidity living room
// 7 - humidity bedroom

// *** sensor order ***
// 1 - BME280 temperature
// 2 - BME280 humidity
// 3 - BME280 pressure
// 4 - DS18B20 temperature

// connect sensors to channels (-1 -> not used)
// *** main ***
const int s1 = 5;
const int s2 = 7;
const int s3 = 2;
const int s4 = 1;
// *** auxiliary ***
//const int s1 = 4;
//const int s2 = 6;
//const int s3 = -1;
//const int s4 = 3;

void setup()
{
  Serial.begin(9600);
  delay(10);

  // initialize display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  PrintToDisplay("Initializing...", true);
  
  InitializeWiFi();

  // initialize DS senzor
  Serial.println("Initializing DS18B20...");
  dsSensor.begin();

  // Initialize BME280 Sensor
  Serial.println("Initializing BME280...");
  bme.begin(0x76);
  
  // Initialize ThingSpeak
  Serial.println("Initializing ThingSpeak...");
  ThingSpeak.begin(client);

  // Enforce startup server entry
  enforceWrite = true;
  enforceRefresh = true;
}

void loop()
{
  unsigned long currentTime = millis();

  // millis() will go back to zero (overflow) after 50 days, reset write and refresh starts as well
  if ((writeStart > writeTime * 2) && (currentTime < (writeStart - writeTime * 2)))
  {
    writeStart = currentTime;
    refreshStart = currentTime;
  }
  
  bool refresh = ((currentTime - refreshStart) >= refreshTime) || enforceRefresh;
  bool post = ((currentTime - writeStart + processTime) >= writeTime) || enforceWrite;

  if (refresh || post)
  {
    // perform data reading
    float tempIn = GetBME280Temperature();
    float humIn = GetBME280Humidity();
    float pressure = GetBME280Pressure();
    float tempOut = GetDS18B20Temperature();   
    
    if (refresh)
    {
      refreshStart = currentTime;
      enforceRefresh = false;
      
      RefreshInfoOnDisplay(tempIn, humIn, pressure, tempOut);
      SerialPrintData(tempIn, humIn, pressure, tempOut);
    }

    if (post)
    {
      writeStart = currentTime;
      enforceWrite = false;
      
      // Assess wifi connection
      if (WiFi.status() != WL_CONNECTED)
      {
        InitializeWiFi();
      }

      // Display info + enforce display refresh
      PrintToDisplay("Sending data to server", true);
      
      // Write to ThingSpeak
      if (!WriteDataToThingSpeak(tempIn, humIn, pressure, tempOut))
      {
        // Write was most probably attempted in the same 15 s interval as in the other station -> shift write time by the half of the write interval
        unsigned long writeShift = writeTime / 2;

        // Ensure, that write start (unsigned long) wont be less than 0 after shift (undefined behavior)
        if (writeStart > writeShift)
        {
          writeStart -= writeShift;
        }
      }

      // Enforce display refresh
      enforceRefresh = true;
    }
  }

  delay(loopFrequency);
}

void InitializeWiFi()
{
  WiFi.begin(ssid, pass); // WiFi connection
  delay(500);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts)
  {
    PrintToDisplay("Connecting to WiFi...", true);
    attempts++;
    delay(500);
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected");
  }
  else
  {
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void SerialPrintData(float tempIn, float humIn, float pressure, float tempOut)
{
  Serial.println("Temperature in: " + String(tempIn) + "°C");
  Serial.println("Humidity in: " + String(humIn) + "%");
  Serial.println("Pressure: " + String(pressure) + " hPa");
  Serial.println("Temperature out: " + String(tempOut) + "°C");
}

void RefreshInfoOnDisplay(float tempIn, float humIn, float pressure, float tempOut)
{
  SetupDisplay();

  if (s1 != -1)
  {
    String tempInStr = "Ti: " +  String(tempIn, 1) + " C";
    display.println(tempInStr);
  }

  if (s2 != -1)
  {
    String humInStr = "H:  " +  String(humIn, 1) + " %";
    display.println(humInStr);
  }

  if (s3 != -1)
  {
    String pressureStr = "P:  " +  String(pressure, 1) + " hPa";
    display.println(pressureStr);
  }

  if (s4 != -1)
  {
    String tempOutStr = "To: " +  String(tempOut, 1) + " C";
    display.println(tempOutStr);
  }
  
  display.display();
}

void PrintToDisplay(String firstLine, bool cleanUpAfter)
{
  SetupDisplay();
  display.println(firstLine);

  delay(infoTime);
  display.display();

  if (cleanUpAfter)
  {
    display.clearDisplay();
  }
}

void SetupDisplay()
{
  // Clear the buffer
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.setTextColor(WHITE);
}

bool WriteDataToThingSpeak(float tempIn, float humIn, float pressure, float tempOut)
{
    // need to update channels at once, for one by one updating time limit between updates applies
    Serial.println("Sending data to ThingSpeak...");
    if (!isnan(tempIn) && (s1 != -1))
    {
      ThingSpeak.setField(s1, String(tempIn, 1));
    }
    if (!isnan(humIn) && (s2 != -1))
    {
      ThingSpeak.setField(s2, String(humIn, 1));
    }
    if (!isnan(pressure) && (s3 != -1))
    {
      ThingSpeak.setField(s3, String(pressure, 1));
    }
    if (!isnan(tempOut) && (s4 != -1))
    {
      ThingSpeak.setField(s4, String(tempOut, 1));
    }

    int response = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    
    // Wait for the response and check the return code
    if (response == 200)
    {
      Serial.println("Channel update successful.");
      return true;
    }
    else
    {
      Serial.println("Problem updating channel. HTTP error code " + String(response));
      PrintToDisplay("HTTP error code " + String(response), true);
      return false;
    }
}

float GetDS18B20Temperature()
{
  dsSensor.requestTemperatures();
  float temperature = dsSensor.getTempCByIndex(0);
  if (temperature == -127.0 || temperature == 85.0)
  {
    Serial.println("Failed to read from DS18B20 sensor!");
    return sqrt (-1); // nan
  }
  else
  {
    return temperature;
  }
}

float GetBME280Pressure()
{
  float pressure = bme.readPressure() / 100.0;
  if (pressure == 0.0)
  {
    pressure = sqrt (-1); // nan
  }
  return pressure;
}

float GetBME280Temperature()
{
  float temperature = bme.readTemperature();
  if (temperature == 0.0)
  {
    temperature = sqrt (-1); // nan
  }
  return temperature;
}

float GetBME280Humidity()
{
  float hum = bme.readHumidity();
  if (hum == 0.0)
  {
    hum = sqrt (-1); // nan
  }
  return hum;
}
