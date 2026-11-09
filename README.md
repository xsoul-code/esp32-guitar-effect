# ESP32 Guitar Effect

ESP32 firmware for the control node of a digital guitar effects pedal (engineering thesis).
Built with ESP-IDF and FreeRTOS.

## Architecture

The pedal uses two microcontrollers connected by a UART link.
The STM32 does all audio processing. The ESP32 does not touch audio; it only sends
commands and shows the state reported by the STM32.

- **STM32F407** (separate repository) - audio DSP: WM8731 codec over I2S, effects chain
  (distortion, chorus, echo, reverb, EQ), button B1 for switching effects.
- **ESP32** (this repository) - control: WiFi, web panel, OLED screen, potentiometers,
  UART link to the STM32.

## Features

- **UART link to the STM32** - text protocol, 115200 8N1 on UART2. The ESP32 sends `PING`
  every 2 s and treats the link as down after 5 s without any line from the STM32.
  When the link comes up or the STM32 restarts (`HELLO`), the ESP32 sends `GET ALL` and
  sends all knob positions again.
- **Web panel** - one HTML page embedded in the firmware, served at `/`. It talks to the
  ESP32 over a WebSocket at `/ws` using the same text lines as the UART link. The ESP32
  forwards `SET` and `GET` from the panel to the STM32 and sends every STM32 line except
  `PONG` to all open panels. The panel has the effect on/off buttons, bypass, sliders for
  all parameters, the macro knob setting and the spectrum chart. The panel text is in Polish.
- **WiFi** - the ESP32 runs in AP+STA mode. It creates its own network "GuitarFX"
  (password `guitarfx` by default) and also connects to the home WiFi set in menuconfig.
  The panel is at http://pedal.local (mDNS, home network) or http://192.168.4.1
  (pedal's own network). If the home WiFi fails after the set number of retries,
  the pedal network still works.
- **OLED status screen** - 128x64 I2C display. Top line: pedal state (`GuitarFX`, `CLEAN` when no
  effect is on, `BYPASS`, or `STM32 ---` when the link is down), WiFi icon and clock (SNTP over the home WiFi,
  Polish time zone). Middle: four tiles DIST / CHORUS / ECHO / REVERB, inverted when on.
  Bottom: panel address and number of open panels; for 3 s after a change it shows the
  parameter name and the value applied by the STM32.
- **Six potentiometers** on ADC1. Five have a fixed parameter: `volume`, `drive`,
  `eq_low_db`, `eq_mid_db`, `eq_high_db`. The sixth is a macro knob: its parameter is chosen
  in the web panel (or `off`) and kept in NVS across power cycles. Each knob sends a `SET`
  only when it really moved, and at most one `SET` per 100 ms. Knobs send nothing while the
  link is down.
- **Live spectrum** - with the panel's spectrum button on, the STM32 sends 16 FFT bands every
  100 ms and the panel draws them as a bar chart. The ESP32 turns it off (`SET fft 0`)
  when no panel is open.

There is no footswitch on the ESP32. Effects are switched from the web panel or with
button B1 on the STM32 board.

## Hardware

- ESP32-DevKitC with ESP32-WROOM-32E (ESP32-D0WD-V3, 4 MB flash)
- 128x64 I2C OLED (SDA GPIO21, SCL GPIO22)
- 6 x B10K potentiometers
- UART to the STM32: GPIO17 (TX) -> PD9, GPIO16 (RX) <- PD8, common GND

Full wiring, pin choice and the board layout: [docs/PINOUT.md](docs/PINOUT.md).

## Building

Tested with ESP-IDF v6.1.

The project path must not contain non-ASCII characters (e.g. Polish diacritics) -
the toolchain mangles such paths and the build fails on a missing `picolibc.specs`.

The `espressif/mdns` dependency is declared in `main/idf_component.yml` and is
fetched automatically on the first build. The SSD1306/SH1106 driver is vendored in
`components/ssd1306`, so a plain `git clone` is enough, no submodule init needed.

`sdkconfig.defaults` sets the flash size, the partition table, WebSocket support in the
HTTP server and the OLED settings. It is only used when there is no `sdkconfig` yet.

Set your home WiFi and check the rest in `menuconfig`:

```
idf.py menuconfig
```

- **WiFi Configuration** - home WiFi SSID, password, max retry; pedal WiFi name and password
  (SoftAP, empty password = open network)
- **SSD1306 Configuration** - I2C interface, SDA/SCL pins, panel type, X offset (2 for SH1106)

Then:

```
idf.py build
idf.py -p <port> flash monitor
```

Give the port with `-p`. With the STM32 board connected over USB, auto-detection can
pick the ST-LINK port instead of the ESP32.

## Source layout

- `main/esp32-guitar-effect.c` - `app_main` (start-up order, tasks), OLED status screen, SNTP clock
- `main/stm32_uart.c` - UART2 driver, line receiver, link supervision, state mirror for the OLED
- `main/web.c` - HTTP server, WebSocket `/ws`, broadcast to panels, mDNS
- `main/wifi.c` - WiFi AP+STA
- `main/knobs.c` - potentiometer reading (ADC1 oneshot), value mapping, macro knob, NVS
- `main/oled.c` - OLED I2C init
- `main/web/index.html` - web panel, embedded in the firmware (`EMBED_FILES`)
- `main/include/` - headers
- `main/Kconfig.projbuild` - WiFi menu
- `components/ssd1306/` - vendored OLED driver
- `docs/` - UART protocol and pinout

## Protocol

The ESP32 and the STM32 exchange ASCII lines: `SET`, `GET`, `PING` one way and `VAL`, `END`,
`HELLO`, `FFT`, `ERR` the other. The full specification, shared with the STM32 repository,
is in [docs/UART_PROTOCOL.md](docs/UART_PROTOCOL.md).

## Documentation references

Used documentation in implementation process:

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html)
- [ESP-IDF Examples on GitHub](https://github.com/espressif/esp-idf/tree/master/examples), used in the code:
  - [peripherals/uart/uart_echo](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/uart/uart_echo) - UART receive task
  - [wifi/getting_started/station](https://github.com/espressif/esp-idf/tree/master/examples/wifi/getting_started/station) and [wifi/softap_sta](https://github.com/espressif/esp-idf/tree/master/examples/wifi/softap_sta) - WiFi AP+STA
  - [protocols/sntp](https://github.com/espressif/esp-idf/tree/master/examples/protocols/sntp) - clock
  - [protocols/http_server/ws_echo_server](https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server/ws_echo_server) - HTTP server and WebSocket
  - [protocols/https_server/wss_server](https://github.com/espressif/esp-idf/tree/master/examples/protocols/https_server/wss_server) - sending to all WebSocket clients
  - [peripherals/adc/oneshot_read](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/adc/oneshot_read) - potentiometers
  - [storage/nvs/nvs_rw_value](https://github.com/espressif/esp-idf/tree/master/examples/storage/nvs/nvs_rw_value) - macro knob setting in NVS
- [esp-protocols mDNS examples](https://github.com/espressif/esp-protocols/tree/master/components/mdns/examples) - `pedal.local`
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [esp-idf-ssd1306](https://github.com/nopnop2002/esp-idf-ssd1306) - SSD1306/SH1106 OLED driver component for ESP-IDF
