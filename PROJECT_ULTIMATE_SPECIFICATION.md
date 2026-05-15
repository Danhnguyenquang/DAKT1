# 📘 TÀI LIỆU ĐẶC TẢ KỸ THUẬT SIÊU CHI TIẾT: HỆ THỐNG GIÁM SÁT Y TẾ & AI IOT

Tài liệu này đóng vai trò là hồ sơ kỹ thuật đầy đủ nhất cho dự án, bao gồm mọi khía cạnh từ lớp vật lý (phần cứng) đến lớp ứng dụng và trí tuệ nhân tạo.

---

## CHƯƠNG 1: KIẾN TRÚC PHẦN CỨNG CHI TIẾT (DAKT1)

### 1.1. Sơ đồ Pinout & Giao thức kết nối
Hệ thống sử dụng **ESP32 DevKit V1** làm trung tâm điều khiển với các chân kết nối sau:

| Linh kiện | Chân linh kiện | Chân ESP32 | Giao diện | Chân năng lượng |
|---|---|---|---|---|
| **ST7789 TFT** | SCL/SDA/DC/RES/CS | 16/17/18/5/19 | SPI (80MHz) | 3.3V / GND |
| **MAX30102** | SDA / SCL | 21 / 22 | I2C (400kHz) | 3.3V / GND |
| **MPU6050** | SDA / SCL | 21 / 22 | I2C (400kHz) | 3.3V / GND |
| **MLX90614** | SDA / SCL | 21 / 22 | I2C (100kHz) | 3.3V / GND |
| **Dust Sensor** | VO / LED | 33 / 14 | Analog / Digital | 5V / GND |
| **GPS Module** | TX / RX | 35 / 32 | UART2 (9600) | 5V / GND |
| **Touch Sensor** | Input | 15 | Digital (ISR) | 3.3V / GND |
| **Active Buzzer** | I/O | 13 | PWM / Digital | 3.3V / GND |

### 1.2. Phân tích Năng lượng (Power Management)
- **Nguồn cấp:** Pin Li-ion 5000mAh (3.7V).
- **Module Boost (XL6009):** Nâng 3.7V lên 5V (Dùng cho ESP32 VIN, GPS, Dust Sensor).
- **Module Buck (AMS1117-3.3):** Hạ 5V xuống 3.3V (Dùng cho các cảm biến I2C và màn hình).
- **Dòng tiêu thụ:**
  - Chế độ bình thường (Normal): **~363.5 mA**.
  - Chế độ báo động (Alert): **~388.5 mA**.
- **Thời lượng pin dự kiến:** **8.1 giờ** (Duy trì gửi dữ liệu và chạy AI liên tục).

---

## CHƯƠNG 2: THUẬT TOÁN & LOGIC XỬ LÝ FIRMWARE

### 2.1. Phát hiện té ngã 3 lớp (Redundancy Fall Detection)
Hệ thống sử dụng kiến trúc dự phòng đa tầng để đảm bảo không bỏ sót bất kỳ cú ngã nào:

1.  **Lớp AI (Edge Impulse):**
    - Sử dụng mạng Neural Network thu nhỏ (CNN/DNN) chạy trực tiếp trên ESP32.
    - Cửa sổ trượt (Sliding Window) 50% overlap.
    - Dataset: Thu thập từ dataset SisFall (chuẩn y khoa quốc tế).
2.  **Lớp Cân bằng vật lý (State Machine):**
    - Theo dõi biến động `vectorSum = sqrt(x^2 + y^2 + z^2)`.
    - Điều kiện: Phát hiện Rơi tự do (`< 0.8G`) tiếp nối bởi Va chạm mạnh (`> 1.6G`) trong vòng **1.5 giây**.
3.  **Lớp Va chạm trực tiếp (Brute Force):**
    - Nếu `vectorSum > 3.0G` (Gia tốc cực lớn), hệ thống lập tức kích hoạt báo động mà không cần qua tầng AI.

### 2.2. Xử lý tín hiệu cảm biến sinh tồn
- **Nhịp tim (BPM):** Sử dụng bộ lọc trung vị (Median Filter) kích thước 7 mẫu để loại bỏ nhiễu trắng và nhiễu do rung tay.
- **Nồng độ Oxy (SpO2):** Triển khai thuật toán Maxim Integrated, tính toán tỉ lệ giữa thành phần AC và DC của ánh sáng IR và Red.
- **Thân nhiệt:** Sử dụng cảm biến hồng ngoại MLX90614 đọc liên tục vùng trán/cổ tay với sai số ±0.2°C.

### 2.3. Cảnh báo phối hợp (Sensor Fusion Alerts)
Hệ thống không chỉ cảnh báo chỉ số lẻ mà còn kết hợp logic:
- `isFalling && BPM < 50`: **Cảnh báo bất tỉnh** (Nguy kịch cao).
- `TempObj > 38.5 && BPM > 110`: **Cảnh báo sốt cao gây loạn nhịp**.
- `Dust > 150 && SpO2 < 92`: **Cảnh báo suy hô hấp cấp do ô nhiễm**.

---

## CHƯƠNG 3: KIẾN TRÚC PHẦN MỀM MOBILE (ANDROID)

### 3.1. Cấu trúc MVVM & Tài nguyên UI
- **View:** Sử dụng `DataBinding` và `ViewBinding` để tối ưu hóa việc cập nhật giao diện.
- **ViewModel:** `DashboardViewModel` quản lý toàn bộ dữ liệu từ Firebase, cung cấp `LiveData` cho các Fragment.
- **UI Design (Glassmorphism):**
  - Card mờ (Translucent) với độ đục `CC` (80%).
  - Bo góc (Corner Radius): `24dp`.
  - Gradient: Linear từ `Primary` sang `PrimaryDark`.

### 3.2. Chức năng chi tiết các Fragment

#### A. HomeFragment (Màn hình giám sát tập trung)
- **ID các thành phần chính:**
  - `tvHomeOverallStatus`: Hiển thị "ỔN ĐỊNH", "PHÁT HIỆN TÉ NGÃ", "NGOẠI TUYẾN".
  - `cardHeartRate`, `cardSpo2`, `cardTemperature`, `cardDust`: Các thẻ tương tác xem chi tiết.
  - `btnCancelAlert`: Nút gửi lệnh về thiết bị để dừng còi báo động.
- **Logic cập nhật:** Tự động lắng nghe thay đổi từ node `/Devices/{DEVICE_ID}/Data`.

#### B. DashboardFragment (Cứu hộ chuyên nghiệp)
- **Google Maps Interaction:**
  - Sử dụng `map_style_medical.json` để ẩn các địa điểm dịch vụ và ưu tiên cơ sở y tế.
  - Tích hợp `searchNearbyHospitals(lat, lon)` sử dụng **Overpass API**.
- **Share Location:** Tạo chuỗi URL `google.com/maps?q=lat,lon` để gửi khẩn cấp qua các ứng dụng OTT.

#### C. MetricDetailFragment (Báo cáo & Lịch sử)
- **Biểu đồ (MPAndroidChart):**
  - Đường nét mềm dẻo (Cubic Intensity = 0.2).
  - Đổ bóng (Fill Alpha = 85).
- **Xuất PDF:**
  - Sử dụng lớp `PdfDocument`.
  - Vẽ Logo bệnh viện, thông tin người dùng, bảng thống kê và biểu đồ mini vào văn bản A4.

---

## CHƯƠNG 4: TRÍ TUỆ NHÂN TẠO & ĐỒNG BỘ DỮ LIỆU

### 4.1. Tích hợp Google Gemini AI
Hệ thống sử dụng Gemini API để đưa ra tư vấn y tế.
- **System Prompt:** "Bạn là một bác sĩ chuyên gia. Hãy dựa trên các chỉ số: BPM={bpm}, SpO2={spo2}, Temp={temp}, Dust={dust} để đưa ra chẩn đoán ngắn gọn và lời khuyên chính xác cao nhất (không quá 100 từ)."
- **Đồng bộ hóa:** Kết quả tư vấn được lưu vào `/Control/GeminiAdvice` trên Firebase và tự động hiển thị trên màn hình TFT của thiết bị đeo.

### 4.2. Schema Cơ sở dữ liệu Firebase
```json
{
  "Devices": {
    "MAC_ADDRESS": {
      "Data": {
        "BPM": "int (Nhịp tim)",
        "SpO2": "int (Phần trăm Oxy)",
        "TempObj": "float (Thân nhiệt)",
        "TempAmb": "float (Nhiệt độ môi trường)",
        "Dust": "float (PM2.5)",
        "GPS_Lat": "double",
        "GPS_Lng": "double",
        "TrangThai": "String (Binh thuong/Canh bao)",
        "ThoiGian": "String (dd/MM/yyyy HH:mm:ss)"
      },
      "Alerts": {
        "Alert_Fall": "boolean",
        "Alert_SOS": "boolean",
        "Alert_Health": "boolean"
      },
      "Control": {
        "Cmd_CancelAlert": "boolean",
        "GeminiAdvice": "String (Lời khuyên từ AI)"
      }
    }
  }
}
```

---

## CHƯƠNG 5: QUY TRÌNH VẬN HÀNH & KHẮC PHỤC SỰ CỐ

### 5.1. Quy trình khởi động
1. ESP32 kết nối WiFi qua **WiFiManager** (với mDNS: `baoyetecare.local`).
2. Đồng bộ thời gian qua GPS (ưu tiên) hoặc NTP (dự phòng).
3. Khởi tạo các cảm biến I2C (Kiểm tra xem có gặp lỗi `Sensor Not Found` không).
4. Khởi chạy luồng FreeRTOS xử lý AI và Firebase.

### 5.2. Các tình huống ngoại lệ & Cách xử lý
- **Lỗi Finger Not Found:** Hệ thống kiểm tra giá trị IR, nếu thấp sẽ hiện thông báo "Vui long dat tay".
- **Mất kết nối WiFi:** ESP32 sẽ tự động chuyển sang lưu trữ dữ liệu vào Buffer và gửi lại khi có mạng.
- **Hủy báo động:** Có thể thực hiện bằng cách CHẠM NGẮN vào thiết bị hoặc nhấn nút TRÊN APP.

---
*Tài liệu này được biên soạn bởi Antigravity AI, phản ánh chính xác cấu trúc và mã nguồn thực tế của dự án Ứng dụng Giám sát Y tế Thông minh.*
