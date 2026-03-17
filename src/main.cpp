#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#define WIFI_SSID "CV_RoboticX7.6"
#define WIFI_PASSWORD "J4e4muVG"

#define API_KEY "AIzaSyBWnQQgUCFiZdGZNO2OJHSnOf7zTCSZZvE"
#define DATABASE_URL "https://healthycaresystems-default-rtdb.firebaseio.com/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("Connected");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {

  int temperature = random(25, 35);

  if (Firebase.RTDB.setInt(&fbdo, "/sensor/temperature", temperature)) {
    Serial.println("Send data success");
  }
  else {
    Serial.println(fbdo.errorReason());
  }

  delay(5000);
}