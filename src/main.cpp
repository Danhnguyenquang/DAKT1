#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
#include <MPU6050_tockn.h>
#include <Adafruit_MLX90614.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h> 

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
TaskHandle_t FirebaseTaskHandle;

// ===================== KHAI BÁO CHÂN PHẦN CỨNG =====================
#define DUST_VO_PIN  34   
#define DUST_LED_PIN 5    
#define TOUCH_PIN    13   
#define BUZZER_PIN   15   

// ===================== KHAI BAO CAM BIEN =====================
MAX30105 particleSensor;
MPU6050 mpu6050(Wire);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// ===================== GPS (HardwareSerial) =====================
static const int RXPin = 26, TXPin = 25;
static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps;
HardwareSerial gpsSerial(2); 

// ===================== I2C ESP32 =====================
#define SDA_PIN 21
#define SCL_PIN 22

// ===================== THOI GIAN IN & BIẾN CỜ =====================
unsigned long lastPrintSensor = 0;
const unsigned long sensorInterval = 500; 

float currentTempObj = 0.0;
float currentTempAmb = 0.0;
float currentMpuX = 0.0;
float currentMpuY = 0.0;
float currentMpuZ = 0.0;
float currentDust = 0.0;

bool isSOS = false;
bool isFalling = false;

const float ALPHA_TEMP = 0.2; 
const float ALPHA_MPU = 0.3;

// ===================== MAX30102 & TIẾN TRÌNH ĐO =====================
uint32_t irBuffer[100];
uint32_t redBuffer[100];
int32_t bufferLength = 100;

int32_t spo2 = 0;
int8_t validSPO2 = 0;
int32_t heartRateValue = 0;
int8_t validHeartRate = 0;

bool max30102Found = false;
int measurementProgress = 0; // BIẾN HIỂN THỊ TRẠNG THÁI % ĐO

const int FILTER_SIZE = 7;
int bpmHistory[FILTER_SIZE] = {0};
int spo2History[FILTER_SIZE] = {0};
int bpmCount = 0;
int spo2Count = 0;
int bpmIndex = 0;
int spo2Index = 0;

// ===================== BIẾN NGẮT NÚT NHẤN =====================
volatile bool touchDetected = false;

void IRAM_ATTR touchISR() {
  touchDetected = true; 
}

// ===================== BIẾN CÒI & CẢNH BÁO TỰ ĐỘNG =====================
unsigned long lastBeepTime = 0;
bool buzzerState = false;
unsigned long healthDangerTimer = 0; 

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

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 45); tft.print("BPM: ");
  tft.setCursor(10, 70); tft.print("SpO2:");

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 100); tft.print("Obj: ");
  tft.setCursor(10, 125); tft.print("Amb: ");
  tft.setCursor(10, 150); tft.print("Dust:"); 

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(10, 180); tft.print("X: ");
  tft.setCursor(85, 180); tft.print("Y: ");
  tft.setCursor(160, 180); tft.print("Z: ");

  tft.setTextColor(ST77XX_MAGENTA);
  tft.setCursor(10, 200); tft.print("GPS: ");

  tft.drawLine(0, 230, 240, 230, ST77XX_BLUE);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 245); tft.print("SYS:");
}

void updateTFT()
{
  tft.setTextSize(2);
  
  // ---------- HIỂN THỊ NHỊP TIM ----------
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setCursor(70, 45);
  if (measurementProgress == 0) {
    tft.print("--   "); // Không có tay
  } else if (measurementProgress < 100) {
    tft.printf("%d%%  ", measurementProgress); // Đang đo, hiện %
  } else {
    tft.printf("%-5d", getFilteredBPM()); // Đo xong, hiện số thực tế
  }

  // ---------- HIỂN THỊ SPO2 ----------
  tft.setCursor(80, 70);
  if (measurementProgress == 0) {
    tft.print("--   ");
  } else if (measurementProgress < 100) {
    tft.printf("%d%%  ", measurementProgress);
  } else {
    tft.printf("%d%%  ", getFilteredSpO2());
  }

  // ---------- CÁC THÔNG SỐ KHÁC ----------
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(70, 100); tft.printf("%-6.1f C", currentTempObj);
  tft.setCursor(70, 125); tft.printf("%-6.1f C", currentTempAmb);
  tft.setCursor(70, 150); tft.printf("%-5.2f mg/m3", currentDust);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setCursor(30, 180); tft.printf("%-5.0f", currentMpuX);
  tft.setCursor(105, 180); tft.printf("%-5.0f", currentMpuY);
  tft.setCursor(180, 180); tft.printf("%-5.0f", currentMpuZ);

  tft.setTextColor(ST77XX_MAGENTA, ST77XX_BLACK);
  if (gps.location.isValid()) {
    tft.setCursor(50, 200); tft.print("OK        ");
    tft.setCursor(10, 215); tft.printf("%-9.4f, %-9.4f", gps.location.lat(), gps.location.lng());
  } else {
    tft.setCursor(50, 200); tft.print("Waiting...");
    tft.setCursor(10, 215); tft.print("                    "); 
  }

  tft.setTextSize(2);
  tft.setCursor(65, 245);
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
  if (touchDetected) {
    touchDetected = false; 
    static unsigned long lastTouchTime = 0;
    if (millis() - lastTouchTime > 500) { 
      if (isFalling || isSOS) {
        isFalling = false;
        isSOS = false;
      } else {
        isSOS = true;
      }
      lastTouchTime = millis();
    }
  }

  float accX = mpu6050.getAccX();
  float accY = mpu6050.getAccY();
  float accZ = mpu6050.getAccZ();
  float svm = sqrt(pow(accX, 2) + pow(accY, 2) + pow(accZ, 2));
  
  if (svm > 2.5) {
    isFalling = true;
  }

  int currentBPM = (measurementProgress == 100 && bpmCount > 0) ? getFilteredBPM() : 0;
  int currentSpO2 = (measurementProgress == 100 && spo2Count > 0) ? getFilteredSpO2() : 0;
  
  bool isHealthDanger = false;
  if (currentBPM > 0 && (currentBPM < 50 || currentBPM > 120)) isHealthDanger = true;
  if (currentSpO2 > 0 && currentSpO2 < 90) isHealthDanger = true;
  if (currentTempObj > 38.5 || (currentTempObj < 35.0 && currentTempObj > 30.0)) isHealthDanger = true;

  if (isHealthDanger) {
    if (healthDangerTimer == 0) healthDangerTimer = millis(); 
    if (millis() - healthDangerTimer > 5000) {
      isSOS = true;
    }
  } else {
    healthDangerTimer = 0; 
  }

  if (isSOS || isFalling) {
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

void initMAX30102Buffer() {
  if (!max30102Found) return;
  for (byte i = 0; i < 100; i++) {
    long start = millis();
    while (particleSensor.available() == false) {
      particleSensor.check();
      if (millis() - start > 100) break; 
      delay(1); 
    }
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }
}

// ===================== CẬP NHẬT CẢM BIẾN NHỊP TIM =====================
void updateMAX30102Fast() {
  if (!max30102Found) return;

  for (byte i = 25; i < 100; i++) {
    redBuffer[i - 25] = redBuffer[i];
    irBuffer[i - 25] = irBuffer[i];
  }

  for (byte i = 75; i < 100; i++) {
    long start = millis();
    while (particleSensor.available() == false) {
      particleSensor.check();
      if (millis() - start > 100) break;
      delay(1); 
    }
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }

  // NẾU RÚT TAY RA: Reset tiến trình và xóa dữ liệu cũ
  if (irBuffer[99] < 50000) {
    bpmCount = 0;   
    spo2Count = 0;  
    measurementProgress = 0; 
    return;         
  }

  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRateValue, &validHeartRate);

  // NẾU TÍN HIỆU TỐT: Lưu kết quả và đặt tiến trình 100%
  if (validHeartRate && heartRateValue >= 50 && heartRateValue <= 120 && validSPO2 && spo2 >= 80 && spo2 <= 100) {
    addBPMValue(heartRateValue);
    addSpO2Value(spo2);
    measurementProgress = 100; 
  } else {
    // ĐANG TÍNH TOÁN: Tăng % giả lập để tạo thanh tiến trình chờ
    if (measurementProgress < 95) {
      measurementProgress += 20; 
    }
  }
}

// ===================== SETUP =====================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(TOUCH_PIN, INPUT); 
  attachInterrupt(digitalPinToInterrupt(TOUCH_PIN), touchISR, RISING); 

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH); 
  
  pinMode(DUST_LED_PIN, OUTPUT);

  // ---------- KẾT NỐI WIFI ----------
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  String mac = WiFi.macAddress();
  mac.replace(":", ""); 
  deviceID = "DEV_" + mac; 

  // ---------- KẾT NỐI FIREBASE ----------
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = "esp32@gmail.com";
  auth.user.password = "12345678";
  signupOK = true; 

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
  gpsSerial.begin(GPSBaud, SERIAL_8N1, RXPin, TXPin);

  currentTempObj = mlx.readObjectTempC();
  currentTempAmb = mlx.readAmbientTempC();
  currentMpuX = mpu6050.getAngleX();
  currentMpuY = mpu6050.getAngleY();
  currentMpuZ = mpu6050.getAngleZ();

  drawStaticUI(); 

  xTaskCreatePinnedToCore(TaskFirebase, "FirebaseTask", 16384, NULL, 1, &FirebaseTaskHandle, 0);                    
}

// ===================== LOOP (Chạy trên Core 1) =====================
void loop()
{
  while (gpsSerial.available() > 0) { gps.encode(gpsSerial.read()); }

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
    
    Serial.print("MAX30102 -> Status: ");
    if (measurementProgress == 0) Serial.println("No Finger");
    else if (measurementProgress < 100) { Serial.print("Measuring "); Serial.print(measurementProgress); Serial.println("%"); }
    else {
      Serial.print("BPM: "); Serial.print(getFilteredBPM());
      Serial.print(" | SpO2: "); Serial.println(getFilteredSpO2());
    }

    Serial.printf("MPU6050  -> X: %.1f Y: %.1f Z: %.1f\n", currentMpuX, currentMpuY, currentMpuZ);
    Serial.printf("MLX90614 -> Obj: %.1f*C | Amb: %.1f*C\n", currentTempObj, currentTempAmb);
    Serial.printf("Dust     -> %.2f mg/m3\n", currentDust);

    updateTFT();
  }
  
  delay(10); 
}

// ===================== TASK FIREBASE (Chạy ngầm trên Core 0) =====================
void TaskFirebase(void *pvParameters)
{
  for (;;)
  {
    if (Firebase.ready() && signupOK) 
    {
      // Nếu đo xong mới lấy dữ liệu, chưa xong thì gửi 0 lên App
      int currentBPM = (measurementProgress == 100 && bpmCount > 0) ? getFilteredBPM() : 0;
      int currentSpO2 = (measurementProgress == 100 && spo2Count > 0) ? getFilteredSpO2() : 0;

      String basePath = "Devices/" + deviceID + "/";

      FirebaseJson json;
      json.set("BPM", currentBPM);
      json.set("SpO2", currentSpO2);
      json.set("TempObj", currentTempObj);
      json.set("TempAmb", currentTempAmb);
      json.set("AngleX", currentMpuX);
      json.set("AngleY", currentMpuY);
      json.set("AngleZ", currentMpuZ);
      json.set("Dust", currentDust);
      json.set("Alert_SOS", isSOS);
      json.set("Alert_Fall", isFalling);

      if (gps.location.isValid()) {
        json.set("GPS_Lat", gps.location.lat());
        json.set("GPS_Lng", gps.location.lng());
      }

      Firebase.RTDB.setJSON(&fbdo, basePath, &json);
    }
    
    vTaskDelay(pdMS_TO_TICKS(1500));
  }
}