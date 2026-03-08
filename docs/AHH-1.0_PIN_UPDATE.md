# AHH-1.0 Handheld: SD SPI Pin Specification Update

This document details the critical pin change for the **AHH-1.0 Handheld** hardware revision. To ensure maximum stability and compatibility with common SD card modules, the SD card bus has been moved to a dedicated SPI bus (HSPI).

## Change Overview

| Component | Signal | Old GPIO | **New GPIO (AHH-1.0)** | Reason |
| :--- | :--- | :--- | :--- | :--- |
| **SD Card** | **MISO** | 19 | **35** | Input-only pin, avoids bus conflict with TFT. |
| **SD Card** | **MOSI** | 23 | **21** | Separate SPI bus (HSPI). |
| **SD Card** | **SCK**  | 18 | **22** | Separate SPI bus (HSPI). |
| **SD Card** | **CS**   | 13 | **13** | (Unchanged) |

## Technical Justification

Many generic SD card modules fail to properly release the MISO line (MISO Tri-state issue) when their Chip Select (CS) is high. When the TFT display and SD card shared the same SPI bus, this resulted in:
1.  **Display Glitches:** Corrupted graphics or total black screen when the SD card was inserted.
2.  **Mount Failures:** Random SD card mount failures due to signal interference.

By moving the SD card to the **HSPI peripheral** using previously unused GPIOs (21, 22) and utilizing an input-only pin for MISO (35), the two peripherals no longer share data lines. 

## Implementation Details

The following components have been updated to reflect these changes:
- **Firmware:** `firmware/ESP_SATDUMP/launcher.cpp` now defaults to the new pins.
- **Bootloader/Launcher:** `ESP32-Multi-OS-Launcher/components/esp_launcher/Kconfig` updated with new defaults.
- **Documentation:** `HARDWARE_PINOUT_EN.md` and `docs/pinout.md` updated.

## Schematic Reference (AHH-1.0 Handheld)

```text
[ESP32]             [SD CARD]
GPIO 13  -------->  CS
GPIO 21  -------->  MOSI
GPIO 35  <--------  MISO
GPIO 22  -------->  SCK
```

The TFT display continues to use the standard **VSPI** pins (GPIO 18, 19, 23, 5).
