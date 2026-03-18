#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
#include <MPU6050_tockn.h>
#include <Adafruit_MLX90614.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

// ===================== THEM THU VIEN TFT =====================
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// ===================== FIREBASE =====================
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// THONG TIN CUA BAN
#define WIFI_SSID "CV_RoboticX7.6"
#define WIFI_PASSWORD "J4e4muVG"

#define API_KEY "AIzaSyBWnQQgUCFiZdGZNO2OJHSnOf7zTCSZZvE"
#define DATABASE_URL "healthycaresystems-default-rtdb.firebaseio.com"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;
String deviceID = "";
TaskHandle_t FirebaseTaskHandle;

// ===================== KHAI BÁO CHÂN PHẦN CỨNG =====================
// Bụi mịn, Cảm ứng, Còi
#define DUST_VO_PIN  4    
#define DUST_LED_PIN 5    
#define TOUCH_PIN    13   
#define BUZZER_PIN   14   

// Màn hình TFT
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   27
#define TFT_DC   4
#define TFT_RST  16

// I2C
#define SDA_PIN 21
#define SCL_PIN 22

// ===================== KHAI BAO CAM BIEN =====================
MAX30105 particleSensor;
MPU6050 mpu6050(Wire);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// ===================== GPS =====================
static const int RXPin = 26, TXPin = 25;
static const uint32_t GPSBaud = 9600;
TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);

// ===================== BIẾN TOÀN CỤC & LỌC NHIỄU =====================
unsigned long lastPrintSensor = 0;
const unsigned long sensorInterval = 500; 

float currentTempObj = 0.0;
float currentTempAmb = 0.0;
float currentMpuX = 0.0;
float currentMpuY = 0.0;
float currentMpuZ = 0.0;
float currentDust = 0.0; // Biến lưu bụi mịn

bool isSOS = false;
bool isFalling = false;
bool lastTouchState = false;

const float ALPHA_TEMP = 0.2; 
const float ALPHA_MPU = 0.3;

// ===================== MAX30102 =====================
uint32_t irBuffer[100];
uint32_t redBuffer[100];
int32_t bufferLength = 100;
int32_t spo2 = 0;
int8_t validSPO2 = 0;
int32_t heartRateValue = 0;
int8_t validHeartRate = 0;
bool max30102Found = false;

const int FILTER_SIZE = 7;
int bpmHistory[FILTER_SIZE] = {0};
int spo2History[FILTER_SIZE] = {0};
int bpmCount = 0, spo2Count = 0;
int bpmIndex = 0, spo2Index = 0;

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ===================== KHAI BAO NGUYEN MAU HAM =====================
void sortArray(int *arr, int size);
int getMedian(int *data, int size);
void addBPMValue(int value);
void addSpO2Value(int value);
int getFilteredBPM();
int getFilteredSpO2();
void displayInfo();
bool initMAX30102();
void initMAX30102Buffer();
void updateMAX30102Fast();
void updateDustSensor();
void checkAlarmsAndButtons();
void initTFT();
void drawStaticUI();
void updateTFT();
void TaskFirebase(void *pvParameters); 

// ===================== HAM HIEN THI TFT =====================
void initTFT()
{
  // Cấu hình lại chân SPI cho ESP32-S3 để màn hình sáng lên
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  
  tft.init(240, 280);   
  tft.setRotation(2);   
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 20);
  tft.println("HEALTH MONITOR");
  tft.drawLine(0, 50, 240, 50, ST77XX_CYAN);
  tft.setCursor(10, 70);
  tft.println("Dang khoi dong...");
}

void drawStaticUI()
{
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_YELLOW); tft.setTextSize(2);
  tft.setCursor(20, 5); tft.println("HEALTH MONITOR");

  tft.setTextColor(ST77XX_CYAN); tft.setTextSize(1);
  tft.setCursor(20, 25); tft.print("ID: "); tft.println(deviceID);
  tft.drawLine(0, 38, 240, 38, ST77XX_BLUE);

  tft.setTextSize(2); tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 45); tft.print("BPM: ");
  tft.setCursor(10, 70); tft.print("SpO2:");

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 100); tft.print("Obj: ");
  tft.setCursor(10, 125); tft.print("Amb: ");

  tft.setTextSize(1); tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(10, 155); tft.print("X: ");
  tft.setCursor(85, 155); tft.print("Y: ");
  tft.setCursor(160, 155); tft.print("Z: ");

  tft.setCursor(10, 175); tft.print("Dust:");
  tft.setTextColor(ST77XX_MAGENTA);
  tft.setCursor(10, 195); tft.print("GPS: ");

  tft.drawLine(0, 230, 240, 230, ST77XX_BLUE);
  tft.setTextSize(2); tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 245); tft.print("SYS:");
}

void updateTFT()
{
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setCursor(70, 45);
  if (bpmCount > 0) tft.printf("%-5d", getFilteredBPM());
  else tft.print("--   ");

  tft.setCursor(80, 70);
  if (spo2Count > 0) tft.printf("%d%%  ", getFilteredSpO2());
  else tft.print("--   ");

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(70, 100); tft.printf("%-6.1f C", currentTempObj);
  tft.setCursor(70, 125); tft.printf("%-6.1f C", currentTempAmb);

  tft.setTextSize(1); tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setCursor(30, 155); tft.printf("%-5.0f", currentMpuX);
  tft.setCursor(105, 155); tft.printf("%-5.0f", currentMpuY);
  tft.setCursor(180, 155); tft.printf("%-5.0f", currentMpuZ);

  tft.setCursor(50, 175); tft.printf("%-6.2f mg/m3", currentDust);

  tft.setTextColor(ST77XX_MAGENTA, ST77XX_BLACK);
  if (gps.location.isValid()) {
    tft.setCursor(50, 195); tft.print("OK        ");
    tft.setCursor(10, 210); tft.printf("%-9.4f, %-9.4f", gps.location.lat(), gps.location.lng());
  } else {
    tft.setCursor(50, 195); tft.print("Waiting...");
    tft.setCursor(10, 210); tft.print("                    "); 
  }

  tft.setTextSize(2); tft.setCursor(65, 245);
  if (isSOS) {
    tft.setTextColor(ST77XX_RED, ST77XX_BLACK); tft.print("SOS ACTIVE! ");
  } else if (isFalling) {
    tft.setTextColor(ST77XX_RED, ST77XX_BLACK); tft.print("FALL DETECT!");
  } else {
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK); tft.print("NORMAL      ");
  }
}

// ===================== ĐỌC BỤI MỊN =====================
void updateDustSensor() {
  digitalWrite(DUST_LED_PIN, LOW);
  delayMicroseconds(280);
  int voMeasured = analogRead(DUST_VO_PIN);
  delayMicroseconds(40);
  digitalWrite(DUST_LED_PIN, HIGH);
  
  float voltage = voMeasured * (3.3 / 4095.0);
  float calculatedDust = (voltage - 0.1) / 0.5;
  if (calculatedDust < 0) calculatedDust = 0;
  
  currentDust = calculatedDust;
}

// ===================== TÉ NGÃ, NÚT BẤM, CÒI =====================
void checkAlarmsAndButtons() {
  bool touchState = digitalRead(TOUCH_PIN);
  if (touchState && !lastTouchState) {
    if (isFalling || isSOS) {
      isFalling = false;
      isSOS = false;
    } else {
      isSOS = true;
    }
  }
  lastTouchState = touchState;

  float accX = mpu6050.getAccX();
  float accY = mpu6050.getAccY();
  float accZ = mpu6050.getAccZ();
  float svm = sqrt(pow(accX, 2) + pow(accY, 2) + pow(accZ, 2));
  
  if (svm > 2.5) isFalling = true; // Ngưỡng phát hiện ngã

  if (isSOS || isFalling) digitalWrite(BUZZER_PIN, HIGH);
  else digitalWrite(BUZZER_PIN, LOW);
}

// ===================== CÁC HÀM XỬ LÝ DỮ LIỆU =====================
void sortArray(int *arr, int size) {
  for (int i = 0; i < size - 1; i++) {
    for (int j = i + 1; j < size; j++) {
      if (arr[j] < arr[i]) {
        int temp = arr[i]; arr[i] = arr[j]; arr[j] = temp;
      }
    }
  }
}

int getMedian(int *data, int size) {
  if (size <= 0) return 0;
  int temp[FILTER_SIZE];
  for (int i = 0; i < size; i++) temp[i] = data[i];
  sortArray(temp, size);
  if (size % 2 == 1) return temp[size / 2];
  else return (temp[size / 2 - 1] + temp[size / 2]) / 2;
}

void addBPMValue(int value) {
  bpmHistory[bpmIndex] = value;
  bpmIndex = (bpmIndex + 1) % FILTER_SIZE;
  if (bpmCount < FILTER_SIZE) bpmCount++;
}

void addSpO2Value(int value) {
  spo2History[spo2Index] = value;
  spo2Index = (spo2Index + 1) % FILTER_SIZE;
  if (spo2Count < FILTER_SIZE) spo2Count++;
}

int getFilteredBPM() { return getMedian(bpmHistory, bpmCount); }
int getFilteredSpO2() { return getMedian(spo2History, spo2Count); }

void displayInfo() {
  if (gps.location.isValid()) {
    // Thu gọn hàm GPS để tránh giật lag Serial
  }
}

bool initMAX30102() {
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("MAX30102 not found."); return false;
  }
  particleSensor.setup(60, 4, 2, 100, 411, 4096);
  particleSensor.setPulseAmplitudeRed(0x1F);
  particleSensor.setPulseAmplitudeIR(0x1F);
  particleSensor.setPulseAmplitudeGreen(0);
  return true;
}

void initMAX30102Buffer() {
  if (!max30102Found) return;
  for (byte i = 0; i < 100; i++) {
    while (particleSensor.available() == false) particleSensor.check();
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRateValue, &validHeartRate);
}

void updateMAX30102Fast() {
  if (!max30102Found) return;
  for (byte i = 25; i < 100; i++) {
    redBuffer[i - 25] = redBuffer[i];
    irBuffer[i - 25] = irBuffer[i];
  }
  for (byte i = 75; i < 100; i++) {
    while (particleSensor.available() == false) particleSensor.check();
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRateValue, &validHeartRate);
  if (validHeartRate && heartRateValue >= 50 && heartRateValue <= 120) addBPMValue(heartRateValue);
  if (validSPO2 && spo2 >= 80 && spo2 <= 100) addSpO2Value(spo2);
}

// ===================== SETUP =====================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(DUST_LED_PIN, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  
  String mac = WiFi.macAddress();
  mac.replace(":", ""); 
  deviceID = "DEV_" + mac; 

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) signupOK = true;
  config.token_status_callback = tokenStatusCallback; 
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Wire.begin(SDA_PIN, SCL_PIN);

  initTFT(); 
  max30102Found = initMAX30102();
  if(max30102Found) initMAX30102Buffer();
  mpu6050.begin();
  mpu6050.calcGyroOffsets(true);
  mlx.begin();
  ss.begin(GPSBaud);

  currentTempObj = mlx.readObjectTempC();
  currentTempAmb = mlx.readAmbientTempC();
  currentMpuX = mpu6050.getAngleX();
  currentMpuY = mpu6050.getAngleY();
  currentMpuZ = mpu6050.getAngleZ();

  drawStaticUI(); 

  xTaskCreatePinnedToCore(TaskFirebase, "FirebaseTask", 10000, NULL, 1, &FirebaseTaskHandle, 0);                    
}

// ===================== LOOP (Core 1) =====================
void loop()
{
  while (ss.available() > 0) { gps.encode(ss.read()); }

  mpu6050.update();
  updateMAX30102Fast(); 
  updateDustSensor();       
  checkAlarmsAndButtons();  

  currentTempObj = (ALPHA_TEMP * mlx.readObjectTempC()) + ((1.0 - ALPHA_TEMP) * currentTempObj);
  currentTempAmb = (ALPHA_TEMP * mlx.readAmbientTempC()) + ((1.0 - ALPHA_TEMP) * currentTempAmb);
  currentMpuX = (ALPHA_MPU * mpu6050.getAngleX()) + ((1.0 - ALPHA_MPU) * currentMpuX);
  currentMpuY = (ALPHA_MPU * mpu6050.getAngleY()) + ((1.0 - ALPHA_MPU) * currentMpuY);
  currentMpuZ = (ALPHA_MPU * mpu6050.getAngleZ()) + ((1.0 - ALPHA_MPU) * currentMpuZ);

  if (millis() - lastPrintSensor >= sensorInterval)
  {
    lastPrintSensor = millis();

    Serial.println("=============== DU LIEU CAM BIEN ===============");
    Serial.println("ID THIẾT BỊ: " + deviceID);
    
    Serial.print("MAX30102 -> BPM: "); Serial.print(bpmCount > 0 ? getFilteredBPM() : 0);
    Serial.print(" | SpO2: "); Serial.println(spo2Count > 0 ? getFilteredSpO2() : 0);

    Serial.printf("MPU6050  -> X: %.1f Y: %.1f Z: %.1f\n", currentMpuX, currentMpuY, currentMpuZ);
    Serial.printf("MLX90614 -> Obj: %.1f*C | Amb: %.1f*C\n", currentTempObj, currentTempAmb);
    Serial.printf("Dust     -> %.2f mg/m3\n", currentDust);

    if (isSOS) Serial.println(">>> CANH BAO: SOS ACTIVE! <<<");
    if (isFalling) Serial.println(">>> CANH BAO: TE NGA! <<<");
    Serial.println("================================================");

    updateTFT();
  }
}

// ===================== TASK FIREBASE (Core 0) =====================
void TaskFirebase(void *pvParameters)
{
  for (;;)
  {
    if (Firebase.ready() && signupOK) 
    {
      int currentBPM = (bpmCount > 0) ? getFilteredBPM() : 0;
      int currentSpO2 = (spo2Count > 0) ? getFilteredSpO2() : 0;
      String basePath = "Devices/" + deviceID + "/";

      Firebase.RTDB.setInt(&fbdo, basePath + "BPM", currentBPM);
      Firebase.RTDB.setInt(&fbdo, basePath + "SpO2", currentSpO2);
      Firebase.RTDB.setFloat(&fbdo, basePath + "TempObj", currentTempObj);
      Firebase.RTDB.setFloat(&fbdo, basePath + "TempAmb", currentTempAmb);
      Firebase.RTDB.setFloat(&fbdo, basePath + "AngleX", currentMpuX);
      Firebase.RTDB.setFloat(&fbdo, basePath + "AngleY", currentMpuY);
      Firebase.RTDB.setFloat(&fbdo, basePath + "AngleZ", currentMpuZ);
      Firebase.RTDB.setFloat(&fbdo, basePath + "Dust", currentDust);

      Firebase.RTDB.setBool(&fbdo, basePath + "Alert_SOS", isSOS);
      Firebase.RTDB.setBool(&fbdo, basePath + "Alert_Fall", isFalling);

      if (gps.location.isValid()) {
        Firebase.RTDB.setFloat(&fbdo, basePath + "GPS_Lat", gps.location.lat());
        Firebase.RTDB.setFloat(&fbdo, basePath + "GPS_Lng", gps.location.lng());
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1500));
  }
}