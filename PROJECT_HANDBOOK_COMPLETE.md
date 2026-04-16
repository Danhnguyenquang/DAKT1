# 📘 SỔ TAY KỸ THUẬT TOÀN DIỆN (MEGA SPECIFICATION): HỆ THỐNG Y TẾ THÔNG MINH IOT

Tài liệu này là "tinh hoa" của toàn bộ dự án, tập hợp mọi thông số từ mức xung nhịp vi xử lý đến từng pixel trên giao diện Android. Đây là nguồn tài liệu duy nhất bạn cần để hiểu thấu đáo hệ thống.

---

## 🛠 PHẦN 1: TẦNG VẬT LÝ & KIẾN TRÚC PHẦN CỨNG (HARDWARE DEEP-DIVE)

### 1.1. Sơ đồ khối Năng lượng & Hiệu suất (Power Logic)
Hệ thống được thiết kế để hoạt động liên tục với độ tin cậy cao nhất.

- **Nguồn cấp chính:** Pin Li-Ion dung lượng 5000mAh.
- **Tầng chuyển đổi 1 (Boost):** XL6009 nâng áp từ pin (~3.7V) lên **5V ổn định**. Hiệu suất đạt **94%**.
- **Tầng chuyển đổi 2 (Buck/LDO):** AMS1117-3.3V hạ từ 5V xuống 3.3V cấp cho MCU và cảm biến I2C.
- **Dòng tiêu thụ chi tiết:**
  - ESP32 (WiFi + AI): ~240mA (Đỉnh cao do xử lý mô hình Neural Network).
  - TFT ST7789: ~60mA (Độ sáng tối đa).
  - GPS NEO-6M: ~50mA (Duy trì khóa vệ tinh).
  - Các cảm biến khác: ~20mA.
- **Tổng dòng rút từ pin:** ~523mA. Thời gian chạy thực tế: **~8.1 giờ**.

### 1.2. Danh mục Pinout "Microscopic" (GPIO Mapping)
| Linh kiện | Chức năng chân | GPIO ESP32 | Giao thức/Đặc tính |
|---|---|---|---|
| **ST7789 (Display)** | SCL | 16 | SPI Clock (Chân tùy chỉnh) |
| | SDA | 17 | SPI MOSI |
| | DC | 18 | Data/Command selection |
| | RES | 5 | Hardware Reset |
| | CS | 19 | Chip Select |
| **MAX30102 (Bio)** | SDA/SCL | 21/22 | I2C Standard (100-400kHz) |
| **MPU6050 (IMU)** | SDA/SCL | 21/22 | I2C (Hỗ trợ DMP) |
| **MLX90614 (Temp)** | SDA/SCL | 21/22 | I2C (Địa chỉ 0x5A) |
| **Dust Sensor** | VO | 33 | ADC1_CH5 (Analog Input) |
| | LED | 14 | Digital Output (Kích xung 0.28ms) |
| **GPS Module** | TX | 35 | UART2 RX (Chỉ đọc) |
| | RX | 32 | UART2 TX |
| **Touch Sensor** | SIG | 15 | External Interrupt (CHANGE) |
| **Buzzer** | SIG | 13 | Digital/PWM Alert |

---

## 🧠 PHẦN 2: "BỘ NÃO" FIRMWARE & THUẬT TOÁN (CORE LOGIC)

### 2.1. Kiến trúc Đa nhân (FreeRTOS Multitasking)
Hệ thống tận dụng tối đa 2 nhân của ESP32:
- **Core 0 (TaskFirebase):** Chuyên trách giao tiếp Cloud (Firebase), lấy dữ liệu thời tiết (OpenWeather), và gọi API Gemini AI. Task này chạy lặp mỗi 2 giây để không làm nghẽn bus I2C của Core 1.
- **Core 1 (Main Loop):** Chuyên trách đọc cảm biến tần số cao và vẽ giao diện TFT (Cần tốc độ khung hình cao).

### 2.2. Thuật toán Phát hiện té ngã 3 tầng (Triple-Check Fall)
Đây là tính năng quan trọng nhất, kết hợp giữa Vật lý và Trí tuệ nhân tạo:
1.  **Tầng AI (Edge Impulse):** Sử dụng `ai_features` nạp gia tốc 3 trục vào mảng trượt. Khi đạt 75% tin cậy nhãn "Fall", hệ thống kích hoạt báo động.
2.  **Tầng State Machine:** Theo dõi `vectorSum`. Nếu xuất hiện trạng thái "lơ lửng" (< 0.8G) sau đó là "va đập" (> 1.6G) trong cửa sổ 1500ms -> Xác nhận ngã.
3.  **Tầng Brute Force:** Va chạm trực tiếp > 3.0G -> Báo động ngay lập tức (Xử lý các cú ngã trượt chân cực mạnh).

### 2.3. Xử lý Chỉ số sinh tồn (Biometric Processing)
- **Median Filter (Lọc trung vị):** Lấy 7 mẫu nhịp tim gần nhất, sắp xếp và lấy giá trị ở giữa để loại bỏ các điểm dữ liệu "nhảy vọt" (Outliers) do nhiễu cơ học.
- **Emergency Flatline:** Nếu người dùng đặt tay lên cảm biến nhưng sau 45 giây không tìm thấy mạch, hệ thống sẽ kích hoạt trạng thái `NGUY KỊCH: KHÔNG TÌM THẤY MẠCH` và gửi cảnh báo đỏ lên App.
- **Maxim Algorithm:** Tính toán SpO2 dựa trên công thức R-Curve: `SpO2 = 110 - 25 * R` (với R là tỉ lệ AC/DC của IR và Red).

---

## 📱 PHẦN 3: ỨNG DỤNG ANDROID & TRẢI NGHIỆM NGƯỜI DÙNG (MOBILE UX)

### 3.1. Thiết kế Hệ thống (System Identity)
- **Aesthetics:** Glassmorphism (Kính mờ). Sử dụng `CardView` với `android:background="@drawable/glass_bg"`.
- **Màu sắc trạng thái:**
  - **#4CAF50 (Xanh):** Ổn định (Healthy).
  - **#FB8C00 (Cam):** Cảnh báo nhẹ (Warning - Sốt nhẹ, Bụi cao).
  - **#F44336 (Đỏ):** Khẩn cấp (Emergency - Té ngã, SOS, SpO2 cực thấp).

### 3.2. Logic chuyên sâu các Module
#### A. Home Fragment - Trung tâm theo dõi
- **Firebase Listener:** Sử dụng `ValueEventListener` lắng nghe node `Devices/{ID}/Data`.
- **Heartbeat Animation:** Sử dụng `ObjectAnimator` để scale biểu tượng trái tim. Tần số scale (`ValueAnimator.setDuration`) được tính toán dựa trên chỉ số BPM thực tế: `duration = 60000 / BPM`.
- **Status Engine:** Nếu `ThoiGian` của dữ liệu cách thời điểm hiện tại > 1 phút -> Tự động chuyển UI sang trạng thái Xám (NGOẠI TUYẾN).

#### B. Dashboard Fragment - Bản đồ y tế
- **Cứu hộ Hospital:** Tích hợp Query Overpass thông minh: `[out:json];node["amenity"~"hospital|clinic"](around:5000, lat, lon);out center;`.
- **Biểu tượng Marker:** Bệnh viện (Đỏ), Phòng khám (Cam), Vị trí thiết bị (Xanh Azure).

#### C. Metric Detail - Phân tích & Xuất PDF
- **Real-time Waveform:** Biểu đồ sóng thời gian thực (30 điểm dữ liệu cuối) sử dụng `CUBIC_BEZIER` để đường cong trông mềm mại như máy đo y tế chuyên dụng.
- **Cơ chế PDF Export:**
  - Canvas tọa độ: `(50, 80)` cho tiêu đề, `(50, 120)` cho thời gian.
  - Vẽ trực tiếp các giá trị High/Low/Average vào file PDF bằng `canvas.drawText`.
  - Sử dụng `FileProvider` để chia sẻ PDF an toàn qua email/zalo.

---

## 🤖 PHẦN 4: TRÍ TUỆ NHÂN TẠO GEMINI AI (INTELLIGENT AGENT)

### 4.1. Hệ thống Prompt "Thần thánh"
Prompts được thiết kế để Gemini đóng vai trò một bác sĩ lọc dữ liệu cực nhanh:
- **Cấu hình trên ESP32 (Lite):** "Trả về tiếng Việt KHÔNG DẤU, tối đa 15-20 từ. Đi thẳng vào vấn đề."
- **Cấu hình trên Android (Full):**
  - **System Instruction:** Gán danh tính "HEALTHY 365", quy định ngưỡng nhịp tim, SpO2 chuẩn y tế.
  - **Quy tắc khẩn cấp:** Nếu SpO2 < 90% hoặc có Ngã -> In hoa toàn bộ: **⚠️ CANH BAO KHAN CAP**.

### 4.2. Chiến lược Caching AI (Tiết kiệm Token)
Để tránh gọi API Gemini liên tục gây tốn kém:
- **Cache Key:** `ai_advice_{metric_type}` lưu cùng `ai_session` (Sáng/Chiều/Tối).
- **Logic:** Mỗi buổi trong ngày (ví dụ: Sáng từ 6h-12h), AI chỉ phân tích một lần duy nhất. Lần mở App sau trong cùng buổi sẽ dùng dữ liệu đã lưu trong `SharedPreferences`.

---

## 📂 PHẦN 5: CẤU TRÚC FILE & QUY TRÌNH TRIỂN KHAI

### 5.1. Bản đồ tệp tin (File Mapping)
- **Firmware:**
  - `secrets.h`: Lưu SSID, Pass, Firebase URL, Gemini Key.
  - `all_frames.h`: Chứa mảng byte hình ảnh (Header File) cho hoạt họa thời tiết.
  - `main.cpp`: Toàn bộ logic điều khiển 1600+ dòng code.
- **Android:**
  - `AppConstants.java`: Cấu hình chung URL và Key.
  - `NotificationHelper.java`: Xử lý hiển thị thông báo đẩy khi có sự cố.
  - `ChatAdapter.java`: Quản lý hiển thị tin nhắn bác sĩ AI.

### 5.2. Quy trình "Build & Run"
1.  **Hardware:** Kết nối theo Pinout tại Chương 1.1.
2.  **Firmware:** Mở dự án bằng VSCode (PlatformIO), nạp BIOS ESP32. Sử dụng OTA để cập nhật từ xa nếu cần.
3.  **App:** Cập nhật `local.properties` với `GEMINI_API_KEY` và `DATABASE_URL`. Biên dịch APK.
4.  **Pairing:** Quét mã QR trên màn hình ESP32 bằng App điện thoại để định danh thiết bị.

---
*Tài liệu này được soạn thảo chi tiết đến từng biến số để đảm bảo tính minh bạch và chuyên nghiệp tuyệt đối cho đồ án 2026.*
