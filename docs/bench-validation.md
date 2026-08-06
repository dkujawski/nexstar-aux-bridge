# Bench validation record

This file records human-operated electrical checks for NEX-14. A passing
firmware-only check does not authorize connection to a telescope mount.

## Gate 1 — Bare DevKit identity and safe application levels

Date: 2026-07-30

Configuration:

- Generic 30-pin ESP32 DevKit V1
- ESP32-D0WD-V3 revision 3.1
- CP2102 USB-UART on COM6
- 4 MB, 3.3 V flash
- `esp32dev_listen_only` firmware
- USB powered
- No TFT, breadboard, AUX cable, or other GPIO wiring attached

| Check | Expected | Measured | Result |
| --- | ---: | ---: | --- |
| Power LED | On | On | PASS |
| GPIO27 / AUX TX disable | approximately 3.3 V | 3.3 V | PASS |
| GPIO26 / pre-interface firmware level | approximately 0 V | 0 V | HISTORICAL |

Observations:

- The board enumerated as a Silicon Labs CP210x device at USB VID:PID
  `10C4:EA60`.
- The listen-only image uploaded without requiring EN or BOOT button input.
- Opening UART0 with normal DTR/RTS behavior reset the board and exposed the
  ESP32 ROM boot banner. No application bytes followed during the observation
  window.

The BUSY topology was subsequently improved to use a second SN74AHCT125
channel as a low-only/high-impedance driver. The corrected firmware releases
BUSY with GPIO26 HIGH. After reflashing the corrected listen-only image, the
bare-board levels were repeated:

| Check | Expected | Measured | Result |
| --- | ---: | ---: | --- |
| GPIO27 / AUX TX disable | approximately 3.3 V | 3.3 V | PASS |
| GPIO26 / BUSY release | approximately 3.3 V | 3.3 V | PASS |

Gate status: **PASS for corrected bare-board application-level checks.**

## Gate 2 — Loose component identification

Date: 2026-07-30

| Component | Nominal | Measured/marked | Result |
| --- | ---: | ---: | --- |
| Pullup/divider resistor | 10 kΩ ±1% | 9.99 kΩ | PASS |
| Divider upper resistor | 5.1 kΩ ±1% | 5.05 kΩ | PASS |
| Divider upper resistor | 2 kΩ ±1% | 2.0 kΩ | PASS |
| Output series resistor | 100 Ω ±1% | 100 Ω | PASS |
| Bypass capacitor | 0.1 µF | marked `104` | PASS |
| Tri-state buffer | SN74AHCT125N | TI-marked 14-pin PDIP | PASS |

Gate status: **PASS for loose component identity and nominal values.**

## Gate 3 — DevKit-derived breadboard rails

Date: 2026-07-30

Configuration:

- DevKit connected to the breadboard with female-to-male jumpers
- Left red rail assigned to USB-derived `VIN`
- Right red rail assigned to `3V3`
- Left and right blue rails bonded as common ground
- SN74AHCT125 not yet connected to any rail

| Check | Measured | Result |
| --- | ---: | --- |
| 5 V rail to ground | 4.72 V | PASS |
| 3.3 V rail to ground | 3.305 V | PASS |
| Ground rail difference | 0 V | PASS |

The measured 4.72 V rail is within the SN74AHCT125 recommended 4.5–5.5 V
supply range.

Gate status: **PASS for unloaded prototype power rails.**

## Gate 4 — Protected interface assembly and functional checks

Dates: 2026-07-30 through 2026-08-01

Configuration:

- SN74AHCT125 powered from the measured 4.72 V rail with a `104` bypass
  capacitor at pins 14 and 7
- 10 kOhm pullups from pins 1 and 4 to 3.3 V
- channel 1 used as the 3.3 V-to-5 V tri-state TX driver
- channel 2 input tied low and used as the low-only/high-impedance BUSY driver
- unused channel inputs grounded and their active-low enables tied to 5 V
- 100 Ohm series resistors on TX and BUSY outputs
- 7.1 kOhm / 10 kOhm dividers on AUX RX and CTS inputs

| Check | Result |
| --- | --- |
| Power-off rail and signal short checks | PASS |
| GPIO27-to-pin-1 pullup, measured 9.98 kOhm | PASS |
| GPIO26-to-pin-4 pullup, measured 9.96 kOhm | PASS |
| AUX RX divider simulated low/high/low | PASS |
| CTS input divider simulated low/high/low | PASS |
| TX disabled, enabled-low, and translated-high states | PASS |
| BUSY released-high and asserted-low states | PASS |
| Normal listen-only levels: GPIO27 HIGH, GPIO26 HIGH | PASS |
| Normal unconnected inputs settle low | PASS |

Gate status: **PASS for the assembled protected interface.**

## Gate 5 — Passive live-bus capture

Date: 2026-08-01

Cable-specific mapping for the new 6P6C lead was continuity-tested with no
cross-conductor shorts:

| RJ12 pin | AUX signal | Conductor | Prototype connection |
| ---: | --- | --- | --- |
| 1 | CTS from mount | white | CTS divider source `j22` |
| 2 | data from mount | black | AUX RX divider source `a22` |
| 3 | +12 V | red | insulated; no prototype connection |
| 4 | data to mount | green | disconnected |
| 5 | ground | yellow | common ground |
| 6 | RTS/BUSY output | blue | disconnected |

Mount-powered static measurements:

| Node | Measured |
| --- | ---: |
| AUX RX raw (`a22`) | 1.7 V average during traffic |
| AUX RX conditioned (`d26` / GPIO16) | 0.96 V average during traffic |
| CTS raw (`j22`) | 0.23 V |
| CTS conditioned (`f26` / GPIO34) | 0.23 V |

The receive-only firmware used UART2 at 19,200 baud with GPIO16 as RX and no
TX pin. During the controlled hand-controller action, counters advanced from
104 bytes / 5 valid frames to 191 bytes / 16 valid frames with zero rejected
frames. Captured command/reply packets included repeated `0D` to `10` and
`10` to `0D` traffic. Status reports held `oe_tx=1` and `oe_busy=1` throughout.

Initial live-capture gate status: **PASS with outputs physically
disconnected.** The subsequent soak and logic-analyzer sections complete the
remaining NEX-15 evidence.

### Thirty-minute passive soak

Date: 2026-08-01

The receive-only build ran continuously for 1,800 seconds while attached to a
powered mount. Final firmware counters were:

| Counter/state | Final value |
| --- | ---: |
| Received UART bytes | 603 |
| Valid decoded frames | 68 |
| Rejected frames | 0 |
| TX active-low output enable | 1 (disabled) |
| BUSY active-low output enable | 1 (released) |
| Conditioned CTS state | 1 (asserted/low; clear-to-send) |

The log contains 65 packet lines captured after the USB monitor opened; the
firmware had decoded three frames immediately before logging began. Ten unique
valid packet byte sequences were observed, including traffic between device
IDs `0D`, `10`, and `11`. The raw timestamped capture is stored at
`docs/captures/nex15-passive-soak-2026-08-01.log`.

Soak status: **PASS for duration, parser stability, checksum validation, and
non-driving safety.** The pin-1 input remained low throughout the sample. The
logic-analyzer comparison below confirms that observation and the documented
pinout identifies it as active-low CTS, not BUSY asserted.

### Logic-analyzer comparison

Date: 2026-08-01

A generic eight-channel FX2 logic analyzer running `fx2lafw` sampled at 1 MHz:

- D0 connected to conditioned AUX RX at `d26`;
- D1 connected to conditioned CTS at `f26`; and
- analyzer ground connected to the prototype common ground.

The startup capture decoded a checksum-valid frame
`3B 03 0D 11 05 DA`. A controlled rate-1 direction-button capture decoded
`3B 04 0D 10 24 01 BA`, also checksum-valid and matching the firmware's
previously reported `0D` to `10` traffic. Message `24 01` is the documented
positive-movement command at rate 1.

D1 remained LOW before, during, and after every captured UART transaction.
This agrees with both the earlier GPIO34 status and the NexStar AUX Command Set
pinout: RJ12 pin 1 is active-low CTS, so LOW means clear-to-send. RJ12 pin 6 is
the accessory RTS/BUSY output and remained physically disconnected throughout
listen-only testing.

Raw PulseView sessions:

- `docs/captures/nex15-analyzer-startup-2026-08-01.sr`
- `docs/captures/nex15-analyzer-controlled-input-2026-08-01.sr`

Logic-analyzer comparison status: **PASS for UART agreement, CTS polarity,
and non-driving safety.**

NEX-15 bench evidence status: **COMPLETE.** This does not authorize active AUX
transmission; the RTS/BUSY and TX output conductors remain disconnected until
the controlled-transmission gate is explicitly started.

## Gate 6 — Reset and power-loss safety

Date: 2026-08-01

With the telescope disconnected and the protected interface assembled:

- GPIO27 and GPIO26 showed no noticeable voltage change while EN was held and
  released; the external pullups kept both active-low enables disabled.
- During separate USB-removal checks, GPIO27 and GPIO26 decayed from 3.3 V to
  0 V without an observed positive excursion.
- With USB removed, both power rails measured approximately 0 V.
- Protected TX (`a18`) and BUSY (`c21`) output test points had no continuity
  to ground.

Gate status: **PASS for EN reset, USB disconnect, and unpowered state.**

## Remaining NEX-14 gates

- [x] Select and document the exact RX, TX-enable, and BUSY circuits.
- [x] Verify resistor and semiconductor values before assembly.
- [x] Check resistance for shorts with all power disconnected.
- [x] Verify 3.3 V and 5 V rails before inserting logic devices.
- [x] Measure conditioned AUX RX and BUSY high/low voltages.
- [x] Measure AUX TX high, low, and high-impedance states.
- [x] Measure BUSY assert and release behavior.
- [x] Repeat measurements during EN reset, USB disconnect, and power-off.
- [x] Confirm TFT activity does not alter AUX safe levels. The display remained
  stable during startup and controlled-input captures; GPIO27 measured 3.27 V
  and GPIO26 measured 3.28 V with TFT and AUX reception active.
- [x] Complete the 30-minute passive soak under NEX-15: 603 bytes, 68 valid
  frames, zero rejected frames, and both output drivers disabled throughout.
- [x] Compare UART and CTS against a logic-analyzer trace and resolve the
  continuously-low input: RJ12 pin 1 is active-low clear-to-send, not BUSY.

All applicable NEX-14 gates now pass. AUX +12 V must nevertheless remain
disconnected and insulated, and the prototype must not drive a telescope bus
until the separate controlled-transmission work is explicitly authorized.

## Gate 7 — External TFT and AUX-interference validation

Date: 2026-08-04

The external display was identified and wired as a 0.96-inch, 80-by-160,
ST7735S four-wire SPI module:

| TFT pin | Prototype connection |
| --- | --- |
| GND | common ground |
| VCC | DevKit 3.3 V rail |
| SCL | GPIO18 / SPI clock |
| SDA | GPIO23 / SPI MOSI |
| RES | GPIO25 |
| DC | GPIO32 |
| CS | GPIO33 |
| BLK | GPIO13, active high |

Power-off continuity tests confirmed every intended connection, no VCC-to-GND
short, and no signal-to-power-rail shorts. The diagnostic firmware initialized
the panel with `INITR_MINI160x80`, rotation 1, and a 4 MHz SPI clock. Red,
green, and blue bars and diagnostic text rendered with the expected color
order and orientation. The backlight stayed off until initialization completed.

The first combined listen-only/TFT capture received 6 valid startup frames
without display corruption. A controlled-input run reached 77 valid frames;
its full-screen redraw caused visible flicker, which was corrected by limiting
runtime updates to the bottom status row at no more than 2 Hz. The final
controlled-input run reached 23 valid frames with a steady display.

Final active-workload safety levels were:

| Check | Measured | Result |
| --- | ---: | --- |
| GPIO27 / AUX TX disable | 3.27 V | PASS |
| GPIO26 / RTS/BUSY release | 3.28 V | PASS |
| TFT reset, corruption, or blanking | none observed | PASS |
| AUX receive counter advancement | 23 valid frames | PASS |

Raw PulseView sessions:

- `docs/captures/nex14-tft-aux-startup-2026-08-04.sr`
- `docs/captures/nex14-tft-aux-controlled-2026-08-04.sr`
- `docs/captures/nex14-tft-aux-final-controlled-2026-08-04.sr`

Gate status: **PASS.** All NEX-14 bench gates are complete for the USB-powered,
listen-only prototype. This result does not authorize active AUX transmission;
AUX +12 V, TX, and RTS/BUSY remained disconnected and insulated.

## NEX-16 controlled active-query gate

Status: **PREPARATION ONLY — ACTIVE AUX TRANSMISSION PROHIBITED.**

The first implementation increment is intentionally host-testable and does not
connect the transmitter to the firmware loop. It provides:

- a builder and allowlist for the payload-free AUX `GET_VERSION` (`FE`) query;
- a per-transaction `ControlledTest` authorization required by the transmitter;
- a transaction-wide deadline that releases BUSY and disables TX on expiry;
- transmitter-side rejection of every command outside that initial allowlist;
- counters for rejected starts, retries, completions, faults, recoveries, and
  maximum BUSY hold time; and
- a 1,000-exchange simulated version-query test.

Passing simulation is not physical acceptance evidence. The acceptance
checkbox for 1,000 exchanges remains open until the same count is completed on
the mounted bench setup with traces and firmware counters recorded here.

### Hold point before active testing

Do not connect the green TX conductor or blue RTS/BUSY conductor, and do not
load a transmit-capable firmware image, until all of the following are true:

- [ ] A second person or explicit operator review confirms the cable mapping,
  common ground, series resistors, divider nodes, and insulated +12 V conductor.
- [ ] The logic analyzer is connected to conditioned RX, CTS, TX, and BUSY with
  a common ground and a capture rate sufficient for 19,200-baud timing.
- [ ] The test build identifies itself as a controlled NEX-16 build and starts
  with authorization prohibited after every boot or recovery.
- [ ] The only queued packet is a checksum-valid, payload-free `GET_VERSION`
  query to one explicitly selected destination.
- [ ] The configured BUSY transaction deadline, claim delay, UART-drain timeout,
  echo timeout, backoff, and maximum-attempt count are written into the bench
  log before the conductors are connected.
- [ ] A physical kill/recovery action is rehearsed that removes TX and BUSY
  drive without cycling mount or bridge power.

### Controlled procedure (not yet started)

1. Capture at least 10 seconds of passive idle/normal traffic and confirm TX and
   BUSY remain released.
2. Arm exactly one version query, capture BUSY claim, claim delay, UART bytes,
   UART drain, TX disable, echo disposition, response, and BUSY release.
3. Disarm immediately. Verify checksum, destination, response, counters, and
   maximum BUSY hold against the analyzer trace before any repetition.
4. Repeat in small operator-confirmed batches, checking for malformed packets,
   checksum failures, unexpected traffic loss, or BUSY deadline violations.
5. Only after clean batches, run the 1,000-exchange version-query soak and save
   the raw trace plus start/end counters under `docs/captures/`.
6. Separately force a busy-bus contention condition and a response/echo timeout.
   Verify bounded backoff, safe output release, fault reporting, and recovery
   without power cycling.

Stop immediately on any unexpected destination/command, malformed checksum,
BUSY deadline breach, missing recovery, mount motion, or loss of legitimate
traffic following an echo.

### Physical results

Not started. No active-transmission result may be entered until the hold point
is explicitly released and the controlled procedure begins.
