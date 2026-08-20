# Hardware — parts and complete wiring

Bench reference. Full step-by-step build order with verification gates is in
`docs/superpowers/plans/2026-08-15-garage-parking-assistant.md`.

## Parts used from the kit

| Part | Qty | Role |
|---|---|---|
| ESP32 board (WROOM-32) | 1 | Everything |
| Ultrasonic sensor HC-SR04 | 1 | The measurement |
| 0.96" OLED display | 1 | Setup and calibration readout |
| DHT11 temp/humidity | 1 | Temperature → speed-of-sound correction |
| 74HC595 shift register | 1 | Drives 5 LEDs from 3 GPIO pins |
| Passive buzzer | 1 | Rising-rate beeping |
| NPN transistor PN2222 | 1 | Switches the buzzer |
| Button | 1 | Calibration (long press) / clear min (tap) |
| Red LED | 1 | Bar position 1 (bottom) |
| Yellow LED | 2 | Bar positions 2–3 |
| Green LED | 2 | Bar positions 4–5 (top) |
| Resistor 100 Ω | 5 | LED current limiting |
| Resistor 1 kΩ | 2 | ECHO divider (1), transistor base (1) |
| Resistor 2 kΩ | 1 | ECHO divider |
| Resistor 10 kΩ | 1 | DHT11 pull-up (skip if module has one) |
| 400-point breadboard | 2 | The ESP32 alone eats most of one |
| Jumper wires | ~30 | |
| USB A-to-C cable | 1 | Programming, then power |

### Powering it — kit-only options

Every part of the **circuit** comes from the kit. Power is the one open question,
and there are three ways to solve it.

**Option 1 — USB power bank (recommended).** Any phone power bank, plugged in with
the kit's USB A-to-C cable. Most people already own one, so for most people this is
not a purchase. ~60 hours of run time from a 10,000 mAh bank; at two minutes per
park that is a few recharges a year. Nothing to build, nothing to get wrong.

**Option 2 — 9 V battery + Power Supply Module (fully kit-only).** Both are in the
kit. The 9 V feeds the Power Supply Module, which regulates it down to clean 5 V and
3.3 V rails; the 5 V then feeds both the ESP32's VIN pin and the HC-SR04.

Check what your 9 V snap connector ends in first:
- **A barrel plug** — plugs straight into the Power Supply Module. This works.
- **Bare wires** — only works if your module has a screw terminal. Otherwise use
  Option 1.

Run time is roughly **4–5 hours** of on-time from an alkaline 9 V (about 500 mAh
against a ~105 mA draw). At two minutes per park that is ~135 sessions, so a couple
of months of twice-daily parking before the battery needs replacing. The module's
regulator also runs warm, which is normal.

**Option 3 — do NOT do this: 9 V straight to VIN.** This is the obvious-looking
approach and it will damage your ultrasonic sensor. On the DevKit, the VIN pin is
the *same electrical node* as the USB 5 V rail — it feeds the onboard regulator, it
is not produced by it. Put 9 V in and the VIN pin sits at 9 V. The ESP32 itself
survives (its regulator accepts up to 15 V), but anything you tap off VIN gets 9 V,
and the HC-SR04 is a 5 V part with an absolute maximum around 5.5 V.

If you want to start building before sorting out power, just run it off your Mac's
USB port. Tasks 0 through 9 are all bench work at your desk anyway.

### Note on jumper wires

The HC-SR04, OLED and DHT11 modules all have 0.1"-spaced male header pins, so they
plug **straight into the breadboard**. You do not need female-to-male wires for
them — which is good, because those three modules need 11 connections and the kit
only ships 10 F-M wires.

## Left over (not used in this build)

GY-6500 gyro, ULN2003 + stepper, SG90 servo, 5 V relay, IR receiver + emitter +
remote, joystick, fan blade + motor, **active** buzzer, L293D, RC522 RFID, membrane
keypad, HC-SR501 PIR, 4 spare buttons, potentiometer, both 7-segment displays, tilt
ball switch, white/blue/RGB LEDs, thermistor, photoresistors, diodes, 1 spare
PN2222, and most resistor values.

The **9 V battery** and the **Power Supply Module** are unused if you power from a
USB power bank (Option 1), and used together if you go fully kit-only (Option 2).

Two of these were considered and deliberately rejected:

- **Active buzzer** — louder, but one fixed pitch. The passive buzzer can vary beep
  *rate*, which is what makes real parking sensors readable without looking.
- **HC-SR501 PIR** — could wake the device on motion, but it would also trigger on
  you walking through the garage. Unplugging is simpler and more predictable.

## The ESP32 does NOT go on the breadboard

**This board has 1.0 inch pin-row spacing.** A breadboard's lettered area spans 1.1
inches, so there is only one row of slack — you can have a free row on the `a` side or
the `j` side, never both. Whichever side loses out has no reachable hole in any of its
pins' groups, and those pins become unusable. No seating position fixes this.

So the ESP32 **sits loose beside the breadboard** and connects with **female-to-male
jumper wires**: the socket end pushes onto the ESP32's pin, the male end goes into a
breadboard hole.

This applies to every wiring table below. Where a table says "GPIO 17", read it as
"a female-to-male wire from the pin marked `TX2`, with its male end in the breadboard
hole you are using for that signal."

Consequences:

- No wire ever runs directly between two components. Everything meets in the breadboard.
- The full build needs ~13 connections to the ESP32. The kit ships 10 female-to-male
  wires and 4 are used by the sensor, so **a 40-pack of male-to-female jumper wires is
  needed** to finish. A few dollars.
- Every breadboard hole stays visible, since nothing overhangs the grid.

## Power rails

The board needs **two separate rails**. Getting these crossed is the one wiring
mistake that can destroy parts.

| Rail | Source | Feeds |
|---|---|---|
| **5 V** | see below | HC-SR04 **only** |
| **3.3 V** | ESP32 `3V3` pin | OLED, DHT11, 74HC595, buzzer, everything else |
| **GND** | ESP32 `GND` | Everything (all grounds common) |

Where the 5 V rail comes from depends on how you power the device:

- **USB power bank or Mac USB (Option 1)** — take it from the ESP32's `VIN` pin,
  which carries ~5 V whenever the board is USB-powered.
- **9 V + Power Supply Module (Option 2)** — take it from the module's 5 V output,
  and feed the ESP32's `VIN` from that same 5 V.

Either way the HC-SR04 sees a regulated 5 V and never sees 9 V.

**The 74HC595 must be on 3.3 V, never 5 V.** A 74HC part running at 5 V needs 3.5 V
to register a logic HIGH, and the ESP32 only outputs 3.3 V — it would work
intermittently or not at all.

## Complete wiring

### HC-SR04 ultrasonic

| Sensor pin | Goes to |
|---|---|
| VCC | **5 V** rail |
| GND | GND rail |
| TRIG | GPIO 17 |
| ECHO | 1 kΩ → GPIO 17's neighbour GPIO 16, **and** 2 kΩ from GPIO 16 → GND |

The two resistors form a divider dropping ECHO's 5 V output to 3.3 V. **Never wire
ECHO straight to a GPIO** — 5 V on a 3.3 V pin damages the ESP32.

Divider detail:

```
HC-SR04 ECHO ──[ 1k ]──┬── GPIO 16
                       │
                     [ 2k ]
                       │
                      GND
```

### OLED display

| OLED pin | Goes to |
|---|---|
| VCC | 3.3 V rail |
| GND | GND rail |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### DHT11

| DHT11 pin | Goes to |
|---|---|
| VCC / `+` | 3.3 V rail |
| GND / `−` | GND rail |
| DATA / `out` | GPIO 4 |

Plus a 10 kΩ from GPIO 4 to 3.3 V. Most DHT11 breakout boards already include this;
a second one in parallel is harmless.

### 74HC595 shift register

Pin 1 is left of the half-moon notch when the notch faces up; numbering runs
counter-clockwise.

| Chip pin | Name | Goes to |
|---|---|---|
| 16 | VCC | **3.3 V** rail |
| 8 | GND | GND rail |
| 13 | OE | GND rail |
| 10 | MR | 3.3 V rail |
| 14 | DS (data) | GPIO 26 |
| 12 | ST_CP (latch) | GPIO 27 |
| 11 | SH_CP (clock) | GPIO 13 |

The chip's OE is chip-pin 13 and the clock is GPIO 13. Unrelated — OE goes to
**ground**, GPIO 13 goes to **chip pin 11**.

### LED bar

Mount vertically, red at the bottom. Each LED's **long leg (anode)** to its 595
output; **short leg (cathode)** through a 100 Ω resistor to GND.

| 595 output | Chip pin | LED | Bar position |
|---|---|---|---|
| Q0 | 15 | red | bottom |
| Q1 | 1 | yellow | |
| Q2 | 2 | yellow | |
| Q3 | 3 | green | |
| Q4 | 4 | green | top |

Q5–Q7 (chip pins 5, 6, 7) unconnected.

Green LEDs may look dimmer than red and yellow — their forward voltage sits close
to the 3.3 V supply. Expected. If one is unusably dim, substitute a yellow. Do not
go below 100 Ω: that is already the brightest value the 595's package current
budget allows across five LEDs.

### Buzzer (passive, via PN2222)

Use the buzzer **without** a sticker on top and with an open PCB underside. The
active one has its own oscillator and will ignore the frequency.

PN2222 in TO-92, flat face toward you, legs down: **E–B–C** left to right.

| From | To |
|---|---|
| GPIO 25 | 1 kΩ → PN2222 **base** (middle leg) |
| PN2222 **emitter** (left) | GND rail |
| PN2222 **collector** (right) | buzzer `−` leg |
| buzzer `+` leg | 3.3 V rail |

### Button

| From | To |
|---|---|
| One leg | GPIO 33 |
| Other leg | GND rail |

No resistor — the internal pull-up is enabled in software. A tactile button's legs
are joined in pairs across the gap, so straddle the breadboard trench.

## Pin map summary

**Board confirmed: ESP32 DevKit V1, 30-pin, USB-C.** Its silkscreen prints serial
names for two of the pins this project uses — there is no pin labelled "16" or "17":

| Code / GPIO | Printed on your board |
|---|---|
| GPIO 16 (HC-SR04 ECHO) | **RX2** |
| GPIO 17 (HC-SR04 TRIG) | **TX2** |

Every other pin is labelled with its number (`D4`, `D13`, `D21`, `D22`, `D25`, `D26`,
`D27`, `D33`). `VIN` and `3V3` are both at the USB-C end, on opposite rows.

| GPIO | Function |
|---|---|
| 4 | DHT11 data |
| 13 | 74HC595 clock |
| 16 | HC-SR04 ECHO (via divider) |
| 17 | HC-SR04 TRIG |
| 21 | OLED SDA |
| 22 | OLED SCL |
| 25 | Buzzer (via PN2222) |
| 26 | 74HC595 data |
| 27 | 74HC595 latch |
| 33 | Button |

Chosen to avoid strapping pins (0, 2, 5, 12, 15), flash pins (6–11) and input-only
pins (34–39). Do not reassign casually — GPIO 5 in particular is a strapping pin
and is a common wrong answer for TRIG.

## Before powering on

- [ ] ECHO goes through the 1 kΩ / 2 kΩ divider, not straight to GPIO 16
- [ ] 74HC595 pin 16 is on **3.3 V**, not 5 V
- [ ] HC-SR04 VCC is on **5 V**, not 3.3 V
- [ ] 74HC595 pin 13 (OE) is on **ground**
- [ ] No wire bridges the 5 V and 3.3 V rails
- [ ] All LED long legs face the 595, short legs face the resistors

Always disconnect USB power before changing wiring.

## How big is it?

Short answer: **about a phone's footprint, but roughly four times as thick.**

### Footprint

A 400-point breadboard is **83 × 55 mm**. You need two, and placed end to end they
come to **166 × 55 mm**.

| | Length | Width | Thickness |
|---|---|---|---|
| Two breadboards, end to end | 166 mm | 55 mm | 10 mm (bare) |
| iPhone 16 Pro Max | 163 mm | 77.6 mm | 8.25 mm |

So the outline is close — very slightly longer than the phone and about 20 mm
narrower.

### Thickness is where it differs

The breadboard is 10 mm on its own, and everything stands on top of it. The
ultrasonic sensor's two barrels are the tallest parts at about 15 mm. With an
enclosure around it you land near **30–35 mm thick**, against the phone's 8.25 mm.

Think "a phone-sized brick about as thick as four phones stacked."

Add a USB power bank and it grows again — a 10,000 mAh bank is roughly
140 × 70 × 15 mm on its own, so the finished object is more like two phones face to
face. The 9 V option (see Powering it, above) is physically smaller: a 9 V battery
is 48 × 26 × 17 mm.

### Can it be done without a breadboard?

Not with what is in the kit. Going breadboard-free means **soldering** the parts to
a piece of perfboard, and that needs three things the kit does not include:

- perfboard / protoboard
- a soldering iron
- solder

It is also a real skill with a real learning curve, and a cold solder joint produces
exactly the kind of intermittent fault that is miserable to diagnose.

**Build it on the breadboards first regardless.** The staged testing in the plan
depends on being able to change wiring between tasks, which soldered work does not
allow. Once it is working and calibrated and you have lived with it for a few weeks,
you will know whether it is worth soldering a permanent version — and by then you
will also know the circuit well enough to do it confidently.

If size turns out to matter more than you expect, the OLED is the easiest thing to
drop: it exists for setup and calibration, and is unreadable from the driver's seat
anyway. The LED bar is the actual driving interface. Removing it would save the
27 × 27 mm module and free four connections, at the cost of having to calibrate
against serial output instead of a screen.
