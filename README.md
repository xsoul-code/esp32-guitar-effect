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
- **MQTT Configuration** — broker URI (e.g. `mqtt://192.168.1.100`)
- **SSD1306 Configuration** — I2C interface, SDA/SCL pins, panel type, X offset (2 for SH1106)

Then:

```
idf.py build
idf.py flash monitor
```

## Communication protocols

### UART (ESP32 ↔ STM32)

Successfully implemented UART_RX data acquisition tested using second ESP32 with basic ArduinoIDE (source code in uart_test_arduino directory) script for UART_TX tester.

Output:
```
I (165016) Smart Guitar Effect: 0x3ffcc6e8   48 45 4c 4c 4f 5f 45 53  50 33 32 0d 0a           |HELLO_ESP32..|
```

### MQTT (Web UI ↔ ESP32)

Client connects to the broker configured in `menuconfig` and handles full lifecycle (connect, subscribe, data, disconnect, error). Topics:

- `pedal/cmd` (RX, QoS 1) — control commands from the web UI
- `pedal/status` (TX) — pedal publishes `"online"` on connect

Tested locally with Mosquitto:

```
mosquitto_sub -h localhost -t 'pedal/#' -v
mosquitto_pub -h localhost -t 'pedal/cmd' -m 'effect:reverb'
```

## Documentation references

Used documentation in implementation process:

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html)
- [ESP-IDF Examples on GitHub](https://github.com/espressif/esp-idf/tree/master) - UART/WIFI/MQTT examples
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [esp-idf-ssd1306](https://github.com/nopnop2002/esp-idf-ssd1306) — SSD1306/SH1106 OLED driver component for ESP-IDF