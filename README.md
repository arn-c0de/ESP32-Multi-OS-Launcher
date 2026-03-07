# ESP32 Multi-OS Launcher (Modular Component)

This project has been transformed into a **highly modular ESP32-IDF component**. You can now integrate this launcher into ANY of your ESP32 projects with just a few lines of code.

## Key Features
- **Ultra-Small Footprint**: Only loads SD and UI logic when the launcher is actually activated.
- **Safe OTA Flashing**: Uses `esp_ota` to always flash to the inactive partition, ensuring your device never "bricks".
- **Kconfig Integration**: Configure all GPIO pins (SD, Encoder, Button) via `menuconfig` without changing code.
- **Software Reboot**: Switch OS from your app's software menu.

## Quick Start (Integration)

1. **Copy Component**: Copy the `components/esp_launcher` folder into your project's `components/` directory.
2. **Setup Partitions**: Ensure your `partitions.csv` has two OTA partitions (`ota_0` and `ota_1`).
3. **Add Code**:
   ```c
   #include "esp_launcher.h"
   
   void app_main() {
       esp_launcher_check_and_run(); // This handles everything!
       // ... your app logic ...
   }
   ```
4. **Configure Pins**: Run `idf.py menuconfig` and go to `ESP32 Multi-OS Launcher` to set your GPIOs.

## Folder Structure
- `components/esp_launcher/`: The core modular component.
- `main/`: A minimal example application.
- `docs/`: Detailed integration and firmware packaging guides.

## Hardware Support
Default pins (configurable via Kconfig):
- **Activation Button**: GPIO 26
- **SD Card**: SPI2 (MISO 19, MOSI 23, SCK 18, CS 13)
- **Encoder**: CLK 33, DT 32, SW 25
