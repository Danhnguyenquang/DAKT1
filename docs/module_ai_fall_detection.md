# Module Phát Hiện Té Ngã (Health & Fall Detection)

> **File implementation chính:** `src/main.cpp` (vòng lặp loop và module `healthycare_inferencing.h`).
> **Cảm biến trọng tâm:** MPU6050

## 1. Nguyên lý 3 Lớp

Thiết bị không chỉ dùng AI mà kết hợp vật lý cổ điển (Heuristics) để tạo dự phòng:

- **Tầng 1 (AI - Edge Impulse):**
  - Mảng dữ liệu: `ai_features` nạp mảng `[accX, accY, accZ]` tuần tự.
  - Cửa sổ cắt trượt: Trượt 50% số mẫu (overlap) để lấy sample liên tục không đứt gãy.
  - Model: Phân loại dựa trên dữ liệu học máy (SisFall Dataset).
  - Ngưỡng: Nếu `label == "Fall"` và `score > 0.75` (75%) -> `isFalling = true`.

- **Tầng 2 (State Machine - Rơi Tự Do -> Va Chạm):**
  - Tính vector tổng: `vectorSum = sqrt(accX^2 + accY^2 + accZ^2)`
  - Rơi tự do: Nếu `< 0.8G` -> Lưu lại thời điểm rơi.
  - Va chạm sàn: Nếu `> 1.6G` xảy ra trong vòng `< 1500ms` kể từ pha rơi tự do -> `isFalling = true`.
  - *Lý do 0.8G thay vì 0.5G: Bù trừ lực ly tâm do cánh tay vung vẩy người già sinh ra.*

- **Tầng 3 (Brute Force - Vấp/Trượt):**
  - Va đập trực diện: `vectorSum > 3.0G` -> `isFalling = true`.
  - Phục vụ cho trượt chân, không có bước lơ lửng trên không trung.

## 2. Sensor Fusion (Hệ lụy Y tế)
Không chỉ báo ngã, hệ thống quét ngay trạng thái nhịp tim:
- Nếu ngã xong + Nhịp tim `lastBPM < 50` => Xác nhận bất tỉnh/ngất xỉu => Báo động Cấp cứu cao nhất.

## 3. Action khi té ngã
Khi `isFalling == true`:
- Còi báo động hú (`Buzzer = HIGH`) ngắt quãng `200ms`.
- Lập tức cờ cảnh báo được trigger để module Cloud `TaskFirebase` thấy và update JSON lên Firebase (có đính kèm location GPS).
