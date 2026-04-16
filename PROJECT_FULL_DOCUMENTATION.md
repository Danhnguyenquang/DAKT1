# TÀI LIỆU CHI TIẾT DỰ ÁN: HỆ THỐNG GIÁM SÁT SỨC KHỎE & CẢNH BÁO TÉ NGÃ (IOT)

Tài liệu này tổng hợp toàn bộ các chức năng, nhiệm vụ, thiết kế UI/UX và kiến trúc kỹ thuật của hệ thống bao gồm Thiết bị đeo (ESP32) và Ứng dụng di động (Android).

---

## 1. TỔNG QUAN HỆ THỐNG (SYSTEM OVERVIEW)
Hệ thống là một giải pháp chăm sóc sức khỏe thông minh, tập trung vào người cao tuổi với các tính năng cốt lõi:
- **Theo dõi chỉ số sinh tồn:** Nhịp tim, SpO2, Thân nhiệt.
- **Giám sát môi trường:** Nồng độ bụi mịn (PM2.5), Nhiệt độ môi trường.
- **An toàn & Cứu hộ:** Phát hiện té ngã bằng AI, Nút SOS khẩn cấp, Định vị GPS.
- **Trí tuệ nhân tạo:** Trợ lý ảo AI (Gemini) phân tích sức khỏe và đưa ra lời khuyên.

---

## 2. CHI TIẾT THIẾT BỊ PHẦN CỨNG (FIRMWARE - DAKT1)

### 2.1. Thành phần lý tính & Sơ đồ chân (Pinout)
- **Vi điều khiển:** ESP32 Dev Kit V1 (Dual-core 240MHz).
- **Màn hình TFT ST7789 (1.3 inch):**
  - `TFT_SCL (SCLK)` -> GPIO 16
  - `TFT_SDA (MOSI)` -> GPIO 17
  - `TFT_RST` -> GPIO 5
  - `TFT_DC` -> GPIO 18
  - `TFT_CS` -> GPIO 19
- **Cảm biến qua I2C (SDA=21, SCL=22):**
  - **MAX30102:** `0x57` - Nhịp tim/SpO2.
  - **MLX90614:** `0x5A` - Nhiệt độ hồng ngoại.
  - **MPU6050:** `0x68` - Gia tốc/Con quay hồi chuyển.
- **Cảm biến khác:**
  - **Dust Sensor:** `VO (Analog)` -> GPIO 33, `LED (Digital)` -> GPIO 14.
  - **GPS:** `TX` -> GPIO 35 (RX2), `RX` -> GPIO 32 (TX2).
  - **Touch Sensor:** `Input` -> GPIO 15 (Sử dụng ISR - Interrupt Service Routine).
  - **Buzzer:** `Output` -> GPIO 13.

### 2.2. Kiến trúc mã nguồn (src/main.cpp)
- **Hệ thống Đa nhiệm (FreeRTOS):**
  - `TaskFirebase`: Chạy trên Core 0, xử lý truyền nhận dữ liệu lên Cloud.
  - `Task chính (Loop)`: Chạy trên Core 1, xử lý đọc cảm biến và hiển thị TFT.
- **Quản lý bộ nhớ:** Sử dụng `LittleFS` để lưu trữ cấu hình cục bộ và `min_spiffs.csv` cho mô hình AI.

### 2.3. Các thuật toán & Logic chi tiết
- **AI Fall Detection (3 Lớp bảo vệ):**
  - **Lớp 1 (Trigger):** MPU6050 phát hiện lực va chạm (Impact) > 3G.
  - **Lớp 2 (AI Inference):** Edge Impulse xử lý dữ liệu 2 giây trước và sau va chạm để phân loại "Ngã" hoặc "Hoạt động mạnh".
  - **Lớp 3 (Orientation):** Kiểm tra góc nghiêng của thiết bị sau va chạm (Nếu nằm ngang > 5 giây).
- **Phát hiện ngón tay (MAX30102):**
  - Kiểm tra `IR Value > 50,000` để bắt đầu đo. Tự động reset trung vị nếu nhấc tay.
- **Cảnh báo phối hợp (Sensor Fusion):**
  - `isFalling && BPM < 50`: Cảnh báo bất tỉnh sau ngã.
  - `TempObj > 38.5 && BPM > 110`: Cảnh báo sốt cao tim đập nhanh.
- **QR Code Identity:** Thiết bị tự tạo mã QR chứa `Device_ID` lấy từ địa chỉ MAC (ChipID) để App quét và kết nối.

---

## 3. CHI TIẾT ỨNG DỤNG DI ĐỘNG (ANDROID APP - LOGINANIMATEDAPP)

### 3.1. Thiết kế UI/UX & Tài nguyên
- **Mã màu chuẩn (HEX):**
  - `orange_main`: #FF9800 (Nút bấm, điểm nhấn).
  - `green_safe`: #4CAF50 (Trạng thái ổn định).
  - `red_alert`: #E53935 (Cảnh báo nguy hiểm).
  - `glass_background`: #CCFFFFFF (Nền mờ Glassmorphism).
- **Typography:** Sử dụng font 'Inter' và 'Comfortaa' cho cảm giác hiện đại, dễ đọc.
- **Layout:** Tối ưu hóa 100% cho màn hình Amoled (Dark mode mượt mà).

### 3.2. Chức năng chi tiết từng Fragment

#### A. HomeFragment.java (Trung tâm điều khiển)
- **Real-time Engine:** Sử dụng `ValueEventListener` của Firebase để cập nhật UI tức thì (<100ms).
- **Hoạt họa (Heartbeat Animation):** Biểu tượng trái tim đập nhanh/chậm dựa theo giá trị BPM thực tế của người dùng.
- **Trình theo dõi pin/kết nối:** Phân tích độ trễ `ThoiGian` từ thiết bị, nếu > 60s sẽ đổi trạng thái sang "NGOẠI TUYẾN".

#### B. DashboardFragment.java (Cứu hộ & Bản đồ)
- **Map Medical Style:** Loại bỏ các địa điểm không cần thiết (quán ăn, shop) để làm nổi bật bệnh viện.
- **Overpass Query:** `node["amenity"~"hospital|clinic"](around:5000, lat, lon)` - Tìm chính xác các cơ sở y tế gần nhất.
- **Location Pinning:** Hiển thị hướng di chuyển và khoảng cách từ điện thoại người giám sát đến thiết bị.

#### C. MetricDetailFragment.java (Phân tích chuyên sâu)
- **MPAndroidChart Customization:**
  - `Cụm LineChart`: Có đổ bóng Gradient bên dưới đường dữ liệu.
  - `CustomMarkerView`: Hiện cửa sổ popup khi chạm vào điểm dữ liệu.
- **PDF Export Engine:** Sử dụng `PdfDocument` Android API, tự vẽ bảng biểu, logo y tế và ký tên xác nhận báo cáo.

#### D. ChatFragment.java (Trí tuệ nhân tạo)
- **Prompt Engineering:** AI được cấu hình đóng vai bác sĩ chuyên gia, phản hồi bằng tiếng Việt có cấu trúc (Triệu chứng, Nguyên nhân, Lời khuyên).
- **Session Caching:** Lưu lịch sử chat vào thẻ nhớ để người dùng xem lại khi không có mạng.

#### E. Tài khoản & Thiết lập (Account Fragment)
- **Quản lý thiết bị:** Kết nối thiết bị qua Device ID hoặc quét mã QR.
- **Cài đặt thông báo:** Bật/tắt các loại cảnh báo (Ngã, SOS, Health).
- **Biometrics:** Đăng nhập bằng vân tay/khuôn mặt kết hợp UI hoạt họa.

---

## 4. CƠ SỞ DỮ LIỆU & GIAO THỨC (DATA & PROTOCOL)

### 4.1. Firebase Realtime DB Structure
```json
{
  "Devices": {
    "DEVICE_ID_001": {
      "Data": {
        "BPM": 75,
        "SpO2": 98,
        "TempObj": 36.5,
        "Dust": 12.5,
        "GPS_Lat": 10.123,
        "GPS_Lng": 106.456,
        "ThoiGian": "22:05:00 15/04/2026"
      },
      "Alerts": {
        "Alert_Fall": false,
        "Alert_SOS": false,
        "Alert_Health": false
      },
      "Control": {
        "Cmd_CancelAlert": false,
        "GeminiAdvice": "Sức khỏe của bạn rất tốt!"
      },
      "History": {
        "timestamp_1": { "BPM": 75, "SpO2": 98, ... },
        "timestamp_2": { ... }
      }
    }
  }
}
```

### 4.2. Bảo mật & Hiệu suất
- **Xử lý bất đồng bộ:** Sử dụng `TaskFirebase` (Multicore) trên ESP32 để không làm giật màn hình khi gửi dữ liệu.
- **Caching AI:** App Android lưu trữ lời khuyên AI để giảm số lần gọi API Gemini (tiết kiệm Token).
- **Offline Mode:** App vẫn hiển thị dữ liệu cuối cùng khi thiết bị mất mạng.

---

## 5. DANH SÁCH NHIỆM VỤ ĐÃ & ĐANG THỰC HIỆN (ROADMAP)

### Giai đoạn 1: Phần cứng & Firmware cơ bản [Hoàn tất]
- [x] Thiết kế mạch và kết nối linh kiện.
- [x] Thu thập dữ liệu gia tốc để huấn luyện mô hình Té ngã.
- [x] Lập trình bộ lọc Median Filter cho MAX30102.
- [x] Xây dựng giao diện TFT đa tầng.

### Giai đoạn 2: App Android & Cloud [Hoàn tất]
- [x] Thiết kế UI Glassmorphism.
- [x] Kết nối Firebase Realtime.
- [x] Tích hợp Google Maps & Overpass API.
- [x] Triển khai biểu đồ MPAndroidChart & PDF Export.

### Giai đoạn 3: Tính năng nâng cao & Tối ưu [Đang triển khai]
- [x] Tích hợp Trợ lý ảo Gemini AI.
- [x] Hệ thống cảnh báo kết hợp (Sensor Fusion Alerts).
- [/] Tối ưu hóa hiệu năng pin và bộ nhớ.
- [/] Kiểm thử độ chính xác của AI Fall Detection trong môi trường thực tế.

---
*Tài liệu được cập nhật tự động bởi Antigravity AI dựa trên hiện trạng mã nguồn ngày 15/04/2026.*
