#include <Arduino.h>
#include <Wire.h>

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
const unsigned long sensorInterval = 1000;

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
// BLK dang noi truc tiep 3.3V nen khong can khai bao dieu khien

// SPI mac dinh ESP32: SCK = 18, MOSI = 23
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
void updateMAX30102();
void initTFT();
void updateTFT();

// ===================== HAM HIEN THI TFT =====================
void initTFT()
{
  tft.init(240, 280);   // ST7789 1.69 inch thuong la 240x280
  tft.setRotation(2);   // Neu hien thi sai chieu, doi 0/1/2/3
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

void updateTFT()
{
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);

  // Tieu de
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(20, 10);
  tft.println("HEALTH MONITOR");

  tft.drawLine(0, 35, 240, 35, ST77XX_BLUE);

  // MAX30102
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  tft.print("BPM: ");
  if (bpmCount > 0)
    tft.println(getFilteredBPM());
  else
    tft.println("--");

  tft.setCursor(10, 80);
  tft.print("SpO2: ");
  if (spo2Count > 0)
  {
    tft.print(getFilteredSpO2());
    tft.println("%");
  }
  else
  {
    tft.println("--");
  }

  // MLX90614
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 110);
  tft.print("Obj: ");
  tft.print(mlx.readObjectTempC(), 1);
  tft.println(" C");

  tft.setCursor(10, 140);
  tft.print("Amb: ");
  tft.print(mlx.readAmbientTempC(), 1);
  tft.println(" C");

  // MPU6050
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);

  tft.setCursor(10, 175);
  tft.print("X: ");
  tft.print(mpu6050.getAngleX(), 1);

  tft.setCursor(10, 190);
  tft.print("Y: ");
  tft.print(mpu6050.getAngleY(), 1);

  tft.setCursor(10, 205);
  tft.print("Z: ");
  tft.print(mpu6050.getAngleZ(), 1);

  // GPS
  tft.setTextColor(ST77XX_MAGENTA);
  tft.setCursor(10, 225);
  tft.print("GPS: ");
  if (gps.location.isValid())
  {
    tft.println("OK");
    tft.setCursor(10, 240);
    tft.print(gps.location.lat(), 4);
    tft.print(",");
    tft.println(gps.location.lng(), 4);
  }
  else
  {
    tft.println("Waiting...");
  }
}

// ===================== HAM SAP XEP PHUC VU TRUNG VI =====================
void sortArray(int *arr, int size)
{
  for (int i = 0; i < size - 1; i++)
  {
    for (int j = i + 1; j < size; j++)
    {
      if (arr[j] < arr[i])
      {
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
      }
    }
  }
}

int getMedian(int *data, int size)
{
  if (size <= 0) return 0;

  int temp[FILTER_SIZE];
  for (int i = 0; i < size; i++)
    temp[i] = data[i];

  sortArray(temp, size);

  if (size % 2 == 1)
    return temp[size / 2];
  else
    return (temp[size / 2 - 1] + temp[size / 2]) / 2;
}

void addBPMValue(int value)
{
  bpmHistory[bpmIndex] = value;
  bpmIndex = (bpmIndex + 1) % FILTER_SIZE;
  if (bpmCount < FILTER_SIZE) bpmCount++;
}

void addSpO2Value(int value)
{
  spo2History[spo2Index] = value;
  spo2Index = (spo2Index + 1) % FILTER_SIZE;
  if (spo2Count < FILTER_SIZE) spo2Count++;
}

int getFilteredBPM()
{
  return getMedian(bpmHistory, bpmCount);
}

int getFilteredSpO2()
{
  return getMedian(spo2History, spo2Count);
}

// ===================== HAM GPS =====================
void displayInfo()
{
  Serial.print(F("Location: "));
  if (gps.location.isValid())
  {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Date/Time: "));
  if (gps.date.isValid())
  {
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F(" "));
  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
    Serial.print(F("."));
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gps.time.centisecond());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.println();
}

// ===================== KHOI TAO MAX30102 =====================
bool initMAX30102()
{
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD))
  {
    Serial.println("MAX30102/MAX30105 was not found. Please check wiring/power.");
    return false;
  }

  byte ledBrightness = 60;
  byte sampleAverage = 4;
  byte ledMode = 2;       // Red + IR
  int sampleRate = 100;
  int pulseWidth = 411;
  int adcRange = 4096;

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  particleSensor.setPulseAmplitudeRed(0x1F);
  particleSensor.setPulseAmplitudeIR(0x1F);
  particleSensor.setPulseAmplitudeGreen(0);

  Serial.println("MAX30102 OK");
  return true;
}

// ===================== DOC MAX30102 =====================
void updateMAX30102()
{
  if (!max30102Found) return;

  for (byte i = 0; i < 100; i++)
  {
    while (particleSensor.available() == false)
    {
      particleSensor.check();
    }

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }

  maxim_heart_rate_and_oxygen_saturation(
      irBuffer,
      bufferLength,
      redBuffer,
      &spo2,
      &validSPO2,
      &heartRateValue,
      &validHeartRate);

  // Chi nhan gia tri hop ly de chong nhieu
  if (validHeartRate && heartRateValue >= 50 && heartRateValue <= 120)
  {
    addBPMValue(heartRateValue);
  }

  // SpO2 hop ly thuong 90-100, mo rong de loc
  if (validSPO2 && spo2 >= 80 && spo2 <= 100)
  {
    addSpO2Value(spo2);
  }
}

// ===================== SETUP =====================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("Khoi dong he thong IoT cham soc suc khoe...");

  Wire.begin(SDA_PIN, SCL_PIN);

  // ---------- TFT ----------
  initTFT();

  // ---------- MAX30102 ----------
  Serial.println("Khoi dong MAX30102...");
  max30102Found = initMAX30102();

  // ---------- MPU6050 ----------
  Serial.println("Khoi dong MPU6050...");
  mpu6050.begin();
  mpu6050.calcGyroOffsets(true);
  Serial.println("MPU6050 OK");

  // ---------- MLX90614 ----------
  Serial.println("Khoi dong MLX90614...");
  if (!mlx.begin())
  {
    Serial.println("Khong tim thay MLX90614. Kiem tra wiring.");
  }
  else
  {
    Serial.println("MLX90614 OK");
  }

  // ---------- GPS ----------
  ss.begin(GPSBaud);
  Serial.println(F("A simple demonstration of TinyGPSPlus with an attached GPS module"));
  Serial.print(F("Testing TinyGPSPlus library v. "));
  Serial.println(TinyGPSPlus::libraryVersion());
  Serial.println(F("by Mikal Hart"));
  Serial.println();

  Serial.println("He thong da san sang.");
  Serial.println("Dat ngon tay yen tren MAX30102 de do BPM va SpO2.");
  Serial.println("====================================================");

  updateTFT();
}

// ===================== LOOP =====================
void loop()
{
  // ---------- DOC GPS ----------
  while (ss.available() > 0)
  {
    if (gps.encode(ss.read()))
      displayInfo();
  }

  // ---------- CAP NHAT MPU6050 ----------
  mpu6050.update();

  // ---------- CAP NHAT MAX30102 ----------
  updateMAX30102();

  // ---------- IN DU LIEU MOI 1 GIAY ----------
  if (millis() - lastPrintSensor >= sensorInterval)
  {
    lastPrintSensor = millis();

    Serial.println("=============== DU LIEU CAM BIEN ===============");

    // MAX30102
    Serial.print("MAX30102 -> ");

    if (bpmCount > 0)
    {
      int bpmFiltered = getFilteredBPM();
      Serial.print("Heart Rate: ");
      Serial.print(bpmFiltered);
      Serial.print(" BPM");

      if (bpmFiltered >= 60 && bpmFiltered <= 100)
        Serial.print(" (binh thuong)");
      else
        Serial.print(" (ngoai nguong 60-100)");
    }
    else
    {
      Serial.print("Heart Rate: dang do...");
    }

    Serial.print(" | ");

    if (spo2Count > 0)
    {
      int spo2Filtered = getFilteredSpO2();
      Serial.print("SpO2: ");
      Serial.print(spo2Filtered);
      Serial.print(" %");
    }
    else
    {
      Serial.print("SpO2: dang do...");
    }

    Serial.println();

    // In them raw de debug khi can
    if (max30102Found)
    {
      Serial.print("MAX30102 RAW -> IR[");
      Serial.print(irBuffer[99]);
      Serial.print("] RED[");
      Serial.print(redBuffer[99]);
      Serial.println("]");
    }

    // MPU6050
    Serial.print("MPU6050  -> angleX : ");
    Serial.print(mpu6050.getAngleX());
    Serial.print("\tangleY : ");
    Serial.print(mpu6050.getAngleY());
    Serial.print("\tangleZ : ");
    Serial.println(mpu6050.getAngleZ());

    // MLX90614
    Serial.print("MLX90614 -> Ambient = ");
    Serial.print(mlx.readAmbientTempC());
    Serial.print("*C\tObject = ");
    Serial.print(mlx.readObjectTempC());
    Serial.println("*C");

    Serial.print("MLX90614 -> Ambient = ");
    Serial.print(mlx.readAmbientTempF());
    Serial.print("*F\tObject = ");
    Serial.print(mlx.readObjectTempF());
    Serial.println("*F");

    // GPS trang thai
    Serial.print("GPS      -> ");
    if (gps.location.isValid())
    {
      Serial.print(gps.location.lat(), 6);
      Serial.print(", ");
      Serial.println(gps.location.lng(), 6);
    }
    else
    {
      Serial.println("Dang cho du lieu GPS...");
    }

    Serial.println("================================================");

    // ---------- CAP NHAT MAN HINH ----------
    updateTFT();
  }
}