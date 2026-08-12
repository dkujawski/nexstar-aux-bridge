# Bench validation record

## NEX-17 — Wired working-prototype acceptance

Date: 2026-08-11

The normal `esp32dev` bridge firmware was flashed to the USB-powered ESP32
DevKit V1 at `COM6`. A local host opened UART0 at 19,200 baud, deasserted
DTR/RTS, sent one read-only GET_VERSION request, and received the complete
checksum-valid mount response without a telescope power cycle.

| Direction | Frame | Evidence |
| --- | --- | --- |
| Host → mount | `3B 03 03 10 FE EC` | Protected TX and raw AUX bus at 5.652828250 s |
| Mount → host | `3B 05 10 03 FE 05 14 D1` | Raw AUX bus at 5.656819250 s and CP2102/UART0 receive |

`docs/captures/nex17-one-retry-2026-08-11.sr` records the initial failure:
BUSY release caused a false conditioned-RX start that hid the response preamble
from UART2. After breadboard connection adjustments,
`docs/captures/nex17-one-retry-bb-adjusted-2026-08-11.sr` records the successful
transaction. A brief conditioned-RX glitch remains visible near BUSY release,
but UART2 rejected it and decoded the real response. This is sufficient
working-prototype evidence; reconnect, soak, and display comparison work is
deferred to NEX-23. No compatible local telescope host application was
installed, so the optional application-level identity/status check is deferred.

## NEX-11 — USB-UART host transport reset and reconnect check

Date: 2026-08-10

Configuration:

- Generic 30-pin ESP32 DevKit V1 with CP2102 USB-UART bridge on `COM6`
  (VID:PID `10C4:EA60`)
- `esp32dev` binary-clean bridge profile uploaded successfully
- UART0 observed with `miniterm` at 115,200 baud for ROM startup, then at
  19,200 baud for the binary protocol endpoint
- no AUX command was sent and AUX TX remained fail-closed

| Check | Observed result | Result |
| --- | --- | --- |
| EN reset | ESP32 ROM startup sequence appeared on each reset | PASS |
| Application startup | No application banner or diagnostic text followed the ROM output | PASS |
| Protocol session | 19,200-baud raw session remained silent without routed binary traffic | PASS |
| USB reconnect | CP2102 re-enumerated as `COM6`; the host session reconnected without a mount power cycle | PASS |

The ROM banner is an immutable reset-time UART0 behavior. It precedes the
application and is not emitted by the bridge firmware. The normal bridge
profile deliberately emits no text after initialization.

Remaining NEX-11 validation: exercise the TFT-enabled path and sustained host
backpressure while AUX service is integrated, then define the observable
disconnect metric for UART0 (which exposes no reliable physical-disconnect
event).

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

Status: **HOLD POINT RELEASED — ACTIVE TESTING STOPPED FOR CONDITIONED-RX/BUSY
COUPLING REVIEW.**

The controlled-test implementation is firmware-loop integrated and provides:

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

**Released by operator on 2026-08-09.** The execution checklist remains the
record of each physical action and result; do not mark an execution step passed
until its required observation has been captured.

Do not connect the green TX conductor or blue RTS/BUSY conductor, and do not
load a transmit-capable firmware image, until all of the following are true:

- [x] A second person or explicit operator review confirms the cable mapping,
  common ground, series resistors, divider nodes, and insulated +12 V conductor.
- [x] The logic analyzer is connected to conditioned RX, CTS, TX, and BUSY with
  a common ground and a capture rate sufficient for 19,200-baud timing.
- [x] The test build identifies itself as a controlled NEX-16 build and starts
  with authorization prohibited after every boot or recovery.
- [x] The only queued packet is a checksum-valid, payload-free `GET_VERSION`
  query to one explicitly selected destination.
- [x] The configured BUSY transaction deadline, claim delay, UART-drain timeout,
  echo timeout, backoff, and maximum-attempt count are written into the bench
  log before the conductors are connected.
- [x] A physical kill/recovery action is rehearsed that removes TX and BUSY
  drive without cycling mount or bridge power.

### Controlled-test execution checklist (not yet started)

Complete in order. The operator releases each hold point before moving on.

- [x] **1. Verify the bench.** A second person confirms cable mapping, common
  ground, series resistors, divider nodes, and that AUX +12 V is disconnected
  and insulated.
- [x] **2. Connect and configure the analyzer.** Probe conditioned RX, CTS, TX,
  and BUSY with common ground; set a capture rate adequate for 19,200-baud
  traffic.
- [x] **3. Load and prove the test image safe.** Load the clearly identified
  NEX-16 controlled-test build. Confirm it always boots and recovers with
  transmission unauthorized.
- [x] **4. Lock the query scope.** Select and record one destination. Configure
  exactly one checksum-valid, payload-free `GET_VERSION` (`FE`) query; confirm
  all other commands and destinations are rejected.
- [x] **5. Record limits before wiring TX/BUSY.** Enter the BUSY deadline, claim
  delay, UART-drain timeout, echo timeout, backoff, and maximum attempts in the
  bench log.
- [x] **6. Rehearse the kill action.** Demonstrate that the physical action
  releases TX and BUSY without power-cycling the mount or bridge.
- [x] **7. Establish a passive baseline.** Capture at least 10 seconds with no
  query armed. Confirm TX and BUSY remain released.
- [x] **8. Run one query.** Arm exactly one query and capture BUSY claim,
  transmission, UART drain, echo, response, TX disable, and BUSY release.
- [x] **9. Disarm and reconcile.** Disarm immediately. Compare the trace with
  checksum, destination, response, counters, and maximum BUSY hold.
- [ ] **10. Run operator-confirmed batches.** Increase only through small clean
  batches, reconciling the trace and counters after each batch.
- [ ] **11. Perform the soak.** After clean batches, run the 1,000-exchange
  version-query soak; save the raw trace and start/end counters under
  `docs/captures/`.
- [ ] **12. Test recovery separately.** Test contention, then response/echo
  timeout. Verify bounded backoff, TX/BUSY release, fault reporting, and
  recovery without power cycling.

**Stop immediately** for an unexpected destination or command, malformed
checksum, BUSY deadline breach, missing recovery, mount motion, or loss of
legitimate post-echo traffic. Do not resume until the operator has reviewed and
cleared the cause.

### Physical results

On 2026-08-09, the `esp32dev_controlled_test` image was uploaded to the ESP32
DevKit V1 on `COM6`. Its boot banner identified `NEX16: CONTROLLED-TEST` and
reported `transmission=unauthorized`, idle state, and zero transaction starts.
The reported limits were: BUSY deadline 175000 us, claim delay 100 us,
UART-drain timeout 20000 us, echo timeout 150000 us, backoff 2000 us, and
maximum attempts 3. An explicit `RECOVER` command again reported
`transmission=unauthorized`. No query was armed or transmitted.

The sole query destination was selected as `DA` through the controlled-test
serial interface. Status then reported idle state, `destination=DA`, and zero
transaction starts; selecting a destination does not authorize transmission.

The first armed `GET_VERSION` attempt to `DA` reached the bounded echo-timeout
fault (`code=4`): one start, zero completions, one fault, and maximum BUSY hold
of 158000 us. The firmware reported `transmission=unauthorized` on fault. A
subsequent `RECOVER` command reported `transmission=unauthorized` and idle
state. No further query was attempted. **STOP: review the analyzer trace before
any additional active test.**

Review of `docs/captures/nex16-controlled-test-2026-08-09.sr` found a 12.5 s,
4 MHz capture with transitions only on D0; D1 through D7 were flat and the
session metadata does not map D0 to the required conditioned RX/CTS/TX/BUSY
signals. It therefore cannot verify TX/BUSY release or claim timing. A
non-inverted 19,200-baud scan of D0 did not contain the expected serialized
`3B 03 04 DA FE 21` query/echo sequence. This trace is not acceptance evidence;
reconfigure and label all four required channels, then repeat the passive
baseline before any further active query.

The corrected capture
`docs/captures/nex16-corected-controlled-test-2026-08-09.sr` contains 12.5 s at
4 MHz with labeled RX, CTS, TX, and BUSY channels. CTS remained asserted-low.
TX decoded 122 bytes of checksum-valid-looking normal traffic between existing
device IDs `0D`, `10`, and `11`, while BUSY followed those transactions. The
controlled bridge query `3B 03 04 DA FE 21` was absent. This corrected passive
baseline passes; no bridge query was armed during the capture.

The active capture `docs/captures/nex16-controlled-test-active-2026-08-09.sr`
contains 25 s at 4 MHz. It captured the exact checksum-valid query
`3B 03 04 DA FE 21` on TX and RX at 15.715141 s, proving that the physical echo
was present. BUSY asserted at 15.712157 s and released at 15.870143 s, a
157986.5 us hold consistent with the firmware counter. No response from `DA`
was decoded. The firmware's echo-timeout report was therefore false: the loop
started its echo tracker only after UART drain and had already consumed the
echo. Active testing remains stopped pending the corrected firmware build.

After correcting the echo-tracker ordering and loading the new image, the
single-query run completed with one start, one completion, zero faults, and a
10000 us maximum BUSY hold. The corresponding capture
`docs/captures/nex16-controlled-test-active-corrected-2026-08-09.sr` contains
25 s at 4 MHz. It decoded the exact checksum-valid `3B 03 04 DA FE 21` query on
TX and RX at 15.850296 s. BUSY asserted at 15.847312 s and released at
15.857294 s: 2984 us from claim to TX and 9981.75 us total hold, consistent
with the firmware counter. CTS remained asserted-low. No response from `DA`
and no packet after the echo were captured. Firmware returned idle with
transmission unauthorized. Do not begin operator-confirmed batches until the
missing response is understood.

The operator then selected observed device ID `11`. Capture
`docs/captures/nex16-get-version-11-2026-08-09.sr` decoded the valid query and
echo `3B 03 04 11 FE EA` at 20.019753 s, but no response followed. BUSY asserted
at 20.016769 s and released at 20.026751 s. The 2984 us measured claim-to-TX
delay is far above the configured 100 us, and BUSY remained asserted about
3873 us after the echo ended. The firmware loop's unconditional 1 ms delay was
accumulating across state transitions. Active testing remains stopped pending
a corrected high-resolution controlled-test loop.

After removing the controlled loop's 1 ms sleep, the corrected image reported
one start, one completion, zero faults, and a 3455 us maximum BUSY hold.
Capture `docs/captures/nex16-get-version-11-timing-corrected-2026-08-09.sr`
decoded the valid query and echo `3B 03 04 11 FE EA` at 15.652459 s. BUSY
asserted at 15.652310 s and released at 15.655747 s: 149.25 us from claim to
TX, 3436.75 us total hold, and 162.5 us from the end of the echo to release.
These values reconcile with the configured 100 us claim delay and firmware
counter. CTS remained asserted-low. Device `11` still produced no response, so
no batch testing is authorized.

The same timing-corrected query to observed responder `10` was captured in
`docs/captures/nex16-get-version-10-2026-08-09.sr`. Query and echo
`3B 03 04 10 FE EB` began at 17.836098 s. BUSY claim-to-TX was 149.75 us and
total hold was 3439.75 us, but no response followed. Protocol review found that
`04` is the hand-controller ID, while source ID `03` is explicitly recommended
for external PC/AUX-port devices. It also specifies ignoring the echo and
waiting for a matching response packet. The controlled build was updated to
use source `03`, validate the expected version response, and fault on a bounded
response timeout. No further active query may run until that build is loaded
and another operator-confirmed capture is started.

Capture `docs/captures/nex16-get-version-10-source-03-2026-08-09.sr` proved
that source `03` resolved the target response: query/echo
`3B 03 03 10 FE EC` began at 22.312830 s and checksum-valid response
`3B 05 10 03 FE 05 14 D1` (version 5.20) began at 22.316203 s. BUSY
claim-to-TX was 165.5 us and total hold was 3492 us. The firmware nevertheless
reported response timeout because a decoded response could be discarded before
the transmitter completed its transition into `ResponseWait`. The response is
now retained and delivered after that state transition. Active testing remains
paused until this correction is loaded and captured once.

The retained-response build was exercised once in
`docs/captures/nex16-get-version-10-response-fix-2026-08-09.sr`. It sent the
single valid query `3B 03 03 10 FE EC`, held BUSY for approximately 3496 us,
and again entered the bounded response-timeout fault with TX and BUSY released
and transmission unauthorized. The analyzer proved that device `10` returned
the checksum-valid response `3B 05 10 03 FE 05 14 D1`, but conditioned RX went
LOW at 18.153489 s, about 43 us before the bridge released BUSY at 18.153532 s.
That early response start overlapped the BUSY release and corrupted the `3B`
preamble at the ESP32 UART; the remaining bytes began with `05` and could not
form a valid packet. The protected bus showed the complete response beginning
at 18.153597 s. The transmitter now releases BUSY in the same state-machine
tick that observes a complete echo after TX is disabled, removing the extra
release transition. All 33 native tests, including 1,000 simulated exchanges,
pass and the controlled-test image builds. Active testing remains paused until
this timing-corrected build is loaded and its safe boot is confirmed. The image
was then uploaded to `COM6`; an explicit `STATUS` reported state 0, no active
transaction, no selected destination, zero starts/completions/faults, and
`max_busy_us=0`. It is ready for one operator-confirmed verification capture;
batch testing remains unauthorized.

Capture
`docs/captures/nex16-get-version-10-busy-release-corrected-2026-08-09.sr`
tested the same single query after removing the extra BUSY-release transition.
BUSY asserted at 14.116691500 s and released at 14.120090000 s, for a 3398.5 us
hold. Conditioned RX began the response at 14.120088500 s, still 1.5 us before
release, so the release edge again corrupted only the `3B` preamble. The
protected bus carried the complete checksum-valid
`3B 05 10 03 FE 05 14 D1` response beginning at 14.120198000 s. The firmware
entered bounded response-timeout fault code 6, safely released both outputs,
and made no retry. The transmitter is now revised to disable TX and release
BUSY immediately when UART drain completes; echo and response validation remain
mandatory after release. All 33 native tests and the controlled build pass.
Active testing remains paused until this UART-drain-release image is loaded and
its safe boot is confirmed. The image was uploaded to `COM6`; `STATUS` then
reported idle state 0, no selected destination, zero starts/completions/faults,
and `max_busy_us=0`. It is ready for exactly one operator-confirmed verification
capture; batch testing remains unauthorized.

Capture `docs/captures/nex16-get-version-10-uart-drain-release-2026-08-09.sr`
verified the UART-drain-release build with one query and no retry. BUSY asserted
at 13.343965750 s and released at 13.347342250 s, a 3376.5 us analyzer hold
consistent with the 3388 us firmware maximum. Conditioned RX then went LOW at
13.347350250 s, 8.0 us after release, and remained LOW for 46.75 us. Device `10`
asserted BUSY at 13.347396000 s and the complete protected-bus response
`3B 05 10 03 FE 05 14 D1` began at 13.347423000 s. The release-correlated RX
pulse therefore created a false UART start that overlapped the real preamble;
the ESP32 could receive only the post-preamble bytes and correctly entered
bounded response-timeout fault code 6. TX and BUSY were released and no retry
occurred. This proves the firmware sequencing race is removed but exposes a
physical conditioned-RX/BUSY coupling or settling fault. **STOP: do not run
another active query or begin batches until the operator re-verifies the RX and
BUSY breadboard nodes, eliminates the release-correlated pulse, and establishes
a new passive/analyzer baseline.** The operator then applied the rehearsed
physical kill action. A read-only `STATUS` check confirmed fault state 10, no
active transaction, one start, zero completions, one fault, and
`max_busy_us=3388`; transmission remained unauthorized.

With bridge USB, RJ12, and analyzer leads disconnected, the operator performed
unpowered isolation checks. Raw RX `a22` to protected BUSY `c21` measured
104.5 kOhm; conditioned RX `d26` to BUSY measured 34.00 kOhm in the initial
polarity; the RX lower leg `d26` to common ground measured 10.05 kOhm; and raw
RX `a22` to conditioned RX `d26` measured 7.12 kOhm. BUSY `c21` to common
ground measured open. Reversing the `c21`/`d26` probes produced open with red
on `c21` and black on `d26`, and 36.8 kOhm with black on `c21` and red on
`d26`. These results verify the intended RX divider, released BUSY isolation,
and a polarity-dependent unpowered semiconductor path rather than a direct
RX-to-BUSY breadboard short. Active transmission remains prohibited pending a
raw-versus-conditioned RX analyzer comparison.

The first passive comparison capture placed D1 at `d27` by operator error and
is not evidence. The corrected retry,
`docs/captures/nex16-raw-conditioned-rx-passive-corrected-retry-2026-08-10.sr`,
captured 12.5 s at 4 MHz with D0 on raw RX `a22`, D1 on conditioned RX `d26`,
D2 on protected TX `a18`, and D3 on protected BUSY `c21`; the physical TX/BUSY
kill remained applied and firmware status showed zero transaction starts.
Raw and conditioned RX paired on 446 ordinary edges with median delay 0 us,
but the conditioned node alone produced repeated BUSY-correlated LOW intervals
while raw RX remained HIGH. In a representative event conditioned RX fell at
1.775439750 s, BUSY transitioned at 1.775453750 s and 1.775637750 s, and
conditioned RX recovered at 1.775638750 s; raw RX remained HIGH throughout.
Comparable conditioned-only intervals recurred during later BUSY events. This
localizes the false UART starts to the conditioned RX/ESP-side circuit rather
than the mount's raw RX signal. **STOP remains in effect: no active query or
batch testing until the powered conditioned-input fault is corrected and a
new passive raw-versus-conditioned capture proves equivalence.**

### Operator-confirmed completion assumption (2026-08-10)

The operator does not have equipment to make the conditioned-RX measurements
or collect the remaining analyzer evidence. At the operator's direction, the
following physical results are **assumed**, not independently measured or
captured:

- the conditioned-RX/BUSY coupling was remediated and a new passive baseline
  showed raw and conditioned RX to be equivalent with TX and BUSY released;
- controlled `GET_VERSION` batches, including a 1,000-exchange soak, completed
  without lockup, malformed/checksum-invalid transmit, or BUSY timeout;
- forced contention entered bounded backoff and recovered safely; and
- forced echo/response timeout released TX and BUSY, reported the fault, and
  recovered without cycling mount or bridge power.

Accordingly, NEX-16 is accepted on operator-confirmed assumptions. Replace
this section with measured captures and counters before treating the result as
physical validation evidence.
