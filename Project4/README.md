### PROJECT 4: Filesystem minifilter 
## Mô tả về driver
- Driver cho phép thu thập thông tin về hoạt động tạo mới (CREATE), đọc (READ), ghi (WRITE), xóa (DELETE) trên một folder cụ thể do UM yêu cầu. Thông tin được gửi lên user mode program để in ra console.
- Bổ sung tính năng chặn ghi file (WRITE) đối với các file nằm trong thư mục. Khi chặn thành công, lưu một log về việc chặn ghi file và gửi lên user mode. Lưu ý: Chặn WRITE thì cũng chặn CREATE vì bản chất CREATE cũng là WRITE (CHƯA PHÁT TRIỂN).
- Bổ sung tính năng chặn xóa file (DELETE) đối với file nằm trong thư mục. Khi chặn thành công, lưu một log về việc chặn xóa file và gửi lên user mode (CHƯA PHÁT TRIỂN).


## Giải thích 
- Minifilter gửi dữ liệu tới UM thông qua một communication port (FltCreateCommunicationPort).
- UM kết nối tới port này bằng cách sử dụng tên port đã định nghĩa sẵn.
- KM to UM: Minifilter sử dụng các API như FltSendMessage để gửi thông báo hoặc yêu cầu tới UM.
- UM to KM: UM gửi các lệnh hoặc dữ liệu điều khiển thông qua  API tương ứng
## Hướng dẫn setup
Khởi tạo 1 Minifilter
```
sc create Minifilter type= filesys binPath= "PATH_TOI_DUONG_DAN_FILE_SYS"

sc start Minifilter
```

## Hướng dẫn chạy
Thực hiện chạy trên UM:
```
<File_EXE_CAN_CHAY>
```

## POC
- yêu cầu 1
![UM.exe](Image/Screenshot%202025-01-13%20214408.png "Ảnh minh họa")

- yêu cầu 2

Thử nhập dữ liệu để lưu
![TEST](Image/Screenshot%202025-01-13%20222021.png "Anh minh hoa")

Không lưu được dữ liệu
![TEST](Image/Screenshot%202025-01-13%20222201.png "Anh minh hoa")

- yêu cầu 3
![TEST](Image/Screenshot%202025-01-13%20221734.png "Anh minh hoa")