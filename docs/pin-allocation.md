# ESP32 DevKit V1 pin allocation

## Verified hardware identity

Photos supplied for NEX-20 identify the active target as a generic 30-pin
ESP32 DevKit V1 carrying:

- an ESP32-WROOM-32 module;
- a Silicon Labs CP2102 USB-to-UART bridge;
- Micro-USB, EN, and BOOT controls; and
- the standard 15-pin-per-side DevKit header layout.

The USB bridge enumerates as VID:PID `10C4:EA60` on the tested Windows host.
A read-only `esptool` probe identified:

- ESP32-D0WD-V3, silicon revision 3.1;
- dual-core 240 MHz capability and a 40 MHz crystal;
- 4 MB flash; and
- 3.3 V flash operation.

PlatformIO uses the `esp32dev` board definition.

## UART0 reset behavior

Opening the CP2102 serial port with ordinary DTR/RTS behavior reset the tested
board and produced the ESP32 ROM boot banner on UART0 before the application
started. The listen-only application emitted no subsequent bytes during the
observation window.

NEX-11 must account for this immutable reset-time output in the host protocol
design. The bridge profile configures UART0 at 19,200 baud and uses it only for
incremental binary AUX-frame input and output. Normal application logging,
TFT messages, and diagnostics remain prohibited on UART0. The transport uses
bounded queues and nonblocking writes, so a stalled or newly reconnected host
cannot block AUX-side service; ESP32 UART0 does not expose a reliable physical
disconnect event, so reconnection is handled as continued byte-stream parsing.

The external display is marked `4-SPI`, `IC: ST7735S`, `Display Color: 65K`,
and `0.96" 80x160 (RGB)`. Its header is labeled `GND VCC SCL SDA RES DC CS
BLK`; on this SPI module, `SCL` is clock and `SDA` is MOSI.

## Active pin contract

| Signal | GPIO | DevKit label | Direction | Reset-safe requirement |
| --- | ---: | --- | --- | --- |
| Host UART TX | 1 | TX0 | CP2102 | reserved for binary host transport |
| Host UART RX | 3 | RX0 | CP2102 | reserved for binary host transport |
| AUX RX | 16 | RX2 | input | external 5 V-to-3.3 V conditioning |
| AUX TX | 17 | TX2 | output | ignored while external buffer is disabled |
| AUX CTS input | 34 | D34 | input only | RJ12 pin 1; external conditioning; no internal pull |
| AUX RTS/BUSY assert | 26 | D26 | output | external pullup; high releases pin 6 |
| AUX TX enable | 27 | D27 | output | external pullup; high disables `/OE` |
| TFT clock | 18 | D18 | output | VSPI clock |
| TFT MOSI | 23 | D23 | output | VSPI MOSI |
| TFT chip select | 33 | D33 | output | driven high before SPI initialization |
| TFT data/command | 32 | D32 | output | initialized before SPI traffic |
| TFT reset | 25 | D25 | output | initialized high before panel setup |
| TFT backlight | 13 | D13 | output | active high; held low until panel setup completes |

The allocation deliberately avoids UART0 GPIO1/3, flash GPIO6-11, and
strapping GPIO0/2/5/12/15. GPIO34 is input-only and does not provide an
internal pullup or pulldown.

## Active polarities

- AUX TX uses a 74AHCT125 active-low `/OE`: high is disabled/safe.
- AUX BUSY uses another 74AHCT125 channel with its data input tied low. A low
  `/OE` asserts BUSY; a high `/OE` releases the output to high impedance.
- The conditioned CTS input is active-low. A low level means the mount has
  asserted clear-to-send; it does not mean that the bus is busy.
- TFT backlight is active high and is held low until panel initialization
  completes.

## TFT runtime behavior

The display is optional. `esp32dev_listen_only_tft` and
`esp32dev_diagnostic` enable it; the default listen-only, bridge, headless,
and controlled-test images compile it out. The ST7735S module uses write-only
SPI and does not acknowledge initialization, so firmware cannot distinguish an
unplugged panel from a working one. Initialization is bounded and runs in the
dedicated display task after AUX outputs have been made safe; a missing panel
does not block bridge startup or AUX processing.

With a panel fitted, initialization uses `INITR_MINI160x80`, 4 MHz SPI,
rotation 1, and the module's observed RGB color order. The backlight remains
off until the first boot screen has been rendered.

## Mandatory gates

Do not connect this assembly to a telescope mount or enable transmission until:

1. A clean DevKit listen-only image builds and uploads.
2. Reset and boot are observed with AUX TX-enable high and RTS/BUSY `/OE`
   high (released).
3. The complete external interface is documented in
   `docs/hardware-interface.md`.
4. All NEX-14 multimeter and logic-analyzer checks are recorded in
   `docs/bench-validation.md`.
5. AUX +12 V remains disconnected and insulated throughout USB-powered
   prototype testing.
6. NEX-15 begins with AUX TX and local BUSY physically disconnected or
   otherwise incapable of driving the mount bus.

The older Heltec V3 diagrams are historical references only and must not be
used to wire this DevKit.
