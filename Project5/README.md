### PROJECT 5: Simple Firewall 
## Mô tả về driver
-  Driver sử dụng Windows Filtering Platform để ghi lại thông tin kết nối internet (TCP/IP, IPV4) vào/ra thiết bị và trả về cho chương trình trên usermode.
-  Driver cho phép nhận xuống một IP (IPV4) từ chương trình usermode và chặn mọi kết nối từ/đến IP này. Khi chặn được, gửi thêm một log cho chương trình trên user mode (Chưa hoàn thành)
-  Driver chặn kết nối internet từ/đến một chương trình có đường dẫn chỉ định bởi usermode. Khi chặn được, gửi thêm một log cho chương trình trên user mode (Chưa hoàn thành)

## Giải thích 
- (giải thích chi tiết luồng hoạt động của driver và UM program tại đây)
Chú ý làm nổi bật tương tác giữa WFP và system
Giải thích hình thức driver lựa chọn (UM hay KM driver, tại sao)
Giải thích về các layer đã hook, các filter và callout đã hook và layer và lý do lựa chọn hook filter/callout vào layer này

- Sử dụng Kernel-mode Driver (KM Driver):
    + Có khả năng chặn các gói tin mạng (network packets) ở mức thấp
    + Cho phép hook các layer của WFP

- Luồng hoạt động:
    1. Khởi tạo Driver
    2. Hook các layer của WFP
    3. Thực hiện giao tiếp giữa UM và KM.

- Các layer đã hook:
    1. Layer FWPS_LAYER_ALE_AUTH_CONNECT_V4 là Application Layer Enforcement (ALE) Auth Connect layer, được sử dụng để xử lý các kết nối TCP/UDP trước khi chúng được thiết lập.

## Hướng dẫn setup
- Thực hiện Build UM tại BlockProcess
- Thực hiện Build KM tại MyDriver1

## Hướng dẫn chạy
```
sc.exe create procnetfilter type= kernel binPath= <path_to_sys_file>
sc.exe start procnetfilter
BlockProcess.exe
```

## POC
- Tính năng 1
![UM.exe](Images/Screenshot%202025-01-28%20174443.png "Ảnh minh họa")
![UM.exe](Images/Screenshot%202025-01-28%20174623.png "Ảnh minh họa")