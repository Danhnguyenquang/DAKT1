# Báo Cáo Kỹ Thuật: Thuật Toán Nhận Diện Té Ngã (Fall Detection) Dựa Trên AI Nhúng

Tài liệu này hệ thống hóa các khái niệm, quy trình xử lý và lý do lựa chọn thông số cài đặt để xây dựng mô hình Nhận diện té ngã (Fall Detection) trên nền tảng vi điều khiển.

---

## PHẦN 1: QUY TRÌNH TIỀN XỬ LÝ DỮ LIỆU (DATA PREPROCESSING)

Dữ liệu thô từ bộ dữ liệu SisFall bao gồm các tệp kéo dài từ 20 đến 60 giây. Trong mỗi tệp, sự kiện té ngã chỉ xảy ra chớp nhoáng, phần lớn thời gian còn lại là các hoạt động sinh hoạt bình thường (ADL). Việc nạp toàn bộ tệp vào mô hình là không khả thi và gây nhiễu lượng lớn.

**1. Khái niệm Cửa sổ trượt (Sliding Window) và Lựa chọn Kích thước 3 Giây**
- **Cửa sổ trượt (Sliding Window)** là phương pháp trích xuất một khoảng thời gian cố định từ chuỗi dữ liệu dài để phân tích.
- **Tại sao không sử dụng 10 giây?** Nếu cửa sổ quá dài (ví dụ 10 giây), sự kiện té ngã sẽ bị áp đảo bởi dữ liệu đi lại bình thường. Điều này khiến mô hình không học được đặc trưng của cú ngã và thường xuyên tạo ra cảnh báo sai (False Positive).
- **Tại sao không sử dụng 1 giây?** Một chu kỳ ngã vật lý gồm 3 giai đoạn: Rơi tự do $\rightarrow$ Va đập (đỉnh gia tốc) $\rightarrow$ Nằm bất động (ổn định). Nếu chỉ lấy 1 giây, ta có thể đánh mất tín hiệu "nằm bất động" sau cú ngã. Trạng thái bất động này là yếu tố cốt lõi giúp phân biệt hành động té ngã và hành động giậm nhảy (nhảy xong vẫn di chuyển tiếp).
- **Lý do chọn 3 giây:** Cửa sổ 3 giây (1.5 giây trước va đập và 1.5 giây sau va đập) bao hàm trọn vẹn toàn bộ 3 giai đoạn của chu kỳ té ngã, cung cấp dữ liệu nguyên vẹn nhất cho mô hình phân loại.

**2. Thuật toán trích xuất mốc va đập (Peak Extraction)**
Để bộ xử lý biết ngã ở giây thứ mấy mà cắt cửa sổ 3 giây, chương trình Python sử dụng Độ lớn Vector tổng hợp của gia tốc kế 3 trục X, Y, Z. Công thức là: **$Magnitude = \sqrt{x^2 + y^2 + z^2}$**. Trạng thái đập người xuống đất luôn tạo ra lực gia tốc tức thời lớn nhất.  Điểm thời gian có giá trị Magnitude cao nhất sẽ được chọn làm mốc trung tâm (tâm chấn) để trích xuất cửa sổ 3 giây.

---

## PHẦN 2: THIẾT KẾ ĐẦU VÀO VÀ BỘ XỬ LÝ KHỐI (CREATE IMPULSE)

Khởi tạo luồng xử lý tín hiệu (Impulse) là bước định hướng cách làm sạch và biểu diễn dữ liệu cảm biến trước khi máy học.

**Tại sao chọn phương pháp Phân tích Quang Phổ (Spectral Analysis) thay vì Dữ liệu thô (Raw Data)?**
- **Chuỗi dữ liệu thời gian (Time-series data)** chứa biểu đồ biên thiên dài 600 mẫu độc lập. Đưa Dữ liệu thô (Raw Data) nguyên bản bắt buộc mô hình phải xử lý sự phức tạp của nhiễu nền, tốn rất nhiều tài nguyên tính toán đối với vi điều khiển.
- **Phân tích Quang Phổ (Spectral Analysis)** thực hiện chuyển đổi biểu đồ dữ liệu từ Miền Thời Gian (Sự thay đổi theo thời gian) sang Miền Tần Số (Mức độ phân bổ năng lượng ở từng tần số dao động). Các hành động ngã thể hiện sự phân bổ năng lượng rất mạnh và khác biệt hoàn toàn so với các hoạt động sinh hoạt, giúp mô hình phân loại dễ dàng nhận dạng chính xác và ít tốn phép tính máy học hơn. 

---

## PHẦN 3: BỘ LỌC VÀ TRÍCH XUẤT ĐẶC TRƯNG TẦN SỐ (SPECTRAL FEATURES)

Mục đích của phần này là áp dụng toán học để trích xuất các **Đặc trưng (Features)** – tức là các đại lượng tiêu biểu nhất đại diện cho chuỗi chuyển động.

**1. Bộ lọc (Filter): Tại sao thiết lập "Low-pass filter" ở dải cắt 5.0 Hz?**
- Con người khi di chuyển, vung tay hoặc té ngã có tần số dao động vật lý tương đối chậm, tập trung chủ yếu ở băng tần dưới 5 Hz (Hertz). Trong khi đó, cảm biến gia tốc thường bị nhiễu bởi các sóng điện từ xung quanh hoặc rung động của linh kiện cơ khí, sinh ra các dao động có tần số rất cao (từ 20Hz trở lên).
- Việc thiết lập **Low-pass filter (Bộ lọc thông thấp)** đóng vai trò chỉ cho phép các tần số dưới 5Hz được đi qua và loại bỏ tất cả các tần số rác trên 5Hz. Áp dụng thông số này giúp đường tín hiệu trở nên rõ ràng và chỉ chứa các cử động thực tế của con người.

**2. Phân tích (Analysis): Chọn "FFT" thay vì "Wavelet"**
- **FFT (Fast Fourier Transform - Biến đổi Fourier Nhanh)** là hàm biến đổi tín hiệu sang biểu đồ phổ năng lượng. Ở các hệ thống thiết bị IoT với RAM hạn chế, FFT cung cấp khả năng tách biệt năng lượng va đập cực kỳ hiệu quả mà vẫn bảo đảm độ nhẹ tính toán. 
- Ngược lại, thuật toán **Wavelet** tuy tối ưu hơn nhờ bảo tồn thông tin thời gian thực, nhưng lại phát sinh khối lượng dữ liệu khổng lồ khiến cho việc cấp phát cấu hình bộ nhớ của vi điều khiển (RAM) có thể bị quá tải nghiêm trọng.

**3. Tại sao chọn độ phân giải FFT Length là 64?**
- **FFT Length** quyết định độ chi tiết (số điểm chia) của dải biểu đồ tần số. Lựa chọn mức 16 mặc định gây ra phân giải thấp, khiến các dải năng lượng dao động của hành động bình thường dễ bị hòa lẫn với tín hiệu té ngã. Bằng cách nâng số điểm chia lên 64 cho cửa sổ 600 số liệu đầu vào, thông tin tần số được cắt nhỏ chi tiết hơn, hỗ trợ mô hình nhận dạng các tín hiệu va đập cực ngắn mà không bị mờ nhòe.

---

## PHẦN 4: KIẾN TRÚC MẠNG NƠ-RON (NETWORKS & CLASSIFIER)

Quá trình "Phân tích phổ" (Spectral Analysis) đã cô đọng 600 mẫu dữ liệu rung động thành **48 con số đặc trưng tĩnh (Features)** vô hướng, đại diện cho bản chất năng lượng của sóng.

**1. Lựa chọn Mạng kết nối đầy đủ (Dense Network) và loại bỏ 1D-CNN:**
- **Mạng tích chập 1 chiều (1D-CNN)** là kiến trúc mô hình chuyên dụng cho việc lướt qua để "đọc đường hướng tuyến tính" của dải đồ thị có tính thời gian. Do tín hiệu đã bị khối Spectral phân giải mất yếu tố thời gian và nén thành 48 chỉ số tĩnh, đưa ma trận đầu vào này tới 1D-CNN sẽ lập tức gây ra lỗi lệch định dạng không gian (Shape Mismatch).
- **Mạng Dense (Fully Connected Network)** là kiến trúc các nơ-ron kết nối đan chéo thành một mạng lưới hệ số tổng hợp. Mạng lưới này tính toán chéo xác suất mức quan trọng độc lập của 48 hệ số tĩnh kia nhằm đối chiếu ra xu thế xác suất tốt nhất. Đây là mô hình chuẩn mực đồng bộ bắt buộc đối với phương pháp xử lý dữ liệu đặc trưng tĩnh (Features).

**2. Tốc độ học (Learning Rate) = 0.005:**
- Thuật toán tối ưu Gradient Descent sẽ điều chỉnh các trọng số (Weights) từng bước nhằm đạt sai số phân loại thấp nhất. Quãng cách của mỗi bước điều chỉnh này gọi là "Tốc độ học". Nếu thiết lập tốc độ học (Learning Rate) quá lớn (ví dụ 0.1), thuật toán hội tụ sẽ nhảy vọt lố qua ngưỡng số liệu mốc và gây bất định. Nếu tham số nhảy quá nhỏ, khả năng đáp ứng hội tụ chậm và mô hình tốn quá nhiều sức mạnh mới kết thúc huấn luyện. Do đó chuẩn thông số 0.005 mang lại tính đáp ứng cân bằng.

**3. Thiết lập Chu kỳ học (Epochs) = 50:**
- **Epochs** là một chu kỳ hoàn chỉnh khi mạng AI học toàn bộ bài toán dữ liệu mẫu một lần. Huấn luyện giới hạn trong thời gian học thấp (ví dụ: 10 Epochs) khiến mạng chưa tối ưu đủ sai số (hiện tượng **Underfitting - Chưa khớp thuật toán**). Huấn luyện chu kỳ quá cao, mạng sẽ ghi nhớ chi tiết đến rập khuôn tuyệt đối cả cấu trúc của dữ liệu mẫu (hiện tượng **Overfitting - Quá khớp / Học vẹt**), dẫn đến suy giảm năng lực nhận diện trên thực tế. Thông số 50 Epochs là chu kỳ hợp lý đảm bảo tối ưu hóa đường biểu diễn Loss Function tiêu chuẩn. 

**4. Kích thước tập kiểm chuẩn (Validation Size) = 20%:**
- Kỹ thuật phân tách ngẫu nhiên dữ liệu **Validation Size 20%** có mục tiêu loại trừ khả năng thuật toán bị "Học Vẹt" (Overfitting). 
- Hệ thống chia kho mẫu làm 2 phần độc lập: 80% số liệu dùng làm "Tập huấn luyện" (Training Set), và khóa lại 20% "Tập kiểm tra" (Validation Set) mà mô hình không được tiếp cận. Sau khi kết thúc huấn luyện mảng Train, mô hình phân tích bài thi với mảng Test. Điểm số tương thích Accuracy sẽ biểu thị chuẩn xác khả năng phản hồi thích ứng và nhận diện môi trường ngoài thực tiễn của mô hình.

---

## PHẦN 5: TRIỂN KHAI VÀ TỐI ƯU HÓA MÃ NHÚNG TẠI ESP32 (DEPLOYMENT)

Với đặc thù của mạch vi điều khiển MCU như ESP32 có tài nguyên xử lý vô cùng thấp (RAM thường ở mức 520 KB), việc đưa AI máy chủ xuống mạch yêu cầu công tác tiết giảm phần mềm.

**1. Lượng tử hóa mô hình (Quantization - Cơ chế ép về INT8):**
Do vi xử lý điện tử rất hạn chế về phép tính thập phân số lượng lớn (Float64 / Float32), hệ thống tiến hành quá trình **Lượng tử hóa (Quantization)** để nén rút toàn bộ các chỉ số của cấu trúc nơ-ron về thang điểm số nguyên cơ bản kiểu 8-bit (INT8). Phép nén này kéo giảm dung lượng khối AI xuống chỉ còn 1/4 so với ban đầu, đẩy nhanh xử lý tốc độ truyền nhưng bù lại hệ thống có mức sai lệch độ chính xác phân loại nằm ở biên độ cho phép.

**2. Trình biên dịch (EON Compiler) chuyên sâu:**
- Mặc định, quy trình của TensorFlow Lite vi điều khiển cần tích hợp sẵn môi trường Trình biên dịch thực thi (**Interpreter**) để dịch thuật xử lý và cấp bộ nhớ động ngốn rất nhiều RAM của mạch.
- Lệnh chức năng chọn ô **Enable EON Compiler** có ý nghĩa thực thi Dịch Trước Hệ Cấu Trúc trên cụm mây chủ (AoT). Khối Lưới Keras AI cùng hàm toán học được xây dựng chuyển đổi tự động thành mã nguồn chuẩn **C/C++ nguyên thủy** theo dạng mã hóa tĩnh. Do loại bỏ trình xử lý dịch ảo phía thiết bị nhúng, năng lực vận hành trực tiếp xử lý giúp giải phóng hiệu quả tài nguyên lên đến **hiệu suất 50% RAM** tiết kiệm sử dụng.

---

## PHẦN 6: HOẠT TRÌNH HỆ THỐNG VÀ CẤU TRÚC GÓI THƯ VIỆN C++

Khi biên dịch hệ thống xuất gói ra vi điều khiển, nền tảng xuất cho người dùng kho lưu trữ nén mang 3 cụm phân vùng cấu trúc file tích hợp sao chép vào `lib/healthycare_inferencing/` tại dự án.  

### Cấu Trúc Khối File Dự Án C++ (SDK Structure)
**1. Thư mục `edge-impulse-sdk` (Lõi Cốt Phần Mềm Phát Triển):**
Chứa cụm tổ hợp các hàm lập trình thuật toán cố định liên kết của hệ sinh thái Edge Impulse. Trong đó cung cấp các khuôn mã tính nền tảng (Ví dụ: hàm toán trị số mảng FFT, chu trình tính bình phương căn của Filter, cấu trúc giao thức học máy thu gọn...). Thư mục này giữ vai trò "Cỗ máy tính toán".

**2. Thư mục `model-parameters` (Thông số Ma trận Hệ Điều Quản):**
Lưu trữ cụm hằng số thông số thiết lập của chúng ta (File biến). Nghĩa là lưu toàn bộ tham chiếu kích thước như: Tốc độ lưới 200Hz, cửa số 3000ms, và mốc cắt Low-pass 5 Hz. Thư mục này giữ vai trò "Bản nháp thiết kế thông số" để truyền lệnh đầu vào thông số bắt cỗ máy vận hành theo quy cách riêng.

**3. Thư mục `tflite-model` (Bộ Não Cấu Trúc Tích Hợp Học Phân Theo):**
Nhóm chứa file tệp lưu khối Lưới Trọng Số (Weights) đã chốt chuẩn INT8, đóng vai trò như khối lưu trữ kết quả phân loại nơ-ron học máy của bài toán té ngã để phán đoán nhị nguyên (Nhận diện Đích).

### Cơ chế vận hành của mã nguồn Nhúng thời gian thực (Runtime Execution Flow)

Việc nhận diện té ngã ngoài thực tế không phải là việc đọc một tập tin văn bản, mà là một quy trình lặp lại liên tục. Khi khởi chạy trên vi mạch ESP32, tệp lệnh do người dùng lập trình (`main.cpp`) sẽ tương tác trực tiếp với bộ thư viện AI theo một lộ trình 5 bước cực kỳ tuần tự:

**Bước 1: Thu thập bộ đệm dữ liệu (Data Buffering)**
Vi điều khiển ESP32 bắt đầu giao tiếp với cảm biến vật lý (ví dụ gia tốc kế MPU) với tốc độ lấy mẫu là 200 Hz. Cứ mỗi mili-giây, `main.cpp` lấy một giá trị gia tốc và lưu vào một mảng chứa dữ liệu tạm thời (Buffer Array). ESP32 sẽ chờ cho đến khi mảng này thu thập đủ chính xác 600 số liệu thô (tương ứng với thời lượng 3 giây).

**Bước 2: Triệu gọi thư viện AI (Calling Classifier)**
Ngay khi mảng chứa đủ 600 số liệu, tệp `main.cpp` lập tức đóng gói mảng này và truyền vào một hàm xử lý phân loại lõi mang tên `run_classifier()`. Hàm này do thư viện phần mềm `edge-impulse-sdk` cung cấp để bắt đầu đưa dữ liệu vào đường ống phân tích.

**Bước 3: Xử lý Kỹ thuật số nền (DSP Inferencing)**
Khi mảng số thô đã lọt vào cấu trúc `edge-impulse-sdk`, hệ thống sẽ đọc lại cấu trúc thiết lập từ thông số tại tệp `model-parameters`. Từ đó, nó tiến hành chạy lưới Lọc thông thấp (Low-pass Filter 5Hz) để xóa sạch nhiễu cảm biến, tiếp đó tự động kích hoạt hàm toán học FFT để ép mảng dữ liệu 600 số thô khổng lồ ấy lại thành đúng **48 con số đặc trưng cốt lõi (48 Features)**.

**Bước 4: Suy luận Mạng Nơ-ron (Neural Network Inference)**
Thư viện `edge-impulse-sdk` tiếp tục chuyển nguyên vẹn 48 Đặc trưng vừa kết xuất được đẩy thẳng vào ma trận nằm ở cụm thư mục `tflite-model`. Tại đây, mô hình mạng học máy (với các tỷ lệ Trọng số đã học tĩnh) sẽ thực thi phép nhân đối chiếu để tính ra tỷ lệ Xác Suất của hành vi (Tính xem hành vi này là ngã hay đi bộ).

**Bước 5: Trả kết quả và Phản hồi (Output & Response)**
Kết thúc phép tính chéo, hàm `run_classifier()` hoàn thành và gửi lại một tệp kết quả. Tại màn hình `main.cpp`, chương trình tách dữ liệu nhận được tỷ lệ phần trăm (Ví dụ: Nhãn `Té_ngã` = 0.95, Nhãn `Bình_thường` = 0.05). Nhờ mốc dữ liệu 0.95 này, hàm điều kiện `If Probability > 0.8` được kích hoạt khiến mạch ESP32 nháy đèn LED đỏ, phát loa hú còi và gửi tín hiệu cảnh báo té ngã. Toàn bộ 5 bước trên ngốn thời gian xử lý vỏn vẹn xấp xỉ vài chục mili-giây.
