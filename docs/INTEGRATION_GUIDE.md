# ESP32 Multi-OS Launcher: Integration Guide

This guide explains how to integrate the modular `esp_launcher` component into any of your ESP32 projects. This allows every application to flash other operating systems from an SD card safely.

## 1. Project Structure

Copy the `components/esp_launcher` folder into your project's `components` directory:

```text
your_project/
├── CMakeLists.txt
├── main/
│   ├── CMakeLists.txt
│   └── main.c
└── components/
    └── esp_launcher/      <-- Copy this folder here
```

## 2. Partition Table Requirements

To support safe flashing between multiple OSs, your project **must** use an OTA-capable partition table. Create a `partitions.csv` in your project root:

```csv
# Name,     Type, SubType, Offset,   Size,     Flags
nvs,         data, nvs,     ,        0x6000,
otadata,     data, ota,     ,        0x2000,
phy_init,    data, phy,     ,        0x1000,
ota_0,       app,  ota_0,   ,        2M,
ota_1,       app,  ota_1,   ,        2M,
```

Enable this in `menuconfig`:
`Partition Table` -> `Partition Table Config` -> `Custom partition table CSV`

## 3. Configuration (Kconfig)

Instead of editing header files, you can now configure all GPIO pins via the ESP-IDF configuration menu:

1. Run `idf.py menuconfig` (or use the VS Code ESP-IDF extension).
2. Navigate to **"Component config" -> "ESP32 Multi-OS Launcher"**.
3. Set your pins for:
   - **Boot/Activation Button** (e.g., GPIO 26)
   - **SD Card SPI** (MOSI, MISO, SCK, CS)
   - **Encoder Interface** (CLK, DT, SW)
   - **Firmware Directory** (e.g., `/firmware`)

## 4. Code Integration

In your `main/main.c`, call the launcher check at the very beginning of `app_main()`:

```c
#include "esp_launcher.h"

void app_main(void)
{
    // 1. Initial check: If button is held or software flag is set, enter launcher.
    // This handles SD mounting, UI, and OTA flashing.
    esp_launcher_check_and_run();

    // 2. Normal application logic starts here
    printf("Application started...\n");
}
```

## 5. Software-Triggered Reboot

To switch to another OS from your application's menu, simply call:
```c
esp_launcher_reboot_to_launcher();
```
The ESP32 will restart and immediately enter the SD selection menu.

## 6. Safety Mechanism

The launcher uses the `esp_ota` API. It identifies the **currently inactive** partition and flashes the new OS there. This ensures that:
1. Your current OS is never overwritten during the flash process.
2. If the flash fails, the device simply boots back into the current OS.
3. Every OS you flash can also contain the launcher, creating a seamless multi-boot environment.
