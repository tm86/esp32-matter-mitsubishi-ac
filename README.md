# ESP32 Mitsubishi AC Controller

PlatformIO firmware for **Seeed XIAO ESP32C6** or **Seeed XIAO ESP32C3** that connects to a Mitsubishi heat pump via the CN105 serial port and exposes local control through a REST API, optional MQTT, a built-in web UI, and native **Matter** (Apple Home / Google Home).

## Features

- Full heat pump control (power, temperature, mode, fan speed, vane direction) via CN105 serial
- REST API for local control
- Optional MQTT with Home Assistant auto-discovery (disabled by default)
- WebSocket for real-time state and log streaming
- Embedded vanilla JS web UI (three-tab SPA: AC Control, Settings, Logs)
- **Matter thermostat** — commissions directly into Apple Home, Google Home, or any Matter controller; bidirectional bridge (commands from Matter update the AC; AC state changes update Matter attributes)
- WiFi AP fallback for first-time setup — if STA connection fails, the device starts an access point and serves the settings UI
- NVS-backed settings — persist across reboots, configurable from the web UI

## Hardware

Two supported boards — same firmware, different PlatformIO environments:

| Board | PIO env | Matter transport | Flash | RAM |
|-------|---------|-----------------|-------|-----|
| Seeed XIAO ESP32**C6** | `seeed_xiao_esp32c6` | WiFi + Thread | 4 MB | 512 KB |
| Seeed XIAO ESP32**C3** | `seeed_xiao_esp32c3` | WiFi only | 4 MB | 400 KB |

### CN105 Wiring

| Signal | XIAO pin | GPIO |
|--------|----------|------|
| TX (ESP → heat pump) | D6 | GPIO21 |
| RX (heat pump → ESP) | D7 | GPIO20 |
| GND | GND | — |

> CN105 uses 5 V logic on most Mitsubishi units. A bidirectional 3.3 V ↔ 5 V level shifter is required between the XIAO and the CN105 connector. UART baud: 2400 8E1.

## Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) or the PlatformIO IDE extension for VS Code
- Python 3.12+ (used by the pre-build scripts)

## Quick Start

### 1. Clone and configure

```bash
git clone <repo-url>
cd esp32-ac-controller
cp .env.example .env
```

Edit `.env` with your WiFi credentials and MQTT settings:

```dotenv
WIFI_SSID=your-network
WIFI_PASSWORD=your-password
DEVICE_NAME=esp32-ac
LOCATION=Living Room

MQTT_ENABLED=false          # set true to enable MQTT
MQTT_HOST=192.168.1.10
MQTT_PORT=1883
MQTT_USERNAME=
MQTT_PASSWORD=
MQTT_TOPIC_ROOT=home/heatpump/esp32-ac
MQTT_LOGS_ENABLED=false

WEB_AP_FALLBACK_ENABLED=true
WEB_AP_SSID=esp32-ac-setup
WIFI_CONNECT_TIMEOUT_SECONDS=20
```

> `.env` is gitignored and never committed. `.env.example` documents all available keys.

### 2. Build

```bash
# ESP32-C6 (default)
pio run -e seeed_xiao_esp32c6

# ESP32-C3
pio run -e seeed_xiao_esp32c3
```

The build runs two pre-scripts automatically:

- `scripts/load_env.py` — reads `.env` → generates `include/build_defaults.generated.h`
- `scripts/gzip_ui.py` — gzip-compresses `data/index.html` → generates `include/WebUiHtml.h` (skipped if HTML is unchanged)

### 3. Flash

```bash
# ESP32-C6
pio run -e seeed_xiao_esp32c6 --target upload

# ESP32-C3
pio run -e seeed_xiao_esp32c3 --target upload
```

### 4. Monitor serial output

```bash
pio device monitor
```

Expected output on first boot:

```
[main] device: esp32-ac
[WiFi] Connecting to 'your-network'
......
[WiFi] Connected. IP: 192.168.1.xxx
[Web] HTTP + WS started on port 80
[Matter] started
[Matter] not commissioned — code: 34970112332
[info] ESP-AC starting up...
[info] HTTP + WebSocket on port 80
[info] Matter thermostat active
[info] HP task started
[warn] Heat pump not responding — check CN105 wiring   ← expected without hardware
```

### 5. Access the web UI

Open `http://<device-ip>/` in a browser. The IP is printed in the serial output.

If WiFi connection fails and AP fallback is enabled, connect to the `esp32-ac-setup` access point, open `http://192.168.4.1/`, go to the Settings tab, configure WiFi credentials, and save (device reboots into STA mode).

## Matter Commissioning

The device advertises itself as a Matter thermostat over WiFi (and Thread on the C6).

1. Open the **Settings tab** in the web UI → **Matter section** — the 11-digit pairing code is shown
2. In Apple Home: **+** → **Add Accessory** → **More Options** → enter the pairing code
3. In Google Home: **+** → **Set up device** → **Matter** → enter the pairing code
4. After commissioning the badge changes to **Paired** and the pairing code is hidden

**Setpoint range:** 16–31 °C. Mode and setpoint changes from Apple Home / Google Home are bridged to the heat pump in real time. AC state changes (from the web UI or MQTT) sync back to Matter within ~2 seconds.

To remove the device from all Matter controllers: Settings tab → **Remove from Matter** (device reboots and re-generates a new pairing code).

## REST API

Base URL: `http://<device-ip>/api/`

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/state` | Full device + AC state |
| POST | `/api/state` | Partial AC command (any subset of power/temp/mode/fan/vane) |
| GET | `/api/settings` | All NVS settings (passwords redacted) |
| POST | `/api/settings` | Update settings — saves to NVS, reboots after 1.5 s |
| POST | `/api/reboot` | Reboot device after 500 ms |
| GET | `/api/matter/status` | Commissioning state + pairing code / QR URL |
| POST | `/api/matter/decommission` | Reset Matter commissioning and reboot |

### Example: read state

```bash
curl http://<device-ip>/api/state
```

```json
{
  "power": true,
  "temp": 22,
  "mode": "COOL",
  "fan": "AUTO",
  "vane": "AUTO",
  "roomTemp": 23.4,
  "hpConnected": true,
  "hostname": "esp32-ac",
  "ip": "192.168.1.105",
  "wifi": -62,
  "location": "Living Room",
  "firmware": "1.0.0",
  "matterStatus": "commissioned"
}
```

### Example: read Matter status

```bash
curl http://<device-ip>/api/matter/status
```

```json
{"status":"not_commissioned","pairingCode":"34970112332","qrUrl":"https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT:..."}
```

After commissioning:

```json
{"status":"commissioned","pairingCode":"","qrUrl":""}
```

### Example: send command

```bash
curl -X POST http://<device-ip>/api/state \
  -H 'Content-Type: application/json' \
  -d '{"power": true, "temp": 21, "mode": "COOL"}'
```

## MQTT

Disabled by default. Enable with `MQTT_ENABLED=true` in `.env` (or via the web UI Settings tab).

### Topics

Root topic is configured via `MQTT_TOPIC_ROOT` (default: `home/heatpump/esp32-ac`).

| Topic | Direction | Payload |
|-------|-----------|---------|
| `{root}/state` | publish | Full state JSON on HP change + 60 s heartbeat |
| `{root}/set` | subscribe | Partial command JSON (`power`, `temp`, `mode`, `fan`, `vane`) |
| `{root}/log` | publish | `{"t":"...","lvl":"...","msg":"..."}` (if `MQTT_LOGS_ENABLED=true`) |
| `{root}/connected` | LWT | `"true"` on connect / `"false"` on disconnect |

### Home Assistant auto-discovery

Set `mqtt_ha_disc` to `true` in the Settings tab (or via `POST /api/settings`). Default: **disabled**.

When enabled, the device publishes a retained discovery payload to `homeassistant/climate/<device-name>/config` on MQTT connect, registering a `climate` entity in Home Assistant.

## WebSocket

Connect to `ws://<device-ip>/ws`. All commands go through the REST API — WebSocket is receive-only.

### Frames from device

```json
// State update (pushed on every HP state change)
{"type":"state","power":true,"temp":22,"mode":"COOL","fan":"AUTO","vane":"AUTO","roomTemp":23.4}

// Log line
{"type":"log","t":"00:01:23","lvl":"info","msg":"WiFi connected"}
```

Log levels: `info`, `warn`, `error`, `debug`.

On connect, the last 200 log entries are replayed immediately before live pushes begin.

## Settings (NVS)

Settings are stored in NVS namespace `esp-ac`. They can be updated from the web UI Settings tab or via `POST /api/settings`. All settings survive reboots.

| Key | Description | Default |
|-----|-------------|---------|
| `wifi_ssid` | WiFi SSID | from `.env` |
| `wifi_pass` | WiFi password | from `.env` |
| `device_name` | Hostname | from `.env` |
| `location` | Room label shown in UI | from `.env` |
| `update_iv` | HP poll interval (seconds) | `1` |
| `mqtt_enabled` | Enable MQTT | from `.env` |
| `mqtt_host` | MQTT broker host | from `.env` |
| `mqtt_port` | MQTT broker port | `1883` |
| `mqtt_user` | MQTT username | from `.env` |
| `mqtt_pass` | MQTT password | from `.env` |
| `mqtt_topic` | MQTT topic root | from `.env` |
| `mqtt_logs` | Publish logs to MQTT | from `.env` |
| `mqtt_ha_disc` | HA auto-discovery | `false` |
| `ap_enabled` | AP fallback on STA failure | from `.env` |
| `ap_ssid` | AP SSID | from `.env` |

## Project Structure

```
data/
  index.html              Vanilla JS web UI source (compiled to WebUiHtml.h)
include/
  ac_state.h              AcState struct + globals
  settings.h              Settings struct
  wifi_manager.h
  heatpump_task.h         LogEntry, AcCommand, task API
  web_server.h
  mqtt.h
  matter_controller.h     Matter thermostat API
  WebUiHtml.h             Auto-generated gzip byte array — do not edit
  build_defaults.generated.h  Auto-generated from .env — do not edit
partitions/
  huge_app.csv            3.75 MB app partition; nvs_key (16 KB) for Matter commissioning data
scripts/
  load_env.py             .env → build_defaults.generated.h (pre-build script)
  gzip_ui.py              data/index.html → include/WebUiHtml.h (pre-build script)
src/
  main.cpp
  settings.cpp
  wifi_manager.cpp
  heatpump_task.cpp
  web_server.cpp
  mqtt.cpp
  matter_controller.cpp   MatterThermostat endpoint + AC bridge
```

## Fan Speed Mapping

The HeatPump library uses numeric fan values. The web UI displays them as labels:

| API value | UI label |
|-----------|----------|
| `AUTO` | AUTO |
| `QUIET` | QUIET |
| `1` | LOW |
| `2` | MED |
| `3` | MID |
| `4` | HIGH |

## Common Commands

```bash
# Build only (C6)
pio run -e seeed_xiao_esp32c6

# Build only (C3)
pio run -e seeed_xiao_esp32c3

# Build + flash (C6)
pio run -e seeed_xiao_esp32c6 --target upload

# Build + flash (C3)
pio run -e seeed_xiao_esp32c3 --target upload

# Build + flash + monitor
pio run --target upload && pio device monitor

# Monitor serial (115200 baud)
pio device monitor

# Clean build cache
pio run --target clean
```

## Dependencies

Managed automatically by PlatformIO:

| Library | Purpose |
|---------|---------|
| [SwiCago/HeatPump](https://github.com/SwiCago/HeatPump) | Mitsubishi CN105 serial protocol |
| [mathieucarbou/AsyncTCP](https://github.com/mathieucarbou/AsyncTCP) | Async TCP (ESP32-C3/C6 compatible) |
| [mathieucarbou/ESPAsyncWebServer](https://github.com/mathieucarbou/ESPAsyncWebServer) | Async HTTP + WebSocket server |
| [knolleary/PubSubClient](https://github.com/knolleary/PubSubClient) | MQTT client |
| [bblanchon/ArduinoJson](https://github.com/bblanchon/ArduinoJson) | JSON serialization |
| Matter (built-in) | Matter SDK — ships with arduino-esp32 3.x, enabled via build flag |

Platform: [pioarduino](https://github.com/pioarduino/platform-espressif32) 54.03.20 (Arduino framework on ESP32-C3/C6).

## Future Phases

- **OTA** — over-the-air firmware updates; `otadata` partition already reserved
- **TLS/MQTTS** — MQTT over TLS
