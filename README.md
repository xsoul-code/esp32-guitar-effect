# ESP32 Guitar Effect

ESP32 firmware for a wireless digital guitar effects pedal. Built with ESP-IDF and FreeRTOS.

## Architecture

Part of a dual-MCU system:
- **STM32F407** — audio DSP (I2S + WM8731 codec, effects chain)
- **ESP32** (this repo) — control node: WiFi/MQTT, web UI, OLED display, footswitch, UART communication with STM32

## Building

Requires ESP-IDF.

 Set your WiFi credentials in `menuconfig` 

```
idf.py menuconfig
```

and broker IP in `main/include/mqtt.h`, 

then:

```
idf.py build
idf.py flash monitor
```
## Documentation references

Used doucmentation in implementation process:

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html)
- [ESP-IDF Examples on GitHub](https://github.com/espressif/esp-idf/tree/master)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html) 