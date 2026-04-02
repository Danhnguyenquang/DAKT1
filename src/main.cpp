#include <Arduino.h>
#include <FS.h>       
#include <LittleFS.h> 
#include <Wire.h>
#include <WiFi.h>
#include <time.h> 

// ===================== AI MACHINE LEARNING =====================
#include <healthycare_inferencing.h> // Gọi "Bộ não" AI vào dự án

// Biến lưu trữ cho mảng dữ liệu AI (Cửa sổ trượt)
float ai_features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
size_t feature_ix = 0;
unsigned long last_ai_sample = 0;

// ===================== CAM BIEN SUC KHOE & MOI TRUONG =====================
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
#include <MPU6050_tockn.h>
#include <Adafruit_MLX90614.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h> 

// ===================== TFT & QR CODE =====================
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "all_frames.h" 
#include "qrcode.h"

// DINH NGHIA MAU SAC MOI
#define ST77XX_ORANGE 0xFD20
#define ST77XX_DARKBLUE 0x01E8
#define ST77XX_DARKGREEN 0x03E0
#define ST77XX_GRAY 0x8410

// ===================== FIREBASE =====================
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ===================== WIFI MANAGER & WEB & THOI TIET =====================
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "secrets.h"

String weatherURL = "http://api.openweathermap.org/data/2.5/forecast?lat=" LAT "&lon=" LON "&appid=" WEATHER_API_KEY "&units=metric&cnt=2&lang=vi";
JSONVar weatherData;
bool weatherValid = false;
String customText = "";

WebServer server(80);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;
String deviceID = "";
TaskHandle_t FirebaseTaskHandle;

// ===================== KHAI BÁO CHÂN PHẦN CỨNG MỚI =====================
#define DUST_VO_PIN  33   // Đã đổi sang G33
#define DUST_LED_PIN 14   // Giữ nguyên G14
#define TOUCH_PIN    15   // Đã đổi sang G15
#define BUZZER_PIN   13   // Đã đổi sang G13

#define SDA_PIN      21   // Giữ nguyên
#define SCL_PIN      22   // Giữ nguyên

// Cấu hình chân TFT thẳng hàng cho mạch 1 lớp
#define TFT_SCL  16
#define TFT_SDA  17
#define TFT_RST  5
#define TFT_DC   18
#define TFT_CS   19
Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);

// Cấu hình GPS: GPS TX -> ESP RX(35) | GPS RX -> ESP TX(32)
static const int RXPin = 35, TXPin = 32;
static const uint32_t GPSBaud = 9600;
TinyGPSPlus gps;
HardwareSerial gpsSerial(2); 

MAX30105 particleSensor;
MPU6050 mpu6050(Wire);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// ===================== BIẾN TOÀN CỤC =====================
unsigned long lastPrintSensor = 0;
const unsigned long sensorInterval = 500; 

float currentTempObj = 0.0, currentTempAmb = 0.0;
float currentMpuX = 0.0, currentMpuY = 0.0, currentMpuZ = 0.0;
float currentDust = 0.0;
bool isSOS = false, isFalling = false, isHealthAlert = false; 

String healthAlertReason = ""; 
unsigned long healthDangerTimer = 0; 
unsigned long lastBeepTime = 0;
bool buzzerState = false;

String currentTimeStr = "--:--:-- --/--/----";
String lastMeasureTimeStr = "Chua do";
String currentStatus = "BINH THUONG"; 

unsigned long lastHistorySave = 0; 
const unsigned long historyInterval = 15 * 60 * 1000; 
bool lastSOSState = false;
bool lastFallState = false;
bool lastHealthState = false;

// --- BIẾN THEO DÕI BIẾN ĐỘNG DỮ LIỆU ---
float lastSavedTemp = 0.0;
int lastSavedBPM = 0;
int lastSavedSpO2 = 0;
float lastSavedDust = 0.0;

uint32_t irBuffer[100];
uint32_t redBuffer[100];
int32_t spo2 = 0;
int8_t validSPO2 = 0;
int32_t heartRateValue = 0;
int8_t validHeartRate = 0;
bool max30102Found = false;

byte currentLEDPower = 0x1F; 
int measurementProgress = 0; 
bool fingerPresent = false; 
unsigned long measureCompleteTime = 0; 
unsigned long lastContinuousUpdateTime = 0; 

int validSamplesCollected = 0;
int ignoredSamples = 0;
const int SAMPLES_TO_IGNORE = 2; 
const int TARGET_SAMPLES = 3;    

int lastBPM = 0;
int lastSpO2 = 0;

const int FILTER_SIZE = 7;
int bpmHistory[FILTER_SIZE] = {0}, spo2History[FILTER_SIZE] = {0};
int bpmCount = 0, spo2Count = 0;
int bpmIndex = 0, spo2Index = 0;

int userScreenIndex = 0; 
int currentScreen = 0; 
int lastScreen = -1;
unsigned long lastAnimFrame = 0;
int currentFrame = 0;
#define FRAME_DELAY 42

volatile unsigned long touchStartTime = 0;
volatile bool isPressed = false;
volatile bool shortPressTriggered = false;

void IRAM_ATTR touchISR() {
  if (digitalRead(TOUCH_PIN) == HIGH) {
    touchStartTime = millis();
    isPressed = true;
  } else {
    if (isPressed) {
      unsigned long duration = millis() - touchStartTime;
      // FIX DELAY: Giảm mức chặn nhiễu từ 50ms xuống 20ms để bắt được chạm lướt siêu nhanh
      if (duration > 20 && duration < 600) { 
        shortPressTriggered = true; 
      }
    }
    isPressed = false;
  }
}

void updateTFT();
void drawWeatherScreenStatic();
void drawWeatherAnimationFrame();
void drawMeasuringUI();
void drawStaticUI();
void drawQRScreen(); 
void TaskFirebase(void *pvParameters);
void handleTouchToggle();

String removeAccents(const String& str) { 
  String s = str;
  s.replace("á", "a"); s.replace("à", "a"); s.replace("ả", "a"); s.replace("ã", "a"); s.replace("ạ", "a");
  s.replace("ă", "a"); s.replace("ắ", "a"); s.replace("ằ", "a"); s.replace("ẳ", "a"); s.replace("ẵ", "a"); s.replace("ặ", "a");
  s.replace("â", "a"); s.replace("ấ", "a"); s.replace("ầ", "a"); s.replace("ẩ", "a"); s.replace("ẫ", "a"); s.replace("ậ", "a");
  s.replace("đ", "d"); s.replace("Đ", "D");
  s.replace("é", "e"); s.replace("è", "e"); s.replace("ẻ", "e"); s.replace("ẽ", "e"); s.replace("ẹ", "e");
  s.replace("ê", "e"); s.replace("ế", "e"); s.replace("ề", "e"); s.replace("ể", "e"); s.replace("ễ", "e"); s.replace("ệ", "e");
  s.replace("í", "i"); s.replace("ì", "i"); s.replace("ỉ", "i"); s.replace("ĩ", "i"); s.replace("ị", "i");
  s.replace("ó", "o"); s.replace("ò", "o"); s.replace("ỏ", "o"); s.replace("õ", "o"); s.replace("ọ", "o");
  s.replace("ô", "o"); s.replace("ố", "o"); s.replace("ồ", "o"); s.replace("ổ", "o"); s.replace("ỗ", "o"); s.replace("ộ", "o");
  s.replace("ơ", "o"); s.replace("ớ", "o"); s.replace("ờ", "o"); s.replace("ở", "o"); s.replace("ỡ", "o"); s.replace("ợ", "o");
  s.replace("ú", "u"); s.replace("ù", "u"); s.replace("ủ", "u"); s.replace("ũ", "u"); s.replace("ụ", "u");
  s.replace("ư", "u"); s.replace("ứ", "u"); s.replace("ừ", "u"); s.replace("ử", "u"); s.replace("ữ", "u"); s.replace("ự", "u");
  s.replace("ý", "y"); s.replace("ỳ", "y"); s.replace("ỷ", "y"); s.replace("ỹ", "y"); s.replace("ỵ", "y");
  s.replace("_", " "); 
  return s;
}

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
  return (size % 2 == 1) ? temp[size / 2] : (temp[size / 2 - 1] + temp[size / 2]) / 2;
}
void addBPMValue(int value) {
  bpmHistory[bpmIndex] = value; bpmIndex = (bpmIndex + 1) % FILTER_SIZE;
  if (bpmCount < FILTER_SIZE) bpmCount++;
}
void addSpO2Value(int value) {
  spo2History[spo2Index] = value; spo2Index = (spo2Index + 1) % FILTER_SIZE;
  if (spo2Count < FILTER_SIZE) spo2Count++;
}
int getFilteredBPM() { return getMedian(bpmHistory, bpmCount); }
int getFilteredSpO2() { return getMedian(spo2History, spo2Count); }

bool initMAX30102() {
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) return false;
  particleSensor.setup(60, 4, 2, 100, 411, 4096);
  currentLEDPower = 0x1F; 
  particleSensor.setPulseAmplitudeRed(currentLEDPower);
  particleSensor.setPulseAmplitudeIR(currentLEDPower);
  particleSensor.setPulseAmplitudeGreen(0);
  return true;
}

void updateMAX30102Fast() {
  if (!max30102Found) return;
  
  for (byte i = 25; i < 100; i++) {
    redBuffer[i - 25] = redBuffer[i]; irBuffer[i - 25] = irBuffer[i];
  }
  for (byte i = 75; i < 100; i++) {
    long start = millis();
    while (!particleSensor.available() && millis() - start < 100) { 
        particleSensor.check(); 
        handleTouchToggle(); // FIX DELAY: Cho phép quét nút bấm ngay khi đang chờ MAX30102
        yield(); 
    }
    redBuffer[i] = particleSensor.getRed(); irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }

  if (irBuffer[99] < 50000) {
    if (fingerPresent) {
      fingerPresent = false;
      measurementProgress = 0; 
      bpmCount = 0; spo2Count = 0;
      bpmIndex = 0; spo2Index = 0;
      validSamplesCollected = 0;
      ignoredSamples = 0;
      lastContinuousUpdateTime = 0; 
      lastScreen = -1; 

      if (currentLEDPower != 0x1F) {
        currentLEDPower = 0x1F;
        particleSensor.setPulseAmplitudeRed(currentLEDPower);
        particleSensor.setPulseAmplitudeIR(currentLEDPower);
      }
    }
    return;        
  }

  if (!fingerPresent) {
    fingerPresent = true;
    bpmCount = 0; spo2Count = 0; 
    bpmIndex = 0; spo2Index = 0;
    validSamplesCollected = 0;
    ignoredSamples = 0;
    
    int32_t currentIR = irBuffer[99];
    if (currentIR < 80000) {
      currentLEDPower = 0x3F; 
    } else if (currentIR > 130000) {
      currentLEDPower = 0x15; 
    } else {
      currentLEDPower = 0x1F; 
    }
    particleSensor.setPulseAmplitudeRed(currentLEDPower);
    particleSensor.setPulseAmplitudeIR(currentLEDPower);

    measurementProgress = 10;
  }

  maxim_heart_rate_and_oxygen_saturation(irBuffer, 100, redBuffer, &spo2, &validSPO2, &heartRateValue, &validHeartRate);
  
  if (validHeartRate && validSPO2 && spo2 >= 90 && spo2 <= 100) {
    int currentBPM = heartRateValue;

    if (currentBPM > 120 && currentBPM <= 160) {
      currentBPM = currentBPM / 2;
    }

    if (currentBPM >= 50 && currentBPM <= 115) {
      if (ignoredSamples < SAMPLES_TO_IGNORE) {
        ignoredSamples++;
      } else {
        if (validSamplesCollected < TARGET_SAMPLES) {
          addBPMValue(currentBPM); 
          addSpO2Value(spo2);
          validSamplesCollected++;
          measurementProgress = 10 + (validSamplesCollected * 90 / TARGET_SAMPLES);

          if (validSamplesCollected >= TARGET_SAMPLES) {
            measurementProgress = 100; 
            measureCompleteTime = millis();
            lastContinuousUpdateTime = millis(); 
            lastMeasureTimeStr = currentTimeStr; 
            
            lastBPM = getFilteredBPM(); 
            lastSpO2 = getFilteredSpO2(); 
          }
        } 
        else if (measurementProgress == 100) {
          addBPMValue(currentBPM); 
          addSpO2Value(spo2);
          
          if (millis() - lastContinuousUpdateTime >= 2000) {
            lastBPM = getFilteredBPM(); 
            lastSpO2 = getFilteredSpO2(); 
            lastMeasureTimeStr = currentTimeStr;
            lastContinuousUpdateTime = millis();
          }
        }
      }
    }
  }

  if (measurementProgress < 90 && fingerPresent) {
    measurementProgress += 2; 
  }
}

void updateDustSensor() {
  digitalWrite(DUST_LED_PIN, LOW); 
  delayMicroseconds(280);          
  
  int voMeasured = analogRead(DUST_VO_PIN); 
  
  delayMicroseconds(40);           
  digitalWrite(DUST_LED_PIN, HIGH); 
  delayMicroseconds(9680);         
  
  float voltage = voMeasured * (3.3 / 4095.0); 
  float rawDustUg = ((0.17 * voltage) - 0.1) * 1000.0;
  
  if (rawDustUg < 0) {
    rawDustUg = 0; 
  }
  
  currentDust = (0.1 * rawDustUg) + (0.9 * currentDust);
}

void updateGPSTime() {
  if (gps.time.isValid() && gps.date.isValid()) {
    int h = gps.time.hour() + 7; 
    int d = gps.date.day(), m = gps.date.month(), y = gps.date.year();
    if (h >= 24) { h -= 24; d += 1; if (d > 31) { d = 1; m += 1; } if (m > 12) { m = 1; y += 1; } }
    char timeBuf[25];
    sprintf(timeBuf, "%02d:%02d:%02d %02d/%02d/%04d", h, gps.time.minute(), gps.time.second(), d, m, y);
    currentTimeStr = String(timeBuf);
  }
}

void handleTouchToggle() {
  if (shortPressTriggered) {
    userScreenIndex = (userScreenIndex + 1) % 3; 
    lastScreen = -1; 
    updateTFT();
    shortPressTriggered = false; 
  }

  static bool longPressHandled = false;
  if (isPressed) {
    if (!longPressHandled && (millis() - touchStartTime >= 600)) {
      if (isFalling || isSOS || isHealthAlert) { 
        isFalling = false; 
        isSOS = false; 
        isHealthAlert = false;
        healthAlertReason = ""; 
        lastBPM = 0;   
        lastSpO2 = 0;  
      } else { 
        isSOS = true; 
      }
      longPressHandled = true;
      lastScreen = -1;
      updateTFT();
    }
  } else {
    longPressHandled = false;
  }
  
  bool danger = false;
  String currentReason = "";

  if (lastBPM > 0 && (lastBPM < 40 || lastBPM > 130)) {
    danger = true;
    currentReason += "Nhip tim bat thuong. ";
  }
  if (lastSpO2 > 0 && lastSpO2 < 92) {
    danger = true;
    currentReason += "SpO2 xuong thap. ";
  }
  if (currentTempObj > 37.5) {
    danger = true;
    currentReason += "Nhiet do cao (Sot). ";
  } else if (currentTempObj > 25.0 && currentTempObj < 30.0) {
    danger = true;
    currentReason += "Nhiet do qua thap. ";
  }
  if (currentDust > 100.0) {
    danger = true;
    currentReason += "Bui min muc do cao. ";
  }

  if (danger) {
    if (healthDangerTimer == 0) healthDangerTimer = millis(); 
    if (millis() - healthDangerTimer > 3000) {
      isHealthAlert = true; 
      healthAlertReason = currentReason;
    }
  } else { 
    healthDangerTimer = 0; 
    isHealthAlert = false; 
    healthAlertReason = ""; 
  }

  if (isSOS || isFalling || isHealthAlert) {
    if (millis() - lastBeepTime > 200) { 
        lastBeepTime = millis(); 
        buzzerState = !buzzerState; 
        digitalWrite(BUZZER_PIN, buzzerState ? LOW : HIGH); 
    }
  } else { 
    digitalWrite(BUZZER_PIN, HIGH); 
    buzzerState = false; 
  }
}

void showBootScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN); tft.setTextSize(2);
  tft.setCursor(20, 120); tft.println("DANG KHOI DONG...");
}

void drawStaticUI() {
  tft.fillScreen(ST77XX_BLACK);
  
  tft.fillRoundRect(0, 0, 240, 35, 0, ST77XX_DARKGREEN); 
  tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2); 
  tft.setCursor(35, 10); tft.println("TRAM Y TE MINI");

  tft.setTextSize(2); tft.setTextColor(ST77XX_ORANGE);
  tft.setCursor(10, 55); tft.print("Nhip tim :"); 
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(10, 95); tft.print("Oxy mau  :");
  
  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 140); tft.print("Nhiet do co the:"); 
  tft.setCursor(10, 170); tft.print("Nhiet do moi tr:"); 
  tft.setCursor(10, 200); tft.print("Nong do bui min:"); 
  
  tft.drawLine(10, 225, 230, 225, ST77XX_GRAY);
  
  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE); 
  tft.setCursor(10, 240); tft.print("Trang thai:");
  tft.setCursor(10, 260); tft.print("Thoi gian :"); 
}

void drawMeasuringUI() {
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRoundRect(10, 10, 220, 260, 10, ST77XX_CYAN);
  tft.setTextColor(ST77XX_YELLOW); tft.setTextSize(2); 
  tft.setCursor(20, 30); tft.print("DANG DO TIM MACH");
  tft.drawLine(20, 60, 220, 60, ST77XX_CYAN);
  tft.setTextColor(ST77XX_WHITE); tft.setTextSize(1); 
  tft.setCursor(25, 240); tft.print("* Vui long dat im ngon tay *");
}

void drawWeatherScreenStatic() {
  tft.fillScreen(ST77XX_BLACK);

  tft.fillRoundRect(0, 0, 240, 35, 0, ST77XX_DARKBLUE);
  tft.setTextColor(ST77XX_WHITE); 
  tft.setTextSize(2);
  tft.setCursor(18, 10); 
  tft.print("THOI TIET TAI CHO");

  if (!weatherValid) {
    tft.setTextColor(ST77XX_RED); 
    tft.setTextSize(1);
    tft.setCursor(45, 120); 
    tft.print("Dang cap nhat du lieu...");
    return;
  }

  double curTemp = (double)weatherData["list"][0]["main"]["temp"];
  int curHumi = (int)weatherData["list"][0]["main"]["humidity"];
  String curDesc = removeAccents((const char*)weatherData["list"][0]["weather"][0]["description"]);
  curDesc.toUpperCase();

  tft.setTextColor(ST77XX_YELLOW); 
  tft.setTextSize(4); 
  tft.setCursor(15, 50); 
  tft.print(curTemp, 1);
  
  int tempWidth = String(curTemp, 1).length() * 24;
  tft.setTextSize(1); 
  tft.setCursor(15 + tempWidth + 5, 50); 
  tft.print("o"); 
  tft.setTextSize(2); 
  tft.setCursor(15 + tempWidth + 12, 55); 
  tft.print("C");

  tft.setTextColor(ST77XX_CYAN); 
  tft.setTextSize(2);
  tft.setCursor(15, 90); 
  tft.print(curDesc);

  tft.setTextColor(ST77XX_WHITE); 
  tft.setTextSize(1);
  tft.setCursor(15, 120); 
  tft.printf("Do am khong khi: %d%%", curHumi);

  tft.drawLine(15, 140, 225, 140, ST77XX_GRAY);

  double nextTemp = (double)weatherData["list"][1]["main"]["temp"];
  String nextDesc = removeAccents((const char*)weatherData["list"][1]["weather"][1]["description"]);
  nextDesc.toUpperCase();

  tft.fillRoundRect(15, 155, 210, 25, 4, ST77XX_DARKGREEN);
  tft.setTextColor(ST77XX_WHITE); 
  tft.setTextSize(1);
  tft.setCursor(65, 163); 
  tft.print("DU BAO 3 GIO TOI");

  tft.setTextColor(ST77XX_MAGENTA); 
  tft.setTextSize(3);
  tft.setCursor(20, 195); 
  tft.print(nextTemp, 1); 
  
  int nextTempWidth = String(nextTemp, 1).length() * 18;
  tft.setTextSize(1); 
  tft.setCursor(20 + nextTempWidth + 3, 195); 
  tft.print("o");
  tft.setTextSize(2); 
  tft.setCursor(20 + nextTempWidth + 10, 200); 
  tft.print("C");

  tft.setTextColor(ST77XX_ORANGE); 
  if (nextDesc.length() > 18) {
    tft.setTextSize(1); 
    tft.setCursor(20, 235);
  } else {
    tft.setTextSize(2); 
    tft.setCursor(20, 235); 
  }
  tft.print(nextDesc);
}

void drawQRScreen() {
  tft.fillScreen(ST77XX_WHITE); 

  tft.fillRoundRect(0, 0, 240, 40, 0, ST77XX_DARKBLUE);
  tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2);
  tft.setCursor(15, 12); tft.print("MA KET NOI (APP)");

  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(3)]; 
  qrcode_initText(&qrcode, qrcodeData, 3, 0, deviceID.c_str());

  int scale = 6; 
  int xOffset = (240 - qrcode.size * scale) / 2;
  int yOffset = (280 - qrcode.size * scale) / 2 + 15;

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        tft.fillRect(xOffset + x * scale, yOffset + y * scale, scale, scale, ST77XX_BLACK);
      }
    }
  }

  tft.setTextColor(ST77XX_BLACK); tft.setTextSize(1);
  tft.setCursor((240 - (10 * 6 + deviceID.length() * 6)) / 2, 260); 
  tft.print("DeviceID: "); tft.print(deviceID);
}

void drawWeatherAnimationFrame() {
  int frameX = 145; 
  int frameY = 50; 
  tft.drawBitmap(frameX, frameY, frames[currentFrame], FRAME_WIDTH, FRAME_HEIGHT, ST77XX_WHITE);
  currentFrame = (currentFrame + 1) % TOTAL_FRAMES;
}

void updateTFT() {
  if (measurementProgress > 0) {
    if (measurementProgress < 100) {
      currentScreen = 1; 
    } else {
      if (millis() - measureCompleteTime < 1500) {
        currentScreen = 1; 
      } else {
        currentScreen = (userScreenIndex == 0) ? 0 : ((userScreenIndex == 1) ? 2 : 3);
      }
    }
  } else {
    currentScreen = (userScreenIndex == 0) ? 0 : ((userScreenIndex == 1) ? 2 : 3);
  }

  if (currentScreen != lastScreen) {
    if (currentScreen == 0) drawStaticUI();
    else if (currentScreen == 1) drawMeasuringUI();
    else if (currentScreen == 2) drawWeatherScreenStatic();
    else if (currentScreen == 3) drawQRScreen();
    lastScreen = currentScreen;
  }

  if (currentScreen == 0) { 
    tft.setTextSize(3); tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    
    tft.setCursor(140, 50); 
    (lastBPM > 0) ? tft.printf("%-4d", lastBPM) : tft.print("--  ");
    
    tft.setCursor(140, 90); 
    if (lastSpO2 > 0) {
      tft.printf("%d%%  ", lastSpO2);
    } else {
      tft.print("--  ");
    }
    
    tft.setTextSize(2); tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(130, 135); tft.printf("%-5.1fC", currentTempObj);
    tft.setCursor(130, 165); tft.printf("%-5.1fC", currentTempAmb);
    
    tft.setCursor(130, 195); 
    tft.printf("%-4.0f", currentDust); 
    tft.setTextSize(1); 
    tft.print(" ug/m3  "); 
    
    tft.setTextSize(1); 
    tft.setCursor(85, 240);
    
    if (isSOS) { 
      tft.setTextColor(ST77XX_RED, ST77XX_BLACK); tft.print("BAO DONG KHOAN CAP! "); 
      currentStatus = "SOS KHOAN CAP!";
    } 
    else if (isFalling) { 
      tft.setTextColor(ST77XX_RED, ST77XX_BLACK); tft.print("PHAT HIEN TE NGA!   "); 
      currentStatus = "PHAT HIEN TE NGA!";
    } 
    else if (isHealthAlert) { 
      tft.setTextColor(ST77XX_ORANGE, ST77XX_BLACK); 
      
      String displayReason = healthAlertReason;
      if (displayReason.indexOf(".") > 0) {
        displayReason = displayReason.substring(0, displayReason.indexOf("."));
      }
      
      while (displayReason.length() < 20) {
        displayReason += " ";
      }
      
      tft.print(displayReason); 
      currentStatus = healthAlertReason; 
    }
    else { 
      tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK); tft.print("BINH THUONG         "); 
      currentStatus = "BINH THUONG";
    }
    
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK); 
    tft.setCursor(85, 260); tft.print(currentTimeStr);
  } 
  else if (currentScreen == 1) { 
    tft.setTextSize(2); tft.setCursor(30, 90); tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK); 
    if (measurementProgress < 30) tft.print("Dang lay mau...   "); 
    else if (measurementProgress < 60) tft.print("Dang loc nhieu... "); 
    else if (measurementProgress < 90) tft.print("Phan tich du lieu."); 
    else tft.print("Da hoan tat!      ");
    
    tft.drawRect(20, 140, 200, 25, ST77XX_WHITE);
    tft.fillRect(22, 142, (measurementProgress * 196) / 100, 21, ST77XX_GREEN);
    tft.setCursor(100, 180); tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); tft.printf("%-3d%%", measurementProgress);
  }
  else if (currentScreen == 2) { 
    if (millis() - lastAnimFrame >= FRAME_DELAY) {
      lastAnimFrame = millis();
      drawWeatherAnimationFrame();
    }
  }
}

const char htmlPage[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang="vi"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>He Thong Giam Sat Y Te</title><style>body { font-family: sans-serif; text-align: center; background: #f4f4f4; padding: 20px; } button { padding: 10px 20px; background: #007bff; color: #fff; border-radius: 5px; border:none; }</style></head><body><h2>HE THONG GIAM SAT Y TE TRAM KHOANG CACH</h2><input type="text" id="oledText" placeholder="Nhap thong bao cho benh nhan"><button onclick="sendText()">Gui Len Man Hinh</button><script>function sendText() { fetch("/settext?msg=" + document.getElementById("oledText").value).then(() => alert("Da gui thanh cong!")); }</script></body></html>)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", htmlPage); 
}

void handleData() {
  String out = weatherValid ? "{\"temp\":" + String((double)weatherData["list"][0]["main"]["temp"], 1) + "}" : "{\"temp\":0}";
  server.send(200, "application/json", out);
}

// ===================== TASK FIREBASE & THỜI TIẾT (Lõi 0) =====================
unsigned long lastWeatherFetch = 0;
const unsigned long weatherFetchInterval = 300000; // 5 phút

void TaskFirebase(void *pvParameters) {
  while (true) {
    if (WiFi.status() == WL_CONNECTED && (millis() - lastWeatherFetch >= weatherFetchInterval || lastWeatherFetch == 0)) {
      HTTPClient http;
      http.begin(weatherURL);
      int httpResponseCode = http.GET();
      if (httpResponseCode > 0) {
        String payload = http.getString();
        JSONVar tempWeather = JSON.parse(payload);
        if (JSON.typeof(tempWeather) != "undefined" && tempWeather.hasOwnProperty("list")) {
          weatherData = tempWeather; 
          weatherValid = true;
        }
      }
      http.end();
      lastWeatherFetch = millis();
      if (currentScreen == 2) drawWeatherScreenStatic();
    }

    if (Firebase.ready() && signupOK && WiFi.status() == WL_CONNECTED) {
      FirebaseJson json;
      
      String trangThaiDo = "Cho do";
      if (fingerPresent) {
        if (measurementProgress < 100) trangThaiDo = "Dang do...";
        else trangThaiDo = "Do lien tuc"; 
      }
      
      String realtimePath = "Devices/" + deviceID + "/";

      json.set("BPM", lastBPM); 
      json.set("SpO2", lastSpO2);
      json.set("TempObj", currentTempObj); 
      json.set("TempAmb", currentTempAmb);
      json.set("AngleX", currentMpuX); 
      json.set("AngleY", currentMpuY); 
      json.set("AngleZ", currentMpuZ);
      json.set("Dust", currentDust); 
      json.set("Alert_SOS", isSOS); 
      json.set("Alert_Fall", isFalling);
      json.set("Alert_Health", isHealthAlert); 
      json.set("Alert_Reason", healthAlertReason); 
      json.set("LastMeasureTime", lastMeasureTimeStr);
      json.set("TrangThai", currentStatus);
      json.set("ThoiGian", currentTimeStr);
      json.set("TrangThaiDo", trangThaiDo);

      if (gps.location.isValid()) { 
        json.set("GPS_Lat", gps.location.lat()); 
        json.set("GPS_Lng", gps.location.lng()); 
      }

      Firebase.RTDB.setJSON(&fbdo, realtimePath, &json);
      
      bool shouldSaveHistory = false;
      
      if (millis() - lastHistorySave >= historyInterval || lastHistorySave == 0) shouldSaveHistory = true;
      if (isSOS && !lastSOSState) shouldSaveHistory = true;
      if (isFalling && !lastFallState) shouldSaveHistory = true;
      if (isHealthAlert && !lastHealthState) shouldSaveHistory = true;

      if (lastHistorySave > 0) { 
        if (abs(currentTempObj - lastSavedTemp) >= 10.0) shouldSaveHistory = true;
        if (lastBPM > 0 && abs(lastBPM - lastSavedBPM) >= 20) shouldSaveHistory = true;
        if (lastSpO2 > 0 && abs(lastSpO2 - lastSavedSpO2) >= 5) shouldSaveHistory = true;
        if (abs(currentDust - lastSavedDust) >= 10.0) shouldSaveHistory = true;
      }

      lastSOSState = isSOS;
      lastFallState = isFalling;
      lastHealthState = isHealthAlert;

      if (shouldSaveHistory) {
        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        
        char timeKey[30];
        sprintf(timeKey, "%04d-%02d-%02d_%02d-%02d-%02d", 
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        
        String historyPath = "Histories/" + deviceID + "/" + String(timeKey);
        Firebase.RTDB.setJSON(&fbdo, historyPath, &json);
        
        lastHistorySave = millis();
        lastSavedTemp = currentTempObj;
        lastSavedBPM = lastBPM;
        lastSavedSpO2 = lastSpO2;
        lastSavedDust = currentDust;
      }
      
      fbdo.clear(); 
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000)); 
  }
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  
  pinMode(TOUCH_PIN, INPUT); 
  attachInterrupt(digitalPinToInterrupt(TOUCH_PIN), touchISR, CHANGE);
  
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, HIGH); 
  pinMode(DUST_LED_PIN, OUTPUT);

  Wire.begin(SDA_PIN, SCL_PIN);
  
  // Bẻ lái giao tiếp SPI của màn hình TFT sang các chân mới
  SPI.begin(TFT_SCL, -1, TFT_SDA, TFT_CS); 
  
  tft.init(240, 280); tft.setRotation(2); tft.setTextWrap(false);
  showBootScreen();

  WiFiManager wm;
  wm.setConfigPortalTimeout(120); 
  if(!wm.autoConnect("ESP32_Y_Te")) {
    Serial.println("Chay Offline Mode (Khong co WiFi)");
  }

  String mac = WiFi.macAddress(); mac.replace(":", ""); deviceID = "DEV_" + mac; 

  if (WiFi.status() == WL_CONNECTED) {
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    int ntpTimeout = 0;
    while (time(nullptr) < 1600000000 && ntpTimeout < 20) { 
      delay(500);
      ntpTimeout++;
    }

    config.timeout.socketConnection = 30 * 1000; 
    config.api_key = FIREBASE_API_KEY; 
    config.database_url = DATABASE_URL;
    auth.user.email = "esp32@gmail.com"; 
    auth.user.password = "12345678";
    signupOK = true; 
    config.token_status_callback = tokenStatusCallback; 
    
    Firebase.begin(&config, &auth); 
    Firebase.reconnectWiFi(true);
  }

  if (MDNS.begin("esp32")) Serial.println("mDNS: http://esp32.local");
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/settext", []() {
    if (server.hasArg("msg")) customText = server.arg("msg");
    if (currentScreen == 2) drawWeatherScreenStatic();
    server.send(200, "text/plain", "OK");
  });
  server.begin();

  max30102Found = initMAX30102();
  mpu6050.begin(); mpu6050.calcGyroOffsets(true);
  mlx.begin();
  gpsSerial.begin(GPSBaud, SERIAL_8N1, RXPin, TXPin);

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_YELLOW); tft.setTextSize(2);
  tft.setCursor(30, 110); tft.println("DANG ON DINH");
  tft.setCursor(45, 140); tft.println("CAM BIEN...");
  
  delay(2500); 

  currentTempObj = mlx.readObjectTempC();
  currentTempAmb = mlx.readAmbientTempC();
  
  mpu6050.update();
  currentMpuX = mpu6050.getAngleX();
  currentMpuY = mpu6050.getAngleY();
  currentMpuZ = mpu6050.getAngleZ();

  updateDustSensor(); 
  delay(10);
  updateDustSensor();

  xTaskCreatePinnedToCore(TaskFirebase, "FirebaseTask", 16384, NULL, 1, &FirebaseTaskHandle, 0);                    
  lastScreen = -1; 
}

// ===================== LOOP =====================
void loop() {
  server.handleClient();
  handleTouchToggle();
  while (gpsSerial.available() > 0) gps.encode(gpsSerial.read()); 
  
  updateGPSTime();
  updateMAX30102Fast(); 
  updateDustSensor();       

  currentTempObj = (0.2 * mlx.readObjectTempC()) + (0.8 * currentTempObj);
  currentTempAmb = (0.2 * mlx.readAmbientTempC()) + (0.8 * currentTempAmb);

  // ================= BỘ NÃO AI HOẠT ĐỘNG TẠI ĐÂY =================
  // Lấy mẫu cảm biến theo đúng tần số (thường là 5ms cho dữ liệu SisFall)
  if (millis() - last_ai_sample >= EI_CLASSIFIER_INTERVAL_MS) {
      last_ai_sample = millis();

      // Cập nhật gia tốc thực tế
      mpu6050.update();
      currentMpuX = (0.3 * mpu6050.getAngleX()) + (0.7 * currentMpuX);
      currentMpuY = (0.3 * mpu6050.getAngleY()) + (0.7 * currentMpuY);
      currentMpuZ = (0.3 * mpu6050.getAngleZ()) + (0.7 * currentMpuZ);

      // Nạp dữ liệu vào mảng (Bảo vệ tràn)
      if (feature_ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
          ai_features[feature_ix++] = mpu6050.getAccX();
          ai_features[feature_ix++] = mpu6050.getAccY();
          ai_features[feature_ix++] = mpu6050.getAccZ();
      }

      // Khi mảng "cửa sổ" đã đầy -> Tiến hành suy luận
      if (feature_ix >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
          signal_t signal;
          int err = numpy::signal_from_buffer(ai_features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

          if (err == 0) {
              ei_impulse_result_t result = { 0 };
              err = run_classifier(&signal, &result, false);

              if (err == EI_IMPULSE_OK) {
                  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
                      // Phát hiện nhãn "Fall" và độ tin cậy > 80% (0.8)
                      if (strcmp(result.classification[ix].label, "Fall") == 0 && result.classification[ix].value > 0.8) {
                          isFalling = true; // Kích hoạt báo động té ngã!
                      }
                  }
              }
          }

          // Trượt cửa sổ: Xóa 50% dữ liệu cũ, nhường chỗ cho dữ liệu mới ở chu kỳ sau
          size_t shift_elements = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE / 2;
          memmove(ai_features, ai_features + shift_elements, (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - shift_elements) * sizeof(float));
          feature_ix -= shift_elements; 
      }
  }
  // ===============================================================

  if (millis() - lastPrintSensor >= sensorInterval) {
    lastPrintSensor = millis();
    updateTFT();
  }
  
  // Rút ngắn độ trễ để AI bắt được dữ liệu liên tục và chính xác nhất
  delay(1); 
}