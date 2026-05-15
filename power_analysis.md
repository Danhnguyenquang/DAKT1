# ⚡ Phân Tích Công Suất & Thời Gian Sử Dụng Pin – DAKT1

> **Pin:** Li-Ion 5000mAh / 3.7V  
> **Module tăng áp:** XL6009 (3.7V → 5V, hiệu suất **94%**)  
> **Module hạ áp:** ASM1117 (5V → 3.3V, **800mA max**, hiệu suất tuyến tính 66%)

---

## Sơ Đồ Cấp Nguồn

```
Pin Li-Ion 3.7V (5000mAh)
        │
        ▼
  ┌─────────────┐
  │  XL6009     │  ← Boost converter, η = 94%
  │  3.7V → 5V  │
  └──────┬──────┘
         │ Rail 5V
         │
         ├──────────────────────────┬──────────────────────────┐
         │                          │                          │
         ▼                          ▼                          ▼
  [ESP32 DevKit]             [GPS Module]             [Cảm biến bụi]
   VIN 5V → LDO nội          ~50 mA                   GP2Y1010
   bộ 3.3V (η=66%)                                    ~6 mA (avg)
   Chip: ~240 mA
         │
         └──────────────────────────────────┐
                                            ▼
                                     ┌─────────────┐
                                     │   ASM1117   │  ← LDO, η = 66%
                                     │  5V → 3.3V  │
                                     └──────┬──────┘
                                            │ Rail 3.3V
                                            │
                    ┌───────────┬───────────┼───────────┬──────────────┐
                    ▼           ▼           ▼           ▼              ▼
               MAX30105    MPU6050     MLX90614    TFT ST7789       Buzzer
                 1 mA        4 mA       2.5 mA      60 mA        25 mA (*)
```
> (*) Buzzer chỉ kêu khi có cảnh báo (SOS / Té ngã / Sức khỏe bất thường)

---

## Bảng Dòng Điện Từng Linh Kiện

### Nhóm 5V (trực tiếp từ XL6009)

| Linh kiện | Dòng tiêu thụ | Ghi chú |
|---|---|---|
| ESP32 (qua LDO nội bộ DevKit) | **240 mA** | WiFi active + chạy AI model liên tục |
| GPS Module (NEO-6M tương đương) | **50 mA** | Chạy liên tục để lấy tọa độ & giờ |
| Cảm biến bụi (GP2Y1010) | **6 mA** | LED pulse 280µs/10ms = duty 28% |

### Nhóm 3.3V (qua ASM1117)

| Linh kiện | Dòng tiêu thụ | Ghi chú |
|---|---|---|
| MAX30105 (SpO2 + nhịp tim) | **1 mA** | Đo liên tục khi có ngón tay |
| MPU6050 (Gia tốc / AI input) | **4 mA** | Cập nhật mỗi 5ms cho AI |
| MLX90614 (Nhiệt độ hồng ngoại) | **2.5 mA** | Đọc mỗi 500ms |
| TFT ST7789 (240×280) | **60 mA** | Màn hình luôn bật, không dimming |
| Buzzer (khi báo động) | **25 mA** | Chỉ tính khi có cảnh báo |

---

## Tính Dòng Tải Trên Rail 5V

> Vì ASM1117 là LDO tuyến tính: **I_vào ≈ I_ra** (phần chênh lệch điện áp biến thành nhiệt)

| Chế độ | Nhóm 5V trực tiếp | ASM1117 kéo từ 5V | **Tổng rail 5V** |
|---|---|---|---|
| Bình thường (không báo) | 296 mA | 67.5 mA | **363.5 mA** |
| Cảnh báo (buzzer kêu) | 296 mA | 92.5 mA | **388.5 mA** |

---

## Tính Dòng Rút Từ Pin (3.7V)

**Công thức áp dụng:**

```
I_pin = P_5V_rail / (V_pin × η_XL6009)
      = (5V × I_5V_total) / (3.7V × 0.94)
```

| Chế độ | Công suất rail 5V | Dòng rút từ pin |
|---|---|---|
| Bình thường | 5 × 363.5 = **1,817 mW** | 1817 / (3.7 × 0.94) = **~523 mA** |
| Cảnh báo | 5 × 388.5 = **1,942 mW** | 1942 / (3.7 × 0.94) = **~558 mA** |

---

## Thời Gian Sử Dụng Thực Tế

> **Pin Li-Ion chỉ dùng được ~85% dung lượng** an toàn  
> (XL6009 cần tối thiểu 3V đầu vào – pin không xả cạn về 0V)

```
Dung lượng khả dụng = 5000 mAh × 85% = 4,250 mAh
```

### Kết quả

| Chế độ | Dòng từ pin | Thời gian |
|---|---|---|
| **Bình thường** (không báo động) | 523 mA | **4250 ÷ 523 ≈ 8.1 giờ** |
| **Cảnh báo liên tục** (buzzer kêu mãi) | 558 mA | **4250 ÷ 558 ≈ 7.6 giờ** |

> [!IMPORTANT]
> **Kết luận: Pin 5000mAh sử dụng được khoảng 7.5 – 8 tiếng** với sơ đồ nguồn XL6009 + ASM1117 như trên.

---

## Phân Tích Nhiệt Tỏa Ra

| Module | Công thức | Nhiệt tỏa |
|---|---|---|
| **LDO nội bộ ESP32 DevKit** | (5 - 3.3)V × 240mA | **0.41 W** ⚠️ |
| **ASM1117** | (5 - 3.3)V × 67.5mA | **0.11 W** ✅ |
| **XL6009** | P_in × (1 - 0.94) | **0.12 W** ✅ |

> [!WARNING]
> LDO nội bộ trong ESP32 DevKit tỏa **0.41W** liên tục. Nếu đóng kín hộp, cần đục lỗ thông gió hoặc dán miếng tản nhiệt nhỏ lên IC AMS1117 trên bo mạch.

> [!NOTE]
> ASM1117 của bạn chịu được **800mA** nhưng chỉ phải chịu ~93mA max → **Hoàn toàn an toàn**, không cần tản nhiệt.

---

## Gợi Ý Tối Ưu (Nếu Muốn Dùng Lâu Hơn)

| Giải pháp | Tiết kiệm thêm | Ghi chú |
|---|---|---|
| Cấp thẳng 3.3V từ XL6009 cho ESP32 | ~0.41W | Bỏ LDO nội bộ DevKit, tăng ~30 phút |
| Tắt màn hình TFT sau 30 giây không chạm | ~60mA | Tiết kiệm ~1 tiếng nếu ít dùng màn hình |
| Giảm tần số AI sampling | ~5-10mA | Giảm từ 5ms lên 10ms mỗi mẫu |

---

## Thông Số Module Tham Khảo

| Module | Thông số quan trọng |
|---|---|
| **XL6009** | Vào: 3~32V / Ra: 5~35V / Dòng: 4A max / η: **94%** |
| **ASM1117** | Vào: 4.5~7V / Ra: 3.3V / Dòng: **800mA max** |
| **Pin Li-Ion** | 3.7V danh định / 5000mAh / Xả tối thiểu ~3.0V |
