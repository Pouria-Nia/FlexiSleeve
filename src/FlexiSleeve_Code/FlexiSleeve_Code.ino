#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "MPU6050_6Axis_MotionApps20.h"

// Wi-Fi credentials
const char* ssid = "ESP32-Robot";
const char* password = "ITECH2025";

IPAddress local_IP(192,168,1,22);
IPAddress gateway(192,168,1,5);
IPAddress subnet(255,255,255,0);

String webpage = "<!DOCTYPE html><html><head><title>Sensor Data</title></head><body style='background-color: #EEEEEE;'><span style='color: #003366;'><h1>Sensor Data</h1><p>Yaw: <span id='yaw'>-</span> degrees</p><p>Pitch: <span id='pitch'>-</span> degrees</p><p>Roll: <span id='roll'>-</span> degrees</p><p>Temperature: <span id='temp'>-</span> °C</p><p>Flex Value: <span id='flex'>-</span></p><p>Button Hold State: <span id='button_hold_state'>-</span></p><p>Toggle Mode: <span id='toggle_mode'>-</span></p><p><button type='button' id='BTN_SEND_BACK'>Send info to ESP32</button></p></span></body><script> var Socket; document.getElementById('BTN_SEND_BACK').addEventListener('click', button_send_back); function init() { Socket = new WebSocket('ws://' + window.location.hostname + ':81/'); Socket.onmessage = function(event) { processCommand(event); }; } function button_send_back() { var msg = {command: 'read_sensor'};Socket.send(JSON.stringify(msg)); } function processCommand(event) {var obj = JSON.parse(event.data);document.getElementById('yaw').innerHTML = obj.yaw;document.getElementById('pitch').innerHTML = obj.pitch;document.getElementById('roll').innerHTML = obj.roll;document.getElementById('temp').innerHTML = obj.temp;document.getElementById('flex').innerHTML = obj.flex;document.getElementById('button_hold_state').innerHTML = obj.button_hold_state;document.getElementById('toggle_mode').innerHTML = obj.toggle_mode; console.log(obj); } window.onload = function(event) { init(); }</script></html>";

// Interval to send sensor data
int interval = 1000; // 1 second
unsigned long previousMillis = 0;

// Web server and websocket initialization
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// MPU6050 and sensor pins initialization
MPU6050 mpu;
const int flexPin = A0;
const int vibrationMotor = 4;

// push and hold button setup
const int buttonHoldPin = 18;
int buttonHoldState = 0;
const int ledHoldPin = 19;

// toggle button setup
const int buttonTogglePin = 16;
const int ledMode1 = 25;
const int ledMode2 = 26;
const int ledMode3 = 27;
int mode = 1;
int buttonToggleState = 0;
int lastButtonState = 0;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
int ToggleMode = 1;

// FIFO buffer
uint8_t fifoBuffer[64];

Quaternion q;
VectorFloat gravity;
float ypr[3];

void setup() {
  Serial.begin(115200);

  // Wi-Fi setup
  Serial.print("Setting up Access point ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print("Setting AP ... ");
  Serial.println(WiFi.softAP(ssid, password) ? "Ready" : "Failed!");

  Serial.print("IP address = ");
  Serial.println(WiFi.softAPIP());

  server.on("/", []() {
    server.send(200, "text/html", webpage);
  });
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // MPU6050 setup
  Wire.begin(21, 22);
  Wire.setClock(400000);

  Serial.println(F("Initializing I2C devices..."));
  mpu.initialize();

  Serial.println(F("Testing device connections..."));
  if (mpu.testConnection()) {
    Serial.println(F("MPU6050 connection successful"));
  } else {
    Serial.println(F("MPU6050 connection failed"));
    while (1); // Halt execution if MPU fails to connect
  }

  // MPU6050 Initialization
  Serial.println(F("Initializing DMP..."));
  uint8_t devStatus = mpu.dmpInitialize();

  // Check if DMP initialization was successful
  if (devStatus == 0) {
    Serial.println(F("DMP Initialized successfully"));

    // Enable DMP
    Serial.println(F("Enabling DMP..."));
    mpu.setDMPEnabled(true);

    Serial.println(F("DMP ready!"));
    // Get expected DMP packet size for later comparison
  } else {
    Serial.print(F("DMP Initialization failed (code "));
    Serial.print(devStatus);
    Serial.println(F(")"));
    while (1); // Halt execution if DMP fails to initialize
  }

  // Vibration motor setup
  pinMode(vibrationMotor, OUTPUT);

  // Setup Push and Hold button
  pinMode(ledHoldPin, OUTPUT);
  pinMode(buttonHoldPin, INPUT);

  // Setup Toggle button
  pinMode(ledMode1, OUTPUT);
  pinMode(ledMode2, OUTPUT);
  pinMode(ledMode3, OUTPUT);
  pinMode(buttonTogglePin, INPUT);
  setMode(mode); //start with mode 1
}

void loop() {
  server.handleClient();
  webSocket.loop();

  // button Hold state reading
  buttonHoldState = digitalRead(buttonHoldPin);
  // check if pushbutton is pressed
  if (buttonHoldState == HIGH) {
    // turn LED on
    digitalWrite(ledHoldPin, HIGH);
  } else {
    // turn LED off
    digitalWrite(ledHoldPin, LOW);
  }
  Serial.println(buttonHoldState);

  unsigned long now = millis();
  if ((unsigned long)(now - previousMillis) > interval) {
    sendSensorData();
    previousMillis = now;
  }

  // button toggle reading with debounce delay
  // Read the state of the button
  int reading = digitalRead(buttonTogglePin);

  // Check if the button state has changed
  if (reading != lastButtonState) {
    // Reset the debounce timer
    lastDebounceTime = millis();
  }

  // If the button state has been stable for the debounce delay
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // If the button state has changed
    if (reading != buttonToggleState) {
      buttonToggleState = reading;

      // If the button is pressed (assuming LOW means pressed)
      if (buttonToggleState == LOW) {
        // Increment the mode
        mode++;
        if (mode > 3) {
          mode = 1;
        }
        // Set the LEDs according to the current mode
        setMode(mode);
        ToggleMode = mode;
      }
    }
  } 
  // Save the reading. Next time through the loop, it'll be the lastButtonState
  lastButtonState = reading;
}

void sendSensorData() {
  if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) {
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

    float yaw = ypr[0] * 180/M_PI;
    float pitch = ypr[1] * 180/M_PI;
    float roll = ypr[2] * 180/M_PI;

    int flexValue = analogRead(flexPin);

    if (flexValue < 2000) {
      digitalWrite(vibrationMotor, HIGH);
    } else {
      digitalWrite(vibrationMotor, LOW);
    }

    // Include buttonHoldState
    buttonHoldState = digitalRead(buttonHoldPin);

    String jsonString = "";
    StaticJsonDocument<200> doc;
    JsonObject object = doc.to<JsonObject>();

    object["yaw"] = yaw;
    object["pitch"] = pitch;
    object["roll"] = roll;
    object["temp"] = 0; // MPU6050 does not provide temperature in DMP mode
    object["flex"] = flexValue;
    object["button_hold_state"] = buttonHoldState; // Add buttonHoldState
    object["toggle_mode"] = ToggleMode; // Add ToggleMode

    serializeJson(doc, jsonString);
    Serial.println(jsonString);
    webSocket.broadcastTXT(jsonString);
  }
}

void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("Client " + String(num) + " disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("Client " + String(num) + " connected");
      break;
    case WStype_TEXT:
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      String command = doc["command"];
      if (command == "read_sensor") {
        sendSensorData();
      }
      break;
  }
}

void setMode(int mode) {
  // Turn off all LEDs
  digitalWrite(ledMode1, LOW);
  digitalWrite(ledMode2, LOW);
  digitalWrite(ledMode3, LOW);

  // Turn on the LED according to the current mode
  switch (mode) {
    case 1:
      digitalWrite(ledMode1, HIGH);
      break;
    case 2:
      digitalWrite(ledMode2, HIGH);
      break;
    case 3:
      digitalWrite(ledMode3, HIGH);
      break;
  }
}
