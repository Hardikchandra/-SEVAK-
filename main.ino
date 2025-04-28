#define BLYNK_TEMPLATE_ID "TMPL31WEHttR_"
#define BLYNK_TEMPLATE_NAME "Krishi jalsevak"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <HardwareSerial.h>

// Your WiFi credentials
char ssid[] = "hardik";
char pass[] = "44556677";

// Your Blynk Auth Token
char auth[] = "oWqYrc6ie8CrQqVFOpjMTlBzARfTa_w7";

// Pin Definitions
#define RELAY_PIN 26
#define SWITCH_PIN 27
#define DHT_PIN 14
#define RAIN_SENSOR_PIN 32
#define SOIL_MOISTURE_PIN 34
#define VOLTAGE_SENSOR_PIN 35

#define TRIG_PIN 12   // New: Ultrasonic Sensor TRIG
#define ECHO_PIN 13   // New: Ultrasonic Sensor ECHO

#define SIM800_TX 17
#define SIM800_RX 16

// DHT Settings
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// Serial for SIM800L
HardwareSerial GSM(1);

// Variables
bool relayState = false;
bool lastSwitchState = HIGH;
bool isRaining = false;
float temperature = 0.0;
float humidity = 0.0;
unsigned long callStartTime = 0;
bool callActive = false;
bool rainSMSsent = false;
bool highMoistureSMSsent = false;
bool boxOpenSMSsent = false; // New variable

// Registered Phone Number
const char *registeredNumber = "+919926211285";

void setup() {
  Serial.begin(115200);
  GSM.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(DHT_PIN, INPUT_PULLUP);
  pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(VOLTAGE_SENSOR_PIN, INPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  dht.begin();
  delay(2000);

  digitalWrite(RELAY_PIN, HIGH);

  Serial.println("System Initialized. Relay OFF.");

  initGSM();

  // Initialize Blynk
  Blynk.begin(auth, ssid, pass);
}

void initGSM() {
  sendCommand("AT");
  delay(1000);
  sendCommand("ATE0");
  delay(500);
  sendCommand("AT+CLIP=1");
  delay(500);
  sendCommand("AT+DDET=1");
  delay(500);
  sendCommand("ATS0=1");
  delay(500);
  Serial.println("GSM Module Initialized.");
}

void sendCommand(const char *cmd) {
  GSM.println(cmd);
  delay(500);
  while (GSM.available()) {
    Serial.write(GSM.read());
  }
}

void checkSwitch() {
  bool currentSwitchState = digitalRead(SWITCH_PIN);
  if (currentSwitchState == LOW && lastSwitchState == HIGH) {
    delay(50);
    if (digitalRead(SWITCH_PIN) == LOW) {
      toggleRelay();
    }
  }
  lastSwitchState = currentSwitchState;
}

void toggleRelay() {
  relayState = !relayState;
  digitalWrite(RELAY_PIN, relayState ? LOW : HIGH);
  sendSMS(relayState ? "Motor Turned ON" : "Motor Turned OFF");
  Blynk.virtualWrite(V1, relayState ? 1 : 0);  // Update motor status on Blynk app
}

void checkRainSensor() {
  int rainValue = digitalRead(RAIN_SENSOR_PIN);
  isRaining = (rainValue == LOW);

  if (isRaining && relayState) {
    Serial.println("Rain Detected! Turning OFF Relay...");
    turnRelayOff();
    if (!rainSMSsent) {
      sendSMS("Rain Detected! Motor Turned OFF.");
      rainSMSsent = true;
    }
  }
  if (!isRaining) {
    rainSMSsent = false;
  }

  // Update rain status on Blynk app
  Blynk.virtualWrite(V3, isRaining ? 1 : 0);
}

void checkSoilMoisture() {
  int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
  int moisturePercent = map(soilMoistureValue, 4095, 1500, 0, 100);

  Serial.print("Soil Moisture: ");
  Serial.print(moisturePercent);
  Serial.println("%");

  if (moisturePercent > 90 && relayState) {
    Serial.println("High Moisture! Turning OFF Relay...");
    turnRelayOff();
    if (!highMoistureSMSsent) {
      sendSMS("High Moisture! Motor Turned OFF.");
      highMoistureSMSsent = true;
    }
  }
  if (moisturePercent <= 90) {
    highMoistureSMSsent = false;
  }

  // Update soil moisture on Blynk app
  Blynk.virtualWrite(V2, moisturePercent);
}

void checkUltrasonicSensor() {
  long duration;
  int distance;

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH);
  distance = duration * 0.034 / 2;

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  // Update ultrasonic sensor status on Blynk app
  Blynk.virtualWrite(V5, distance);

  if (distance > 5) {
    if (!boxOpenSMSsent) {
      sendSMS("Alert: Box Opened!");
      boxOpenSMSsent = true;
    }
  } else {
    boxOpenSMSsent = false;
  }
}

void turnRelayOn() {
  if (!relayState) {
    relayState = true;
    digitalWrite(RELAY_PIN, LOW);
    sendSMS("Motor Turned ON");
    Blynk.virtualWrite(V1, 1);  // Update motor status on Blynk app
  }
}

void turnRelayOff() {
  if (relayState) {
    relayState = false;
    digitalWrite(RELAY_PIN, HIGH);
    sendSMS("Motor Turned OFF");
    Blynk.virtualWrite(V1, 0);  // Update motor status on Blynk app
  }
}

void sendSMS(String message) {
  GSM.print("AT+CMGF=1\r");
  delay(100);
  GSM.print("AT+CMGS=\"");
  GSM.print(registeredNumber);
  GSM.println("\"");
  delay(100);
  GSM.print(message);
  delay(100);
  GSM.write(26);
  delay(1000);
}

void handleCall() {
  if (GSM.available()) {
    String data = GSM.readString();
    Serial.println("GSM Data: " + data);

    if (data.indexOf("+CLIP:") >= 0) {
      if (data.indexOf(registeredNumber) != -1) {
        Serial.println("Call from registered number... Auto picking up.");
        answerCall();
      }
    }
    if (data.indexOf("+DTMF:") >= 0) {
      int dtmfTone = data.substring(data.indexOf("+DTMF:") + 6).toInt();
      handleDTMF(dtmfTone);
    }
  }
}

void answerCall() {
  sendCommand("ATA");
  delay(2000);
  callStartTime = millis();
  callActive = true;
  Serial.println("Call Answered Successfully.");
}

void checkCallTimeout() {
  if (callActive && millis() - callStartTime > 20000) {
    Serial.println("No response, disconnecting call.");
    sendCommand("ATH");
    callActive = false;
    callStartTime = 0;
  }
}

void handleDTMF(int tone) {
  switch (tone) {
    case 1:
      turnRelayOn();
      sendSMS("Motor Turned ON via DTMF.");
      break;
    case 2:
      turnRelayOff();
      sendSMS("Motor Turned OFF via DTMF.");
      break;
    case 3:
      turnRelayOn();
      sendSMS("Motor Turned ON for 2 min.");
      delay(120000);
      turnRelayOff();
      sendSMS("Motor Turned OFF after 2 min.");
      break;
    case 4:
      turnRelayOn();
      sendSMS("Motor Turned ON for 5 min.");
      delay(300000);
      turnRelayOff();
      sendSMS("Motor Turned OFF after 5 min.");
      break;
    case 5:
      sendHumidityAndMoisture();
      break;
    case 6:
      checkPowerStatus();
      break;
    case 0:
      sendSMS("Current Location: https://goo.gl/maps/WsPVGdWnkpPqCjSN7");
      break;
    default:
      Serial.println("Invalid DTMF Tone Received.");
      break;
  }
}

void sendHumidityAndMoisture() {
  float currentHumidity = dht.readHumidity();
  int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
  int moisturePercent = map(soilMoistureValue, 4095, 1500, 0, 100);

  if (isnan(currentHumidity)) {
    sendSMS("Error reading humidity sensor!");
    return;
  }

  String message = "Humidity: " + String(currentHumidity, 1) + "%, "
                   + "Soil Moisture: " + String(moisturePercent) + "%";

  sendSMS(message);
}

void checkPowerStatus() {
  int voltageStatus = digitalRead(VOLTAGE_SENSOR_PIN);

  if (voltageStatus == HIGH) {
    sendSMS("Power is Available.");
  } else {
    sendSMS("No Power Supply Detected.");
  }
}

void updateSensorData() {
  float newTemp = dht.readTemperature();
  float newHum = dht.readHumidity();

  if (isnan(newTemp) || isnan(newHum)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  temperature = newTemp;
  humidity = newHum;

  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.print("°C, Hum: ");
  Serial.print(humidity);
  Serial.println("%");

  // Update temperature and humidity on Blynk app
  Blynk.virtualWrite(V6, temperature);
  Blynk.virtualWrite(V4, humidity);
}

void loop() {
  Blynk.run();
  checkSwitch();
  checkRainSensor();
  checkSoilMoisture();
  checkUltrasonicSensor(); // 👈 New
  updateSensorData();
  handleCall();
  checkCallTimeout();
  delay(2000);
}