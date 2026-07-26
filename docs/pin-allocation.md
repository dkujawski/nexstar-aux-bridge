# Heltec V3 pin allocation

## Scope and revision assumption

This contract targets the **Heltec WiFi LoRa 32 V3 / ESP32-S3** selected by
PlatformIO's `heltec_wifi_lora_32_V3` variant. It is based on Heltec's original
V3/V3.1 documentation, not the later V3.2 layout. Check the silkscreen and board
revision before wiring.

Primary references:

- [Heltec V3 resource directory](https://resource.heltec.cn/download/WiFi_LoRa_32_V3/)
- [Heltec V3 schematic](https://resource.heltec.cn/download/WiFi_LoRa_32_V3/HTIT-WB32LA(F)_V3_Schematic_Diagram.pdf)
- [Heltec V3 pin map](https://resource.heltec.cn/download/WiFi_LoRa_32_V3/HTIT-WB32LA(F)_V3.png)
- [Espressif ESP32-S3 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)

The checked-in PlatformIO board variant independently identifies CP2102 UART0
as GPIO43/44, OLED as GPIO17/18/21, Vext as GPIO36, LED as GPIO35, battery ADC
as GPIO1, and SX1262/SPI as GPIO8–14. `include/board_support.hpp` is the
firmware authority for those assignments.

## Candidate AUX allocation

| Signal | GPIO | Direction | Reset-safe requirement |
| --- | ---: | --- | --- |
| `AUX_RX` | 4 | input from level shifter | high impedance |
| `AUX_TX` | 5 | output to tri-state buffer | ignored while buffer disabled |
| `AUX_BUSY_IN` | 6 | input from level shifter | high impedance |
| `AUX_BUSY_ASSERT` | 7 | output to MOSFET gate | external pulldown; low releases BUSY |
| `AUX_TX_ENABLE` | 2 | output to 74AHCT125 `/OE` | external pullup to 3.3 V; high disables TX |

GPIO4 and GPIO5 are routed through a separate ESP32-S3 hardware UART by the
GPIO matrix. GPIO2, GPIO4, GPIO5, GPIO6, and GPIO7 are exposed, are not ESP32-S3
strapping pins, and do not overlap the board-owned functions above.

The CP2102 UART remains exclusively on GPIO43/44. Do not emit logs or startup
text on it. OLED, LoRa, battery measurement, Vext, LED, boot/programming, and
native USB pins remain reserved.

## Polarity contract

- `AUX_TX_ENABLE` drives a 74AHCT125 active-low `/OE`: **high is disabled/safe**.
- `AUX_BUSY_ASSERT` drives an open-drain MOSFET gate: **low releases BUSY/safe**.
- `AUX_BUSY_IN` is logically asserted when the level-shifted shared BUSY line is
  low.
- External resistors, not firmware, establish safe states throughout reset,
  bootloader execution, brownout, and ESP32 power loss.

## Rejected alternatives

- GPIO43/44: CP2102 host protocol UART.
- GPIO17/18/21: onboard OLED.
- GPIO8–14: SX1262 and its SPI bus.
- GPIO1: battery ADC.
- GPIO35/36: LED and Vext control.
- GPIO0, GPIO3, GPIO45, GPIO46: ESP32-S3 strapping pins.
- GPIO19/20: native USB D-/D+ on this design.
- GPIO33/34/47/48: currently spare, but less convenient than the contiguous
  GPIO2/4/5/6/7 header group; retain as fallback candidates.

## Hardware-unverified assumptions

The following checks require the actual board and interface circuit and must
pass before enabling any AUX transmission:

1. Confirm the board is V3 or V3.1 and continuity-test each header pin.
2. Confirm GPIO2/4/5/6/7 are not loaded by the assembled board.
3. With the ESP32 held in reset and then unpowered, verify TX is high impedance
   and local BUSY is released.
4. Verify level-shifter polarity and that no interface node drives an ESP32 pin
   above 3.3 V.
5. Build listen-only firmware first and compare RX/BUSY signals with a logic
   analyzer before fitting or enabling the TX path.

Until those checks are recorded, this allocation is a reviewed **candidate**,
not a validated telescope interface.
