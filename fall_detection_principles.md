# 🛡️ Nguyên Lý Phát Hiện Té Ngã – Hệ Thống DAKT1

> **File tham khảo:** `src/main.cpp` – ESP32 Health & Fall Detection System  
> **Cảm biến sử dụng:** MPU6050 (Gia tốc 3 trục + Con quay hồi chuyển)

---

## Tổng Quan Kiến Trúc

Hệ thống sử dụng **2 tầng hoạt động song song**, bổ sung lẫn nhau để tối đa độ tin cậy:

```
              ┌─────────────────────────────────────────┐
              │           MPU6050 (accX/Y/Z)            │
              └──────────────────┬──────────────────────┘
                                 │
              ┌──────────────────▼──────────────────────┐
              │        Tính vectorSum (|a|)             │
              │   vectorSum = √(accX² + accY² + accZ²)  │
              └──────┬──────────────────────────────────┘
                     │
     ┌───────────────┼─────────────────────┐
     ▼               ▼                     ▼
[Tầng 1: AI]  [Tầng 2A: State Machine] [Tầng 2B: Brute Force]
 Edge Impulse  Rơi tự do → Va chạm    |a| > 3.0G trực tiếp
     │               │                     │
     └───────────────┴─────────────────────┘
                      │
              isFalling = true 🚨
                      │
              ┌───────▼────────────┐
              │  Cảnh báo phối hợp │  (BPM < 50 → Ngất xỉu)
              └────────────────────┘
```

---

## Tầng 1: AI Machine Learning (Edge Impulse)

### Nguyên lý hoạt động

Mô hình AI được huấn luyện từ dataset **SisFall** – nhận diện các mẫu chuyển động phức tạp mà các công thức vật lý đơn giản không thể bắt được.

```cpp
// Nạp dữ liệu gia tốc vào mảng cửa sổ trượt
ai_features[feature_ix++] = accX;
ai_features[feature_ix++] = accY;
ai_features[feature_ix++] = accZ;

// Khi cửa sổ đầy → chạy AI
if (feature_ix >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
    run_classifier(&signal, &result, false);

    // Kích hoạt nếu nhãn "Fall" đạt ngưỡng tin cậy
    if (strcmp(result.classification[ix].label, "Fall") == 0 &&
        result.classification[ix].value > 0.75) {
        isFalling = true;
    }

    // Trượt cửa sổ 50% để tiết kiệm RAM và liên tục
    memmove(ai_features, ai_features + shift_elements, ...);
}
```

### Thông số kỹ thuật

| Thông số | Giá trị | Mô tả |
|---|---|---|
| **Dữ liệu huấn luyện** | SisFall Dataset | Dataset té ngã chuẩn quốc tế |
| **Đặc trưng đầu vào** | `accX`, `accY`, `accZ` | Gia tốc 3 trục (đơn vị: G) |
| **Tần số lấy mẫu** | `EI_CLASSIFIER_INTERVAL_MS` | Được định nghĩa bởi Edge Impulse |
| **Kỹ thuật cửa sổ** | Cửa sổ trượt (Sliding Window) | 50% overlap – không bỏ sót sự kiện |
| **Ngưỡng kích hoạt** | `> 0.75` (75%) | Độ tin cậy tối thiểu để báo động |

> [!TIP]
> Kỹ thuật **cửa sổ trượt 50%** giúp giảm nguy cơ bỏ sót sự kiện nằm ở ranh giới giữa 2 cửa sổ liên tiếp. Đây là thực hành tiêu chuẩn trong nhận diện hoạt động (Activity Recognition).

---

## Tầng 2A: State Machine – Rơi Tự Do → Va Chạm

### Nguyên lý hoạt động

Dựa trên **định luật vật lý cơ bản**: Khi cơ thể bắt đầu té ngã, trong khoảnh khắc cơ thể "lơ lửng" (rơi tự do), tổng gia tốc trên 3 trục giảm gần về 0. Chỉ vài giây sau đó, cơ thể đập xuống đất tạo ra xung gia tốc rất lớn.

```
Pha 1: Rơi tự do     Pha 2: Va chạm
|a| << 1G  ──────>   |a| >> 1G
(lơ lửng)            (đập xuống)
```

```cpp
float vectorSum = sqrt(accX*accX + accY*accY + accZ*accZ);
static unsigned long lastFreeFallTime = 0;

// BƯỚC 1: Phát hiện rơi tự do (ngưỡng nới lỏng vì thiết bị xoay)
if (vectorSum < 0.8) {
    lastFreeFallTime = millis();
}

// BƯỚC 2: Phát hiện va chạm trong vòng 1.5 giây sau rơi tự do
if (vectorSum > 1.6 && lastFreeFallTime > 0 &&
    (millis() - lastFreeFallTime < 1500)) {
    isFalling = true;
    lastFreeFallTime = 0;  // Reset để tránh kích hoạt lặp
}
```

### Thông số kỹ thuật

| Thông số | Giá trị | Lý do lựa chọn |
|---|---|---|
| **Ngưỡng rơi tự do** | `< 0.8G` | Nới lỏng từ 0.5G để chịu được lực ly tâm khi thiết bị quay |
| **Ngưỡng va chạm** | `> 1.6G` | Tương đương lực đập nhẹ đến trung bình xuống sàn |
| **Cửa sổ thời gian** | `< 1500ms` | Bao phủ cả trường hợp té ngã từ vị trí cao |

> [!NOTE]
> Ngưỡng `0.8G` (thay vì `0.5G` lý tưởng) là điều chỉnh thực nghiệm: khi người dùng xoay người hoặc tay, thiết bị đeo chịu thêm lực ly tâm, làm `vectorSum` tăng giả tạo dù đang trong pha rơi.

---

## Tầng 2B: Brute Force – Va Chạm Cực Mạnh

### Nguyên lý hoạt động

Xử lý ngoại lệ: **trượt ngã, vấp ngã** – không có pha rơi tự do vì người ngã ngay trên mặt phẳng, nhưng lực đập vẫn cực kỳ lớn.

```cpp
// Ngoại lệ: Trượt ngã / Vấp ngã → lực đập trực tiếp > 3G
if (vectorSum > 3.0) {
    isFalling = true;
}
```

### Ví dụ trường hợp bắt được

| Tình huống | State Machine | Brute Force |
|---|---|---|
| Ngã từ ghế, giường | ✅ Có pha rơi tự do | - |
| Trượt ngã trên sàn trơn | ❌ Không có pha rơi | ✅ Lực đập > 3G |
| Vấp chân, nhào về phía trước | ❌ Không có pha rơi | ✅ Lực đập > 3G |
| Ngã từ thang cao | ✅ Pha rơi dài | ✅ Lực đập rất lớn |

> [!IMPORTANT]
> Ngưỡng `3.0G` được chọn đủ cao để **tránh báo động giả** trong sinh hoạt bình thường (đi bộ, chạy bộ thường chỉ đạt ~2G), nhưng đủ thấp để bắt được các cú ngã thực sự.

---

## Tầng 3: Cảnh Báo Phối Hợp (Sensor Fusion)

### Nguyên lý hoạt động

Sau khi phát hiện té ngã, hệ thống **kết hợp đa cảm biến** để đánh giá mức độ nguy hiểm và tìm ra các tình huống có thể đe dọa tính mạng tiềm ẩn khác:

```cpp
// Phối hợp 1: Ngất xỉu sau ngã (Nguy kịch nhất!)
if (isFalling && lastBPM > 0 && lastBPM < 50)
    --> "Nguy kịch: Ngat xiu sau nga!"

// Phối hợp 2: Sốt cao li bì
if (currentTempObj > 38.5 && lastBPM > 110)
    --> "Bao dong: Sot cao li bi!"

// Phối hợp 3: Suy hô hấp do môi trường độc hại
if (currentDust > 150.0 && lastSpO2 > 0 && lastSpO2 < 92)
    --> "Bao dong: Suy ho hap do o nhiem!"
```

### Bảng cảnh báo đơn lẻ

| Cảm biến | Ngưỡng nguy hiểm | Cảnh báo |
|---|---|---|
| **Nhịp tim (BPM)** | `< 40` hoặc `> 130` | Nhịp tim bất thường |
| **SpO2** | `< 92%` | Thiếu oxy máu |
| **Nhiệt độ cơ thể** | `> 37.5°C` | Sốt |
| **Nhiệt độ cơ thể** | `< 30°C` và `> 25°C` | Hạ thân nhiệt |
| **Bụi mịn** | `> 100 µg/m³` | Không khí độc hại |

> [!WARNING]
> Cảnh báo phối hợp được ưu tiên cao hơn cảnh báo đơn lẻ. Khi có nhiều dấu hiệu nguy hiểm cùng lúc, hệ thống hiển thị tình trạng nghiêm trọng nhất trước tiên (kiểm tra `if-else if` theo thứ tự ưu tiên).

---

## Luồng Xử Lý Tổng Thể

```
loop() [Core 1 - 1ms/chu kỳ]
  │
  ├─► updateMAX30102Fast()     → BPM, SpO2
  ├─► mlx.readObjectTempC()   → Nhiệt độ cơ thể
  ├─► updateDustSensor()      → Nồng độ bụi mịn
  │
  └─► [AI BLOCK - mỗi EI_CLASSIFIER_INTERVAL_MS]
        │
        ├── mpu6050.update()
        ├── accX/Y/Z = mpu6050.getAccX/Y/Z()
        ├── vectorSum = √(accX² + accY² + accZ²)
        │
        ├── [State Machine] vectorSum < 0.8 → ghi nhớ thời điểm
        ├── [State Machine] vectorSum > 1.6 + dt < 1500ms → isFalling
        ├── [Brute Force]   vectorSum > 3.0 → isFalling
        │
        ├── [AI] nạp vào ai_features[]
        └── [AI] khi đầy → run_classifier() → "Fall" > 75% → isFalling
              │
              └── handleTouchToggle()
                    │
                    └── Sensor Fusion → isHealthAlert + healthAlertReason
```

---

## Kết Luận

| Tầng | Loại té ngã bắt được | Ưu điểm | Hạn chế |
|---|---|---|---|
| **AI (Edge Impulse)** | Mọi loại (học từ data) | Xử lý mẫu phức tạp | Cần đủ dữ liệu huấn luyện tốt |
| **State Machine** | Ngã + rơi tự do | Không cần training, hiểu được cơ học | Bỏ sót trượt ngã |
| **Brute Force** | Trượt ngã, vấp ngã | Bắt các ngoại lệ đơn giản | Có thể báo giả nếu va đập mạnh |
| **Sensor Fusion** | Biến chứng sau ngã | Đánh giá toàn diện sức khỏe | Cần đo được BPM/SpO2 trước đó |

> [!TIP]
> **Điểm mạnh khi bảo vệ đồ án**: Hệ thống sử dụng kiến trúc **redundancy** (dự phòng đa lớp). Khi một tầng có thể bỏ sót, tầng khác sẽ bù vào. Đây là nguyên tắc thiết kế hệ thống an toàn (Safety-Critical Systems) được áp dụng trong thiết bị y tế chuyên nghiệp.
