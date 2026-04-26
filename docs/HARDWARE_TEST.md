# Hardware Test Checklist

Use this checklist for each supported board profile before publishing a release.

## Setup

- Browser: Chrome or Edge with Web Serial support.
- USB cable supports data, not charge-only.
- Serial monitor is closed before flashing.
- Board has at least 4 MB flash.

## Flash Flow

1. Open the GitHub Pages flasher.
2. Select the matching board profile.
3. Upload 1, 3, and maximum-count photo sets in separate runs.
4. Enter a known-good SSID, password, and timezone.
5. Flash and confirm the progress reaches 100%.
6. Confirm the board resets and logs a valid asset header.

## Firmware Behavior

- Device joins Wi-Fi and starts SNTP without hardcoded credentials.
- Date/time overlay appears over the gallery image.
- Photos rotate at the selected interval.
- Short press cycles date/time -> countdown -> stopwatch.
- Long press starts and pauses countdown.
- Long press starts and pauses stopwatch.
- No red Pomodoro screen or Pomodoro-specific behavior remains.

## Resource Checks

- Confirm no full-screen framebuffer allocation is added.
- Confirm UI task owns all display calls.
- Log FreeRTOS stack high-water marks during debug builds before release.
- Reflash with different Wi-Fi credentials and confirm NVS migration uses the new asset ID.
