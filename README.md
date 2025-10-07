# esp32-matter-mitsubishi-ac

Control a Mitsubishi AC unit using an ESP32 module.

## PlatformIO project

This repository contains a ready-to-build [PlatformIO](https://platformio.org/) project configured for the `esp32-c3-devkitc-02` board, the Arduino framework, and the [SwiCago/HeatPump](https://github.com/SwiCago/HeatPump) library. The configuration automatically pulls the HeatPump driver as a dependency and sets up the serial monitor at 115200 baud.

### Building and uploading

```bash
pio run              # build the firmware
pio run -t upload    # flash over USB
pio device monitor   # open the serial console
```

### Hardware configuration

The demo sketch in `src/main.cpp` uses `Serial1` to communicate with the heat pump over the CN105 connector. By default the firmware expects:

- RX on GPIO 4 (`HEATPUMP_SERIAL_RX_PIN`)
- TX on GPIO 5 (`HEATPUMP_SERIAL_TX_PIN`)
- Serial speed of 2400 baud with even parity (`HEATPUMP_SERIAL_BAUD_RATE`)

You can adjust these defaults by defining the macros above in `platformio.ini` (using `build_flags = -DHEATPUMP_SERIAL_RX_PIN=<pin> ...`) or by editing `src/main.cpp` directly.

On boot the firmware connects to the heat pump, applies a set of initial settings (power on, HEAT mode, 22 °C, AUTO fan), and then periodically synchronises with the unit. Status, settings changes, and room temperature updates are logged to the USB serial console to provide visibility into the connection.
