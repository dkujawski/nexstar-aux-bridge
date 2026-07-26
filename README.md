# Nexstar AUX Bridge

Firmware for a USB-to-Celestron AUX bridge built around the **Heltec WiFi LoRa
32 V3** (ESP32-S3).

The board's CP2102-connected UART is reserved for binary Mount-USB traffic. The
telescope-facing AUX bus requires an external 3.3 V/5 V interface; do not
connect the Heltec GPIO pins directly to a mount. LoRa is disabled and outside
the scope of this project.

## Current state

The firmware remains deliberately fail-closed:

- candidate AUX control pins are initialized to safe states before services
  start, while RX/TX bus operation remains disabled;
- no serial banner or debug text is emitted;
- Wi-Fi, Bluetooth, and LoRa are not initialized;
- the firmware idles until the board-support and transport issues are
  implemented.

The onboard OLED shows mode, host/AUX state, packet activity, counters, errors,
and battery voltage when available, without writing to the protocol UART.
Blocking I2C work runs in a low-priority display task fed by a one-element
snapshot mailbox. Build the `heltec_v3_headless` environment to compile the
display implementation and its libraries out.

The default build is listen-only. Pin allocation and mandatory bench checks are
documented in [`docs/pin-allocation.md`](docs/pin-allocation.md); the candidate
TX path must not be enabled until those checks pass.

This makes the image safe to provision on a board that was previously used by
another project, but it is not yet a functional AUX bridge.

## Build and test

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/index.html),
then run:

```text
pio run -e heltec_v3
pio run -e heltec_v3_listen_only
pio run -e heltec_v3_headless
pio test -e native
```

The build log reports the firmware version and selected profile. To completely
replace an earlier project on a board connected as `COM6`:

```text
pio run -e heltec_v3 -t erase --upload-port COM6
pio run -e heltec_v3 -t upload --upload-port COM6
```

Confirm the port before running these commands; Windows can assign a different
COM number after reconnecting the device.
