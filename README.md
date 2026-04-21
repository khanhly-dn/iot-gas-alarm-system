# 🔥 IoT Gas & Fire Alarm System

<p align="center">
  <img src="https://img.shields.io/badge/Platform-ESP32-blue?style=for-the-badge&logo=espressif" />
  <img src="https://img.shields.io/badge/Sensor-MQ--2-orange?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Protocol-WiFi%20%7C%20HTTP-green?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Notification-Telegram-26A5E4?style=for-the-badge&logo=telegram" />
  <img src="https://img.shields.io/badge/License-MIT-yellow?style=for-the-badge" />
</p>

<p align="center">
  Hệ thống cảnh báo khói và gas thông minh sử dụng <strong>ESP32 + MQ-2</strong>, tích hợp <strong>Web Dashboard</strong>, <strong>OLED</strong>, <strong>Buzzer</strong> và <strong>Telegram Bot</strong>.
</p>

---

## 📌 Giới thiệu

Dự án **IoT Gas & Fire Alarm System** được phát triển nhằm xây dựng hệ thống **cảnh báo cháy và rò rỉ khí gas** theo thời gian thực.  
Hệ thống liên tục đọc giá trị từ cảm biến **MQ-2**, phân tích mức độ nguy hiểm và phản hồi ngay lập tức thông qua:
- 🌐 **Web Dashboard** hiện đại truy cập từ bất kỳ thiết bị nào
- 📟 **Màn hình OLED** hiển thị trạng thái trực tiếp
- 🔔 **Buzzer** cảnh báo âm thanh theo mức độ
- 📲 **Telegram Bot** gửi thông báo khẩn cấp tới điện thoại

---

## ⚙️ Chức năng chính

- **Phát hiện khói / gas** liên tục qua ADC (GPIO34) và DO (GPIO35)
- **3 mức cảnh báo:** `SAFE` → `WARNING` → `DANGER`
- **Buzzer thông minh** – tần suất chuông thay đổi theo mức độ nguy hiểm
- **OLED hiển thị** – thông tin trạng thái và giá trị ADC trực tiếp
- **Web Dashboard** – giám sát, test alert, im lặng buzzer, reset hệ thống
- **Telegram Bot** – gửi cảnh báo tức thì kèm hướng dẫn xử lý
- **Activity Log** – lưu lịch sử 20 sự kiện gần nhất
- **LED trạng thái** – báo hiệu trực quan ngay trên bo mạch

---

## 🧩 Sơ đồ hoạt động

<p align="center">
  <img width="700" alt="Sơ đồ hoạt động" src="https://github.com/khanhly-dn/iot-gas-alarm-system/blob/main/SD.png?raw=true" />
</p>

```
Cảm biến MQ-2 (AO+DO) → ESP32 phân tích ADC
        ↓
  < 1000  →  SAFE   → LED tắt, Buzzer tắt, OLED an toàn
  < 2500  →  WARNING → LED sáng, Buzzer chậm, OLED cảnh báo, Telegram
  ≥ 2500  →  DANGER  → LED sáng, Buzzer nhanh, OLED khẩn cấp, Telegram
        ↓
  Web Dashboard cập nhật mỗi 1 giây
```

---

## 🛠️ Phần cứng sử dụng

| Linh kiện | Chân kết nối | Mô tả |
|---|---|---|
| **ESP32** | – | Vi điều khiển chính, WiFi tích hợp |
| **Cảm biến MQ-2** | AO → GPIO34, DO → GPIO35 | Phát hiện khói, gas, LPG |
| **LED trạng thái** | GPIO2 | Báo hiệu khi có cảnh báo |
| **Buzzer** | GPIO5 | Cảnh báo âm thanh |
| **OLED SSD1306** | SDA/SCL (I²C, 0x3C) | Hiển thị trạng thái 128×64 |
| **Nguồn** | 5V USB | Cấp điện toàn bộ hệ thống |

<p align="center">
  <img width="600" alt="Linh kiện" src="https://github.com/khanhly-dn/iot-gas-alarm-system/blob/main/TB.png?raw=true" />
</p>

---

## 💻 Phần mềm & Công nghệ

- **Ngôn ngữ:** Arduino C++, HTML/CSS/JavaScript
- **Framework:** Arduino ESP32 Core
- **Thư viện:**
  - `WiFi.h` – kết nối mạng
  - `WebServer.h` – HTTP server nội bộ
  - `Adafruit_SSD1306` + `Adafruit_GFX` – điều khiển OLED
  - `UniversalTelegramBot` + `ArduinoJson` – gửi Telegram
  - `WiFiClientSecure` – kết nối HTTPS tới Telegram API
- **Giao tiếp:** HTTP REST API qua WiFi nội bộ
- **Giao diện:** Web App nhúng trực tiếp trong ESP32 (không cần server ngoài)

---

## 🌐 API Endpoints

| Endpoint | Mô tả |
|---|---|
| `GET /` | Giao diện Web Dashboard chính |
| `GET /status` | Trả về JSON: smoke, level, alert, uptime, log... |
| `GET /info` | Thông tin thiết bị: IP, SSID, RSSI |
| `GET /test` | Kích hoạt test cảnh báo DANGER |
| `GET /silence` | Tắt buzzer (giữ nguyên cảnh báo) |
| `GET /reset` | Reset toàn bộ trạng thái hệ thống |

---

## 📊 Thông số kỹ thuật

| Thông số | Giá trị |
|---|---|
| Ngưỡng WARNING | ADC ≥ 1000 |
| Ngưỡng DANGER | ADC ≥ 2500 |
| Tần suất đọc cảm biến | 500ms / lần |
| Cooldown gửi Telegram | 60 giây |
| Dung lượng log sự kiện | 20 mục gần nhất |
| Cổng WebServer | 80 (HTTP) |
| Màn hình OLED | 128×64px, I²C 0x3C |

---

## 🚀 Hướng dẫn cài đặt

**1. Cài đặt môi trường**
```bash
# Cài Arduino IDE >= 2.0
# Thêm ESP32 board vào Board Manager:
# https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

**2. Cài thư viện** (Arduino Library Manager)
```
Adafruit SSD1306
Adafruit GFX Library
UniversalTelegramBot
ArduinoJson
```

**3. Cấu hình trong code**
```cpp
#define WIFI_SSID   "Tên_WiFi_của_bạn"
#define WIFI_PASS   "Mật_khẩu_WiFi"
#define BOT_TOKEN   "Token_Telegram_Bot"
#define CHAT_ID     "Chat_ID_Telegram"
```

**4. Nạp code & chạy**
```
1. Mở file Fire_Alert_IoT_System.ino trong Arduino IDE
2. Chọn đúng board: ESP32 Dev Module
3. Nạp code qua cáp USB
4. Mở Serial Monitor (115200 baud) để xem địa chỉ IP
5. Truy cập địa chỉ IP trên trình duyệt → Web Dashboard xuất hiện
```

---

## 📷 Demo

| Web Dashboard | OLED Display |
|:---:|:---:|
| ![Demo](https://github.com/khanhly-dn/iot-gas-alarm-system/blob/main/DEMO.jpg?raw=true) | Hiển thị trực tiếp trên màn hình 128×64 |

🎬 **Video hoạt động:** *https://drive.google.com/file/d/1uQ9-96Eneh9pxS7wGY8gWtTG6LRalRix/view?usp=sharing*

---

## 🚀 Hướng phát triển

- [ ] Tích hợp **MQTT** để kết nối Home Assistant / Node-RED
- [ ] Thêm **cảm biến nhiệt độ** (DHT22) để cảnh báo kết hợp
- [ ] **OTA Update** – cập nhật firmware qua WiFi
- [ ] Hỗ trợ **nhiều zone cảm biến** trên một dashboard
- [ ] Thêm **lịch sử biểu đồ** ADC theo thời gian thực
- [ ] Tích hợp **âm thanh thông báo giọng nói** qua loa ngoài

---

## 👤 Thực hiện

**Lý Gia Khánh**  
Khoa Công nghệ Thông tin – Trường Đại học Đại Nam

---

<p align="center">
  Made with using ESP32 · MQ-2 · Telegram · Arduino
</p>
