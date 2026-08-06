# Nexstar AUX Bridge

Firmware for a USB-to-Celestron AUX bridge built around a generic 30-pin
**ESP32 DevKit V1** with an ESP32-WROOM-32 module and CP2102 USB-UART bridge.

The CP2102-connected UART0 is reserved for binary Mount-USB traffic. The
telescope-facing AUX bus requires an external 3.3 V/5 V interface; do not
connect ESP32 GPIO pins directly to a mount.

## Current state

The firmware remains deliberately fail-closed:

- candidate AUX control pins are initialized to safe states before services
  start, while RX/TX bus operation remains disabled;
- normal bridge/headless profiles emit no serial banner or debug text; the
  explicitly selected capture and diagnostic profiles may report bench data;
- Wi-Fi, Bluetooth, and LoRa are not initialized;
- the firmware idles until the board-support and transport issues are
  implemented.

The former Heltec OLED implementation is retired. The external 0.96-inch
80x160 ST7735S TFT has passed supply, initialization, rotation, color-order,
backlight, steady-update, and listen-only AUX-interference checks. It is enabled
only in the diagnostic and dedicated `esp32dev_listen_only_tft` validation
profiles while the ordinary bridge and listen-only profiles remain headless.

The default build is listen-only. Pin allocation and mandatory bench checks are
documented in [`docs/pin-allocation.md`](docs/pin-allocation.md); the candidate
TX path must not be enabled until those checks pass.

This makes the image safe to provision on a board that was previously used by
another project, but it is not yet a functional AUX bridge.

## Build and test

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/index.html),
then run:

```text
pio run -e esp32dev
pio run -e esp32dev_listen_only
pio run -e esp32dev_listen_only_tft
pio run -e esp32dev_headless
pio run -e esp32dev_diagnostic
pio test -e native
```

The build log reports the firmware version and selected profile. To completely
replace an earlier project on a board connected as `COM6`:

```text
pio run -e esp32dev_listen_only -t erase --upload-port COM6
pio run -e esp32dev_listen_only -t upload --upload-port COM6
```

Confirm the port before running these commands; Windows can assign a different
COM number after reconnecting the device.
