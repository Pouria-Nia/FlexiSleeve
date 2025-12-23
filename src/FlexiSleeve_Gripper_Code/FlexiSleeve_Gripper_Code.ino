#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// Wi-Fi credentials
const char* ssid = "ESP32-Robot";
const char* password = "ITECH2025";

// WebSocket client
WebSocketsClient webSocket;

// Servo object
Servo myServo;

// Constant for the servo data pin
const int servoPin = 13;

// Function to connect to Wi-Fi
void connectToWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
}

// WebSocket event handler
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("Disconnected!");
      break;
    case WStype_CONNECTED:
      Serial.println("Connected to server");
      // Request sensor data
      webSocket.sendTXT("{\"command\":\"read_sensor\"}");
      break;
    case WStype_TEXT:
      // Process incoming message
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      int flexValue = doc["flex"];
      Serial.print("Flex Value: ");
      Serial.println(flexValue);
      
      // Map the flex sensor value to the servo range
      int motorPosition = map(flexValue, 2000, 500, 0, 95);
      motorPosition = constrain(motorPosition, 0, 95); // Ensure value is within range
      Serial.print("Motor Position: ");
      Serial.println(motorPosition);

      // Control the servo based on the mapped value
      myServo.write(motorPosition);
      break;
  }
}

void setup() {
  // Initialize Serial monitor
  Serial.begin(115200);

  // Initialize Servo object
  myServo.attach(servoPin);
  myServo.write(0); // Start with the motor deactivated

  // Connect to Wi-Fi
  connectToWiFi();

  // Initialize WebSocket client
  webSocket.begin("192.168.1.22", 81, "/"); // IP of the first ESP32 and WebSocket port
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000); // Try to reconnect every 5 seconds
}

void loop() {
  webSocket.loop();
}
