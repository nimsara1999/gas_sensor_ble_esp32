#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

const char *ap_ssid = "ESP32-AP";
const char *ap_password = "123456789";

WebServer server(80);

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
      server.send(200, "text/plain", "Successfully connected");

      delay(1000);

      WiFi.softAPdisconnect(true); // Disable the AP mode after response
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    }
    else
    {
      Serial.println("\nFailed to connect to Wi-Fi");
      server.send(200, "text/plain", "SSID or PASSWORD wrong!");
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

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/send", HTTP_POST, handleSend);

  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  server.handleClient();
}
