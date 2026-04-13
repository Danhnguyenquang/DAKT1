# Module Giao diện (Màn hình) và Kám Nối (Cloud Firebase)

> **File liên kết:** `src/main.cpp`
> **Công nghệ:** TFT ST7789, Firebase RTDB.

## 1. Máy trạng thái Giao Diện TFT
Màn hình được cấp lệnh vẽ lại `updateTFT()` mỗi khi chạm nút ngắn. Vòng lặp giao diện (`userScreenIndex`):
1. **Screen 0:** `drawStaticUI()` (Hiển thị chỉ số nhịp tim, oxy, nhiệt độ, bụi, trạng thái thiết bị thời gian thực).
2. **Screen 1:** Kích hoạt nếu tay đặt vào ô nhịp tim. Hiện thanh `DANG DO TIM MACH` loading.
3. **Screen 2:** `drawWeatherScreenStatic()` / `drawWeatherAnimationFrame()` (Gọi API OpenWeatherMap).
4. **Screen 3:** `drawAIDoctorScreen()` (Hiện lời khuyên bằng text đổ về từ Android App quét theo định tuyến).
5. **Screen 4:** `drawQRScreen()` (Khởi tạo mã QR bằng module thư viện để sinh UUID/DeviceID quét từ App điện thoại).

## 2. Cảm ứng chạm (Touch Sensor)
- Có sử dụng bộ kích ngắt phần cứng (`touchISR`):
  - Anti-debounce delay 20ms-600ms quy cho là chạm lướt (Next Screen).
  - Tắt SOS / Alert Timer nếu giữ hơn 600ms (Long Press -> Toggle còi báo khẩn cấp hoặc Reset SOS).

## 3. Kết nối Firebase
Phần lớn thao tác kết nối được cô lập ra một RTOS Task chạy trên nhân 0 (hoặc vòng lặp độc lập ngoài Loop đồ họa) -> `TaskFirebase`.
Nhiệm vụ:
- Định tuyến lưu trữ: `/User_Health/<DeviceID>`
- Push định kỳ (History Log): Đăng `temp`, `bpm`, `spo2`, `dust`, `timestamp` lưu lịch sử vào node `HistoryLog`.
- Cập nhật tức thời (Status Updates): Cờ `SOS`, Cờ `Fall`, Cờ tọa độ báo nguy hiểm đồng bộ nhanh để Mobile App kịp nhận chuông.
- Nhận phản hồi: Đọc liên tục string `geminiAdvice` từ Firebase do App sinh ra để in lên màn hình TFT.
