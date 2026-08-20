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

A portable device that **rides in the car**, resting on the driver's door sill with the
window lowered, measuring lateral clearance between the car's left flank and whatever is
beside it, and warning the driver via an LED bar, an OLED readout, and rising-rate
beeping before contact.

Powered from the car's USB port. Works anywhere the car goes, not only in the garage.

> **Revised 2026-08-19.** The original design mounted the device on the garage entrance
> wall. Car-mounting replaced it: the OLED and buzzer only work at cabin range, the
> calibration becomes permanent because the sensor is fixed relative to the car, and the
> device travels. The cost is that the window must be down (ultrasonic cannot pass
> through glass) and the front corner is not directly covered. See Mounting.

## Non-goals

- Frontal stop distance (the factory sensors already cover this adequately).
- Permanent installation, mains wiring, or anything spliced into the car's electrics.
- Sub-quarter-inch accuracy. See "Accuracy budget" below.
- Any network connectivity in v1.

## Core design decision: profile, not point

A side-facing sensor does not observe a static distance — the car and the obstacle move
relative to one another, and the gap traces a profile over the few seconds of the pass.

Under the revised car-mounted geometry the sensor rides with the car and sweeps past the
wall, reading the narrowest point of the gap as it goes. Under the original wall-mounted
geometry the car swept past a fixed sensor, which saw: empty air → front bumper corner →
fender → wheel → **mirror** → door → rear quarter.

Either way the physics and the required logic are identical: relative motion, a profile
rather than a point, and a narrowest moment that may last a fraction of a second.

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

## Power — revised 2026-08-19

**The car's USB port, or a 12 V USB adapter.** The device is in the vehicle, so it
powers on with the car and off with it.

This deletes the entire power problem from the original design: no power bank, no
bank auto-shutoff concern, no 9 V regulator heat, no runtime budget, no deep-sleep
question. Draw is ~105 mA, trivial for any car USB port.

The USB power bank remains a fine fallback for bench work at a desk.

## Mounting — car-mounted (revised 2026-08-19)

**The device rides in the car, not on the garage wall.** It rests on the driver's
door sill or armrest with the window lowered a few inches, sensor pointing straight
out the left side. It must not be held while driving — resting it keeps hands on the
wheel and gives a steadier reading.

Aim it **horizontal and perpendicular** to the car. The beam is a ~30 degree cone, so
a downward tilt catches the road surface.

### Why this beats the original wall mount

- **The OLED and buzzer become useful.** At garage-wall range a 0.96" screen is
  unreadable and a kit buzzer is marginal through glass. Inside the cabin both work.
- **Calibration becomes permanent.** The sensor is fixed relative to the car, so the
  offset never changes. The wall version needed the identical mounting position every
  single time or the saved reference was meaningless.
- **It travels.** Garage, parking lots, parallel parking — the original goal.
- **No mounting ritual.** Nothing to stick up and take down each trip.

### The hard constraint: glass

**Ultrasonic cannot see through a window.** Sound reflects off glass almost
completely; a closed window returns a constant ~2 cm reading regardless of what is
outside. The window must be down far enough for the sensor to have clear air.

### The limitation: front corner

This measures clearance **at the sensor's position**, roughly alongside the driver.
When the front corner swings wide on a turn-in it is closer to the wall than the door
is at that instant, and this will not see it. Mounting as far forward as practical
reduces the gap but does not close it. If front-corner scrapes prove to be the real
failure mode, the wall-mounted geometry catches them better.

### What the saved reference means now

The sensor sits near the door plane; the mirror protrudes roughly 6-8 inches further.
So a 10 inch reading means about 2 inches of mirror clearance. None of that needs
measuring — park with the mirror exactly as close as is acceptable and long-press.
The offset is captured permanently.

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
