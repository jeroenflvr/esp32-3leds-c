# 3-LED Controller Usage Guide

## Overview

A WiFi-controlled 3-LED (WS2813) light on a Seeed XIAO ESP32-C6 with LiPo
battery monitoring. It serves a local website where you pick a pattern and
control brightness.

## Features

- **Patterns**: solid color (RGB picker), candle (3 independently dancing
  flames), lightning/stormy night, heartbeat (lub-dub double pulse,
  30-180 bpm, color picker)
- **Global brightness**: always adjustable, independent of pattern
- **Battery monitoring**: voltage + percentage shown in the web UI
- **Persistence**: pattern/brightness/color/bpm restored after reboot
- **Web provisioning**: first-time WiFi setup via access point

## Hardware Setup

See [WIRING.md](WIRING.md) for complete wiring instructions.

**Quick reference:**
- WS2813 DI → GPIO0 (D0)
- WS2813 5V/GND → 5V/GND
- Battery divider (2x 330k + 100nF) → GPIO1 (D1)

## First-Time Setup (Provisioning)

When the device boots for the first time (or after NVS is erased):

1. The device creates a WiFi access point named **"leds-setup"**
2. Connect to this WiFi network from your phone or computer
3. Open a web browser and navigate to **http://192.168.4.1**
4. Enter your WiFi SSID and password
5. Click "Save & Reboot"

## Normal Operation

After boot the device:

1. Restores the last saved LED state and starts animating immediately
2. Connects to WiFi (check the serial monitor for the IP address)
3. Serves the control UI at **http://\<device-ip\>/**

### Web UI

- Pattern buttons: Off, Solid Color, Candle, Lightning, Heartbeat
- Brightness slider (applies to every pattern)
- Color picker (shown for Solid and Heartbeat)
- Heart rate slider 30-180 bpm (shown for Heartbeat)
- Battery status in the header, refreshed every 30 s

### HTTP API

```
GET  /api/state
  → {"pattern":"candle","brightness":128,"r":255,"g":40,"b":40,
     "bpm":60,"battery_v":3.98,"battery_pct":93}

POST /api/state           (any subset of fields)
  {"pattern":"heartbeat","brightness":200,"bpm":72,"r":255,"g":0,"b":0}
```

Patterns: `off`, `solid`, `candle`, `lightning`, `heartbeat`.

## Development Mode (.env Configuration)

You can skip web provisioning with a `.env` file in SPIFFS — see
[DEVELOPMENT.md](DEVELOPMENT.md).

```ini
WIFI_SSID=your-wifi-ssid
WIFI_PASSWORD=your-wifi-password
```

## Building and Flashing

```bash
# Build the project
make build

# Flash to device
make flash

# Monitor serial output
make monitor
```

**Using idf.py directly:**
```bash
idf.py build
idf.py -p /dev/cu.usbmodem101 flash monitor
```

## Resetting Configuration

To reset the device and re-enter provisioning mode:

```bash
idf.py -p /dev/cu.usbmodem101 erase-flash
idf.py -p /dev/cu.usbmodem101 flash
```

## Troubleshooting

### Can't connect to "leds-setup" WiFi
- Check that your device supports 2.4GHz WiFi
- Try forgetting and reconnecting to the network

### LEDs don't light up
- Check DI is on GPIO0 and the strip is powered from 5V
- WS2813 has a backup data line (BI) — only DI needs to be connected

### WiFi connection fails
- Verify SSID and password are correct
- ESP32-C6 only supports 2.4GHz
- The LEDs keep animating; the device reboots and retries after 30 s

### Battery reading looks wrong
- Verify the divider and the 100 nF cap (see WIRING.md)
- Re-check `ADC_CALIBRATION_FACTOR` in battery.c against a multimeter

## License

MIT License - Feel free to modify and use as needed.
