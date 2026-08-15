# Portable Lateral Clearance Assistant — Design

**Date:** 2026-08-15
**Status:** Approved, pending implementation plan

## Problem

Driving a Lexus NX into a garage, the car's factory parking sensors alarm constantly
while real clearance is still adequate, training the driver to ignore them. Two
scratches have already occurred. The scratches are lateral — the side of the car and
the front corner as it swings in — not the front bumper against the end wall.

The factory sensors solve a different problem (frontal proximity to the end wall) than
the one causing damage (lateral clearance at the garage entrance). This device measures
the clearance that actually matters.

## Goal

A battery-powered, non-permanent device that hangs on the garage entrance wall at the
point the car first passes, measures lateral clearance between the car's flank and the
wall, and signals the driver via a visible LED bar and audible beeping before contact.

Portable: it must run from a USB power bank and mount with Command strips or a picture
hanger, so it can be moved or taken to other locations.

## Non-goals

- Frontal stop distance (the factory sensors already cover this adequately).
- Permanent installation or mains wiring.
- Sub-quarter-inch accuracy. See "Accuracy budget" below.
- Any network connectivity in v1.

## Core design decision: profile, not point

A side-mounted sensor does not observe a static distance. The car moves past it, and the
sensor sees a sequence: empty air → front bumper corner → fender → front wheel → **mirror**
→ door → rear quarter. Lateral offset differs at each. The mirror typically protrudes
furthest and passes the sensor in a fraction of a second.

Therefore the device tracks **minimum clearance observed during the current pass**, in
addition to live distance. Live distance tells the driver how to steer; the held minimum
answers "did I actually clear everything."

### Responsiveness vs. filtering

Raw HC-SR04 output jitters roughly ±0.3–0.5 in and intermittently returns spurious
values. Filtering is mandatory, but every sample of filter delay costs spatial resolution
while the car is moving.

At 2 mph the car travels ~35 in/sec. A **rolling** median-of-3 (ping every 20 ms, emit the
median of the last 3 on every ping) yields a filtered value every 20 ms ≈ 0.7 in of travel,
with only a 2-sample settling lag. A batch median-of-5 would cost ~125 ms ≈ 4.4 in — enough
to miss a mirror entirely. The rolling window is the correct structure here.

## Accuracy budget

| Source | Contribution |
|---|---|
| HC-SR04 raw jitter | ±0.3–0.5 in |
| Rolling median-of-3 | reduces jitter, rejects outliers |
| Temperature compensation (DHT11) | removes up to ~6% seasonal error (~0.6 in at 10 in) |
| Beam cone (~30° total) | footprint ~6 in dia. at 12 in range; ~16 in at 30 in |

**Realistic result: ±0.25 to ±0.5 in over the 4–30 in working range.**

This is a reliable "you have 10 inches / you have 4 inches" instrument. It is not a
"you have 1.75 inches" instrument. Warning thresholds are set with that error budget
in mind: to preserve a true 6 in of clearance, warn at 8 in.

## Architecture

Four independent units with clear boundaries, so the sensing side can later be split
onto a separate board (see "Future") without rewriting the rest.

### 1. Ranger — distance acquisition
- Fires TRIG (10 µs HIGH), measures ECHO with `pulseIn(echoPin, HIGH, 12000)`.
  The 12 ms timeout caps useful range at ~80 in and, critically, prevents the 1-second
  block that an untimed `pulseIn` incurs when no echo returns.
- Ping interval 20 ms.
- Maintains a rolling 3-sample window; emits the median.
- Rejects readings below 1.5 in (below the HC-SR04's ~0.8 in floor, treated as spurious)
  and timeouts (returned as "no target").
- Converts µs → inches using the current speed of sound from Thermo.

**Interface:** `bool read(float &inches)` — false means no valid target.

### 2. Thermo — speed-of-sound correction
- Reads DHT11 every 5 seconds on a non-blocking timer. The DHT11 takes ~250 ms per read
  and cannot be polled faster than every 2 s, so it must never sit in the ranging loop.
- `c(m/s) = 331.3 + 0.606 × T(°C)`, converted to in/µs.
- Falls back to the last good value; if none, 20 °C (0.013521 in/µs).
- DHT11's rated floor is 0 °C. Below that, reads fail and the fallback holds.

**Interface:** `float inchesPerMicrosecond()`

### 3. Tracker — pass state and thresholds
- **Pass active** when a valid target is closer than 40 in.
- **Pass ends** after 3 s with no valid target inside 40 in.
- While active, accumulates the minimum filtered distance.
- On pass end, holds the minimum for 30 s of display, then clears.
- A parked car keeps the pass open indefinitely — correct behavior; the pass closes when
  the car leaves.

Threshold bands are expressed as offsets above the saved reference `D_ref`, because for
lateral clearance the driver is avoiding a floor, not hitting a target:

| Band | Condition | LEDs lit | Sound |
|---|---|---|---|
| SAFE | d ≥ D_ref + 12 | 5 | silent |
| OK | D_ref + 8 ≤ d < D_ref + 12 | 4 | slow beep |
| CAUTION | D_ref + 4 ≤ d < D_ref + 8 | 3 | medium beep |
| WARN | D_ref + 1.5 ≤ d < D_ref + 4 | 2 | fast beep |
| DANGER | d < D_ref + 1.5 | 1, flashing | solid tone |

Beep interval maps linearly from 800 ms (OK) to 120 ms (WARN); DANGER is continuous.

### 4. Presenter — LED bar, buzzer, OLED
Consumes Tracker state only. Knows nothing about ultrasonics.

## Calibration

`D_ref` is "the tightest clearance I am willing to accept," not a target to hit.

- **Long-press (2 s)** the button while parked → current filtered distance is stored as
  `D_ref` in NVS via the `Preferences` library. Survives power loss. Confirmed by a
  two-tone chirp.
- **Short press** → clears the held minimum.
- Default on first boot: `D_ref = 8.0 in`.
- Debounce: 50 ms.

## Hardware

### Pin map

Chosen to avoid ESP32 strapping pins (0, 2, 5, 12, 15), flash pins (6–11), and
input-only pins (34–39). Note: the original reference plan used GPIO 5 for TRIG, which
is a strapping pin.

| Function | GPIO |
|---|---|
| HC-SR04 TRIG | 17 |
| HC-SR04 ECHO | 16 (via divider) |
| OLED SDA / SCL | 21 / 22 |
| DHT11 data | 4 (10 kΩ pullup to 3.3 V) |
| 74HC595 DATA / LATCH / CLOCK | 26 / 27 / 13 |
| Passive buzzer | 25 (via PN2222) |
| Calibrate button | 33 (INPUT_PULLUP → GND) |

**Board: confirmed ESP32-WROOM-32.** GPIO 16/17 carry no PSRAM claim on this module, so
the TRIG/ECHO assignment above stands. (On a WROVER these would have been reserved and
would have moved to 18/19.)

### ECHO level shifting

HC-SR04 requires 5 V VCC (from the ESP32's VIN pin while USB-powered) and its ECHO line
swings to 5 V, which exceeds the ESP32's 3.3 V tolerance. Divider: ECHO → 1 kΩ → GPIO 16,
and 2 kΩ from GPIO 16 → GND. Output = 5 × 2/(1+2) = 3.33 V.

**Sensor: confirmed plain HC-SR04**, so the divider is **required**. (The HC-SR04**P**
variant is 3.3 V capable and would not have needed it.) Connecting ECHO directly to a
GPIO without the divider risks damaging the ESP32.

### LED bar via 74HC595

**The 595 must be powered at 3.3 V, not 5 V.** A 74HC part at 5 V requires V_IH = 0.7 × 5
= 3.5 V; the ESP32's 3.3 V logic would be below threshold and unreliable. At 3.3 V,
V_IH = 2.31 V and the interface is solid. (A 74HC**T**595 would tolerate 5 V operation, but
the kit lists HC.)

Chip pin numbers below refer to the **74HC595 DIP package**, not to ESP32 GPIO numbers —
note that the chip's OE happens to be chip-pin 13 while CLOCK happens to be GPIO 13. They
are unrelated.

| 74HC595 chip pin | Name | Connect to |
|---|---|---|
| 14 | DS (data) | GPIO 26 |
| 12 | ST_CP (latch) | GPIO 27 |
| 11 | SH_CP (clock) | GPIO 13 |
| 13 | OE | GND |
| 10 | MR | 3.3 V |
| 16 | VCC | 3.3 V |
| 8 | GND | GND |
| 15, 1–4 | Q0–Q4 | LEDs (Q5–Q7 unused) |

- Series resistors: **100 Ω on all five.** The kit stocks 100 Ω and 220 Ω but no 150 Ω;
  100 Ω is the right pick because brightness is the binding constraint — this bar must
  read from a car seat in a lit garage.
- Package current with all 5 lit: red ~13 mA, yellow ~12 mA each, green 3–11 mA each
  depending on LED type. Worst case (all low-V_f) ≈ **59 mA**, typical ≈ **43 mA** — both
  under the 74HC595's 70 mA package limit.
- If more margin is wanted, 220 Ω on the two yellows drops the worst case to ~47 mA at
  the cost of visible brightness.

**LED color layout (bottom→top): 1 red, 2 yellow, 2 green.** Red is the band that must be
bright, and red LEDs (V_f ≈ 2.0 V) are the brightest of the three at 3.3 V. Green LEDs may
have V_f up to ~3.0 V and will run dim — acceptable, since "dim green" means "plenty of
room," the least urgent signal. If green is unusably dim, substitute a yellow or drop to
68 Ω.

This is a correction to the reference plan, which specified 220 Ω on all LEDs from 3.3 V —
that yields ~1.4 mA on a green LED, effectively invisible in a lit garage.

### Buzzer

Passive buzzer driven through a PN2222 (1 kΩ base resistor) rather than directly off a
GPIO, keeping switching current off the ESP32 pin. Driven with the LEDC peripheral for
variable-rate beeping.

**Expectation setting:** through a closed car window at ~8 ft, a kit buzzer is marginal.
It is genuinely useful for calibration feedback and walk-up use. **The LED bar is the
primary driving signal.**

## Power

- USB power bank via USB-A-to-C. Active draw ~105 mA (ESP32 with WiFi off ~45 mA,
  OLED ~15 mA, HC-SR04 ~15 mA, LED bar ~31 mA at full).
- Power-bank auto-shutoff (typically below ~50 mA) is **not** a risk here, because the
  device is only powered while actively sensing. This is a direct benefit of the
  on-demand usage model.
- ~60 hours of active use from a 10,000 mAh bank (bank capacity is rated at 3.7 V cell
  voltage; usable energy at 5 V is roughly 65% of the label). At ~2 minutes per park,
  recharging is a few-times-a-year event.
- Off = unplug. No deep sleep in v1: it would drop draw below the bank's cutoff, killing
  power entirely and making wake impossible without a replug.
- The 9 V battery in the kit is rejected: the DevKit's linear regulator dissipates
  ~0.46 W as heat and yields roughly 5 hours.

## Mounting

Garage entrance wall, on the side the car passes first, at the height of the widest point
of the car's flank (door crease or mirror height). Sensor aimed perpendicular to the car's
path — the ~30° beam cone will pick up the floor or ceiling if aimed off-axis.

Command strips (Velcro type) so it can be repositioned. Enclosure: any small box with
holes for the two transducer barrels, the LED bar, and the OLED.

## Testing

- **Ranger:** static targets at measured distances (tape measure) at 4, 8, 12, 24, 36 in;
  verify ±0.5 in. Verify timeout returns "no target" within 12 ms rather than blocking.
- **Thermo:** verify the correction shifts readings in the right direction (breathe on the
  DHT11); verify fallback when the sensor is disconnected.
- **Tracker:** walk a flat board past the sensor at varying speeds; verify the held minimum
  matches the closest approach measured by hand.
- **Calibration:** save a value, power-cycle, confirm it persists.
- **Integration:** drive the actual car past at parking speed; compare held minimum against
  a tape measure at the tightest point.

## Future (not in v1)

Two-unit ESP-NOW system for parking-lot use: a sensor pod on the car's own flank
transmitting to a dash-mounted display. Requires a second ESP32. The Ranger/Presenter
split above is the seam this would follow — an addition, not a rewrite.

Note on the originally-proposed parking-lot approach (adhering the unit to an adjacent
stranger's car): rejected as impractical — it requires exiting and re-entering the car
around the park, the hardware is exposed to loss, and it does not travel with the driver.
Standing the unit on the ground or a curb beside the gap, as a spotter before committing,
works with the v1 hardware unchanged.
