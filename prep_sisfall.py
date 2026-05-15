import os
import glob
import math
import csv

# ================= CONFIGURATION =================
# Đường dẫn tới thư mục chứa dataset SisFall (đã giải nén)
SISFALL_DIR = "SisFall_dataset" 

# Thư mục chứa kết quả CSV để upload lên Edge Impulse
OUTPUT_DIR = "EdgeImpulse_CSV"

# Tần số lấy mẫu gốc của SisFall
SAMPLE_RATE = 200 # Hz

# Độ dài tính bằng giây cho mỗi sample upload lên (3 giây = 600 samples)
# Đây là cửa sổ thời gian tuyệt vời nhất cho Fall Detection. 
WINDOW_SECONDS = 3  

# Các thông số chuyển đổi raw value
# Gia tốc (ADXL345): +-16g, 13-bit -> (32 / 8192) * 9.81 = 0.03832 m/s^2 per LSB
ACCEL_SCALE = (32.0 / 8192.0) * 9.81
# Vận tốc góc (ITG3200): +-2000 deg/s, 16-bit -> 1/14.375 = 0.06956 deg/s per LSB
GYRO_SCALE = 1.0 / 14.375
# =================================================

def parse_sisfall_file(filepath):
    """
    Đọc file SisFall thô, trả về mảng dữ liệu đã convert sang m/s^2 và deg/s
    """
    data = [] # [accX, accY, accZ, gyroX, gyroY, gyroZ]
    with open(filepath, 'r') as f:
        for line in f:
            try:
                # Dữ liệu của SisFall cách nhau bằng dấu phẩy
                parts = line.strip().strip(';').split(',')
                if len(parts) >= 6:
                    accX = int(parts[0]) * ACCEL_SCALE
                    accY = int(parts[1]) * ACCEL_SCALE
                    accZ = int(parts[2]) * ACCEL_SCALE
                    gyroX = int(parts[3]) * GYRO_SCALE
                    gyroY = int(parts[4]) * GYRO_SCALE
                    gyroZ = int(parts[5]) * GYRO_SCALE
                    data.append([accX, accY, accZ, gyroX, gyroY, gyroZ])
            except ValueError:
                continue
    return data

def find_fall_peak_index(data):
    """
    Tìm đỉnh cao nhất của gia tốc (lực đập mạnh nhất khi ngã)
    """
    max_acc = 0
    peak_idx = 0
    for i, row in enumerate(data):
        # Tính gia tốc tổng (magnitude) theo định lý Pytago
        mag = math.sqrt(row[0]**2 + row[1]**2 + row[2]**2)
        if mag > max_acc:
            max_acc = mag
            peak_idx = i
    return peak_idx

def save_to_csv(data_window, filename):
    """
    Lưu dữ liệu thành định dạng CSV chuẩn Edge Impulse
    """
    filepath = os.path.join(OUTPUT_DIR, filename)
    with open(filepath, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        # Header bắt buộc của Edge Impulse
        writer.writerow(['timestamp', 'accX', 'accY', 'accZ', 'gyroX', 'gyroY', 'gyroZ'])
        
        # Edge Impulse tính timestamp bằng mili-giây (ms)
        ts_interval = 1000.0 / SAMPLE_RATE
        for i, row in enumerate(data_window):
            # Format row: [timestamp, accX, accY, accZ, gyroX, gyroY, gyroZ]
            # Giới hạn 4 chữ số thập phân cho gọn file
            formatted_row = [int(i * ts_interval)] + [round(val, 4) for val in row]
            writer.writerow(formatted_row)

def main():
    print(f"==================================================")
    print(f"      SISFALL TO EDGE IMPULSE CSV CONVERTER       ")
    print(f"==================================================")
    
    if not os.path.exists(SISFALL_DIR):
        print(f"[!] LỖI: Không tìm thấy thư mục: {SISFALL_DIR}")
        print(f"    Bạn cần tạo thư mục '{SISFALL_DIR}', sau đó tải dataset")
        print(f"    và giải nén toàn bộ các thư mục SA01, SE01,... vào trong đó.")
        return

    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)
        print(f"[*] Đã tạo thư mục đầu ra: {OUTPUT_DIR}")
        
    print(f"[*] Bắt đầu xử lý dữ liệu từ {SISFALL_DIR}...")
    window_samples = WINDOW_SECONDS * SAMPLE_RATE
    half_window = window_samples // 2
    
    # Tìm tất cả các file .txt trong thư mục dataset
    search_path = os.path.join(SISFALL_DIR, "**", "*.txt")
    files = glob.glob(search_path, recursive=True)
    
    if not files:
        print(f"[!] LỖI: Không tìm thấy file .txt nào trong {SISFALL_DIR}.")
        print(f"    Hãy chắc chắn giải nén đúng cấu trúc.")
        return
        
    fall_cnt = 0
    adl_cnt = 0
    
    for filepath in files:
        basename = os.path.basename(filepath)
        # Bỏ qua những file không bắt đầu bằng F (Fall) hoặc D (ADL)
        if not basename.startswith('F') and not basename.startswith('D'):
            continue 
            
        data = parse_sisfall_file(filepath)
        if len(data) < window_samples:
            continue # Bỏ qua báo cáo nếu file quá ngắn
            
        is_fall = basename.startswith('F')
        
        if is_fall:
            # Nếu là file Ngã: Tìm đỉnh va chạm và cắt nửa trước, nửa sau
            peak_idx = find_fall_peak_index(data)
            
            start_idx = peak_idx - half_window
            end_idx = peak_idx + half_window
            
            # Cân chỉnh window nếu ngã ở qua sát đầu hoặc sát cuối file (hiếm gặp)
            if start_idx < 0:
                start_idx = 0
                end_idx = window_samples
            if end_idx > len(data):
                end_idx = len(data)
                start_idx = end_idx - window_samples
                
            window_data = data[start_idx:end_idx]
            
            # Edge Impulse dựa vào tiền tố file để phân loại class
            out_name = f"fall.{basename.replace('.txt', '')}.csv"
            save_to_csv(window_data, out_name)
            fall_cnt += 1
            
        else:
            # Nếu là file ADL (Hoạt động bình thường): Cắt ngẫu nhiên 1 đoạn ở giữa
            mid_idx = len(data) // 2
            start_idx = mid_idx - half_window
            end_idx = mid_idx + half_window
            
            window_data = data[start_idx:end_idx]
            out_name = f"adl.{basename.replace('.txt', '')}.csv"
            save_to_csv(window_data, out_name)
            adl_cnt += 1
            
        print(f"Đã xử lý: {basename} ({'Fall' if is_fall else 'ADL'})", end='\r')
        
    print(f"\n\n[=] HOÀN THÀNH!")
    print(f"    [+] {fall_cnt} file cấu hình label 'fall'")
    print(f"    [+] {adl_cnt} file cấu hình label 'adl'")
    print(f"    -> Đã xuất ra thành công tại: {OUTPUT_DIR}/")
    print(f"\n[?] BƯỚC TIẾP THEO:")
    print(f"    1. Mở trang web Edge Impulse > Data Acquisition > click nút 'Upload Data'.")
    print(f"    2. Kéo thả toàn bộ các file CSV trong {OUTPUT_DIR}/ vào khung tải lên.")
    print(f"    3. Đảm bảo tuỳ chọn 'Infer class from filename' được tích.")

if __name__ == '__main__':
    main()
