# AUX electrical interface

Status: **bench validated for USB-powered listen-only use; active AUX
transmission remains prohibited**

This document is the authoritative NEX-14 interface description for the
USB-powered, listen-only breadboard prototype. Active transmission requires a
separate controlled-transmission gate.

## Fixed constraints

- The ESP32 is a 3.3 V-only device and no GPIO may be exposed to 5 V.
- The first prototype is USB powered.
- AUX +12 V is disconnected and insulated.
- AUX TX must default to high impedance.
- Local BUSY assertion must default to released.
- GPIO27 is the TX-enable control.
- GPIO26 is the BUSY-assert control.
- GPIO16 receives conditioned AUX RX.
- GPIO34 receives conditioned active-low AUX CTS from RJ12 pin 1 and has no
  internal pull resistor.
- All grounds must be common during bench testing.

## Candidate architecture

The intended design uses:

- one SN74AHCT125N channel, powered from 5 V, as the tri-state AUX TX driver;
- a pull resistor that holds the active-low SN74AHCT125 `/OE` disabled while
  the ESP32 is resetting;
- resistor-divider or explicitly verified buffered inputs for AUX RX and CTS;
- a second SN74AHCT125 channel with its data input tied to ground, allowing
  local BUSY to be driven low or released to high impedance;
- local decoupling at the SN74AHCT125; and
- optional small series resistors on driven signal paths.

## Selected SN74AHCT125N channel allocation

The photographed device is a Texas Instruments SN74AHCT125N, 14-pin PDIP.
Pin numbering is viewed from the top with the notch/dot at the pin 1 end.

| Pin | Name | Prototype connection |
| ---: | --- | --- |
| 1 | `1OE` | GPIO27 plus pullup to 3.3 V; HIGH disables AUX TX |
| 2 | `1A` | GPIO17 / AUX UART TX |
| 3 | `1Y` | AUX TX through an optional small series resistor |
| 4 | `2OE` | GPIO26 plus pullup to 3.3 V; HIGH releases BUSY |
| 5 | `2A` | Ground |
| 6 | `2Y` | AUX BUSY through an optional small series resistor |
| 7 | GND | Common ground |
| 8 | `3Y` | Not connected |
| 9 | `3A` | Ground |
| 10 | `3OE` | 5 V / disabled |
| 11 | `4Y` | Not connected |
| 12 | `4A` | Ground |
| 13 | `4OE` | 5 V / disabled |
| 14 | VCC | 5 V |

A 0.1 µF ceramic bypass capacitor is required directly between pins 14 and 7.
The 3.3 V `/OE` pullups are appropriate for this USB-powered prototype because
the AHCT input-high threshold is 2 V. The buffer and DevKit power down
together. Any later AUX-powered design requires a fresh power-sequencing
review.

## Selected resistor values

The available resistors are 1%, 1/4 W metal-film parts.

- TX `/OE` pullup: 10 kΩ from pin 1 to 3.3 V.
- BUSY `/OE` pullup: 10 kΩ from pin 4 to 3.3 V.
- AUX TX series resistor: 100 Ω between pin 3 and the future AUX TX node.
- BUSY series resistor: 100 Ω between pin 6 and the future AUX BUSY node.
- AUX RX divider upper leg: 5.1 kΩ plus 2 kΩ in series from the 5 V signal.
- AUX RX divider lower leg: 10 kΩ from GPIO16 to ground.
- AUX CTS divider upper leg: 5.1 kΩ plus 2 kΩ in series from RJ12 pin 1.
- AUX CTS divider lower leg: 10 kΩ from GPIO34 to ground.

The divider ratio is `10 / (5.1 + 2 + 10)`, producing approximately:

| Input | ESP32 divider output |
| ---: | ---: |
| 4.5 V | 2.63 V |
| 5.0 V | 2.92 V |
| 5.5 V | 3.22 V |

This keeps the nominal 5 V input below 3.3 V while remaining above the ESP32
logic-high threshold across the planned range. The actual voltage must be
measured before either divider is connected to an ESP32 GPIO.

The resistor values and 0.1 µF bypass capacitor were verified before assembly;
the completed measurements are recorded in `docs/bench-validation.md`.

## AUX flow-control terminology

The NexStar AUX Command Set Issue 1.0 identifies RJ12 pin 1 as active-low CTS
from the mount/interconnect and pin 6 as the accessory's RTS output. The
prototype's low-only/high-impedance channel on pin 6 is therefore the RTS/BUSY
assertion driver. A steady LOW observed on conditioned pin 1 means
clear-to-send is asserted; it must not be reported as BUSY asserted.

Reference: https://paquettefamily.ca/nexstar/NexStar_AUX_Commands_10.pdf,
Chapter 3, AUX port access.
