# ESP32 Multi-OS Launcher (Modular Component)

[![ESP-IDF Version](https://img.shields.io/badge/ESP--IDF-v5.0+-blue.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-red.svg)](https://www.espressif.com/en/products/socs/esp32)

A modular ESP32-IDF component that lets any application flash other operating systems from an SD card. It turns an ESP32 into a multi-boot system where each integrated firmware can launch the same shared updater flow.

The launcher component provides the shared update menu and flashing flow only. It does not replace the normal UI of the application that integrates it.

> ## Development Status
> 
> This project is in active development and primarily serves as a component for testing multi-boot workflows in custom projects.
> Because of that, behavior and interfaces may change continuously over time.
> Stable versions are provided as packages and can be imported into your own projects under the MIT License.
>
---

## Required Hardware

To build a full Multi-OS setup, the following hardware components are required:

| Part | Description | Recommended Model |
|------|-------------|-------------------|
| **Microcontroller** | ESP32 Development Board | ESP32-WROOM-32 |
| **Storage** | Micro SD Card Module | SPI Interface Module |
| **Input** | Rotary Encoder | KY-040 (or similar) |
| **Input** | Boot/Activation Button | Tactile Momentary Switch |
| **Display (Optional)** | TFT Display | ST7796 (SPI) or similar |
| **Memory** | Micro SD Card | 4GB - 32GB (FAT32 formatted) |

---

## Key Features
- **Ultra-Small Footprint**: SD and UI logic are only loaded when the launcher is activated, saving RAM for your main application.
- **Safe OTA Flashing**: Uses the native `esp_ota` API to always flash to the inactive partition, preventing system corruption.
- **Kconfig Integration**: Configure all GPIO pins (SD, Encoder, Button) via `menuconfig` without modifying source code.
- **Software Reboot**: Switch between operating systems directly from your application's software menu.
- **Shared Standard Menu**: The launcher owns the same boot-or-browse flow for every integrated firmware, so apps no longer need to carry their own launcher menu code.
- **Gladiator-Style Launcher UI**: The built-in launcher menu uses the Gladiator launcher look on an ST7796 SPI display by default, while staying neutrally branded as `MULTI-OS LAUNCHER`.
- **Serial Fallback**: If the TFT UI is disabled or the panel cannot be initialized, the launcher falls back to the serial UI automatically.
- **Dual-Partition Boot**: Full support for seamless switching between `ota_0` and `ota_1` partitions.

---

## Quick Start (Integration)

1. **Copy Component**: Copy the `components/esp_launcher` folder into your project's `components/` directory.
2. **Setup Partitions**: Use an OTA-capable `partitions.csv` (see `docs/INTEGRATION_GUIDE.md` for details).
3. **Add Code**:
   ```c
   #include "esp_launcher.h"
   
   void app_main() {
       esp_launcher_set_app_label("Example App");

       // Checks for button press or software flag on boot
       esp_launcher_check_and_run(); 
       
       // ... your normal app logic continues here ...
   }
   ```
4. **Configure Hardware**: Run `idf.py menuconfig` -> `Component config` -> `ESP32 Multi-OS Launcher`.

After integration, the shared launcher flow is:
- `Boot <App Name>`
- `SD Card Browser`
- `Select Firmware`

This only standardizes the launcher menu and flashing flow. Your firmware's normal UI and application menus remain your own.

## Firmware Packaging

Build your application as usual and copy the generated `.bin` file to the SD card. The launcher looks for firmware files in:
- `/firmware`
- `/firmwares`
- `/`

Example:

```text
/firmware/My_App_v1.0.0.bin
```

Use `esp_launcher_set_app_label("My App")` if you want the shared entry to be shown as `Boot My App`.

---

## Project Structure
- `components/esp_launcher/`: The core modular component.
- `main/`: A minimal template application.
- `docs/`:
    - `INTEGRATION_GUIDE.md`: Comprehensive addition guide.
    - `HARDWARE_PINOUT.md`: Default wiring and SPI sharing details.
    - `SD_FIRMWARE_PACKAGING.md`: SD card preparation instructions.

---

## Default Hardware Configuration (AHH-1.0 Handheld)
*(Configurable via Kconfig)*
- **Activation Button**: GPIO 26
- **SD Card (HSPI)**: MISO 12, MOSI 14, SCK 22, CS 13
- **Encoder**: CLK 33, DT 32, SW 25
- **Display (VSPI / ST7796)**: CS 5, DC 16, RST 17, BL 4, MOSI 23, MISO 19, SCK 18

## Launcher UI Notes
- The shared TFT launcher UI is intended to carry over the Gladiator launcher style, not the complete Gladiator firmware UI.
- Display support is controlled in `menuconfig` with `Enable built-in ST7796 TFT launcher UI`.
- The launcher menu remains branded as `MULTI-OS LAUNCHER` so the component can be reused across different projects.
- If the TFT UI is unavailable, the same launcher flow remains available over Serial.

---

## License
Distributed under the MIT License. See `LICENSE` for further details.


## Pushbuttons (AHH-1.0)

- Pushbutton 1: GPIO26
- Pushbutton 2: GPIO27
