#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <time.h>

#include <ArduinoOTA.h>

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
#include <Adafruit_MLX90614.h>
#include <HardwareSerial.h>
#include <MPU6050_tockn.h>
#include <TinyGPSPlus.h>

// ===================== TFT & QR CODE =====================
#include "all_frames.h"
#include "qrcode.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// DINH NGHIA MAU SAC MOI
#define ST77XX_ORANGE 0xFD20
#define ST77XX_DARKBLUE 0x01E8
#define ST77XX_DARKGREEN 0x03E0
#define ST77XX_GRAY 0x8410

// ===================== FIREBASE =====================
#include "addons/RTDBHelper.h"
#include "addons/TokenHelper.h"
#include <Firebase_ESP_Client.h>

// ===================== WIFI MANAGER & WEB & THOI TIET =====================
#include "secrets.h"
#include <Arduino_JSON.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFiManager.h>

String weatherURL =
    "http://api.openweathermap.org/data/2.5/forecast?lat=" LAT "&lon=" LON
    "&appid=" WEATHER_API_KEY "&units=metric&cnt=2&lang=vi";
JSONVar weatherData;
bool weatherValid = false;
String customText = "";

String geminiAdvice = "Vui long do Nhip tim/SpO2 de nhan loi khuyen!";
unsigned long lastGeminiFetch = 0;
volatile bool needGeminiFetch = false;
volatile bool needWeatherRedraw = false;
volatile bool needAIRedraw = false;

SemaphoreHandle_t dataMutex = NULL;

WebServer server(80);
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;
String deviceID = "";
TaskHandle_t FirebaseTaskHandle;

// ===================== KHAI BÁO CHÂN PHẦN CỨNG MỚI =====================
#define DUST_VO_PIN 33  // Đã đổi sang G33
#define DUST_LED_PIN 14 // Giữ nguyên G14
#define TOUCH_PIN 15    // Đã đổi sang G15
#define BUZZER_PIN 13   // Đã đổi sang G13

#define SDA_PIN 21 // Giữ nguyên
#define SCL_PIN 22 // Giữ nguyên

// Cấu hình chân TFT thẳng hàng cho mạch 1 lớp
#define TFT_SCL 16
#define TFT_SDA 17
#define TFT_RST 5
#define TFT_DC 18
#define TFT_CS 19
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

// ===================== SMART FALL DETECTION (Đếm ngược 10s)
// =====================
bool fallWarning = false;
unsigned long fallWarningStart = 0;
const unsigned long FALL_COUNTDOWN_MS = 10000;

// ===================== DISPLAY TIMEOUT (Tắt màn hình 30s)
// =====================
unsigned long lastActivityTime = 0;
const unsigned long SCREEN_TIMEOUT_MS = 30000;
bool screenOff = false;

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
float smoothBPM = 0; // Khôi phục Biến lọc nhiễu nhịp tim (EMA Filter)

byte currentLEDPower = 0x1F;
int measurementProgress = 0;
bool fingerPresent = false;
unsigned long measureCompleteTime = 0;
unsigned long lastContinuousUpdateTime = 0;
unsigned long lastAliveTime = 0; // Độc lập theo dõi sự sống để tránh còi hú bậy
unsigned long pulseSearchStart = 0;
unsigned long cancelCooldownUntil =
    0; // Cooldown sau huỷ cảnh báo, ngăn tái kích hoạt đo

int validSamplesCollected = 0;
int ignoredSamples = 0;
const int SAMPLES_TO_IGNORE = 2;
const int TARGET_SAMPLES = 5;

int lastBPM = 0;
int lastSpO2 = 0;

const int FILTER_SIZE = 9;
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
      // FIX DELAY: Giảm mức chặn nhiễu từ 50ms xuống 20ms để bắt được chạm lướt
      // siêu nhanh
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
void drawFallCountdownScreen();
void drawSleepScreen();
void drawStatusBar();
void TaskFirebase(void *pvParameters);
void handleTouchToggle();

String removeAccents(const String &str) {
  String s = str;
  s.replace("á", "a");
  s.replace("à", "a");
  s.replace("ả", "a");
  s.replace("ã", "a");
  s.replace("ạ", "a");
  s.replace("ă", "a");
  s.replace("ắ", "a");
  s.replace("ằ", "a");
  s.replace("ẳ", "a");
  s.replace("ẵ", "a");
  s.replace("ặ", "a");
  s.replace("â", "a");
  s.replace("ấ", "a");
  s.replace("ầ", "a");
  s.replace("ẩ", "a");
  s.replace("ẫ", "a");
  s.replace("ậ", "a");
  s.replace("đ", "d");
  s.replace("Đ", "D");
  s.replace("é", "e");
  s.replace("è", "e");
  s.replace("ẻ", "e");
  s.replace("ẽ", "e");
  s.replace("ẹ", "e");
  s.replace("ê", "e");
  s.replace("ế", "e");
  s.replace("ề", "e");
  s.replace("ể", "e");
  s.replace("ễ", "e");
  s.replace("ệ", "e");
  s.replace("í", "i");
  s.replace("ì", "i");
  s.replace("ỉ", "i");
  s.replace("ĩ", "i");
  s.replace("ị", "i");
  s.replace("ó", "o");
  s.replace("ò", "o");
  s.replace("ỏ", "o");
  s.replace("õ", "o");
  s.replace("ọ", "o");
  s.replace("ô", "o");
  s.replace("ố", "o");
  s.replace("ồ", "o");
  s.replace("ổ", "o");
  s.replace("ỗ", "o");
  s.replace("ộ", "o");
  s.replace("ơ", "o");
  s.replace("ớ", "o");
  s.replace("ờ", "o");
  s.replace("ở", "o");
  s.replace("ỡ", "o");
  s.replace("ợ", "o");
  s.replace("ú", "u");
  s.replace("ù", "u");
  s.replace("ủ", "u");
  s.replace("ũ", "u");
  s.replace("ụ", "u");
  s.replace("ư", "u");
  s.replace("ứ", "u");
  s.replace("ừ", "u");
  s.replace("ử", "u");
  s.replace("ữ", "u");
  s.replace("ự", "u");
  s.replace("ý", "y");
  s.replace("ỳ", "y");
  s.replace("ỷ", "y");
  s.replace("ỹ", "y");
  s.replace("ỵ", "y");
  s.replace("_", " ");
  return s;
}

void sortArray(int *arr, int size) {
  for (int i = 0; i < size - 1; i++) {
    for (int j = i + 1; j < size; j++) {
      if (arr[j] < arr[i]) {
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
      }
    }
  }
}
int getMedian(int *data, int size) {
  if (size <= 0)
    return 0;
  int temp[FILTER_SIZE];
  for (int i = 0; i < size; i++)
    temp[i] = data[i];
  sortArray(temp, size);
  return (size % 2 == 1) ? temp[size / 2]
                         : (temp[size / 2 - 1] + temp[size / 2]) / 2;
}
void addBPMValue(int value) {
  bpmHistory[bpmIndex] = value;
  bpmIndex = (bpmIndex + 1) % FILTER_SIZE;
  if (bpmCount < FILTER_SIZE)
    bpmCount++;
}
void addSpO2Value(int value) {
  spo2History[spo2Index] = value;
  spo2Index = (spo2Index + 1) % FILTER_SIZE;
  if (spo2Count < FILTER_SIZE)
    spo2Count++;
}
int getFilteredBPM() { return getMedian(bpmHistory, bpmCount); }
int getFilteredSpO2() { return getMedian(spo2History, spo2Count); }

bool initMAX30102() {
  // Đổi tốc độ I2C về I2C_SPEED_STANDARD (100kHz) do dùng chung Bus với
  // MLX90614 (chỉ hỗ trợ tối đa 100kHz)
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD))
    return false;

  // Cấu hình thông số đã tinh chỉnh cho dự án:
  // sampleAverage=8 + sampleRate=200 -> Effective Rate = 200/8 = 25 Hz
  // sampleAverage=8 loại bỏ hoàn toàn nhiễu ánh sáng 50Hz/60Hz nhờ trung bình 8
  // mẫu phần cứng
  byte ledBrightness = 60; // Options: 0=Off to 255=50mA
  byte sampleAverage =
      8; // Options: 1, 2, 4, 8, 16, 32 (8 = lọc nhiễu phần cứng tối ưu)
  byte ledMode = 2; // Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 200; // Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411;  // Options: 69, 118, 215, 411
  int adcRange = 4096;   // Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate,
                       pulseWidth, adcRange);

  currentLEDPower = 0x1F;
  particleSensor.setPulseAmplitudeRed(currentLEDPower);
  particleSensor.setPulseAmplitudeIR(currentLEDPower);
  particleSensor.setPulseAmplitudeGreen(0);
  return true;
}

void updateMAX30102Fast() {
  if (!max30102Found)
    return;

  for (byte i = 25; i < 100; i++) {
    redBuffer[i - 25] = redBuffer[i];
    irBuffer[i - 25] = irBuffer[i];
  }
  for (byte i = 75; i < 100; i++) {
    long start = millis();
    while (!particleSensor.available() && millis() - start < 100) {
      particleSensor.check();
      handleTouchToggle(); // FIX DELAY: Cho phép quét nút bấm ngay khi đang chờ
                           // MAX30102
      delay(1);
    }
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }

  if (irBuffer[99] < 50000) {
    if (fingerPresent) {
      fingerPresent = false;
      measurementProgress = 0;
      bpmCount = 0;
      spo2Count = 0;
      bpmIndex = 0;
      spo2Index = 0;
      validSamplesCollected = 0;
      ignoredSamples = 0;
      lastContinuousUpdateTime = 0;
      lastAliveTime = 0;
      pulseSearchStart = 0;
      lastScreen = -1;

      // Reset biến lọc
      smoothBPM = 0;

      if (currentLEDPower != 0x1F) {
        currentLEDPower = 0x1F;
        particleSensor.setPulseAmplitudeRed(currentLEDPower);
        particleSensor.setPulseAmplitudeIR(currentLEDPower);
      }
    }
    return;
  }

  // COOLDOWN: Sau khi huỷ cảnh báo, chặn tái kích hoạt đo trong 3 giây
  // để người dùng có thời gian rút tay/giấy ra khỏi cảm biến
  if (!fingerPresent && cancelCooldownUntil > 0 &&
      millis() < cancelCooldownUntil) {
    return; // Bỏ qua, chưa hết cooldown
  }
  cancelCooldownUntil = 0; // Hết cooldown, cho phép đo lại bình thường

  if (!fingerPresent) {
    fingerPresent = true;
    bpmCount = 0;
    spo2Count = 0;
    bpmIndex = 0;
    spo2Index = 0;
    validSamplesCollected = 0;
    ignoredSamples = 0;
    pulseSearchStart = millis();

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

  maxim_heart_rate_and_oxygen_saturation(irBuffer, 100, redBuffer, &spo2,
                                         &validSPO2, &heartRateValue,
                                         &validHeartRate);

  // Khôi phục: Nếu tay vẫn đang nằm trên cảm biến và máy có chạy phép tính, vẫn
  // gia hạn thời gian sống
  if (fingerPresent && heartRateValue > 0 && spo2 > 0) {
    lastAliveTime = millis();
  }

  // Kiểm duyệt nghiêm ngặt theo chuẩn nhà sản xuất Maxim — KHÔNG BAO GIỜ nới
  // lỏng để chống rác nhảy loạn (ví dụ 109)
  bool isValid = (validHeartRate && validSPO2 && spo2 >= 85 && spo2 <= 100);

  if (isValid) {
    int currentBPM = heartRateValue;

    // Khôi phục: Thuật toán phát hiện sóng đôi (Dicrotic Notch) thông minh
    if (smoothBPM > 0) {
      // Nới lỏng rào cản nhận diện sóng dội từ 1.6x xuống 1.35x.
      // Do đôi khi giá trị đo mượt đầu tiên bị tính nhỉnh hơn thực tế (vd 75
      // thay vì 58). Gấp đôi 58 là 116. 116 / 75 = 1.54. Nếu để 1.6x như cũ
      // thuật toán sẽ để lọt con số 116!
      if (currentBPM > (smoothBPM * 1.35) && currentBPM < (smoothBPM * 2.6)) {
        currentBPM = currentBPM / 2;
      }
    } else {
      // Lúc mới bắt đầu đo (đang nghỉ ngơi), nhịp >= 90 đa phần là sóng đôi dội
      // do thành mạch dẻo dai (người trẻ)
      if (currentBPM >= 85) {
        currentBPM = currentBPM / 2;
      }
    }

    // Khôi phục: Dùng bộ lọc Smooth (EMA filter) để chặn các cú nhảy loạn
    if (smoothBPM == 0) {
      smoothBPM = currentBPM;
    } else {
      smoothBPM =
          (smoothBPM * 0.8) + (currentBPM * 0.2); // Tin cậy 80% mượt, 20% mới
    }
    currentBPM = (int)smoothBPM;

    // Chỉ chấp nhận nhịp tim trong khoảng sinh lý hợp lệ (40-140 BPM)
    if (currentBPM >= 40 && currentBPM <= 140) {
      if (ignoredSamples < SAMPLES_TO_IGNORE) {
        ignoredSamples++;
      } else {
        if (validSamplesCollected < TARGET_SAMPLES) {
          addBPMValue(currentBPM);
          addSpO2Value(spo2);
          validSamplesCollected++;
          measurementProgress =
              10 + (validSamplesCollected * 90 / TARGET_SAMPLES);

          if (validSamplesCollected >= TARGET_SAMPLES) {
            measurementProgress = 100;
            measureCompleteTime = millis();
            lastContinuousUpdateTime = millis();
            lastMeasureTimeStr = currentTimeStr;

            lastBPM = getFilteredBPM();
            lastSpO2 = getFilteredSpO2();
            needGeminiFetch = true;
          }
        } else if (measurementProgress == 100) {
          addBPMValue(currentBPM);
          addSpO2Value(spo2);

          if (millis() - lastContinuousUpdateTime >= 2000) {
            lastBPM = getFilteredBPM();
            lastSpO2 = getFilteredSpO2();
            if (dataMutex != NULL)
              xSemaphoreTake(dataMutex, portMAX_DELAY);
            lastMeasureTimeStr = currentTimeStr;
            if (dataMutex != NULL)
              xSemaphoreGive(dataMutex);
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

  float voltage = voMeasured * (4.95 / 4095.0);
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
    if (h >= 24) {
      h -= 24;
      d += 1;
      if (d > 31) {
        d = 1;
        m += 1;
      }
      if (m > 12) {
        m = 1;
        y += 1;
      }
    }
    char timeBuf[25];
    sprintf(timeBuf, "%02d:%02d:%02d %02d/%02d/%04d", h, gps.time.minute(),
            gps.time.second(), d, m, y);
    if (dataMutex != NULL)
      xSemaphoreTake(dataMutex, portMAX_DELAY);
    currentTimeStr = String(timeBuf);
    if (dataMutex != NULL)
      xSemaphoreGive(dataMutex);
  }
}

void handleTouchToggle() {
  if (shortPressTriggered) {
    lastActivityTime = millis();

    if (screenOff) {
      screenOff = false;
      lastScreen = -1;
      updateTFT();
      shortPressTriggered = false;
      return;
    }

    if (fallWarning) {
      fallWarning = false;
      fallWarningStart = 0;
      lastScreen = -1;
      updateTFT();
      shortPressTriggered = false;
      return;
    }

    userScreenIndex = (userScreenIndex + 1) % 4;
    lastScreen = -1;
    updateTFT();
    shortPressTriggered = false;
  }

  static bool longPressHandled = false;
  if (isPressed) {
    lastActivityTime = millis();
    if (!longPressHandled && (millis() - touchStartTime >= 600)) {
      if (screenOff) {
        screenOff = false;
        lastScreen = -1;
        updateTFT();
        longPressHandled = true;
        return;
      }
      if (isFalling || isSOS || isHealthAlert) {
        isFalling = false;
        isSOS = false;
        isHealthAlert = false;
        fallWarning = false;
        healthAlertReason = "";
        lastBPM = 0;
        lastSpO2 = 0;

        // RESET TOÀN BỘ TRẠNG THÁI ĐO ĐẠC - Ngăn thuật toán SpO2 tiếp tục chạy
        // trên dữ liệu rác gây crash
        fingerPresent = false;
        measurementProgress = 0;
        pulseSearchStart = 0;
        lastContinuousUpdateTime = 0;
        lastAliveTime = 0;
        smoothBPM = 0;
        validSamplesCollected = 0;
        ignoredSamples = 0;
        cancelCooldownUntil =
            millis() + 3000; // Cooldown 3 giây trước khi cho phép đo lại
        bpmCount = 0;
        spo2Count = 0;
        bpmIndex = 0;
        spo2Index = 0;
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

  // 1. Phối hợp: Bất tỉnh / Ngất xỉu
  if (isFalling && lastBPM > 0 && lastBPM < 50) {
    danger = true;
    currentReason = "Ngat xiu sau nga!";
  }
  // 2. Phối hợp: Sốt cao li bì
  else if (currentTempObj > 38.5 && lastBPM > 110) {
    danger = true;
    currentReason = "Sot cao li bi!";
  }
  // 3. Phối hợp: Môi trường độc hại gây suy hô hấp
  else if (currentDust > 150.0 && lastSpO2 > 0 && lastSpO2 < 92) {
    danger = true;
    currentReason = "Suy ho hap do o nhiem!";
  }
  // Các cảnh báo đơn lẻ ban đầu
  else {
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
  }

  if (danger) {
    if (healthDangerTimer == 0)
      healthDangerTimer = millis();
    if (millis() - healthDangerTimer > 3000) {
      isHealthAlert = true;
      if (dataMutex != NULL)
        xSemaphoreTake(dataMutex, portMAX_DELAY);
      healthAlertReason = currentReason;
      if (dataMutex != NULL)
        xSemaphoreGive(dataMutex);
    }
  } else {
    healthDangerTimer = 0;
    bool shouldClear = true;
    if (dataMutex != NULL)
      xSemaphoreTake(dataMutex, portMAX_DELAY);
    if (healthAlertReason == "KHONG TIM THAY MACH!" ||
        healthAlertReason == "MAT MACH KHI DO LIEN TUC!") {
      shouldClear = false;
    }
    if (dataMutex != NULL)
      xSemaphoreGive(dataMutex);

    if (shouldClear) {
      isHealthAlert = false;
      if (dataMutex != NULL)
        xSemaphoreTake(dataMutex, portMAX_DELAY);
      healthAlertReason = "";
      if (dataMutex != NULL)
        xSemaphoreGive(dataMutex);
    }
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
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(20, 120);
  tft.println("DANG KHOI DONG...");
}

// ===================== TFT STATUS BAR (y=0~20) =====================
void drawStatusBar() {
  tft.fillRect(0, 0, 240, 20, ST77XX_BLACK);
  tft.setTextSize(1);
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(4, 6);
    tft.print("WiFi OK");
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(4, 6);
    tft.print("NO WiFi");
  }
  tft.setTextColor(ST77XX_YELLOW);
  if (dataMutex != NULL)
    xSemaphoreTake(dataMutex, portMAX_DELAY);
  String timeOnly = currentTimeStr;
  if (dataMutex != NULL)
    xSemaphoreGive(dataMutex);
  if (timeOnly.length() >= 8)
    timeOnly = timeOnly.substring(0, 8);
  int timeWidth = timeOnly.length() * 6;
  tft.setCursor(240 - timeWidth - 4, 6);
  tft.print(timeOnly);
  tft.drawLine(0, 19, 240, 19, ST77XX_GRAY);
}

// ===================== MAN HINH DEM NGUOC TE NGA =====================
void drawFallCountdownScreen() {
  tft.fillRect(0, 20, 240, 260, ST77XX_BLACK);
  drawStatusBar();
  unsigned long elapsed = millis() - fallWarningStart;
  int remaining = (int)((FALL_COUNTDOWN_MS - elapsed) / 1000);
  if (remaining < 0)
    remaining = 0;

  uint16_t borderColor =
      ((millis() / 300) % 2 == 0) ? ST77XX_RED : ST77XX_ORANGE;
  tft.drawRoundRect(5, 25, 230, 250, 10, borderColor);
  tft.drawRoundRect(6, 26, 228, 248, 10, borderColor);

  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2);
  tft.setCursor(35, 40);
  tft.print("!! CANH BAO !!");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(30, 70);
  tft.print("Phat hien te nga! Cham de");
  tft.setCursor(30, 85);
  tft.print("HUY trong vong:");

  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(7);
  char buf[4];
  sprintf(buf, "%02d", remaining);
  tft.setCursor(75, 110);
  tft.print(buf);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(90, 175);
  tft.print("GIAY");

  int barWidth = (int)(200.0 * elapsed / FALL_COUNTDOWN_MS);
  if (barWidth > 200)
    barWidth = 200;
  tft.drawRect(19, 210, 202, 20, ST77XX_WHITE);
  tft.fillRect(20, 211, barWidth, 18, ST77XX_RED);

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(30, 245);
  tft.print(">> CHAM de HUY BAO DONG <<");
}

// ===================== MAN HINH NGU (BLACKOUT) =====================
void drawSleepScreen() {
  tft.fillScreen(ST77XX_BLACK);
  uint16_t dimColor = tft.color565(15, 15, 15);
  int cx = 120, cy = 140, armW = 12, armH = 40;
  tft.fillRect(cx - armW / 2, cy - armH, armW, armH * 2, dimColor);
  tft.fillRect(cx - armH, cy - armW / 2, armH * 2, armW, dimColor);
  tft.setTextColor(tft.color565(30, 30, 30));
  tft.setTextSize(1);
  tft.setCursor(55, 200);
  tft.print("Cham de danh thuc man hinh");
}

void drawStaticUI() {
  tft.fillScreen(ST77XX_BLACK);
  drawStatusBar();

  tft.fillRoundRect(0, 20, 240, 35, 0, ST77XX_DARKGREEN);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(35, 28);
  tft.println("TRAM Y TE MINI");

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_ORANGE);
  tft.setCursor(10, 65);
  tft.print("Nhip tim :");
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(10, 105);
  tft.print("Oxy mau  :");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 145);
  tft.print("Nhiet do co the:");
  tft.setCursor(10, 170);
  tft.print("Nhiet do moi tr:");
  tft.setCursor(10, 200);
  tft.print("Nong do bui min:");

  tft.drawLine(10, 225, 230, 225, ST77XX_GRAY);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 237);
  tft.print("Trang thai:");
  tft.setCursor(10, 257);
  tft.print("Thoi gian :");
}

void drawMeasuringUI() {
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRoundRect(10, 10, 220, 260, 10, ST77XX_CYAN);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(20, 30);
  tft.print("DANG DO TIM MACH");
  tft.drawLine(20, 60, 220, 60, ST77XX_CYAN);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(25, 240);
  tft.print("* Vui long dat im ngon tay *");
}

void drawWeatherScreenStatic() {
  tft.fillScreen(ST77XX_BLACK);

  tft.fillRoundRect(0, 0, 240, 35, 0, ST77XX_DARKBLUE);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(18, 10);
  tft.print("THOI TIET TAI CHO");

  if (dataMutex != NULL)
    xSemaphoreTake(dataMutex, portMAX_DELAY);
  bool valid = weatherValid;
  if (!valid) {
    if (dataMutex != NULL)
      xSemaphoreGive(dataMutex);
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(1);
    tft.setCursor(45, 120);
    tft.print("Dang cap nhat du lieu...");
    return;
  }

  double curTemp = (double)weatherData["list"][0]["main"]["temp"];
  int curHumi = (int)weatherData["list"][0]["main"]["humidity"];
  String curDesc = removeAccents(
      (const char *)weatherData["list"][0]["weather"][0]["description"]);
  double nextTemp = (double)weatherData["list"][1]["main"]["temp"];
  String nextDesc = removeAccents(
      (const char *)weatherData["list"][1]["weather"][1]["description"]);
  if (dataMutex != NULL)
    xSemaphoreGive(dataMutex);

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
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(15, 12);
  tft.print("MA KET NOI (APP)");

  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  qrcode_initText(&qrcode, qrcodeData, 3, 0, deviceID.c_str());

  int scale = 6;
  int xOffset = (240 - qrcode.size * scale) / 2;
  int yOffset = (280 - qrcode.size * scale) / 2 + 15;

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        tft.fillRect(xOffset + x * scale, yOffset + y * scale, scale, scale,
                     ST77XX_BLACK);
      }
    }
  }

  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setCursor((240 - (10 * 6 + deviceID.length() * 6)) / 2, 260);
  tft.print("DeviceID: ");
  tft.print(deviceID);
}

void drawWeatherAnimationFrame() {
  int frameX = 145;
  int frameY = 50;
  tft.drawBitmap(frameX, frameY, frames[currentFrame], FRAME_WIDTH,
                 FRAME_HEIGHT, ST77XX_WHITE);
  currentFrame = (currentFrame + 1) % TOTAL_FRAMES;
}

void drawAIDoctorScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.fillRoundRect(0, 0, 240, 35, 0, ST77XX_MAGENTA);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(18, 10);
  tft.print("BAC SI CLOUD AI");

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(10, 50);
  tft.print(">> Loi khuyen tu Gemini:");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);

  int cursorY = 80;
  if (dataMutex != NULL)
    xSemaphoreTake(dataMutex, portMAX_DELAY);
  String copyStr = geminiAdvice;
  if (dataMutex != NULL)
    xSemaphoreGive(dataMutex);
  while (copyStr.length() > 0) {
    if (copyStr.length() <= 16) {
      tft.setCursor(10, cursorY);
      tft.print(copyStr);
      break;
    } else {
      int splitIndex = copyStr.lastIndexOf(' ', 16);
      if (splitIndex == -1)
        splitIndex = 16;
      tft.setCursor(10, cursorY);
      tft.print(copyStr.substring(0, splitIndex));
      copyStr = copyStr.substring(splitIndex + 1);
      if (copyStr.startsWith(" "))
        copyStr = copyStr.substring(1);
      cursorY += 25;
      if (cursorY > 240)
        break;
    }
  }
}

void updateTFT() {
  // UU TIEN 1: Dem nguoc te nga
  if (fallWarning) {
    drawFallCountdownScreen();
    if (millis() - fallWarningStart >= FALL_COUNTDOWN_MS) {
      fallWarning = false;
      fallWarningStart = 0;
      isFalling = true;
      lastScreen = -1;
    }
    return;
  }

  // UU TIEN 2: Man hinh ngu
  if (screenOff)
    return;

  if (isHealthAlert || isSOS || isFalling) {
    currentScreen = 0;
    userScreenIndex = 0;
  } else if (measurementProgress > 0) {
    if (measurementProgress < 100) {
      currentScreen = 1;
    } else {
      if (millis() - measureCompleteTime < 1500) {
        currentScreen = 1;
      } else {
        currentScreen =
            (userScreenIndex == 0)
                ? 0
                : ((userScreenIndex == 1) ? 2
                                          : ((userScreenIndex == 2) ? 3 : 4));
      }
    }
  } else {
    currentScreen =
        (userScreenIndex == 0)
            ? 0
            : ((userScreenIndex == 1) ? 2 : ((userScreenIndex == 2) ? 3 : 4));
  }

  if (currentScreen != lastScreen) {
    if (currentScreen == 0)
      drawStaticUI();
    else if (currentScreen == 1)
      drawMeasuringUI();
    else if (currentScreen == 2)
      drawWeatherScreenStatic();
    else if (currentScreen == 3)
      drawQRScreen();
    else if (currentScreen == 4)
      drawAIDoctorScreen();
    lastScreen = currentScreen;
  }

  if (currentScreen == 0) {
    drawStatusBar();
    tft.setTextSize(3);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);

    tft.setCursor(140, 60);
    (lastBPM > 0) ? tft.printf("%-4d", lastBPM) : tft.print("--  ");

    tft.setCursor(140, 100);
    if (lastSpO2 > 0) {
      tft.printf("%d%%  ", lastSpO2);
    } else {
      tft.print("--  ");
    }

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(130, 140);
    tft.printf("%-5.1fC", currentTempObj);
    tft.setCursor(130, 165);
    tft.printf("%-5.1fC", currentTempAmb);

    tft.setCursor(130, 195);
    tft.printf("%-4.0f", currentDust);
    tft.setTextSize(1);
    tft.print(" ug/m3  ");

    tft.setTextSize(1);
    tft.setCursor(85, 237);

    if (isSOS) {
      tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
      tft.print("BAO DONG KHOAN CAP! ");
      if (dataMutex != NULL)
        xSemaphoreTake(dataMutex, portMAX_DELAY);
      currentStatus = "SOS KHOAN CAP!";
      if (dataMutex != NULL)
        xSemaphoreGive(dataMutex);
    } else if (isFalling) {
      tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
      tft.print("PHAT HIEN TE NGA!   ");
      if (dataMutex != NULL)
        xSemaphoreTake(dataMutex, portMAX_DELAY);
      currentStatus = "PHAT HIEN TE NGA!";
      if (dataMutex != NULL)
        xSemaphoreGive(dataMutex);
    } else if (isHealthAlert) {
      tft.setTextColor(ST77XX_ORANGE, ST77XX_BLACK);

      String displayReason = healthAlertReason;
      if (displayReason.indexOf(".") > 0) {
        displayReason = displayReason.substring(0, displayReason.indexOf("."));
      }

      while (displayReason.length() < 20) {
        displayReason += " ";
      }

      tft.print(displayReason);
      if (dataMutex != NULL)
        xSemaphoreTake(dataMutex, portMAX_DELAY);
      currentStatus = healthAlertReason;
      if (dataMutex != NULL)
        xSemaphoreGive(dataMutex);
    } else {
      tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
      tft.print("BINH THUONG         ");
      if (dataMutex != NULL)
        xSemaphoreTake(dataMutex, portMAX_DELAY);
      currentStatus = "BINH THUONG";
      if (dataMutex != NULL)
        xSemaphoreGive(dataMutex);
    }

    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(85, 257);
    tft.print(currentTimeStr);
  } else if (currentScreen == 1) {
    tft.setTextSize(2);
    tft.setCursor(30, 90);
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    if (measurementProgress < 30)
      tft.print("Dang lay mau...   ");
    else if (measurementProgress < 60)
      tft.print("Dang loc nhieu... ");
    else if (measurementProgress < 90)
      tft.print("Phan tich du lieu.");
    else
      tft.print("Da hoan tat!      ");

    tft.drawRect(20, 140, 200, 25, ST77XX_WHITE);
    tft.fillRect(22, 142, (measurementProgress * 196) / 100, 21, ST77XX_GREEN);
    tft.setCursor(100, 180);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.printf("%-3d%%", measurementProgress);
  } else if (currentScreen == 2) {
    if (millis() - lastAnimFrame >= FRAME_DELAY) {
      lastAnimFrame = millis();
      drawWeatherAnimationFrame();
    }
  }
}

const char htmlPage[] PROGMEM =
    R"rawliteral(<!DOCTYPE html><html lang="vi"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>He Thong Giam Sat Y Te</title><style>body { font-family: sans-serif; text-align: center; background: #f4f4f4; padding: 20px; } button { padding: 10px 20px; background: #007bff; color: #fff; border-radius: 5px; border:none; }</style></head><body><h2>HE THONG GIAM SAT Y TE TRAM KHOANG CACH</h2><input type="text" id="oledText" placeholder="Nhap thong bao cho benh nhan"><button onclick="sendText()">Gui Len Man Hinh</button><script>function sendText() { fetch("/settext?msg=" + document.getElementById("oledText").value).then(() => alert("Da gui thanh cong!")); }</script></body></html>)rawliteral";

void handleRoot() { server.send_P(200, "text/html", htmlPage); }

void handleData() {
  String out =
      weatherValid
          ? "{\"temp\":" +
                String((double)weatherData["list"][0]["main"]["temp"], 1) + "}"
          : "{\"temp\":0}";
  server.send(200, "application/json", out);
}

// ===================== TASK FIREBASE & THỜI TIẾT (Lõi 0) =====================
unsigned long lastWeatherFetch = 0;
const unsigned long weatherFetchInterval = 300000; // 5 phút

void TaskFirebase(void *pvParameters) {
  while (true) {
    if (WiFi.status() == WL_CONNECTED &&
        (millis() - lastWeatherFetch >= weatherFetchInterval ||
         lastWeatherFetch == 0)) {
      HTTPClient http;
      http.begin(weatherURL);
      int httpResponseCode = http.GET();
      if (httpResponseCode > 0) {
        String payload = http.getString();
        JSONVar tempWeather = JSON.parse(payload);
        if (JSON.typeof(tempWeather) != "undefined" &&
            tempWeather.hasOwnProperty("list")) {
          if (dataMutex != NULL)
            xSemaphoreTake(dataMutex, portMAX_DELAY);
          weatherData = tempWeather;
          weatherValid = true;
          if (dataMutex != NULL)
            xSemaphoreGive(dataMutex);
        }
      }
      http.end();
      lastWeatherFetch = millis();
      needWeatherRedraw = true; // Yeu cau Core 1 cap nhat TFT (TRÁNH GỌI TFT TỪ
                                // CORE 0 DE CHONG CRASH SPI)
    }

    if (WiFi.status() == WL_CONNECTED && needGeminiFetch) {
      needGeminiFetch = false;
      if (millis() - lastGeminiFetch >= 120000 || lastGeminiFetch == 0) {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.setTimeout(20000); // Tăng thời gian chờ AI phản hồi lên 20 giây
        http.begin(client,
                   "https://generativelanguage.googleapis.com/v1beta/models/"
                   "gemini-2.5-flash-lite:generateContent?key=" +
                       String(GEMINI_API_KEY));
        http.addHeader("Content-Type", "application/json");

        char promptBuffer[1024];
        snprintf(
            promptBuffer, sizeof(promptBuffer),
            "BAN LA TRO LY Y TE AI 'HEALTHY 365'.\n"
            "QUY TAC PHAN TICH:\n"
            "1. NHIP TIM: 60-100 OK. >100 nhanh. <50 hoac >120 la bat thuong.\n"
            "2. SpO2: 95-100 OK. <94 can theo doi. <90 la NGUY HIEM.\n"
            "3. NHIET DO DA: 31-35 OK. >37.5 la Sot. <30 la Lanh.\n"
            "4. PM2.5: <50 an toan. >100 canh bao. >200 nguy hai.\n"
            "KHAN CAP: Neu SpO2 < 90 hoac phat hien Te Nga/SOS: 'CANH BAO KHAN "
            "CAP: NGUY HIEM TINH MANG'.\n"
            "PHONG CACH: Di thang vao phan tich y chinh. Khong chao hoi. Tra "
            "ve TIENG VIET KHONG DAU.\n"
            "DU LIEU: Nhiet do: %.1fC, Nhip tim: %d bpm, SpO2: %d%%, Bui: "
            "%.1f, SOS: %s, Te nga: %s.\n"
            "YEU CAU: Tra ve toi da 15-20 tu. TIENG VIET KHONG DAU. Ket thuc "
            "bang cau: *Luu y: Chi xem de tham khao y te.*",
            currentTempObj, lastBPM, lastSpO2, currentDust,
            (isSOS ? "CO" : "KHONG"), (isFalling ? "CO" : "KHONG"));

        String requestBody = "{\"contents\":[{\"parts\":[{\"text\":\"";
        requestBody += promptBuffer;
        requestBody += "\"}]}]}";

        int httpResponseCode = http.POST(requestBody);
        if (httpResponseCode > 0) {
          String response = http.getString();
          // Linh hoạt hơn khi tìm vị trí mảng chứa text do JSON có lúc chứa
          // space có lúc không
          // Tìm trường "text" linh hoạt: hỗ trợ cả "text":" và "text" : "
          int textIndex = response.indexOf("\"text\"");
          if (dataMutex != NULL)
            xSemaphoreTake(dataMutex, portMAX_DELAY);
          if (textIndex > 0) {
            // Tìm dấu " mở đầu nội dung text (bỏ qua dấu : và khoảng trắng)
            int colonPos = response.indexOf(':', textIndex + 5);
            int start = response.indexOf('"', colonPos) + 1;
            // Tìm dấu " kết thúc, bỏ qua các \" bên trong chuỗi
            int end = start;
            while (end < (int)response.length()) {
              end = response.indexOf('"', end);
              if (end < 0)
                break;
              // Kiểm tra xem dấu " này có bị escape bởi \ không
              if (end > 0 && response.charAt(end - 1) == '\\') {
                end++; // Bỏ qua \" và tìm tiếp
                continue;
              }
              break;
            }
            if (start > 0 && end > start) {
              String rawAdvice = response.substring(start, end);
              rawAdvice.replace("\\n", " ");
              rawAdvice.replace("\\r", "");
              geminiAdvice = removeAccents(rawAdvice);
            } else {
              geminiAdvice = "AI tra loi nhung dinh dang khong doc duoc.";
            }
          } else {
            geminiAdvice = "AI khong tra ve noi dung. (HTTP " +
                           String(httpResponseCode) + ")";
          }
          if (dataMutex != NULL)
            xSemaphoreGive(dataMutex);
        } else {
          if (dataMutex != NULL)
            xSemaphoreTake(dataMutex, portMAX_DELAY);
          geminiAdvice =
              "AI phan hoi (Timeout / Het quyen): " + String(httpResponseCode);
          if (dataMutex != NULL)
            xSemaphoreGive(dataMutex);
        }
        http.end();
        lastGeminiFetch = millis() == 0 ? 1 : millis();
        needAIRedraw =
            true; // Yeu cau Core 1 cap nhat TFT de ngan ngua Crash bus SPI
      }
    }

    if (Firebase.ready() && signupOK && WiFi.status() == WL_CONNECTED) {
      FirebaseJson json;

      String trangThaiDo = "Cho do";
      bool isEmergencyFlatline = false;
      if (fingerPresent) {
        if (measurementProgress < 100) {
          trangThaiDo = "Dang do...";
          // Timeout phát hiện mạch: 45 giây
          if (pulseSearchStart > 0 && (millis() - pulseSearchStart > 45000)) {
            isEmergencyFlatline = true;
            isHealthAlert = true;
            if (dataMutex != NULL)
              xSemaphoreTake(dataMutex, portMAX_DELAY);
            healthAlertReason = "KHONG TIM THAY MACH!";
            if (dataMutex != NULL)
              xSemaphoreGive(dataMutex);
            lastBPM = 0;
            lastSpO2 = 0;
            measurementProgress = 0; // Bẻ gãy thanh tiến trình đo đạc
            pulseSearchStart = 0;    // Ngăn chặn timeout gọi liên tục
          }
        } else {
          trangThaiDo = "Do lien tuc";
          // Cảnh báo MẤT MẠCH khi đo liên tục: 45 giây không tìm thấy nhịp
          if (lastAliveTime > 0 && (millis() - lastAliveTime > 45000)) {
            isEmergencyFlatline = true;
            isHealthAlert = true;
            if (dataMutex != NULL)
              xSemaphoreTake(dataMutex, portMAX_DELAY);
            healthAlertReason = "MAT MACH KHI DO LIEN TUC!";
            if (dataMutex != NULL)
              xSemaphoreGive(dataMutex);
            lastBPM = 0;
            lastSpO2 = 0;
            measurementProgress = 0; // Đẩy người dùng ra khỏi màn hình đo
            pulseSearchStart = 0;
            lastContinuousUpdateTime = 0;
          }
        }
      }

      String realtimePath = "Devices/" + deviceID + "/";

      // Chỉ cập nhật BPM/SpO2 khi đo xong 100% (và giá trị > 0) hoặc có biến cố
      // khẩn cấp
      if ((measurementProgress == 100 && lastBPM > 0) || isEmergencyFlatline) {
        json.set("BPM", lastBPM);
        json.set("SpO2", lastSpO2);
      }

      json.set("TempObj", currentTempObj);
      json.set("TempAmb", currentTempAmb);
      json.set("AngleX", currentMpuX);
      json.set("AngleY", currentMpuY);
      json.set("AngleZ", currentMpuZ);
      json.set("Dust", currentDust);
      json.set("Alert_SOS", isSOS);
      json.set("Alert_Fall", isFalling);
      json.set("Alert_Health", isHealthAlert);

      if (dataMutex != NULL)
        xSemaphoreTake(dataMutex, portMAX_DELAY);
      json.set("Alert_Reason", healthAlertReason);
      json.set("LastMeasureTime", lastMeasureTimeStr);
      json.set("TrangThai", currentStatus);
      json.set("ThoiGian", currentTimeStr);
      if (dataMutex != NULL)
        xSemaphoreGive(dataMutex);

      json.set("TrangThaiDo", trangThaiDo);

      if (gps.location.isValid()) {
        json.set("GPS_Lat", gps.location.lat());
        json.set("GPS_Lng", gps.location.lng());
      }

      // Dùng updateNode (PATCH) thay vì setJSON để giữ lại các trường BPM/SpO2
      // cũ khi không đo
      Firebase.RTDB.updateNode(&fbdo, realtimePath, &json);

      // ===== TINH NANG #4: Doc lenh huy canh bao tu App =====
      if (isSOS || isFalling || isHealthAlert) {
        String cmdPath = "Devices/" + deviceID + "/Cmd_CancelAlert";
        if (Firebase.RTDB.getBool(&fbdo, cmdPath)) {
          if (fbdo.boolData() == true) {
            isSOS = false;
            isFalling = false;
            isHealthAlert = false;
            fallWarning = false;
            healthAlertReason = "";
            Firebase.RTDB.setBool(&fbdo, cmdPath, false);
            lastScreen = -1;

            // HỦY TOÀN BỘ TRẠNG THÁI ĐO ĐẠC - NGĂN THUẬT TOÁN SpO2 CHẠY TRÊN DỮ
            // LIỆU RÁC GÂY CRASH
            fingerPresent = false;
            measurementProgress = 0;
            pulseSearchStart = 0;
            lastContinuousUpdateTime = 0;
            lastAliveTime = 0;
            smoothBPM = 0;
            validSamplesCollected = 0;
            ignoredSamples = 0;
            cancelCooldownUntil =
                millis() + 3000; // Cooldown 3 giây trước khi cho phép đo lại
            bpmCount = 0;
            spo2Count = 0;
            bpmIndex = 0;
            spo2Index = 0;
          }
        }
      }

      bool shouldSaveHistory = false;

      if (millis() - lastHistorySave >= historyInterval || lastHistorySave == 0)
        shouldSaveHistory = true;
      if (isSOS && !lastSOSState)
        shouldSaveHistory = true;
      if (isFalling && !lastFallState)
        shouldSaveHistory = true;
      if (isHealthAlert && !lastHealthState)
        shouldSaveHistory = true;

      if (lastHistorySave > 0) {
        if (abs(currentTempObj - lastSavedTemp) >= 10.0)
          shouldSaveHistory = true;
        if (lastBPM > 0 && lastSavedBPM > 0 &&
            abs(lastBPM - lastSavedBPM) >= 20)
          shouldSaveHistory = true;
        if (lastSpO2 > 0 && abs(lastSpO2 - lastSavedSpO2) >= 5)
          shouldSaveHistory = true;
        if (abs(currentDust - lastSavedDust) >= 10.0)
          shouldSaveHistory = true;
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

        // Tạo JSON sạch cho lịch sử
        FirebaseJson historyJson;
        historyJson.set("TempObj", currentTempObj);
        historyJson.set("TempAmb", currentTempAmb);
        historyJson.set("Dust", currentDust);
        historyJson.set("Alert_SOS", isSOS);
        historyJson.set("Alert_Fall", isFalling);
        historyJson.set("Alert_Health", isHealthAlert);
        if (dataMutex != NULL)
          xSemaphoreTake(dataMutex, portMAX_DELAY);
        historyJson.set("Alert_Reason", healthAlertReason);
        historyJson.set("ThoiGian", currentTimeStr);
        if (dataMutex != NULL)
          xSemaphoreGive(dataMutex);

        // CHỈ LƯU BPM/SpO2 KHI CÓ KẾT QUẢ ĐÃ LỌC NHIỄU VÀ HỢP LỆ (>0) HOẶC
        // NGỪNG TIM
        if ((measurementProgress == 100 && lastBPM > 0) ||
            isEmergencyFlatline) {
          historyJson.set("BPM", lastBPM);
          historyJson.set("SpO2", lastSpO2);
        }

        Firebase.RTDB.setJSON(&fbdo, historyPath, &historyJson);

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
  dataMutex = xSemaphoreCreateMutex();

  pinMode(TOUCH_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(TOUCH_PIN), touchISR, CHANGE);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  pinMode(DUST_LED_PIN, OUTPUT);

  Wire.begin(SDA_PIN, SCL_PIN);

  // Bẻ lái giao tiếp SPI của màn hình TFT sang các chân mới
  SPI.begin(TFT_SCL, -1, TFT_SDA, TFT_CS);

  tft.init(240, 280);
  tft.setRotation(2);
  tft.setTextWrap(false);
  showBootScreen();

  WiFiManager wm;
  wm.setConfigPortalTimeout(120);
  if (!wm.autoConnect("ESP32_Y_Te")) {
    Serial.println("Chay Offline Mode (Khong co WiFi)");
  }

  String mac = WiFi.macAddress();
  mac.replace(":", "");
  deviceID = "DEV_" + mac;

  lastActivityTime = millis();

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

    // Khoi tao OTA
    ArduinoOTA.setHostname("DAKT1-ESP32-AI");
    ArduinoOTA.setPassword("admin123");
    ArduinoOTA.begin();
  }

  if (MDNS.begin("esp32"))
    Serial.println("mDNS: http://esp32.local");
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/settext", []() {
    if (server.hasArg("msg"))
      customText = server.arg("msg");
    if (currentScreen == 2)
      drawWeatherScreenStatic();
    server.send(200, "text/plain", "OK");
  });
  server.begin();

  max30102Found = initMAX30102();
  mpu6050.begin();
  mpu6050.calcGyroOffsets(true);
  mlx.begin();
  gpsSerial.begin(GPSBaud, SERIAL_8N1, RXPin, TXPin);

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(30, 90);
  tft.println("DANG ON DINH");
  tft.setCursor(45, 120);
  tft.println("CAM BIEN...");

  // ===== SHOW IP ADDRESS O DAY =====
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(20, 170);
  if (WiFi.status() == WL_CONNECTED) {
    tft.print("IP: ");
    tft.print(WiFi.localIP());
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.print("KHONG CO MANG (OFFLINE)");
  }

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

  xTaskCreatePinnedToCore(TaskFirebase, "FirebaseTask", 32768, NULL, 1,
                          &FirebaseTaskHandle, 0);

  lastHistorySave =
      millis(); // Khởi tạo để tránh lưu lịch sử ngay khi vừa bật máy
  lastScreen = -1;
}

// ===================== LOOP =====================
void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  handleTouchToggle();
  while (gpsSerial.available() > 0)
    gps.encode(gpsSerial.read());

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

    float accX = mpu6050.getAccX();
    float accY = mpu6050.getAccY();
    float accZ = mpu6050.getAccZ();

    // THUẬT TOÁN DỰ PHÒNG: State Machine (Freefall -> Impact)
    float vectorSum = sqrt(accX * accX + accY * accY + accZ * accZ);
    static unsigned long lastFreeFallTime = 0;

    // 1. Rơi tự do (Gia tốc trên 3 trục gần bằng 0) - Lo lỏng ngưỡng lên 0.8 do
    // thiết bị xoay vòng sẽ có lực li tâm
    if (vectorSum < 0.8) {
      lastFreeFallTime = millis();
    }

    // 2. Va chạm (Impact)
    // Nới lỏng ngưỡng đập xuống 1.6G. Thời gian cửa sổ là 1.5 giây để khớp cả
    // những cú rớt từ vị trí cao.
    if (vectorSum > 1.6 && lastFreeFallTime > 0 &&
        (millis() - lastFreeFallTime < 1500)) {
      if (!isFalling && !fallWarning) {
        fallWarning = true;
        fallWarningStart = millis();
      }
      lastFreeFallTime = 0;
    }

    // 3. Ngoại lệ tàn bạo: Trượt té trên mặt phẳng (không rớt tự do) nhưng lực
    // đập cực kỳ mạnh!
    if (vectorSum > 3.0) {
      if (!isFalling && !fallWarning) {
        fallWarning = true;
        fallWarningStart = millis();
      }
    }

    // Nạp dữ liệu vào mảng (Bảo vệ tràn)
    if (feature_ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
      ai_features[feature_ix++] = accX;
      ai_features[feature_ix++] = accY;
      ai_features[feature_ix++] = accZ;
    }

    // Khi mảng "cửa sổ" đã đầy -> Tiến hành suy luận
    if (feature_ix >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
      signal_t signal;
      int err = numpy::signal_from_buffer(
          ai_features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

      if (err == 0) {
        ei_impulse_result_t result = {0};
        err = run_classifier(&signal, &result, false);

        if (err == EI_IMPULSE_OK) {
          for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            // Khôi phục ngưỡng AI xuống 75% để không chặn các nhận diện đúng
            // của mô hình
            if (strcmp(result.classification[ix].label, "Fall") == 0 &&
                result.classification[ix].value > 0.75) {
              if (!isFalling && !fallWarning) {
                fallWarning = true;
                fallWarningStart = millis();
              }
            }
          }
        }
      }

      // Trượt cửa sổ: Xóa 50% dữ liệu cũ, nhường chỗ cho dữ liệu mới ở chu kỳ
      // sau
      size_t shift_elements = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE / 2;
      memmove(ai_features, ai_features + shift_elements,
              (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - shift_elements) *
                  sizeof(float));
      feature_ix -= shift_elements;
    }
  }
  // ===============================================================

  if (millis() - lastPrintSensor >= sensorInterval) {
    lastPrintSensor = millis();
    updateTFT();
  }

  // ===================== DISPLAY TIMEOUT =====================
  // Chặn cơ chế ngủ màn hình khi đang đo nhịp tim
  if (fingerPresent || (measurementProgress > 0 && measurementProgress < 100)) {
    lastActivityTime = millis();
    if (screenOff) {
      screenOff = false;
      lastScreen = -1;
    }
  }

  if (!screenOff && !isSOS && !isFalling && !isHealthAlert && !fallWarning) {
    if (millis() - lastActivityTime >= SCREEN_TIMEOUT_MS) {
      screenOff = true;
      drawSleepScreen();
    }
  }
  if (screenOff && (isSOS || isFalling || isHealthAlert || fallWarning)) {
    screenOff = false;
    lastScreen = -1;
    lastActivityTime = millis();
  }

  if (needWeatherRedraw) {
    needWeatherRedraw = false;
    if (currentScreen == 2)
      drawWeatherScreenStatic();
  }

  if (needAIRedraw) {
    needAIRedraw = false;
    if (currentScreen == 4)
      drawAIDoctorScreen();
  }

  delay(1);
}