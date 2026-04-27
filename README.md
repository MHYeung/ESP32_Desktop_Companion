# Desktop Companion

ESP-IDF firmware plus a browser-based flasher for a 240×240 ST7789 photo frame: your photos, clock, Pomodoro timer, and (with Wi‑Fi) a quick weather readout from [Open-Meteo](https://open-meteo.com/).

---

## Flashing from the browser (recommended)

You need **Chrome or Edge** (Web Serial), a **USB data cable**, and a board this repo supports (e.g. ESP32-S3, ESP32-C3, ESP32-C6—check the **Board** dropdown in the flasher).

1. **Open the flasher**  
   Use the hosted GitHub Pages build, or run it locally after `cd web && npm ci && npm run build` and serve the `web/dist` folder over **HTTPS** (or `localhost`). Plain `file://` URLs will not expose serial.

2. **Pick the board**  
   Choose the profile that matches your chip. The flasher uses the right firmware manifest and asset size for that target.

   ![Board profile selection](docs/board_selection.png)

3. **Add photos**  
   Select one or more images; they’re cropped to 240×240, slightly dimmed, and converted to RGB565. You need at least one photo before flashing.

   ![Photo upload](docs/photos_selection.png)

4. **Wi‑Fi, timezone, and defaults**  
   Enter your **SSID** and **password**, choose a **timezone** (this sets both POSIX TZ for the clock and approximate coordinates for weather), then set **photo rotation** and **Pomodoro length** if you want something other than the defaults.

   ![Wi‑Fi and timezone](docs/wifi_tz_selection.png)

5. **Connect USB and flash**  
   Plug in the board, click **Generate and Flash**, and pick the serial port when the browser asks. Let it run to 100%; the device resets when done.

   ![Flash action](docs/board_confirmation.png)

**First boot:** the device reads Wi‑Fi and settings from the flashed assets partition and copies them into NVS where needed. If something looks wrong after a flash, do a hard refresh on the flasher page so you aren’t using a cached old manifest.

---

## If flashing fails

`invalid header: 0xffffffff` means the ROM is reading erased flash where the bootloader should be—the board is usually **not** bricked.

The web app’s offsets come from the same ESP-IDF build as the bundled binaries (`flasher_args.json` → manifest). Fix: deploy or pull the **latest** flasher + firmware bundle, hard-refresh the page, try again, and use **BOOT + reset** to force download mode if your board needs it.

---

## On-device behavior (firmware)

- **Gallery** is the base screen; photos rotate at the interval you flashed.
- **Clock overlay:** large time, weekday and date (leading `*` on the date if Wi‑Fi time isn’t synced yet).
- **Short press** cycles: **clock → Pomodoro → weather → clock**.
- **Pomodoro:** double press **+5 min**, triple press **−5 min** (minimum 5 min), long press **start/stop**. The timer keeps running while you’re on the clock or weather screen.
- **Weather:** needs Wi‑Fi; fetches current temperature and a short condition label via HTTPS. Refreshes when you open that screen and about every ten minutes while you stay on it.

Hardware assembly and a release checklist live in [`docs/HARDWARE_TEST.md`](docs/HARDWARE_TEST.md).

---

## Flash layout

The generated **assets** partition sits after the factory app:

| Partition | Offset   | Size     | Purpose |
|-----------|----------|----------|---------|
| `nvs`     | `0x9000` | `0x6000` | Wi‑Fi / runtime config |
| `phy_init`| `0xf000` | `0x1000` | RF calibration |
| `factory` | `0x10000`| `2M`     | ESP-IDF app |
| `assets`  | `0x210000` | `0x1F0000` | 256-byte header + RGB565 images |

Each 240×240 RGB565 image is `115200` bytes. The current assets region fits **17** photos after the header.

### Asset header (256 bytes, little-endian)

- Magic `DCAS`, format version, screen size, image count  
- Rotation interval, Pomodoro seconds, asset ID (NVS migration)  
- Timezone string, SSID, password  
- Weather latitude/longitude (µ°, stored in the reserved tail—see `web/src/asset_pack.ts`)

Credentials in flash are convenient for a desk toy; they are **not** secret unless you enable flash encryption.

---

## Developing firmware locally

```bash
idf.py set-target esp32s3   # or esp32c3 / esp32c6
idf.py build
```

## Developing / hosting the web flasher

```bash
cd web
npm ci
npm run build
```

CI builds firmware for supported targets, turns `flasher_args.json` into per-board web manifests, copies binaries into the site, and can deploy GitHub Pages.

Before changing partitions, NVS, or Wi‑Fi storage, skim the ESP-IDF docs for **partition tables**, **NVS**, **`esp_partition`**, and **Wi‑Fi station**.

### Customizing timezone / city list

Edit **`web/src/timezone_presets.ts`** (`TIMEZONE_PRESETS`: label, POSIX `value`, group, and lat/lon for weather).
