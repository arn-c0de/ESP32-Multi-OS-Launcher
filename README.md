# ESP32 Multi-OS Launcher (Modular Component)

[![ESP-IDF Version](https://img.shields.io/badge/ESP--IDF-v5.0+-blue.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-red.svg)](https://www.espressif.com/en/products/socs/esp32)

A highly modular ESP32-IDF component that allows any application to flash other operating systems from an SD card. It transforms your ESP32 into a multi-boot system where every OS can launch another.

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
- **Dual-Partition Boot**: Full support for seamless switching between `ota_0` and `ota_1` partitions.

---

## Quick Start (Integration)

1. **Copy Component**: Copy the `components/esp_launcher` folder into your project's `components/` directory.
2. **Setup Partitions**: Use an OTA-capable `partitions.csv` (see `docs/INTEGRATION_GUIDE.md` for details).
3. **Add Code**:
   ```c
   #include "esp_launcher.h"
   
   void app_main() {
       // Checks for button press or software flag on boot
       esp_launcher_check_and_run(); 
       
       // ... your normal app logic continues here ...
   }
   ```
4. **Configure Hardware**: Run `idf.py menuconfig` -> `Component config` -> `ESP32 Multi-OS Launcher`.

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
- **SD Card (HSPI)**: MISO 35, MOSI 21, SCK 22, CS 13
- **Encoder**: CLK 33, DT 32, SW 25

---

## License
Distributed under the MIT License. See `LICENSE` for further details.
