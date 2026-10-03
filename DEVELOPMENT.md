# Development Mode Setup

This guide explains how to use `.env` file configuration for faster development cycles.

## Overview

Instead of using the web provisioning interface every time you flash new firmware, you can store your configuration in a `.env` file that gets uploaded to the device's SPIFFS partition. The device will automatically load this configuration on boot.

## Quick Start

1. **Create the .env file:**
   ```bash
   make create-env
   ```

2. **Edit with your configuration:**
   ```bash
   nano data/.env
   ```

   Example content:
   ```ini
   WIFI_SSID=MyHomeWiFi
   WIFI_PASSWORD=MySecurePassword
   ```

3. **Upload to device:**
   ```bash
   make upload-spiffs
   ```

4. **Flash and test:**
   ```bash
   make flash monitor
   ```

## How It Works

On boot, the firmware checks for `/spiffs/.env`:
- If found and complete → loads configuration and skips provisioning
- If missing or incomplete → falls back to NVS (previously saved config)
- If NVS is empty → enters provisioning mode (AP + web interface)

This creates a priority chain: **.env > NVS > Provisioning**

## Benefits

- **Faster iterations**: Change WiFi settings without re-flashing firmware
- **Multi-device testing**: Easy to test same firmware on multiple devices with different configs
- **Version control friendly**: Keep `.env.example` in git, `.env` in `.gitignore`
- **No web UI needed**: Perfect for headless development/testing

## Partition Layout

```
0x000000 - Bootloader
0x008000 - Partition Table (24KB)
0x009000 - NVS (24KB)
0x00F000 - PHY Init (4KB)
0x010000 - App (1536KB / 1.5MB)
0x190000 - SPIFFS Storage (64KB) ← .env file lives here
```

## Common Workflows

### Change WiFi Network
```bash
# Edit .env with new WiFi credentials
nano data/.env

# Upload to device (no firmware reflash needed!)
make upload-spiffs

# Reboot device
idf.py -p /dev/cu.usbmodem101 monitor
# Press Ctrl+T, Ctrl+R to reboot
```

## Troubleshooting

### .env file not loading
Check serial output for:
```
I (1234) config: Checking for .env file...
I (1235) config: Loading configuration from .env file
```

If you see "SPIFFS not formatted", run:
```bash
make upload-spiffs
```

### Incomplete .env warning
Ensure both fields are present:
- WIFI_SSID
- WIFI_PASSWORD

### Want to force provisioning mode
Erase the SPIFFS partition and NVS:
```bash
make erase-flash
make flash
```

## Security Note

The `.env` file contains sensitive credentials. Never commit it to version control:
```bash
echo "data/.env" >> .gitignore
```

Only commit `data/.env.example` with placeholder values.
