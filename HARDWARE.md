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

### Not in the kit — you must supply

**A USB power bank.** Any phone bank works. This is the only thing you need to buy.

Do not use the kit's 9 V battery: the DevKit's linear regulator burns most of it
as heat and you would get roughly 5 hours.

### Note on jumper wires

The HC-SR04, OLED and DHT11 modules all have 0.1"-spaced male header pins, so they
plug **straight into the breadboard**. You do not need female-to-male wires for
them — which is good, because those three modules need 11 connections and the kit
only ships 10 F-M wires.

## Left over (not used in this build)

GY-6500 gyro, ULN2003 + stepper, SG90 servo, 5 V relay, IR receiver + emitter +
remote, joystick, fan blade + motor, **active** buzzer, L293D, RC522 RFID, membrane
keypad, HC-SR501 PIR, 4 spare buttons, potentiometer, both 7-segment displays, tilt
ball switch, white/blue/RGB LEDs, thermistor, photoresistors, diodes, 9 V battery,
1 spare PN2222, the breadboard power supply module, and most resistor values.

Two of these were considered and deliberately rejected:

- **Active buzzer** — louder, but one fixed pitch. The passive buzzer can vary beep
  *rate*, which is what makes real parking sensors readable without looking.
- **HC-SR501 PIR** — could wake the device on motion, but it would also trigger on
  you walking through the garage. Unplugging is simpler and more predictable.

## Power rails

The board needs **two separate rails**. Getting these crossed is the one wiring
mistake that can destroy parts.

| Rail | Source | Feeds |
|---|---|---|
| **5 V** | ESP32 `VIN` pin | HC-SR04 **only** |
| **3.3 V** | ESP32 `3V3` pin | OLED, DHT11, 74HC595, buzzer, everything else |
| **GND** | ESP32 `GND` | Everything (all grounds common) |

`VIN` carries ~5 V whenever the board is USB-powered.

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
