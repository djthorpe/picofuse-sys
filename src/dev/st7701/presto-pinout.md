# ST7701 + Touch Pinout (Presto / RP2350)

This note captures the display FPC pinout and RP2350 signal routing from the provided schematic screenshots.

## Scope

- Display controller: ST7701 (RGB interface, SPI control)
- Touch controller signals on same FPC
- MCU: RP2350B

## RP2350 Signal Mapping

### Display Control/Timing

| Signal | RP2350 GPIO |
| --- | --- |
| LCD_RESET | GPIO44 |
| LCD_SPI_CLK | GPIO26 |
| LCD_SPI_DATA | GPIO27 |
| LCD_SPI_CS | GPIO28 |
| LCD_DOT_CLK | GPIO22 |
| LCD_DE | GPIO21 |
| VSYNC | GPIO20 |
| HSYNC | GPIO19 |
| BACKLIGHT_EN | GPIO45 |

### Display RGB Data Bus

| Signal | RP2350 GPIO |
| --- | --- |
| R2 | GPIO17 |
| R3 | GPIO16 |
| R4 | GPIO15 |
| R5 | GPIO14 |
| R6 | GPIO13 |
| R7 | GPIO12 |
| G2 | GPIO11 |
| G3 | GPIO10 |
| G4 | GPIO9 |
| G5 | GPIO8 |
| G6 | GPIO7 |
| G7 | GPIO6 |
| B2 | GPIO18 |
| B3 | GPIO5 |
| B4 | GPIO4 |
| B5 | GPIO3 |
| B6 | GPIO2 |
| B7 | GPIO1 |

### Touch Signals

| Signal | RP2350 GPIO |
| --- | --- |
| TOUCH_INT | GPIO32 |
| TOUCH_SDA | GPIO30 |
| TOUCH_SCL | GPIO31 |
| TOUCH_RESET | GPIO42 |

## FPC-40 Pin Map

| FPC Pin | Net |
| --- | --- |
| 1 | LEDA |
| 2 | LEDK |
| 3 | LEDK |
| 4 | GND (as drawn) |
| 5 | 3V3 |
| 6 | LCD_RESET |
| 7 | IM1 (NC) |
| 8 | GND |
| 9 | LCD_SPI_DATA |
| 10 | LCD_SPI_CLK |
| 11 | LCD_SPI_CS |
| 12 | LCD_DOT_CLK |
| 13 | LCD_DE |
| 14 | VSYNC |
| 15 | HSYNC |
| 16 | R2 |
| 17 | R3 |
| 18 | R4 |
| 19 | R5 |
| 20 | R6 |
| 21 | R7 |
| 22 | G2 |
| 23 | G3 |
| 24 | G4 |
| 25 | G5 |
| 26 | G6 |
| 27 | G7 |
| 28 | B2 |
| 29 | B3 |
| 30 | B4 |
| 31 | B5 |
| 32 | B6 |
| 33 | B7 |
| 34 | GND (as drawn) |
| 35 | TOUCH_INT |
| 36 | TOUCH_SDA |
| 37 | TOUCH_SCL |
| 38 | TOUCH_RESET |
| 39 | 3V3 |
| 40 | GND (as drawn) |

## Notes

- Pins marked "GND (as drawn)" are based on the screenshot wiring and should be re-verified against the source CAD/netlist before final board bring-up.
- ST7701 data bus appears to be 18-bit RGB666 style (`R2..R7`, `G2..G7`, `B2..B7`).
- Touch bus appears to be I2C with a dedicated interrupt and reset line.
