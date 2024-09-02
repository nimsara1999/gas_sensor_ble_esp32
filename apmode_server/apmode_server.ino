#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>  // Include the ArduinoJson library

// Define the SSID and Password for the ESP32 Access Point
const char *ssid = "ESP32-AP";
const char *password = "123456789";  // Minimum 8 characters

// Define the HTTP server on port 80
WebServer server(80);

void handleSend() {
  if (server.method() == HTTP_POST) {
    // Create a JSON document
    DynamicJsonDocument doc(1024);

    // Parse the JSON data from the request
    String requestBody = server.arg("plain");
    DeserializationError error = deserializeJson(doc, requestBody);

    // Check for errors
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      server.send(400, "text/plain", "Invalid JSON");
      return;
    }

    // Extract values from JSON
    String ssid = doc["ssid"].as<String>();
    String password = doc["password"].as<String>();

    // Print the received values
    Serial.print("Received SSID: ");
    Serial.println(ssid);
    Serial.print("Received Password: ");
    Serial.println(password);

    // Send a response
    delay(5000);
    server.send(200, "text/plain", "Data received");
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void setup() {
  Serial.begin(115200);

  // Start the WiFi Access Point
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Define the route for sending data
  server.on("/send", HTTP_POST, handleSend);

  // Start the HTTP server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
