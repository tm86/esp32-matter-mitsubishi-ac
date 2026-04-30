from pathlib import Path

Import("env")


def parse_dotenv(path: Path):
    values = {}
    if not path.exists():
        return values
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        value = value.strip()
        if (value.startswith('"') and value.endswith('"')) or (
            value.startswith("'") and value.endswith("'")
        ):
            value = value[1:-1]
        values[key.strip()] = value
    return values


def bool_literal(value: str) -> str:
    return "1" if value.strip().lower() in {"1", "true", "yes", "on"} else "0"


def string_literal(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


project_dir = Path(env.subst("$PROJECT_DIR"))
dotenv_path = Path(env.subst(env.GetProjectOption("custom_dotenv_path")))
example_path = Path(env.subst(env.GetProjectOption("custom_dotenv_example_path")))
source_path = dotenv_path if dotenv_path.exists() else example_path
values = parse_dotenv(source_path)

defaults = {
    "WIFI_SSID": "",
    "WIFI_PASSWORD": "",
    "DEVICE_NAME": "esp32-ac",
    "MQTT_ENABLED": "true",
    "MQTT_HOST": "",
    "MQTT_PORT": "1883",
    "MQTT_USERNAME": "",
    "MQTT_PASSWORD": "",
    "MQTT_TOPIC_ROOT": "home/heatpump/esp32-ac",
    "MQTT_LOGS_ENABLED": "true",
    "WEB_AP_FALLBACK_ENABLED": "true",
    "WEB_AP_SSID": "esp32-ac-setup",
    "WIFI_CONNECT_TIMEOUT_SECONDS": "20",
    "LOCATION": "",
}
defaults.update(values)

generated_path = project_dir / "include" / "build_defaults.generated.h"
generated_path.parent.mkdir(parents=True, exist_ok=True)
generated_path.write_text(
    "\n".join(
        [
            "#pragma once",
            f"#define BUILD_DEFAULT_WIFI_SSID {string_literal(defaults['WIFI_SSID'])}",
            f"#define BUILD_DEFAULT_WIFI_PASSWORD {string_literal(defaults['WIFI_PASSWORD'])}",
            f"#define BUILD_DEFAULT_DEVICE_NAME {string_literal(defaults['DEVICE_NAME'])}",
            f"#define BUILD_DEFAULT_MQTT_ENABLED {bool_literal(defaults['MQTT_ENABLED'])}",
            f"#define BUILD_DEFAULT_MQTT_HOST {string_literal(defaults['MQTT_HOST'])}",
            f"#define BUILD_DEFAULT_MQTT_PORT {int(defaults['MQTT_PORT'])}",
            f"#define BUILD_DEFAULT_MQTT_USERNAME {string_literal(defaults['MQTT_USERNAME'])}",
            f"#define BUILD_DEFAULT_MQTT_PASSWORD {string_literal(defaults['MQTT_PASSWORD'])}",
            f"#define BUILD_DEFAULT_MQTT_TOPIC_ROOT {string_literal(defaults['MQTT_TOPIC_ROOT'])}",
            f"#define BUILD_DEFAULT_MQTT_LOGS_ENABLED {bool_literal(defaults['MQTT_LOGS_ENABLED'])}",
            f"#define BUILD_DEFAULT_WEB_AP_FALLBACK_ENABLED {bool_literal(defaults['WEB_AP_FALLBACK_ENABLED'])}",
            f"#define BUILD_DEFAULT_WEB_AP_SSID {string_literal(defaults['WEB_AP_SSID'])}",
            f"#define BUILD_DEFAULT_WIFI_CONNECT_TIMEOUT_SECONDS {int(defaults['WIFI_CONNECT_TIMEOUT_SECONDS'])}",
            f"#define BUILD_DEFAULT_LOCATION {string_literal(defaults['LOCATION'])}",
            "",
        ]
    ),
    encoding="utf-8",
)
