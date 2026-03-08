# ESP32 Multi-OS Launcher: Hardware Pinout

This document describes the default wiring for the Multi-OS Launcher. You can change these pins in your project via `idf.py menuconfig`.

## Core System
| Feature | GPIO | Description |
|---------|------|-------------|
| **Boot Button** | 26 | Hold LOW to enter Launcher during boot |

## SD Card (HSPI - Dedicated Bus AHH-1.0)
| SPI Pin | GPIO |
|---------|------|
| **MOSI** | 21 |
| **MISO** | 35 |
| **SCK** | 22 |
| **CS** | 13 |

## Encoder
| Pin | GPIO |
|---------|------|
| **CLK** | 33 |
| **DT** | 32 |
| **SW (Button)** | 25 |

## Display (ST7796 4-Wire SPI)
*Note: The modular component currently uses a Serial UI. These are the recommended pins for your application's display.*

| Pin | GPIO |
|---------|------|
| **TFT_CS** | 5 |
| **TFT_DC** | 16 |
| **TFT_RST** | 17 |
| **TFT_BL** | 4 |
| **TFT_MOSI** | 23 |
| **TFT_MISO** | 19 |
| **TFT_SCK** | 18 |

## SPI Bus Configuration (AHH-1.0)
For the AHH-1.0 Handheld, the SD card and the Display use **separate SPI buses** to avoid signal interference.
- **SD Card:** Uses the **HSPI** peripheral (GPIO 21, 22, 35).
- **Display:** Uses the **VSPI** peripheral (GPIO 18, 19, 23).
- **CS Pins:** SD uses **GPIO 13**, Display uses **GPIO 5**.
