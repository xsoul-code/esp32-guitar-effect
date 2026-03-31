# ESP32 Guitar Effect

ESP32 firmware for a wireless digital guitar effects pedal. Built with ESP-IDF and FreeRTOS.

## Architecture

Part of a dual-MCU system:
- **STM32F407** — audio DSP (I2S + WM8731 codec, effects chain)
- **ESP32** (this repo) — control node: WiFi/MQTT, web UI, OLED display, footswitch, UART communication with STM32

## Building

Requires ESP-IDF.

Set your WiFi credentials and OLED display settings in `menuconfig`:

```
idf.py menuconfig
```

- **WiFi Configuration** — SSID, password, max retry
- **SSD1306 Configuration** — I2C interface, SDA/SCL pins, panel type, X offset (2 for SH1106)

Set broker IP in `main/include/mqtt.h`, then:

```
idf.py build
idf.py flash monitor
```
## Documentation references

Used doucmentation in implementation process:

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html)
- [ESP-IDF Examples on GitHub](https://github.com/espressif/esp-idf/tree/master)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [esp-idf-ssd1306](https://github.com/nopnop2002/esp-idf-ssd1306) — SSD1306/SH1106 OLED driver component for ESP-IDF