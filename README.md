# ESP32 Wi-Fi Deauther with Web Interface

⚠ **Educational Purposes Only**  
Use only on networks you own or have permission to test.

This project lets you:
- Scan nearby Wi-Fi networks
- Select a target and send deauthentication frames
- Control everything from a web UI hosted on the ESP32
- Use an RGB LED for status indication (Idle, Scanning, Attacking)

---

## Features
- **Access Point mode** — ESP32 hosts its own Wi-Fi network
- **Web UI** — No serial commands needed
- **NeoPixel LED support**
- **Packet injection** using `esp_wifi_80211_tx`

---

## Hardware Required
- ESP32 board (tested on ESP32-S3)
- 1x NeoPixel RGB LED
- Micro USB cable

---

## Wiring
| ESP32 Pin | LED Pin |
|-----------|--------|
| GPIO 48   | Data In |
| 3.3V      | VCC    |
| GND       | GND    |

---

## How to Install
1. **Install Arduino IDE**
2. Install **ESP32 board support** in Arduino IDE
3. Install libraries:
   - `Adafruit NeoPixel`
