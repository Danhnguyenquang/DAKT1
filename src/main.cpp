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

// ===================== BIẾN LƯU MÃ THIẾT BỊ =====================
String deviceID = "";

// Khai báo Task cho FreeRTOS
TaskHandle_t FirebaseTaskHandle;

// ===================== KHAI BAO CAM BIEN =====================
MAX30105 particleSensor;
MPU6050 mpu6050(Wire);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// ===================== GPS =====================
static const int RXPin = 26, TXPin = 25;
static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);

// ===================== I2C ESP32 =====================
#define SDA_PIN 21
#define SCL_PIN 22

// ===================== THOI GIAN IN =====================
unsigned long lastPrintSensor = 0;
const unsigned long sensorInterval = 500; // Cập nhật màn hình nhanh mỗi 0.5s

// ===================== BIẾN TOÀN CỤC VÀ LỌC NHIỄU EMA =====================
float currentTempObj = 0.0;
float currentTempAmb = 0.0;
float currentMpuX = 0.0;
float currentMpuY = 0.0;
float currentMpuZ = 0.0;

// Hệ số lọc EMA (0.0 đến 1.0). Càng nhỏ số càng mượt.
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

// ===================== BO LOC TRUNG VI =====================
const int FILTER_SIZE = 7;
int bpmHistory[FILTER_SIZE] = {0};
int spo2History[FILTER_SIZE] = {0};
int bpmCount = 0;
int spo2Count = 0;
int bpmIndex = 0;
int spo2Index = 0;

// ===================== TFT ST7789 =====================
#define TFT_CS   27
#define TFT_DC   4
#define TFT_RST  16

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
void initTFT();
void drawStaticUI();
void updateTFT();
void TaskFirebase(void *pvParameters); 

// ===================== HAM HIEN THI TFT =====================
void initTFT()
{
  tft.init(240, 280);   
  tft.setRotation(2);   
  tft.fillScreen(ST77XX_BLACK);

  // Màn hình khởi động
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
  
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(20, 5);
  tft.println("HEALTH MONITOR");

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(20, 25);
  tft.print("ID: ");
  tft.println(deviceID);

  tft.drawLine(0, 38, 240, 38, ST77XX_BLUE);

  // Các nhãn cố định không bao giờ vẽ lại
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 45); tft.print("BPM: ");
  tft.setCursor(10, 70); tft.print("SpO2:");

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 100); tft.print("Obj: ");
  tft.setCursor(10, 125); tft.print("Amb: ");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(10, 160); tft.print("X: ");
  tft.setCursor(10, 175); tft.print("Y: ");
  tft.setCursor(10, 190); tft.print("Z: ");

  tft.setTextColor(ST77XX_MAGENTA);
  tft.setCursor(10, 215); tft.print("GPS: ");
}

void updateTFT()
{
  // Mẹo: Dùng màu nền (ST77XX_BLACK) làm tham số thứ 2 để xóa chữ cũ mà không chớp
  tft.setTextSize(2);
  
  // MAX30102
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setCursor(70, 45);
  if (bpmCount > 0) tft.printf("%-5d", getFilteredBPM());
  else tft.print("--   ");

  tft.setCursor(80, 70);
  if (spo2Count > 0) tft.printf("%d%%  ", getFilteredSpO2());
  else tft.print("--   ");

  // MLX90614
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(70, 100); tft.printf("%-6.1f C", currentTempObj);
  tft.setCursor(70, 125); tft.printf("%-6.1f C", currentTempAmb);

  // MPU6050
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setCursor(30, 160); tft.printf("%-7.1f", currentMpuX);
  tft.setCursor(30, 175); tft.printf("%-7.1f", currentMpuY);
  tft.setCursor(30, 190); tft.printf("%-7.1f", currentMpuZ);

  // GPS
  tft.setTextColor(ST77XX_MAGENTA, ST77XX_BLACK);
  if (gps.location.isValid())
  {
    tft.setCursor(50, 215); tft.print("OK        ");
    tft.setCursor(10, 230); tft.printf("%-9.4f, %-9.4f", gps.location.lat(), gps.location.lng());
  }
  else
  {
    tft.setCursor(50, 215); tft.print("Waiting...");
    tft.setCursor(10, 230); tft.print("                    "); // Xóa rác
  }
}

// ===================== HAM SAP XEP PHUC VU TRUNG VI =====================
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

// ===================== HAM GPS =====================
void displayInfo() {
  if (gps.location.isValid()) {
    // Ẩn in Serial để giảm gánh nặng xử lý
  }
}

// ===================== KHOI TAO MAX30102 =====================
bool initMAX30102() {
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("MAX30102 not found.");
    return false;
  }
  particleSensor.setup(60, 4, 2, 100, 411, 4096);
  particleSensor.setPulseAmplitudeRed(0x1F);
  particleSensor.setPulseAmplitudeIR(0x1F);
  particleSensor.setPulseAmplitudeGreen(0);
  return true;
}

// Nạp đầy 100 mẫu lần đầu
void initMAX30102Buffer() {
  if (!max30102Found) return;
  Serial.println("Dang nap bo dem MAX30102 (1 giay)...");
  for (byte i = 0; i < 100; i++) {
    while (particleSensor.available() == false) particleSensor.check();
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRateValue, &validHeartRate);
}

// ===================== UPDATE MAX30102 NHANH =====================
void updateMAX30102Fast() {
  if (!max30102Found) return;

  // Dịch mảng sang trái, giữ 75 mẫu cũ
  for (byte i = 25; i < 100; i++) {
    redBuffer[i - 25] = redBuffer[i];
    irBuffer[i - 25] = irBuffer[i];
  }

  // Chỉ đọc 25 mẫu mới
  for (byte i = 75; i < 100; i++) {
    while (particleSensor.available() == false) particleSensor.check();
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }

  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRateValue, &validHeartRate);

  if (validHeartRate && heartRateValue >= 50 && heartRateValue <= 120) {
    addBPMValue(heartRateValue);
  }
  if (validSPO2 && spo2 >= 80 && spo2 <= 100) {
    addSpO2Value(spo2);
  }
}

// ===================== SETUP =====================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  // ---------- KẾT NỐI WIFI ----------
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Dang ket noi WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi OK!");

  // ---------- TẠO MÃ THIẾT BỊ TỪ MAC ADDRESS ----------
  String mac = WiFi.macAddress();
  mac.replace(":", ""); 
  deviceID = "DEV_" + mac; 

  // ---------- KẾT NỐI FIREBASE ----------
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    signupOK = true;
  }
  config.token_status_callback = tokenStatusCallback; 
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Wire.begin(SDA_PIN, SCL_PIN);

  // ---------- KHỞI TẠO ----------
  initTFT(); 

  max30102Found = initMAX30102();
  if(max30102Found) initMAX30102Buffer();

  mpu6050.begin();
  mpu6050.calcGyroOffsets(true);

  mlx.begin();
  ss.begin(GPSBaud);

  // Lấy dữ liệu lần đầu làm nền tảng cho bộ lọc
  currentTempObj = mlx.readObjectTempC();
  currentTempAmb = mlx.readAmbientTempC();
  currentMpuX = mpu6050.getAngleX();
  currentMpuY = mpu6050.getAngleY();
  currentMpuZ = mpu6050.getAngleZ();

  drawStaticUI(); // Chuyển sang vẽ UI cố định trước khi vào Loop

  // ---------- KHỞI TẠO TASK TRÊN CORE 0 ----------
  xTaskCreatePinnedToCore(
    TaskFirebase,          
    "FirebaseTask",        
    10000,                 
    NULL,                  
    1,                     
    &FirebaseTaskHandle,   
    0);                    
}

// ===================== LOOP (Chạy trên Core 1) =====================
void loop()
{
  // ---------- DOC GPS ----------
  while (ss.available() > 0)
  {
    if (gps.encode(ss.read()))
      displayInfo();
  }

  // ---------- CAP NHAT ----------
  mpu6050.update();
  updateMAX30102Fast(); // Gọi hàm đọc nhanh

  // ---------- LỌC NHIỄU EMA ----------
  currentTempObj = (ALPHA_TEMP * mlx.readObjectTempC()) + ((1.0 - ALPHA_TEMP) * currentTempObj);
  currentTempAmb = (ALPHA_TEMP * mlx.readAmbientTempC()) + ((1.0 - ALPHA_TEMP) * currentTempAmb);
  
  currentMpuX = (ALPHA_MPU * mpu6050.getAngleX()) + ((1.0 - ALPHA_MPU) * currentMpuX);
  currentMpuY = (ALPHA_MPU * mpu6050.getAngleY()) + ((1.0 - ALPHA_MPU) * currentMpuY);
  currentMpuZ = (ALPHA_MPU * mpu6050.getAngleZ()) + ((1.0 - ALPHA_MPU) * currentMpuZ);

  // ---------- UPDATE TFT MỖI 0.5 GIÂY ----------
  if (millis() - lastPrintSensor >= sensorInterval)
  {
    lastPrintSensor = millis();

    Serial.println("=============== DU LIEU CAM BIEN ===============");
    Serial.println("ID THIẾT BỊ: " + deviceID);
    
    Serial.print("MAX30102 -> BPM: ");
    Serial.print(bpmCount > 0 ? getFilteredBPM() : 0);
    Serial.print(" | SpO2: ");
    Serial.println(spo2Count > 0 ? getFilteredSpO2() : 0);

    Serial.print("MPU6050  -> X: "); Serial.print(currentMpuX);
    Serial.print(" Y: "); Serial.print(currentMpuY);
    Serial.print(" Z: "); Serial.println(currentMpuZ);

    Serial.print("MLX90614 -> Obj: "); Serial.print(currentTempObj);
    Serial.print("*C | Amb: "); Serial.print(currentTempAmb); Serial.println("*C");

    Serial.println("================================================");

    updateTFT();
  }
}

// ===================== TASK FIREBASE (Chạy ngầm trên Core 0) =====================
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

      if (gps.location.isValid()) {
        Firebase.RTDB.setFloat(&fbdo, basePath + "GPS_Lat", gps.location.lat());
        Firebase.RTDB.setFloat(&fbdo, basePath + "GPS_Lng", gps.location.lng());
      }
    }
    
    // Đẩy dữ liệu nhanh mỗi 1.5 giây
    vTaskDelay(pdMS_TO_TICKS(1500));
  }
}