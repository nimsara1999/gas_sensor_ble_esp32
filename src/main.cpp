#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

const char *ap_ssid = "Gateway";
const char *ap_password = "123456789";
const int scanTimeSeconds = 1;

const int BOOT_PIN = 9;
bool inAPMode = false;
bool bluetooth_sending_status = false;
unsigned long previousMillis = 0;
const long interval = 500;

const char *serverHost = "elysiumapi.overleap.lk";
const int httpsPort = 443;
const char *apiPath = "/api/v1/gas/stream"; // API endpoint

BLEScan *pBLEScan;

WebServer server(80);
WiFiClientSecure client;

const int SSID_ADDR = 0;
const int PASS_ADDR = 50;
const int EEPROM_SIZE = 512;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);

String postData;

int led_state = 0;

void indicateSuccessfulConnection();
static unsigned long lastSendTime = 0;

String tankSize = "";
String timeZone = "";
String longitude = "";
String latitude = "";

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

void sendDataToServer(void *param)
{
  if (client.connect(serverHost, httpsPort))
  {
    Serial.println("Connected to server!");

    client.println(String("POST ") + apiPath + " HTTP/1.1");
    client.println(String("Host: ") + serverHost);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(postData.length());
    client.println();
    client.println(postData);

    unsigned long timeout = millis();
    bool isHeader = true;
    int responseCode = -1;
    String responseBody = "";

    while (client.connected() && (millis() - timeout) < 5000)
    {
      String response = client.readString();
      responseCode = response.substring(9, 12).toInt();
      Serial.println("Server responseCode:");
      Serial.println(responseCode);
      if (responseCode == 200)
      {
        Serial.println("Data sent successfully");
        break;
      }
    }
    client.stop();
  }
  else
  {
    Serial.println("Connection to server failed");
  }

  vTaskDelete(NULL); // Delete the task once the HTTPS request is done
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

        int measurement = hex_to_int(measurementHex);
        int battery = hex_to_int(batteryHex);

        unsigned long epochTime = timeClient.getEpochTime();

        // Instead of using char arrays, let's use a safer String object
        postData = String("{\"DATETIME\":") + String(epochTime) +
                   ",\"IMEI\":\"A4C138CCD9ED\"," +
                   "\"NCU_FW_VER\":109," +
                   "\"GAS_METER\":" + String(measurement) + "," +
                   "\"CSQ\":104," +
                   "\"MCU_TEMP\":30," +
                   "\"BAT_VOL\":" + String(battery) + "," +
                   "\"METER_TYPE\":4," +
                   "\"TIME_ZONE\":" + String(timeZone) + "," +
                   "\"TANK_SIZE\":" + String(tankSize) + "," +
                   "\"GAS_PERCENT\":0, " +
                   "\"LONGITUDE\":" + String(longitude) + "," +
                   "\"LATITUDE\":" + String(latitude) + "," +
                   "\"RSSI\":" + String(advertisedDevice.getRSSI()) + "}";

        // Ensure there's a delay between transmissions
        if (millis() - lastSendTime > 10000)
        {
          Serial.println("****************************************************************************************************");
          Serial.println("Received JSON Data:");
          Serial.println(postData);
          createHTTPSTask();
          lastSendTime = millis();
        }
      }
    }
  }
};

void switchToAPMode()
{
  Serial.println("Switching to AP mode...");
  bluetooth_sending_status = false;
  WiFi.mode(WIFI_AP);
  inAPMode = true;
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("Access Point Started");
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());
}

void blinkLEDInAPMode()
{
  if (inAPMode)
  {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
      previousMillis = currentMillis;
      led_state = !led_state;
      digitalWrite(BUILTIN_LED, led_state);
    }
  }
}

void handleButtonPress()
{
  if (digitalRead(BOOT_PIN) == LOW && inAPMode == false)
  {
    delay(100);
    if (digitalRead(BOOT_PIN) == LOW)
    {
      switchToAPMode();
    }
  }
}

void saveWiFiCredentials(const String &ssid, const String &password)
{
  EEPROM.begin(EEPROM_SIZE);

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
}

void loadWiFiCredentials(String &ssid, String &password)
{
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
}

bool tryConnectToSavedWiFi()
{
  String savedSSID, savedPassword;
  loadWiFiCredentials(savedSSID, savedPassword);

  if (savedSSID.length() > 0 && savedPassword.length() > 0)
  {
    Serial.print("Trying to connect to saved SSID: ");
    Serial.println(savedSSID);

    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

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
      Serial.println("\nSuccessfully connected to saved Wi-Fi");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      bluetooth_sending_status = true;
      indicateSuccessfulConnection();
      return true;
    }
  }
  return false;
}

void handle_check_internet_connection()
{
  if (server.method() == HTTP_GET)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      if (client.connect("www.google.com", 443))
      {
        Serial.println("Connected to the Internet");

        server.send(200, "application/json", "{\"internet_connected\": 1, \"wifi_connected\": 1}");

        WiFi.mode(WIFI_STA);
        bluetooth_sending_status = true;
        inAPMode = false;
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

void indicateSuccessfulConnection()
{
  digitalWrite(BUILTIN_LED, HIGH); // Turn the LED on
  delay(3000);                     // Keep the LED on for 3 seconds
  digitalWrite(BUILTIN_LED, LOW);  // Turn the LED off
  inAPMode = false;                // Exit AP mode, stop blinking
}

void handle_other_config()
{
  if (server.method() == HTTP_POST && server.uri() == "/configuration/v1/other-config")
  {
    Serial.println("Receiving other configuration data");

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

    Serial.print("Received TankSize: ");
    Serial.print(tankSize);
    Serial.print("\tTimeZone: ");
    Serial.print(timeZone);
    Serial.print("\tLongitude: ");
    Serial.print(longitude);
    Serial.print("\tLatitude: ");
    Serial.println(latitude);

    server.send(200, "application/json", "{\"status\": 1}");
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

    Serial.print("Received SSID: ");
    Serial.println(ssid);
    Serial.print("Received Password: ");
    Serial.println(password);

    WiFi.mode(WIFI_AP_STA); // Set mode to both AP and STA
    WiFi.begin(ssid.c_str(), password.c_str());

    int maxRetries = 20;
    int retries = 0;

    while (WiFi.status() != WL_CONNECTED && retries < maxRetries)
    {
      delay(500);
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
      WiFi.mode(WIFI_AP); // Return to AP mode if connection fails
      inAPMode = true;
      WiFi.softAP(ap_ssid, ap_password);
      Serial.println("Re-enabled AP mode");
    }
  }
  else
  {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void setup()
{
  Serial.begin(115200);

  EEPROM.begin(EEPROM_SIZE);

  pinMode(BOOT_PIN, INPUT_PULLUP);
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);

  server.on("/configuration/v1/wifi-config", HTTP_POST, handle_connect_to_new_wifi);
  server.on("/configuration/v1/other-config", HTTP_POST, handle_other_config);
  server.on("/check/v1/check-internet", HTTP_GET, handle_check_internet_connection);

  // Try to connect to saved Wi-Fi credentials
  if (!tryConnectToSavedWiFi())
  {
    // If failed, start in AP mode
    Serial.println("Starting AP mode");
    WiFi.mode(WIFI_AP);
    inAPMode = true;
    WiFi.softAP(ap_ssid, ap_password);
    Serial.println("Access Point Started");
    Serial.print("AP IP Address: ");
    bluetooth_sending_status = false;
    Serial.println(WiFi.softAPIP());
  }

  server.begin();
  Serial.println("HTTP server started");

  client.setInsecure(); // For development purposes, skip certificate validation

  Serial.println("Scanning for Gas sensors...");
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
  timeClient.update();
  server.handleClient();

  handleButtonPress();
  blinkLEDInAPMode();

  if (bluetooth_sending_status)
  {
    BLEScanResults foundDevices = pBLEScan->start(scanTimeSeconds, false);
    pBLEScan->clearResults();
  }
}
