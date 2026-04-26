# Desktop Companion

ESP-IDF firmware and a GitHub Pages web flasher for a 240x240 ST7789 desktop photo companion.

## User Flow

1. Open the hosted web flasher in Chrome or Edge.
2. Select the target board profile: ESP32-S3, ESP32-C3, or ESP32-C6.
3. Upload photos. The browser crops them to 240x240, dims them to 85% brightness, and converts them to RGB565.
4. Enter local Wi-Fi credentials, timezone, photo rotation interval, and Pomodoro duration.
5. Connect the ESP32 over USB and flash firmware plus the generated user asset partition.

Web Serial requires HTTPS, so GitHub Pages is the intended hosting target.

## Recovery If Flashing Fails

`invalid header: 0xffffffff` means the ROM is reading erased flash where the bootloader should be. The board is not bricked.

The web flasher reads `manifest.json` generated from ESP-IDF's `flasher_args.json`, so the bootloader, partition table, and app offsets come from the same build that produced the binaries. If you see this error, redeploy the latest web flasher, hard refresh the page, hold BOOT while connecting if needed, and flash again.

## Firmware Behavior

- Gallery is the base screen.
- Photos rotate every 60 seconds by default, or by the value entered in the web flasher.
- The default overlay shows large time with the weekday/date below it.
- Short button press switches between the clock overlay and custom Pomodoro overlay.
- Double tap adds 5 minutes to the Pomodoro duration.
- Triple tap removes 5 minutes from the Pomodoro duration, clamped at 5 minutes.
- Long press starts or stops the Pomodoro timer.
- The Pomodoro timer keeps counting while the clock/photo overlay is visible.
- When Pomodoro reaches zero, it stops and resets to the last user-set duration.

## Flash Layout

The web flasher writes a generated `assets` data partition after the app:

| Partition | Offset | Size | Purpose |
|---|---:|---:|---|
| `nvs` | `0x9000` | `0x6000` | Wi-Fi/runtime config |
| `phy_init` | `0xf000` | `0x1000` | RF calibration |
| `factory` | `0x10000` | `2M` | ESP-IDF app |
| `assets` | `0x210000` | `0x1F0000` | Header + RGB565 photos |

Each 240x240 RGB565 photo is `240 * 240 * 2 = 115200` bytes. The current 0x1F0000 asset partition fits 17 photos after the 256-byte header.

## Asset Header

The generated asset image starts with a 256-byte little-endian header:

- Magic: `DCAS`
- Format version: `1`
- Screen geometry and image count
- Rotation interval and Pomodoro duration
- Asset ID for one-time migration into NVS
- Timezone, SSID, and password strings

Credentials are convenient to flash this way, but they are not secret unless you later enable flash encryption.

## Development

Firmware build:

```bash
idf.py set-target esp32s3
idf.py build
```

Web flasher build:

```bash
cd web
npm ci
npm run build
```

The GitHub Actions workflow builds S3/C3/C6 firmware artifacts, converts ESP-IDF `flasher_args.json` into per-board web manifests, copies everything into the web app, and deploys GitHub Pages.

Before changing storage or Wi-Fi behavior, read the ESP-IDF Programming Guide sections for Partition Tables, NVS, `esp_partition`, and Wi-Fi station provisioning/storage.