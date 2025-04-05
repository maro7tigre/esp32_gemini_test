#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "credentials.h"
#include "my_image.h"  // Include the base64 image header

const char* host = "generativelanguage.googleapis.com";
const int httpsPort = 443;

String geminiReply = "";  // Variable to store Gemini's reply

void sendGeminiRequest(String prompt, String imageBase64 = "") {
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

  // Build JSON payload with text AND image if provided
  DynamicJsonDocument doc(16384); // Increased size for image data
  
  // Create the "contents" array
  doc["contents"] = JsonArray();
  doc["contents"][0]["parts"] = JsonArray();
  
  // Add the text part
  doc["contents"][0]["parts"][0]["text"] = prompt;
  
  // Add the image part if provided
  if (imageBase64.length() > 0) {
    // Add image as a second part in the same content
    JsonObject imagePart = doc["contents"][0]["parts"][1].to<JsonObject>();
    imagePart["inline_data"]["mime_type"] = "image/jpeg"; // Change if using a different image format
    imagePart["inline_data"]["data"] = imageBase64;
  }
  
  // Set generation configuration
  doc["generationConfig"]["maxOutputTokens"] = 100;
  
  String payload;
  serializeJson(doc, payload);

  Serial.println("Sending HTTP POST request to Gemini...");
  // Debug payload size to make sure it's not too large
  Serial.print("Payload size: ");
  Serial.print(payload.length());
  Serial.println(" bytes");
  
  // Only print the first part of the payload for debugging to avoid flooding serial
  Serial.println("Payload start (truncated):");
  Serial.println(payload.substring(0, 200) + "...");

  // Send the HTTP POST request
  client.println("POST " + url + " HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(payload.length());
  client.println();
  client.println(payload);

  Serial.println("Request sent. Waiting for response headers...");
  // Read and print headers with timeout
  unsigned long timeout = millis();
  while(client.connected() && millis() - timeout < 10000){ // 10 second timeout
    String headerLine = client.readStringUntil('\n');
    if (headerLine == "\r") {
      Serial.println("End of headers.");
      break;
    }
  }
  
  Serial.println("Reading response body...");
  String response = "";
  timeout = millis();
  while(client.available() && millis() - timeout < 30000) { // 30 second timeout
    char c = client.read();
    response += c;
    // Print a dot every 100 characters to show progress
    if (response.length() % 100 == 0) {
      Serial.print(".");
    }
  }
  
  Serial.println("\nResponse received.");
  
  // Remove any preceding chunk size if present (start from the first '{')
  int jsonStart = response.indexOf('{');
  if (jsonStart != -1) {
    response = response.substring(jsonStart);
  }
  
  DynamicJsonDocument responseDoc(8192);
  DeserializationError error = deserializeJson(responseDoc, response);
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    geminiReply = "Parse error: " + String(error.c_str());
    client.stop();
    return;
  }
  
  // Check for error in response
  if (responseDoc.containsKey("error")) {
    geminiReply = "API Error: " + responseDoc["error"]["message"].as<String>();
    Serial.println("API returned an error:");
    Serial.println(geminiReply);
  }
  // Check for candidates
  else if (responseDoc.containsKey("candidates") && responseDoc["candidates"].is<JsonArray>()) {
    geminiReply = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
    Serial.println("Parsed Gemini reply successfully.");
  } 
  else {
    geminiReply = "No valid reply found in JSON";
    Serial.println("No candidates array found in JSON response.");
  }
  
  client.stop();
}

void setup() {
  Serial.begin(9600);  // Set serial rate to 9600 as requested
  delay(1000);
  Serial.println("Starting ESP32 Gemini Test with Image Support");
  Serial.println("-------------------------------------------");
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

  Serial.println("\nInstructions:");
  Serial.println("Type any prompt and it will be sent with the embedded image");
  Serial.println("\nEnter your prompt:");
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() > 0) {
      Serial.print("> ");
      Serial.println(input);
      
      // Always send the prompt with the image
      Serial.println("Sending prompt with image...");
      sendGeminiRequest(input, String(MY_IMAGE_BASE64));
      
      Serial.println("\n----- GEMINI RESPONSE -----");
      Serial.println(geminiReply);
      Serial.println("----------------------------");
      Serial.println("\nEnter your next prompt:");
    }
  }
}