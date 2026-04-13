# Kiến Trúc Tổng Quan - DAKT1 (ESP32 Health & Fall Detection)

Tài liệu này đóng vai trò là "Agent Context" (Ngữ cảnh cho AI) để giúp hiểu rõ bức tranh toàn cục của thiết bị.

## 1. Mục tiêu dự án
Thiết bị đeo (Wearable/Mini Station) hỗ trợ theo dõi sức khỏe người cao tuổi và cảnh báo té ngã. Giao tiếp Real-time với Mobile App (Android) thông qua Firebase.

## 2. Phần cứng định tuyến (Pinout Configuration)
- **TFT Display (ST7789):** `SCL=16`, `SDA=17`, `RST=5`, `DC=18`, `CS=19`
- **I2C Bus (SDA=21, SCL=22):** 
  - MAX30102 (Đo nhịp tim, SpO2)
  - MPU6050 (Phát hiện té ngã, đo gia tốc kế / con quay hồi chuyển)
  - MLX90614 (Đo nhiệt độ hồng ngoại)
- **GPS (TinyGPSPlus):** `RX=35`, `TX=32`
- **Dust Sensor (Bụi mịn):** `VO=33`, `LED=14`
- **Ngoại vi khác:**
  - Nút chạm (Touch Sensor): `PIN=15` (Chống dội 20ms-600ms)
  - Còi báo (Buzzer): `PIN=13`

## 3. Bản đồ Module
Giao thức cụ thể ở từng tính năng, AI hãy tra cứu các file sau nằm trong thư mục `docs/`:

1. **[module_ai_fall_detection.md](./docs/module_ai_fall_detection.md)**: Logic phát hiện té ngã 3 lớp, cửa sổ trượt AI và cấu hình Edge Impulse.
2. **[module_health_sensors.md](./docs/module_health_sensors.md)**: Logics MAX30102, bộ lọc nhịp tim Median Filter, cảnh báo sốt & môi trường, cơ chế Sensor Fusion.
3. **[module_ui_and_cloud.md](./docs/module_ui_and_cloud.md)**: Máy trạng thái giao diện TFT, mã QR định danh, kết nối mạng WiFi/mDNS và đồng bộ Firebase Realtime DB.

## 4. Biên dịch & Vận hành
- Platform: **PlatformIO** (`platformio.ini`)
- Chip ESP32 Dev kit, framework Arduino.
- Phân vùng: `min_spiffs.csv` (để lấy dung lượng nhồi mô hình Edge Impulse AI vào flash).
- Tối ưu hóa: Bật cờ `-O3` để suy luận AI nhanh tuyệt đối.
- Cập nhật từ xa: Hỗ trợ nạp code OTA (`upload_protocol = espota`).
