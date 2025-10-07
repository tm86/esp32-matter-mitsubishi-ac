# esp32-matter-mitsubishi-ac

Control a Mitsubishi AC unit using an ESP32 module.

## PlatformIO project

This repository contains a ready-to-build [PlatformIO](https://platformio.org/) project configured for the Arduino framework and the [SwiCago/HeatPump](https://github.com/SwiCago/HeatPump) library. Two example environments are included out of the box:

- `esp32-c3-devkitc-02` – the original reference target.
- `seeed_xiao_esp32c6` – suitable for the [Seeed Studio XIAO ESP32C6](https://wiki.seeedstudio.com/XIAO_ESP32C6_Getting_Started/) board.

Both environments share the same source code and automatically pull the HeatPump driver dependency while configuring the serial monitor at 115200 baud.

### Building and uploading

```bash
# Build for the default ESP32-C3 DevKitC-02 target
pio run

# Build & upload for the default target
pio run -t upload

# Build & upload for the Seeed Studio XIAO ESP32C6
pio run -e seeed_xiao_esp32c6 -t upload

# Open the serial console (works for either environment)
pio device monitor
```

### Hardware configuration

The demo sketch in `src/main.cpp` uses `Serial1` to communicate with the heat pump over the CN105 connector. The default pin mappings differ slightly per environment:

| Environment | RX pin (`HEATPUMP_SERIAL_RX_PIN`) | TX pin (`HEATPUMP_SERIAL_TX_PIN`) |
|-------------|-----------------------------------|-----------------------------------|
| `esp32-c3-devkitc-02` | GPIO 4 | GPIO 5 |
| `seeed_xiao_esp32c6`  | GPIO 17 (D7) | GPIO 16 (D6) |

Both configurations use a serial speed of 2400 baud with even parity (`HEATPUMP_SERIAL_BAUD_RATE`).

You can adjust these defaults by defining the macros above in `platformio.ini` (using `build_flags = -DHEATPUMP_SERIAL_RX_PIN=<pin> ...`) or by editing `src/main.cpp` directly.

On boot the firmware connects to the heat pump, applies a set of initial settings (power on, HEAT mode, 22 °C, AUTO fan), and then periodically synchronises with the unit. Status, settings changes, and room temperature updates are logged to the USB serial console to provide visibility into the connection.
