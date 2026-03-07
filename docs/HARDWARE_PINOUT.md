# ESP32 Multi-OS Launcher: Hardware Pinout

This document describes the default wiring for the Multi-OS Launcher. You can change these pins in your project via `idf.py menuconfig`.

## Core System
| Feature | GPIO | Description |
|---------|------|-------------|
| **Boot Button** | 26 | Hold LOW to enter Launcher during boot |

## SD Card (SPI2)
| SPI Pin | GPIO |
|---------|------|
| **MOSI** | 23 |
| **MISO** | 19 |
| **SCK** | 18 |
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
| **TFT_MOSI** | 23 (Shared with SD) |
| **TFT_MISO** | 19 (Shared with SD) |
| **TFT_SCK** | 18 (Shared with SD) |

## SPI Bus Sharing
The SD card and the Display share the same SPI bus (SPI2).
- Use **GPIO 13** as Chip Select (CS) for the SD card.
- Use **GPIO 5** as Chip Select (CS) for the Display.
