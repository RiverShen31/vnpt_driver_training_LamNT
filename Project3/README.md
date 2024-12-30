### PROJECT 3: PROCESS MONITOR
## Mô tả về driver
- Bài tập "Process Monitor" gồm hai phần chính: một driver ở kernel mode và một chương trình ở user mode. Driver chịu trách nhiệm thu thập thông tin về các tiến trình (process), thread và file Image trên hệ thống, bao gồm các tiến trình(process), thread và file Image được tạo mới và bị hủy, thread . Chương trình user mode nhận và xử lý dữ liệu từ driver, cho phép người dùng xem thông tin về các tiến trình, thread và file Image hoặc thực hiện một số hành động như hủy tiến trình hoặc ngăn cản tiến trình khởi động.

## Giải thích 
- Giám sát tiến trình và luồng: Sử dụng các callback như OnProcessNotify, OnThreadNotify để thu thập thông tin khi tiến trình hoặc luồng được tạo hoặc kết thúc.
- Sử dụng OnImageLoadNotify để thu thập thông tin khi một chương trình (image) được tải vào bộ nhớ.
- UM có thể yêu cầu driver trả về thông tin tiến trình, luồng thông qua IRP_MJ_READ.
- Driver nhận các lệnh điều khiển từ UM như hủy tiến trình (PID) thông qua SysMonDeviceControl.
#### Phương thức giao tiếp
- IOCTL (Input/Output Control): UM gửi lệnh điều khiển (IOCTL) cho driver để thực hiện hành động như hủy tiến trình hoặc chặn chương trình.
- IRP_MJ_READ: UM có thể đọc dữ liệu (thông tin tiến trình, luồng) từ driver.
## Hướng dẫn setup
- Thực hiện Build bằng Visual studio

## Hướng dẫn chạy
- Chạy start Driver và chạy UM.exe để giao tiếp với Driver

## POC
```
UM.exe
```
![UM.exe](Image/Screenshot%202024-12-30%20094759.png "Ảnh minh họa")

```
UM.exe -f
```
![UM.exe -f](Image/Screenshot%202024-12-30%20094906.png "Ảnh minh họa")
```
UM.exe -k <pid>
```
![UM.exe -k](Image/Screenshot%202024-12-30%20095011.png "Ảnh minh họa")
![UM.exe -k](Image/Screenshot%202024-12-30%20095112.png "Ảnh minh họa")
