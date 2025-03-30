#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "credentials.h"

const char* host = "generativelanguage.googleapis.com";
const int httpsPort = 443;

String geminiReply = "";  // Variable to store Gemini's reply

void sendGeminiRequest(String prompt) {
  WiFiClientSecure client;
  client.setInsecure();  // For testing only. In production, verify the certificate.

  Serial.println("Attempting connection to Gemini server...");
  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection to Gemini server failed!");
    geminiReply = "Connection failed";
    return;
  }
  Serial.println("Connected to Gemini server.");

  // Build the request URL using your API key from credentials.h
  String url = "/v1beta/models/gemini-1.5-flash:generateContent?key=" + String(GEMINI_API_KEY);

  // Build JSON payload according to Gemini's expected structure:
  // {
  //   "contents": [
  //     {
  //       "parts": [
  //         { "text": "YOUR_PROMPT" }
  //       ]
  //     }
  //   ],
  //   "generationConfig": {
  //     "maxOutputTokens": 100
  //   }
  // }
  //
  // Using the indexing operator auto-creates nested objects/arrays.
  StaticJsonDocument<1024> doc;
  // Create the "contents" array and assign its first element's "parts" array and "text" field
  doc["contents"] = JsonArray();
  // This auto-creates an object at index 0 in the "contents" array.
  doc["contents"][0]["parts"] = JsonArray();
  doc["contents"][0]["parts"][0]["text"] = prompt;
  // Set generation configuration
  doc["generationConfig"]["maxOutputTokens"] = 100;
  
  String payload;
  serializeJson(doc, payload);

  Serial.println("Sending HTTP POST request to Gemini...");
  Serial.println("Payload:");
  Serial.println(payload);

  // Send the HTTP POST request
  client.println("POST " + url + " HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(payload.length());
  client.println();
  client.println(payload);

  Serial.println("Request sent. Waiting for response headers...");
  // Read and print headers
  while(client.connected()){
    String headerLine = client.readStringUntil('\n');
    Serial.print("Header: ");
    Serial.println(headerLine);
    if (headerLine == "\r") {
      Serial.println("End of headers.");
      break;
    }
  }
  
  Serial.println("Reading response body...");
  String response = client.readString();
  Serial.println("Raw response:");
  Serial.println(response);

  // Remove any preceding chunk size if present (start from the first '{')
  int jsonStart = response.indexOf('{');
  if (jsonStart != -1) {
    response = response.substring(jsonStart);
  }
  
  StaticJsonDocument<2048> responseDoc;
  DeserializationError error = deserializeJson(responseDoc, response);
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    geminiReply = "Parse error";
    client.stop();
    return;
  }
  
  if (responseDoc["candidates"].is<JsonArray>()) {
    geminiReply = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
    Serial.println("Parsed Gemini reply successfully.");
  } else {
    geminiReply = "No reply found in JSON";
    Serial.println("No candidates array found in JSON response.");
  }
  
  client.stop();
}

void setup() {
  Serial.begin(9600);  // Set serial rate to 9600 as requested
  delay(1000);
  Serial.println("Starting ESP32 Gemini Test");
  Serial.println("----------------------------");
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retryCount++;
    if (retryCount > 60) {  // After roughly 30 seconds, restart if not connected
      Serial.println("\nFailed to connect to WiFi. Restarting...");
      ESP.restart();
    }
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Enter your prompt text via Serial Monitor:");
}

void loop() {
  if (Serial.available() > 0) {
    String prompt = Serial.readStringUntil('\n');
    prompt.trim();
    if (prompt.length() > 0) {
      Serial.println("User prompt received:");
      Serial.println(prompt);
      sendGeminiRequest(prompt);
      Serial.println("Reply from Gemini:");
      Serial.println(geminiReply);
      Serial.println("Enter your prompt text via Serial Monitor:");
    }
  }
}
