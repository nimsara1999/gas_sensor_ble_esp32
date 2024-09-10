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
// #include <WiFiClientSecure.h>
// #include <ArduinoHttpClient.h>

const char *ap_ssid = "Gateway";
const char *ap_password = "123456789";
const int scanTimeSeconds = 1;

const int BOOT_PIN = 9;
bool inAPMode = false;
bool bluetooth_sending_status = false;
unsigned long previousMillis = 0;
const long interval = 500;

// const char *serverUrl = "https://mockapi.io/projects/66e0c26e2fb67ac16f2a8398#";
const char *serverUrl = "jsonplaceholder.typicode.com";
// const char *serverUrl = "https://elysiumapi.overleap.lk/api/v1/gas/stream";
const int serverPort = 443; // Standard HTTPS port

BLEScan *pBLEScan;
WebServer server(80);

// WiFiClientSecure wifiClient;

const char *rootCACertificate =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDpzCCA06gAwIBAgIQdejYjznIYgETZBgOsch3VjAKBggqhkjOPQQDAjA7MQsw\n"
    "CQYDVQQGEwJVUzEeMBwGA1UEChMVR29vZ2xlIFRydXN0IFNlcnZpY2VzMQwwCgYD\n"
    "VQQDEwNXRTEwHhcNMjQwODIwMDcyOTAyWhcNMjQxMTE4MDcyOTAxWjAXMRUwEwYD\n"
    "VQQDEwx0eXBpY29kZS5jb20wWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAARsuzL/\n"
    "o7KTI+BvU0j6DSz8qHQxR/E4yH1OjVsz25iWduFMajk1oOooN+DOwSIyk5WyLao/\n"
    "TMSFcsQ/3KFbkcZDo4ICVjCCAlIwDgYDVR0PAQH/BAQDAgeAMBMGA1UdJQQMMAoG\n"
    "CCsGAQUFBwMBMAwGA1UdEwEB/wQCMAAwHQYDVR0OBBYEFPZMW366VYShU4L4QUQy\n"
    "bFL2Cky3MB8GA1UdIwQYMBaAFJB3kjVnxP+ozKnme9mAeXvMk/k4MF4GCCsGAQUF\n"
    "BwEBBFIwUDAnBggrBgEFBQcwAYYbaHR0cDovL28ucGtpLmdvb2cvcy93ZTEvZGVn\n"
    "MCUGCCsGAQUFBzAChhlodHRwOi8vaS5wa2kuZ29vZy93ZTEuY3J0MCcGA1UdEQQg\n"
    "MB6CDHR5cGljb2RlLmNvbYIOKi50eXBpY29kZS5jb20wEwYDVR0gBAwwCjAIBgZn\n"
    "gQwBAgEwNgYDVR0fBC8wLTAroCmgJ4YlaHR0cDovL2MucGtpLmdvb2cvd2UxL1h3\n"
    "N2s5OTVCdlZnLmNybDCCAQUGCisGAQQB1nkCBAIEgfYEgfMA8QB2AO7N0GTV2xrO\n"
    "xVy3nbTNE6Iyh0Z8vOzew1FIWUZxH7WbAAABkW7nYEAAAAQDAEcwRQIgatvQ+qUI\n"
    "OeFGZKsu/Bxbm3cVeD6NcGcAetfETDKIVqoCIQCP9B8ifu3fMTNpsPnOofNZdAh1\n"
    "ra1yFGUlaRjV/btE8AB3AEiw42vapkc0D+VqAvqdMOscUgHLVt0sgdm7v6s52IRz\n"
    "AAABkW7nYGkAAAQDAEgwRgIhAPMk3oDZrcqImM1P5DTlV2g66Fd8vzZuzEwryLHi\n"
    "r5CBAiEAydCWA7AwtLDZzCuGz3erdbOUf9Xtv4CKhdCB56aq4mcwCgYIKoZIzj0E\n"
    "AwIDRwAwRAIgV/8yoUEmOfB9EKqfjcJMOotxY/X2mDDTIjYgTXveyjkCIHCHty68\n"
    "+d9psmRwMhNy6WwGVcU/S/XM+fQxOe/qp2ES\n"
    "-----END CERTIFICATE-----";

const int SSID_ADDR = 0;
const int PASS_ADDR = 50;
const int EEPROM_SIZE = 512;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);

int led_state = 0;

void indicateSuccessfulConnection();
static unsigned long lastSendTime = 0;

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

class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    if (strcmp(advertisedDevice.getName().c_str(), "") == 0)
    {
      std::string hexAdvData = string_to_hex(advertisedDevice.getManufacturerData());
      if (hexAdvData.rfind("544e", 0) == 0)
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

        char postData[256]; // Define a buffer for the data
        snprintf(postData, sizeof(postData),
                 "{\"DATETIME\":%lu,\"IMEI\":\"A4C138CCD9ED\",\"NCU_FW_VER\":109,\"GAS_METER\":%d,\"CSQ\":104,\"MCU_TEMP\":30,\"BAT_VOL\":%d,\"METER_TYPE\":4,\"TIME_ZONE\":\"+07\",\"TANK_SIZE\":\"20lb\",\"GAS_PERCENT\":0, \"RSSI\":%d}",
                 epochTime, measurement, battery, advertisedDevice.getRSSI());
        Serial.printf("*********************\n");
        // Serial.printf("Received Data: %s\n", format_hex_string(hexAdvData).c_str());
        // Serial.printf("Frame Head: %s\n", frameHead1.c_str());
        // Serial.printf("Type: %s\n", type.c_str());
        // Serial.printf("Cmd: %s\n", cmd.c_str());
        // Serial.printf("Measurement Result (US): %d\n", measurement);
        // Serial.printf("Battery: %d % \n", battery);
        // Serial.printf("RSSI: %d\n", advertisedDevice.getRSSI());
        // Serial.printf("Prepared packet: \n");
        Serial.println(postData);
        // Serial.println(ESP.getFreeHeap()); // Print available heap memory

        if (millis() - lastSendTime > 10000)
        {
          if (WiFi.status() == WL_CONNECTED)
          {
            HTTPClient http;
            http.begin(serverUrl);
            http.addHeader("Content-Type", "application/json");

            int httpResponseCode = http.POST(postData);
            // Check the HTTP response code
            if (httpResponseCode > 0)
            {
              String response = http.getString(); // Get the response payload
              Serial.println("HTTP Response code: " + String(httpResponseCode));
              Serial.println("Response: " + response);
            }
            else
            {
              Serial.println("Error on sending POST: " + String(httpResponseCode));
            }

            http.end();
          }
          else
          {
            Serial.println("Could not send data to server. Not connected to Wi-Fi");
          }
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
  ssid = "SW-AC5-WIN11";
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
      HTTPClient http;
      http.begin("http://www.google.com");
      int httpCode = http.GET();

      if (httpCode > 0)
      {
        Serial.println("Connected to the Internet");
        http.end();
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
      http.end();
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

void handle_connect_to_new_wifi()
{
  if (server.method() == HTTP_POST)
  {
    JsonDocument doc; // Allocate a fixed size for the JSON document

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

  server.on("/connect-to-new-wifi", HTTP_POST, handle_connect_to_new_wifi);
  server.on("/check-internet", HTTP_GET, handle_check_internet_connection);

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
