// Firmware version: s3-1.0.5

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32httpUpdate.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <Adafruit_NeoPixel.h>
#include <esp_task_wdt.h>

#define LED_PIN 48
#define NUM_PIXELS 1
#define WDT_TIMEOUT 120 // Watchdog Timeout in seconds

bool inAPMode = false;
bool bluetooth_sending_status = false;
bool inSensorSearchingMode = false;
unsigned long previousMillis = 0;
unsigned long previousMillisForAPMode = 0;
unsigned long previousMillisForUpdateCheck = 0;
int led_state = 0;
int number_of_failed_attempts_to_connect_to_server = 0;
int number_of_failed_responses_from_server = 0;
int max_number_of_failed_attempts = 4;
int max_number_of_failed_responses = 3;
int updateCheckTimer = 0;
bool automatically_put_to_AP_mode = false;

String tankSize = "NA";
String timeZone = "NA";
String longitude = "NA";
String latitude = "NA";
String loadedHeight = "NA";
String gatewayName = "NA";
String selected_sensor_mac_address = "NA";
String current_fw_version = "NA";
String CHIPID = "123456";
String ap_ssid = "Gateway";
String postData;
String apiKey = "NA";

const int scanTimeSeconds = 1;
const int BOOT_PIN = 0;
const int SSID_ADDR = 0;
const int PASS_ADDR = 50;
const int TANKSIZE_ADDR = 100;
const int TIMEZONE_ADDR = 150;
const int LONGITUDE_ADDR = 200;
const int LATITUDE_ADDR = 250;
const int LOADED_HEIGHT_ADDR = 300;
const int SENSOR_MAC_ADDR = 350;
const int FW_VERSION_ADDR = 400;
const int GATEWAY_NAME_ADDR = 450;
const int API_KEY_ADDR = 500;
const int EEPROM_SIZE = 1024;
const int LED_brightness = 70;
const int httpsPort = 443;
const int sound_speed = 757;
const int data_packet_sending_time = 10000;
const int delay_after_failed_attempt_to_send_data = 2000;
const long blink_interval = 500;          // Blink interval in milliseconds
const long fw_update_interval = 60000;    // Firmware update check interval in milliseconds
const long wifi_search_interval = 120000; // Wi-Fi search interval in milliseconds
const char *ap_password = "123456789";

const char *serverHost = "gateway.industryx.io";
const char *apiPath = "/api/v1/gas/stream/esp32_que"; // API endpoint

// old server details
//  const char *serverHost = "elysiumapi.overleap.lk";
//  const char *apiPath = "/api/v2/gas/stream/esp32_que"; // API endpoint

static char apSsidBuffer[50];
static unsigned long lastSendTime = 0;

BLEScan *pBLEScan;

WebServer server(80);
WiFiClientSecure client;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);

Adafruit_NeoPixel strip(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

void indicateSuccessfulConnection();
void indicateUnsuccessfulConnection();
void indicateGatewayStart();
void indicateReadyToReceiveData();
bool tryConnectToSavedWiFi();
void doUpdate(const String &new_fw_version, const String &old_fw_version);
void check_for_fw_updates(int interval);
bool handleButtonPress();
void blinkRGBLedInPattern(int numTimes, int red, int green, int blue, int onTime1, int offTime1, int onTime2 = 0, int offTime2 = 0);

std::string string_to_hex(const std::string &input)
{
  static const char hex_digits[] = "0123456789abcdef";
  std::string output;
  output.reserve(input.length() * 2);
  for (unsigned char c : input)
  {
    output.push_back(hex_digits[c >> 4]);
    output.push_back(hex_digits[c & 15]);
  }
  return output;
}

std::string format_hex_string(const std::string &hexString)
{
  std::string formattedString;
  for (size_t i = 0; i < hexString.length(); i += 2)
  {
    formattedString += hexString.substr(i, 2);
    if (i + 2 < hexString.length())
    {
      formattedString += " ";
    }
  }
  return formattedString;
}

int hex_to_int(const std::string &hexString)
{
  return strtol(hexString.c_str(), nullptr, 16);
}

// Function to blink the LED in a specific RGB pattern
void blinkRGBLedInPattern(int numTimes, int red, int green, int blue, int onTime1, int offTime1, int onTime2, int offTime2)
{
  for (int i = 0; i < numTimes; i++)
  {
    strip.setPixelColor(0, strip.Color(red, green, blue));
    strip.show();
    delay(onTime1);
    strip.setPixelColor(0, strip.Color(0, 0, 0));
    strip.show();
    delay(offTime1);

    if (onTime2 > 0 || offTime2 > 0)
    {
      strip.setPixelColor(0, strip.Color(red, green, blue));
      strip.show();
      delay(onTime2);
      strip.setPixelColor(0, strip.Color(0, 0, 0));
      strip.show();
      delay(offTime2);
    }
  }
}

void sendDataToServer(void *param)
{
  Serial.println("\n****************************************************************************************************");

  if (client.connect("www.google.com", 443))
  {
    Serial.println("Internet connection: OK");
  }
  else
  {
    Serial.println("Internet connection: FAILED!!!!!");
  }
  client.stop();

  if (client.connect(serverHost, httpsPort))
  {
    Serial.println("Connected to server. Sending data... ");
    Serial.println("JSON Data:");
    Serial.println(postData);

    client.println(String("POST ") + apiPath + " HTTP/1.1");
    client.println(String("Host: ") + serverHost);
    client.println("Content-Type: application/json");
    client.println("X-API-KEY: " + apiKey);
    client.print("Content-Length: ");
    client.println(postData.length());
    client.println();
    client.println(postData);

    unsigned long timeout = millis();
    bool isHeader = true;
    int responseCode = -1;
    String responseBody = "";

    int freeHeap = ESP.getFreeHeap();
    Serial.println("Free heap memory: " + String(freeHeap));

    if (freeHeap < 20000)
    {
      Serial.println("Free heap memory is low. Restarting ESP");
      blinkRGBLedInPattern(8, LED_brightness, 0, 0, 100, 100); // blink red LED short pulses
      ESP.restart();
    }

    while (client.connected() && (millis() - timeout) < data_packet_sending_time)
    {
      String response = client.readString();
      responseCode = response.substring(9, 12).toInt();
      Serial.print("Server responseCode: ");
      Serial.println(responseCode);
      if (responseCode == 200)
      {
        Serial.println("Data sent successfully");
        blinkRGBLedInPattern(1, 0, LED_brightness, 0, 30, 30, 60, 0); // blink green LED short pulses
        esp_task_wdt_reset();
        Serial.println("Watch dog timer reset");
        number_of_failed_attempts_to_connect_to_server = 0;
        number_of_failed_responses_from_server = 0;
        break;
      }
      else
      {
        number_of_failed_responses_from_server++;
        delay(delay_after_failed_attempt_to_send_data);
        if (number_of_failed_responses_from_server >= max_number_of_failed_responses)
        {
          number_of_failed_responses_from_server = 0;
          number_of_failed_attempts_to_connect_to_server++;
          Serial.println("Tried to send data " + String(max_number_of_failed_attempts) + " times. Ignoring packet");
          blinkRGBLedInPattern(1, LED_brightness, 0, 0, 30, 30, 60, 0);
          break;
          if (number_of_failed_attempts_to_connect_to_server >= max_number_of_failed_attempts)
          {
            number_of_failed_attempts_to_connect_to_server = 0;
            Serial.println("Restarting ESP");
            blinkRGBLedInPattern(max_number_of_failed_attempts, LED_brightness, 0, 0, 400, 200); // blink red LED long pulses for max_number_of_failed_attempts
            ESP.restart();
          }
        }
      }
    }
    client.stop();
  }
  else
  {
    Serial.println("Connection to server failed");
    blinkRGBLedInPattern(1, LED_brightness, 0, 0, 30, 30, 60, 0); // blink red LED short pulses
    number_of_failed_attempts_to_connect_to_server++;
    Serial.println("Number of failed attempts: " + String(number_of_failed_attempts_to_connect_to_server));
    if (number_of_failed_attempts_to_connect_to_server >= max_number_of_failed_attempts)
    {
      Serial.println("Restarting ESP");
      blinkRGBLedInPattern(max_number_of_failed_attempts, LED_brightness, 0, 0, 400, 200); // blink red LED long pulses for max_number_of_failed_attempts
      ESP.restart();
    }
  }

  vTaskDelete(NULL);
}

void createHTTPSTask()
{
  xTaskCreate(
      sendDataToServer, // Task function
      "HTTPS Task",     // Task name
      8192,             // Stack size
      NULL,             // Task input parameter
      1,                // Priority
      NULL              // Task handle
  );
}

class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{

  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    if (strcmp(advertisedDevice.getName().c_str(), "") == 0)
    {
      std::string hexAdvData = string_to_hex(advertisedDevice.getManufacturerData());
      if (hexAdvData.rfind("544e", 0) == 0) // Check for a valid prefix
      {
        std::string frameHead1 = hexAdvData.substr(0, 4);
        std::string type = hexAdvData.substr(4, 2);
        std::string cmd = hexAdvData.substr(6, 2);
        std::string frameHead2 = hexAdvData.substr(8, 4);
        std::string measurementHex = hexAdvData.substr(12, 4);
        std::string batteryHex = hexAdvData.substr(16, 2);
        std::string macAddress = advertisedDevice.getAddress().toString().c_str();

        if (inSensorSearchingMode)
        {
          if (type == "21")
          {
            Serial.println("\nFound a SYNCed sensor");
            inSensorSearchingMode = false;
            selected_sensor_mac_address = macAddress.c_str();
            Serial.print("MAC Address: ");
            Serial.println(selected_sensor_mac_address);
          }
        }
        else
        {
          if (strcmp(macAddress.c_str(), selected_sensor_mac_address.c_str()) == 0)
          {
            float measurement = (hex_to_int(measurementHex))*sound_speed / 2000;

            int battery = hex_to_int(batteryHex);

            // Serial.print("Received Payload: ");
            // Serial.println(hexAdvData.c_str());

            timeClient.update();
            unsigned long epochTime = timeClient.getEpochTime();

            postData = String("{\"DATETIME\":") + String(epochTime) +
                       ",\"IMEI\":\"" + String(advertisedDevice.getAddress().toString().c_str()) + "\"," +
                       "\"NCU_FW_VER\":\"" + String(current_fw_version) + "\"," +
                       "\"GAS_METER\":" + String(measurement / 10) + "," +
                       "\"CSQ\":104," +
                       "\"MCU_TEMP\":30," +
                       "\"BAT_VOL\":" + String(battery) + "," +
                       "\"METER_TYPE\":4," +
                       "\"TIME_ZONE\":\"" + String(timeZone) + "\"," +
                       "\"TANK_SIZE\":\"" + String(tankSize) + "\"," +
                       "\"GAS_PERCENT\":" + String(measurement * 100 / loadedHeight.toFloat()) + "," +
                       "\"LONGITUDE\":\"" + String(longitude) + "\"," +
                       "\"LATITUDE\":\"" + String(latitude) + "\"," +
                       "\"LOADED_HEIGHT\":" + String(loadedHeight.toFloat()) + "," +
                       "\"GW_NAME\":\"" + String(gatewayName) + "\"," +
                       "\"RSSI\":" + String(advertisedDevice.getRSSI()) + "}";

            if (millis() - lastSendTime > 5000)
            {
              createHTTPSTask();
              lastSendTime = millis();
            }
          }
        }
      }
    }
  }
};

void switchToAPMode()
{
  esp_task_wdt_reset();
  Serial.println("Watch dog timer reset");
  Serial.println("\n\n****************************************************************************************************\n****************************************************************************************************");
  Serial.println("\nManually switching to AP mode...");
  bluetooth_sending_status = false;
  WiFi.mode(WIFI_AP);
  inAPMode = true;
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("\nAccess Point Started");
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());
}

void blinkLEDInAPMode()
{
  if (automatically_put_to_AP_mode)
  {
    Serial.println("Trying to connect to the last saved Wi-Fi network...");
    if (tryConnectToSavedWiFi())
    {
      inAPMode = false;
      WiFi.mode(WIFI_STA);
      indicateSuccessfulConnection();
      bluetooth_sending_status = true;
      Serial.println("Connected to the last saved Wi-Fi network... Restarting the gateway");
      esp_task_wdt_reset();
      Serial.println("Watch dog timer reset");
      delay(500);
      ESP.restart();
    }
    else
    {
      Serial.println("Failed to connect to the last saved Wi-Fi network.\nRetrying in 2 second...");
      delay(2000);
      WiFi.mode(WIFI_AP_STA);
      handleButtonPress();
    }
  }
  else
  {
    unsigned long currentMillis = millis();
    // Serial.println("AP mode manually triggered");
    if (currentMillis - previousMillis >= blink_interval)
    {
      esp_task_wdt_reset();
      // Serial.println("Watch dog timer reset");
      previousMillis = currentMillis;
      led_state = !led_state;
      if (led_state)
      {
        strip.setPixelColor(0, strip.Color(0, 0, LED_brightness));
      }
      else
      {
        strip.setPixelColor(0, strip.Color(0, 0, 0));
      }
      strip.show();
    }
  }
}

bool handleButtonPress()
{
  if (digitalRead(BOOT_PIN) == LOW)
  {
    delay(100);
    if (digitalRead(BOOT_PIN) == LOW)
    {
      Serial.println("\nBoot Button press recognized");
      automatically_put_to_AP_mode = false;
      switchToAPMode();
      return true;
    }
    return false;
  }
  return false;
}

void saveWiFiCredentials(const String &ssid, const String &password)
{
  EEPROM.begin(EEPROM_SIZE);
  esp_task_wdt_reset();
  Serial.println("Watch dog timer reset");
  for (int i = SSID_ADDR; i < SSID_ADDR + 50; i++)
    EEPROM.write(i, 0);
  for (int i = PASS_ADDR; i < PASS_ADDR + 50; i++)
    EEPROM.write(i, 0);

  for (int i = 0; i < ssid.length(); i++)
    EEPROM.write(SSID_ADDR + i, ssid[i]);
  for (int i = 0; i < password.length(); i++)
    EEPROM.write(PASS_ADDR + i, password[i]);
  Serial.println("Saved Wi-Fi credentials to EEPROM");

  EEPROM.commit();

  blinkRGBLedInPattern(1, 0, LED_brightness, LED_brightness, 1000, 10); // cyan LED single long pulse
  blinkRGBLedInPattern(1, 0, LED_brightness, 0, 50, 50, 100, 100);      // green LED two short pulses
}

void saveFwVersion(const String &fw_version)
{
  esp_task_wdt_reset();
  Serial.println("Watch dog timer reset");
  EEPROM.begin(EEPROM_SIZE);

  for (int i = FW_VERSION_ADDR; i < FW_VERSION_ADDR + 50; i++)
    EEPROM.write(i, 0);

  for (int i = 0; i < fw_version.length(); i++)
    EEPROM.write(FW_VERSION_ADDR + i, fw_version[i]);
  Serial.println("Saved FW Version to EEPROM");

  EEPROM.commit();
}

void saveOtherConfigDataToEEPROM(const String &tankSize, const String &timeZone, const String &longitude, const String &latitude, const String &loadedHeight, const String &gatewayName, const String &apiKey)
{
  esp_task_wdt_reset();
  Serial.println("Watch dog timer reset");
  EEPROM.begin(EEPROM_SIZE);

  for (int i = TANKSIZE_ADDR; i < TANKSIZE_ADDR + 50; i++)
    EEPROM.write(i, 0);
  for (int i = TIMEZONE_ADDR; i < TIMEZONE_ADDR + 50; i++)
    EEPROM.write(i, 0);
  for (int i = LONGITUDE_ADDR; i < LONGITUDE_ADDR + 50; i++)
    EEPROM.write(i, 0);
  for (int i = LATITUDE_ADDR; i < LATITUDE_ADDR + 50; i++)
    EEPROM.write(i, 0);
  for (int i = LOADED_HEIGHT_ADDR; i < LOADED_HEIGHT_ADDR + 50; i++)
    EEPROM.write(i, 0);
  for (int i = GATEWAY_NAME_ADDR; i < GATEWAY_NAME_ADDR + 50; i++)
    EEPROM.write(i, 0);
  for (int i = API_KEY_ADDR; i < API_KEY_ADDR + 50; i++)
    EEPROM.write(i, 0);

  for (int i = 0; i < tankSize.length(); i++)
    EEPROM.write(TANKSIZE_ADDR + i, tankSize[i]);
  for (int i = 0; i < timeZone.length(); i++)
    EEPROM.write(TIMEZONE_ADDR + i, timeZone[i]);
  for (int i = 0; i < longitude.length(); i++)
    EEPROM.write(LONGITUDE_ADDR + i, longitude[i]);
  for (int i = 0; i < latitude.length(); i++)
    EEPROM.write(LATITUDE_ADDR + i, latitude[i]);
  for (int i = 0; i < loadedHeight.length(); i++)
    EEPROM.write(LOADED_HEIGHT_ADDR + i, loadedHeight[i]);
  for (int i = 0; i < gatewayName.length(); i++)
    EEPROM.write(GATEWAY_NAME_ADDR + i, gatewayName[i]);
  for (int i = 0; i < apiKey.length(); i++)
    EEPROM.write(API_KEY_ADDR + i, apiKey[i]);

  EEPROM.commit();

  Serial.println("Saved other configuration data to EEPROM");
  blinkRGBLedInPattern(1, 0, LED_brightness, LED_brightness, 1000, 10); // cyan LED single long pulse
  blinkRGBLedInPattern(1, 0, LED_brightness, 0, 50, 50, 100, 100);      // green LED two short pulses
}

void loadWiFiCredentials(String &ssid, String &password)
{
  esp_task_wdt_reset();
  Serial.println("Watch dog timer reset");
  EEPROM.begin(EEPROM_SIZE);

  char ssidBuff[50];
  char passBuff[50];

  for (int i = 0; i < 50; i++)
    ssidBuff[i] = EEPROM.read(SSID_ADDR + i);
  for (int i = 0; i < 50; i++)
    passBuff[i] = EEPROM.read(PASS_ADDR + i);

  ssid = String(ssidBuff);
  password = String(passBuff);
  Serial.println("Loaded Wi-Fi credentials from EEPROM");

  char tankSizeBuff[50];
  char timeZoneBuff[50];
  char longitudeBuff[50];
  char latitudeBuff[50];
  char loadedHeightBuff[50];
  char gatewayNameBuff[50];
  char selectedSensorMacBuff[50];
  char fwVersionBuff[50];
  char apiKeyBuff[50];

  for (int i = 0; i < 50; i++)
    tankSizeBuff[i] = EEPROM.read(TANKSIZE_ADDR + i);
  for (int i = 0; i < 50; i++)
    timeZoneBuff[i] = EEPROM.read(TIMEZONE_ADDR + i);
  for (int i = 0; i < 50; i++)
    longitudeBuff[i] = EEPROM.read(LONGITUDE_ADDR + i);
  for (int i = 0; i < 50; i++)
    latitudeBuff[i] = EEPROM.read(LATITUDE_ADDR + i);
  for (int i = 0; i < 50; i++)
    loadedHeightBuff[i] = EEPROM.read(LOADED_HEIGHT_ADDR + i);
  for (int i = 0; i < 50; i++)
    selectedSensorMacBuff[i] = EEPROM.read(SENSOR_MAC_ADDR + i);
  for (int i = 0; i < 50; i++)
    fwVersionBuff[i] = EEPROM.read(FW_VERSION_ADDR + i);
  for (int i = 0; i < 50; i++)
    gatewayNameBuff[i] = EEPROM.read(GATEWAY_NAME_ADDR + i);
  for (int i = 0; i < 50; i++)
    apiKeyBuff[i] = EEPROM.read(API_KEY_ADDR + i);

  tankSize = String(tankSizeBuff);
  timeZone = String(timeZoneBuff);
  longitude = String(longitudeBuff);
  latitude = String(latitudeBuff);
  loadedHeight = String(loadedHeightBuff);
  gatewayName = String(gatewayNameBuff);
  selected_sensor_mac_address = String(selectedSensorMacBuff);
  current_fw_version = String(fwVersionBuff);
  apiKey = String(apiKeyBuff);

  Serial.println("Loaded other configuration data from EEPROM");
  blinkRGBLedInPattern(1, 0, LED_brightness, LED_brightness, 500, 10); // cyan LED single long pulse
  blinkRGBLedInPattern(1, 0, LED_brightness, 0, 50, 50, 100, 100);     // blue LED two short pulses
}

bool tryConnectToSavedWiFi()
{
  WiFi.mode(WIFI_AP_STA);
  String savedSSID, savedPassword;
  loadWiFiCredentials(savedSSID, savedPassword);

  if (savedSSID.length() > 0 && savedPassword.length() > 0)
  {
    esp_task_wdt_reset();
    Serial.println("Watch dog timer reset");
    Serial.print("Trying to connect to saved SSID: ");
    Serial.print(savedSSID);

    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

    int maxRetries = 8;
    int retries = 0;

    while (WiFi.status() != WL_CONNECTED && retries < maxRetries)
    {
      delay(950);
      blinkRGBLedInPattern(1, LED_brightness, LED_brightness, LED_brightness, 50, 0);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\nSuccessfully connected to saved Wi-Fi");
      esp_task_wdt_reset();
      Serial.println("Watch dog timer reset");
      indicateSuccessfulConnection();
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      inAPMode = false;
      return true;
    }
  }
  Serial.println("Failed to connect to saved Wi-Fi");
  indicateUnsuccessfulConnection();
  WiFi.mode(WIFI_AP_STA);
  return false;
}

void handle_check_internet_connection()
{
  Serial.println("\nChecking Internet connection...");
  if (server.method() == HTTP_GET)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      if (client.connect("www.google.com", 443))
      {
        Serial.println("Connected to the Internet");
        esp_task_wdt_reset();
        Serial.println("Watch dog timer reset");
        Serial.println("Restart the gateway to start sending data to the server");
        server.send(200, "application/json", "{\"internet_connected\": 1, \"wifi_connected\": 1}");
      }
      else
      {
        Serial.println("No Internet access");
        server.send(200, "application/json", "{\"internet_connected\": 0, \"wifi_connected\": 1}");
      }

      client.stop();
    }
    else
    {
      Serial.println("Not connected to Wi-Fi");
      server.send(200, "application/json", "{\"internet_connected\": 0, \"wifi_connected\": 0}");
    }
  }
}

// blink blue and then green for 2 times
void indicateSuccessfulConnection()
{
  blinkRGBLedInPattern(1, 0, 0, LED_brightness, 500, 0);
  blinkRGBLedInPattern(1, 0, LED_brightness, 0, 500, 500);
  blinkRGBLedInPattern(1, 0, 0, LED_brightness, 500, 0);
  blinkRGBLedInPattern(1, 0, LED_brightness, 0, 500, 0);
}

// blink blue and then red for 2 times
void indicateUnsuccessfulConnection()
{
  blinkRGBLedInPattern(1, 0, 0, LED_brightness, 500, 0);
  blinkRGBLedInPattern(1, LED_brightness, 0, 0, 500, 500);
  blinkRGBLedInPattern(1, 0, 0, LED_brightness, 500, 0);
  blinkRGBLedInPattern(1, LED_brightness, 0, 0, 500, 0);
}

// blink white, cyan, magenta, yellow, white short pulses
void indicateGatewayStart()
{
  blinkRGBLedInPattern(1, LED_brightness, LED_brightness, LED_brightness, 300, 10);
  blinkRGBLedInPattern(1, 0, LED_brightness, LED_brightness, 300, 10);
  blinkRGBLedInPattern(1, LED_brightness, 0, LED_brightness, 300, 10);
  blinkRGBLedInPattern(1, LED_brightness, LED_brightness, 0, 300, 10);
  blinkRGBLedInPattern(1, LED_brightness, LED_brightness, LED_brightness, 300, 500);
}

// blink white, green, white, green, white, green short pulses
void indicateReadyToReceiveData()
{
  blinkRGBLedInPattern(1, LED_brightness, LED_brightness, LED_brightness, 500, 10);
  blinkRGBLedInPattern(1, 0, LED_brightness, 0, 500, 10);
  blinkRGBLedInPattern(1, LED_brightness, LED_brightness, LED_brightness, 500, 10);
  blinkRGBLedInPattern(1, 0, LED_brightness, 0, 500, 10);
  blinkRGBLedInPattern(1, LED_brightness, LED_brightness, LED_brightness, 500, 10);
  blinkRGBLedInPattern(1, 0, LED_brightness, 0, 500, 500);
}

void handle_other_config()
{
  if (server.method() == HTTP_POST && server.uri() == "/configuration/v1/other-config")
  {
    esp_task_wdt_reset();
    Serial.println("Watch dog timer reset");
    Serial.println("\nReceiving other configuration data");

    JsonDocument doc;
    String requestBody = server.arg("plain");
    DeserializationError error = deserializeJson(doc, requestBody);

    if (error)
    {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      server.send(400, "text/plain", "Invalid JSON");
      return;
    }

    tankSize = doc["tankSize"].as<String>();
    timeZone = doc["timeZone"].as<String>();
    longitude = doc["longitude"].as<String>();
    latitude = doc["latitude"].as<String>();
    loadedHeight = doc["loadedHeight"].as<String>();
    gatewayName = doc["gatewayName"].as<String>();
    apiKey = doc["apiKey"].as<String>();

    Serial.print("Received TankSize: ");
    Serial.print(tankSize);
    Serial.print("\tTimeZone: ");
    Serial.print(timeZone);
    Serial.print("\tLongitude: ");
    Serial.print(longitude);
    Serial.print("\tLatitude: ");
    Serial.println(latitude);
    Serial.print("\tLoaded Height: ");
    Serial.println(loadedHeight);
    Serial.print("\tGateway Name: ");
    Serial.println(gatewayName);

    server.send(200, "application/json", "{\"status\": 1}");

    saveOtherConfigDataToEEPROM(tankSize, timeZone, longitude, latitude, loadedHeight, gatewayName, apiKey);
    Serial.println("Restart the gateway to start sending data to the server");
  }
  else
  {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void handle_connect_to_new_wifi()
{
  if (server.method() == HTTP_POST)
  {
    esp_task_wdt_reset();
    Serial.println("Watch dog timer reset");
    JsonDocument doc;

    String requestBody = server.arg("plain");
    DeserializationError error = deserializeJson(doc, requestBody);

    if (error)
    {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      server.send(400, "text/plain", "Invalid JSON");
      return;
    }

    String ssid = doc["ssid"].as<String>();
    String password = doc["password"].as<String>();

    Serial.println("\nReceiving Wi-Fi credentials...");
    Serial.print("Received SSID: ");
    Serial.println(ssid);

    Serial.print("Trying to connect to the new Wi-Fi network");

    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    int maxRetries = 10;
    int retries = 0;

    while (WiFi.status() != WL_CONNECTED && retries < maxRetries)
    {
      delay(1000);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\nSuccessfully connected to Wi-Fi");
      server.send(200, "application/json", "{\"status\": 1, \"ssid\": \"" + ssid + "\"}");

      delay(1000);

      saveWiFiCredentials(ssid, password);
      Serial.print("Wi-Fi client IP Address: ");
      Serial.println(WiFi.localIP());
      Serial.print("Wi-Fi server (AP) IP Address: ");
      Serial.println(WiFi.softAPIP());

      indicateSuccessfulConnection();
    }
    else
    {
      Serial.println("\nFailed to connect to Wi-Fi");
      server.send(200, "application/json", "{\"status\": 0, \"ssid\": \"" + ssid + "\"}");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ap_ssid, ap_password);
      Serial.println("Re-enabled AP mode");
    }
  }
  else
  {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void handle_sync_sensor()
{
  if (server.method() == HTTP_GET)
  {
    esp_task_wdt_reset();
    Serial.println("Watch dog timer reset");
    Serial.println("\nWaiting for user to press SYNC button on sensor...");
    blinkRGBLedInPattern(3, LED_brightness, 0, LED_brightness, 50, 50); // blink 3 purple LED short pulses
    inSensorSearchingMode = true;
    selected_sensor_mac_address = "NA";
    while (inSensorSearchingMode && inAPMode)
    {
      BLEScanResults foundDevices = pBLEScan->start(scanTimeSeconds, false);
      pBLEScan->clearResults();
      if (selected_sensor_mac_address != "NA")
      {
        server.send(200, "application/json", "{\"status\": 1, \"sync_mac\": \"" + selected_sensor_mac_address + "\"}");
        Serial.println("SYNCed sensor mac sent to the app");
        blinkRGBLedInPattern(2, LED_brightness, 0, LED_brightness, 100, 100); // blink 2 purple LED short pulses
        inSensorSearchingMode = false;
        break;
      }
    }
  }
}

void handle_confirm_synced_sensor()
{
  if (server.method() == HTTP_GET)
  {
    if (selected_sensor_mac_address != "NA")
    {
      esp_task_wdt_reset();
      Serial.println("Watch dog timer reset");
      Serial.println("Confirmed synced sensor. Writing to EEPROM...");
      bluetooth_sending_status = false;
      EEPROM.begin(EEPROM_SIZE);
      for (int i = SENSOR_MAC_ADDR; i < SENSOR_MAC_ADDR + 50; i++)
        EEPROM.write(i, 0);
      for (int i = 0; i < selected_sensor_mac_address.length(); i++)
        EEPROM.write(SENSOR_MAC_ADDR + i, selected_sensor_mac_address[i]);
      EEPROM.commit();
      blinkRGBLedInPattern(1, 0, LED_brightness, LED_brightness, 1000, 10); // cyan LED single long pulse
      blinkRGBLedInPattern(1, 0, LED_brightness, 0, 50, 50, 100, 100);      // green LED two short pulses
      Serial.println("Saved mac address of the sensor to the EEPROM");
      server.send(200, "application/json", "{\"status\": 1, \"confirmed_mac\": \"" + selected_sensor_mac_address + "\"}");
    }
  }
}

void check_for_fw_updates(int interval)
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisForUpdateCheck >= interval)
  {

    Serial.println("\n******************FW UPDATE CHECK******************");

    if (client.connect("www.google.com", 443))
    {
      Serial.println("Internet connection: OK");
      client.stop();
    }
    else
    {
      Serial.println("Internet connection: FAILED!\nUpdate check failed");
      client.stop();
      blinkRGBLedInPattern(2, LED_brightness, 0, 0, 30, 30); // 2 red LED short pulses
      return;
    }

    previousMillisForUpdateCheck = currentMillis;
    HTTPClient http;
    String serverName = "https://raw.githubusercontent.com/nimsara1999/OTAupdate/refs/heads/main/check.txt";
    http.begin(serverName);

    // Send HTTP GET request
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0)
    {
      String payload = http.getString();
      payload.trim();
      Serial.print("Current firmware version: ");
      Serial.print(current_fw_version);
      Serial.print("\tLatest firmware version: ");
      Serial.println(payload);
      http.end();

      if (payload == current_fw_version)
      {
        Serial.println("Firmware update check done. No new updates available");
        esp_task_wdt_reset();
        Serial.println("Watch dog timer reset");
        blinkRGBLedInPattern(1, LED_brightness, LED_brightness, 0, 400, 5); // yellow LED single long pulse
        blinkRGBLedInPattern(1, 0, LED_brightness, 0, 70, 100);             // green LED single long pulse
      }
      else
      {
        Serial.println("Firmware update check done. New updates available. Initiating firmware update in 5 seconds...");
        esp_task_wdt_reset();
        Serial.println("Watch dog timer reset");
        blinkRGBLedInPattern(5, LED_brightness, LED_brightness, 0, 800, 200); // 5 yellow LED long pulses
        doUpdate(payload, current_fw_version);
      }
    }
    else
    {
      Serial.print("Update check error. Error code: ");
      Serial.println(httpResponseCode);
    }
  }
}

// updating firmware fetching latest firmware bin file from server
void doUpdate(const String &new_fw_version, const String &old_fw_version)
{
  strip.setPixelColor(0, strip.Color(LED_brightness, LED_brightness, 0));
  strip.show();

  Serial.println("Fetching Update...");
  String url = "https://raw.githubusercontent.com/nimsara1999/OTAupdate/refs/heads/main/firmware.bin";

  saveFwVersion(new_fw_version);

  t_httpUpdate_return ret = ESPhttpUpdate.update(url);

  switch (ret)
  {
  case HTTP_UPDATE_FAILED:
    Serial.println("Update failed!");
    saveFwVersion(old_fw_version);
    blinkRGBLedInPattern(1, 0, 0, 0, 30, 30, 200, 5);        // LED off
    blinkRGBLedInPattern(3, LED_brightness, 0, 0, 300, 300); // 3 red LED short pulses
    break;
  case HTTP_UPDATE_NO_UPDATES:
    Serial.println("No new update available");
    saveFwVersion(old_fw_version);
    break;
  // We can't see this, because of reset chip after update OK
  case HTTP_UPDATE_OK:
    Serial.println("Update OK");
    break;

  default:
    break;
  }
}

bool isValidString(String data, size_t maxLength)
{
  if (data.isEmpty())
  {
    Serial.println("Invalid string loaded : String is empty");
    return false;
  }
  if (data.length() == 0 || data.length() > maxLength)
  {
    Serial.println("Invalid string loaded : String length is invalid");
    return false;
  }
  for (size_t i = 0; i < data.length(); i++)
  {
    char c = data.charAt(i);
    if (c < 32 || c > 126)
    {
      Serial.println("Invalid string loaded : Invalid character found");
      return false; // Invalid character found
    }
  }
  return true; // String is valid
}

void setup()
{
  Serial.begin(115200);

  esp_task_wdt_init(WDT_TIMEOUT, true); // timeout, true to trigger a reset
  esp_task_wdt_add(NULL);               // Add the current task to the WDT

  strip.begin(); // Initialize the LED
  strip.show();  // Turn off all LEDs

  pinMode(BOOT_PIN, INPUT_PULLUP);

  Serial.println("\n\nStarting Gateway...\n");
  indicateGatewayStart(); // blink white, cyan, magenta, yellow, white short pulses

  // Get the MAC address
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char macStr[13];
  snprintf(macStr, sizeof(macStr), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  ap_ssid += "-";
  ap_ssid += macStr;

  Serial.print("AP SSID: ");
  Serial.println(ap_ssid);
  Serial.println("");

  EEPROM.begin(EEPROM_SIZE);

  server.on("/configuration/v1/wifi-config", HTTP_POST, handle_connect_to_new_wifi);
  server.on("/configuration/v1/other-config", HTTP_POST, handle_other_config);
  server.on("/check/v1/check-internet", HTTP_GET, handle_check_internet_connection);
  server.on("/check/v1/confirm-synced-sensor", HTTP_GET, handle_confirm_synced_sensor);
  server.on("/check/v1/sync-sensor", HTTP_GET, handle_sync_sensor);
  server.on("/about-us", []()
            { server.send(200, "text/plain", "Made by Nimsara & Sasindu."); });

  bluetooth_sending_status = false;
  client.setInsecure();

  // Try to connect to saved Wi-Fi credentials and load other configuration data
  if (!tryConnectToSavedWiFi() || timeZone == "NA" || tankSize == "NA" || longitude == "NA" || latitude == "NA" || loadedHeight == "NA" || selected_sensor_mac_address == "NA" || gatewayName == "NA" || apiKey == "NA")
  {
    if (!handleButtonPress())
    {
      Serial.println("Automatically starting AP mode");
      blinkRGBLedInPattern(3, 0, 0, LED_brightness, 30, 50); // blink blue LED short pulses for 3 times
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(ap_ssid, ap_password);
      Serial.println("Access Point Started");
      Serial.print("AP IP Address: ");
      Serial.println(WiFi.softAPIP());
      automatically_put_to_AP_mode = true;
      inAPMode = true;
    }
  }
  else
  {
    inAPMode = false;
    WiFi.mode(WIFI_STA);
    indicateSuccessfulConnection();
    if (isValidString(timeZone, 50) && isValidString(tankSize, 50) && isValidString(longitude, 50) && isValidString(latitude, 50) && isValidString(loadedHeight, 50) && isValidString(gatewayName, 50))
    {
      if (!isValidString(apiKey, 50))
      {
        apiKey = "S+6nCxThMZvzQYDy3z2NMWSaF6wvPjSvCtPOkPMrKII=";
        Serial.println("Invalid API key loaded. Using default API key");
      }
      bluetooth_sending_status = true;
      Serial.println("Data loaded from EEPROM.");
      Serial.print("Gateway Name: ");
      Serial.println(gatewayName);
      Serial.print("Firmware Version: ");
      Serial.println(current_fw_version);
      check_for_fw_updates(0);
      indicateReadyToReceiveData();
      Serial.println("\nScanning for Gas sensor of MAC address: " + selected_sensor_mac_address);
    }
    else
    {
      Serial.println("Invalid data loaded from EEPROM. Restarting the gateway");
      while (1)
      {
        delay(100);
        blinkRGBLedInPattern(1, LED_brightness, 0, 0, 100, 200); // blink red LED short pulses
        if (handleButtonPress())
        {
          break;
        }
      }
    }
  }

  server.begin();

  timeClient.begin();

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(false);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

void loop()
{
  server.handleClient();

  if (inAPMode)
  {
    blinkLEDInAPMode();
  }
  else
  {
    check_for_fw_updates(fw_update_interval);
    handleButtonPress();
    // esp_task_wdt_reset();
    BLEScanResults foundDevices = pBLEScan->start(scanTimeSeconds, false);
    pBLEScan->clearResults();
  }
}
