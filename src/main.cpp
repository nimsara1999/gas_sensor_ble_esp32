#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <HTTPClient.h>

const char *ap_ssid = "ESP32-AP";
const char *ap_password = "123456789";

WebServer server(80);

const int SSID_ADDR = 0;
const int PASS_ADDR = 50;
const int EEPROM_SIZE = 512;

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
      return true;
    }
  }
  return false;
}

void check_internet_connection()
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

void handleSend()
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
      server.send(200, "application/json", "{\"status\": 1, \"ssid\": " + ssid + "}");

      delay(1000);

      // WiFi.softAPdisconnect(true); // Disable the AP mode after response
      saveWiFiCredentials(ssid, password);
      Serial.print("Wi-Fi client IP Address: ");
      Serial.println(WiFi.localIP());
      Serial.print("Wi-Fi server (AP) IP Address: ");
      Serial.println(WiFi.softAPIP());
    }
    else
    {
      Serial.println("\nFailed to connect to Wi-Fi");
      server.send(200, "application/json", "{\"status\": 0, \"ssid\": " + ssid + "}");
      WiFi.mode(WIFI_AP); // Return to AP mode if connection fails
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

  // Try to connect to saved Wi-Fi credentials
  if (!tryConnectToSavedWiFi())
  {
    // If failed, start in AP mode
    Serial.println("Starting AP mode");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    Serial.println("Access Point Started");
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
  }

  server.on("/connect-to-new-wifi", HTTP_POST, handleSend);
  server.on("/check-internet", HTTP_GET, check_internet_connection);

  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  server.handleClient();
}
