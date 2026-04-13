# Module Cảm biến Sức Khỏe sinh tồn (Health Sensors)

> **File liên kết:** `src/main.cpp` và các file thư viện/thuật toán trong gốc `MAX30105.h`, `spo2_algorithm.h`.
> **Cảm biến:** MAX30102, MLX90614, Cảm biến bụi quang học (Dust).

## 1. Cảm biến Nhịp tim & Oxy máu (MAX30102)

- Nhạy cảm với ánh sáng và chuyển động (Motion Artifacts).
- **Quy trình đo:**
  - Nhận diện ngón tay: Kiểm tra cường độ đọc `irBuffer[99] > 50000`, nếu dưới => rút tay. Tự động điều chỉnh độ sáng LED (Pulse Amplitude) `0x3F` hoặc `0x15` để bù sáng cho bề mặt da dày hoặc mỏng.
  - Thu thập vào Buffer (100 mẫu).
  - Thuật toán `maxim_heart_rate_and_oxygen_saturation` trích xuất `heartRateValue` và `spo2`.
  - Lọc nhiễu dữ liệu: Sử dụng **Median Filter** (Bộ lọc trung vị), size = 7 (`FILTER_SIZE = 7`). 
  - Khắc phục lỗi vọt lố: Loại bỏ n mẫu đầu tiên. Cần duy trì tính hợp lệ (`validSamplesCollected`) mới cập nhật chỉ số lên màn hình.

## 2. Cảm biến Nhiệt độ MLX90614
- Xử lý đơn giản qua I2C. `mlx.readObjectTempC()`.
- Cảnh báo sức khỏe tích hợp: 
  - Dưới `30 độ C` / Trên `25 độ C`: Cảnh báo sốc/Hạ thân nhiệt.
  - Trên `37.5 độ C`: Cảnh báo Sốt.

## 3. Cảm biến Bụi
- Cơ chế quét hồng ngoại ngắt xung (pulse):
  - LED bật làm sáng bụi -> Chờ `280µs` -> Đọc Analog -> Chờ `40µs` -> Tắt LED -> Chờ `9680µs`.
  - Quy đổi qua V áp thành mật độ ug/m3. Cảnh báo suy hô hấp nếu nồng độ bụi > `100.0`.

## 4. Sensor Fusion đặc biệt (Phối hợp chỉ số)
- Sốt + Nhịp tim cao (`Temp > 38.5` & `BPM > 110`): Cảnh báo sốt cao li bì.
- Môi trường + Oxy tuột (`Dust > 150` & `SpO2 < 92`): Cảnh báo suy hô hấp do ô nhiễm và sốc khói bụi.
