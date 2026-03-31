# ESP32 Guitar Effect

ESP32 firmware for a wireless digital guitar effects pedal. Built with ESP-IDF and FreeRTOS.

## Architecture

Part of a dual-MCU system:
- **STM32F407** — audio DSP (I2S + WM8731 codec, effects chain)
- **ESP32** (this repo) — control node: WiFi/MQTT, web UI, OLED display, footswitch, UART communication with STM32

## Building

Requires ESP-IDF. Set your WiFi credentials in `main/wifi.c` and broker IP in `main/include/mqtt.h`, then:

```
idf.py build
idf.py flash monitor
```
