# Nexstar AUX Bridge

Firmware for a USB-to-Celestron AUX bridge built around a generic 30-pin
**ESP32 DevKit V1** with an ESP32-WROOM-32 module and CP2102 USB-UART bridge.

The CP2102-connected UART0 is reserved for binary Mount-USB traffic. The
telescope-facing AUX bus requires an external 3.3 V/5 V interface; do not
connect ESP32 GPIO pins directly to a mount.

## Current state

The wired bridge is a verified working prototype. On 2026-08-11 the normal
`esp32dev` profile forwarded the host GET_VERSION request
`3B 03 03 10 FE EC` to a live mount and returned its checksum-valid response
`3B 05 10 03 FE 05 14 D1` over the same CP2102/UART0 endpoint. The captures
and exact timing are recorded in [`docs/bench-validation.md`](docs/bench-validation.md).
NEX-23 adds native bounded-recovery coverage for partial host-frame reconnects
and AUX timeout recovery. Hardware reconnect, long soak, and TFT comparison
remain pending; they must not be represented as physical validation while the
conditioned-RX release glitch is unresolved.

The firmware remains deliberately fail-closed outside an authorized bridge
transmission:

- candidate AUX control pins are initialized to safe states before services
  start, while RX/TX bus operation remains disabled;
- normal bridge/headless profiles emit no serial banner or debug text; the
  explicitly selected capture and diagnostic profiles may report bench data;
- Wi-Fi, Bluetooth, and LoRa are not initialized;
- the bridge profile opens UART0 at 19,200 baud as a binary-only Mount-USB
  endpoint. It incrementally validates host frames into bounded queues and
  uses nonblocking partial writes for AUX-to-host traffic; it emits no
  application banner or diagnostics on that protocol channel.
- the normal bridge profile sends checksum-valid host packets through the
  protected AUX interface only after BUSY arbitration, and forwards valid
  non-echo AUX packets back to the host; TX and BUSY return to their safe,
  released state after transmission.

The former Heltec OLED implementation is retired. The external 0.96-inch
80x160 ST7735S TFT has passed supply, initialization, rotation, color-order,
backlight, steady-update, and listen-only AUX-interference checks. It is enabled
only in the diagnostic and dedicated `esp32dev_listen_only_tft` profiles while
the ordinary bridge and listen-only profiles remain headless. The TFT image
shows a short boot screen followed by compact mode, AUX, host, packet, error,
and BUSY-timeout status; listen-only mode explicitly states that TX is locked.
Fault information is shown temporarily without performing display work in the
AUX path.

The default build is listen-only. Pin allocation and mandatory bench checks are
documented in [`docs/pin-allocation.md`](docs/pin-allocation.md); the candidate
TX path must not be enabled until those checks pass.

This makes the image safe to provision on a board that was previously used by
another project while providing a functional wired AUX bridge.

## Build and test

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/index.html),
then run:

```text
pio run -e esp32dev
pio run -e esp32dev_listen_only
pio run -e esp32dev_listen_only_tft
pio run -e esp32dev_headless
pio run -e esp32dev_diagnostic
pio run -e esp32dev_controlled_test
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

To use the optional display, upload its dedicated image instead of the default
headless image:

```text
pio run -e esp32dev_listen_only_tft -t upload --upload-port COM6
```

The ST7735S module cannot report whether it is electrically present over its
write-only SPI connection. A missing or failed display therefore cannot block
the bridge: display setup is bounded, runs in a separate task, and all normal
profiles retain a compile-time display-disabled build.

The NEX-16 image is deliberately separate from every bridge profile. It boots
with TX and BUSY released. `SELECT <destination-hex>` records the sole allowed
destination without authorizing TX; only the separate `ARM` command issues one
payload-free `GET_VERSION` (`FE`) query. `STATUS` reports its counters;
`RECOVER` releases outputs, clears the selection, and leaves transmission
unauthorized. Use it only after the hold point in
[`docs/bench-validation.md`](docs/bench-validation.md) is released.

The controlled image uses source ID `03`, the AUX command-set recommendation
for an external PC/AUX-port device. It ignores the physical echo for completion
and waits for a matching checksum-valid version response addressed to `03`.

If [just](https://github.com/casey/just) is installed, `just deps` installs the
pinned PlatformIO Core dependency, `just controlled-test` builds this image,
and `just controlled-test-upload COM6` uploads it after the port is confirmed.
