# Garage Proximity Sensor

An ESP32 ultrasonic rangefinder that warns you before you scrape your car.

Built from a starter kit to solve a specific problem: a Lexus NX whose factory
parking sensors alarm constantly while there is still plenty of room — and which
had already been scratched twice on the sides, where those sensors do not help.

![Gengar on the OLED](garage_sensor/gengar_preview.png)

## What it does

Measures distance with an HC-SR04 ultrasonic sensor and shows it on a 0.96" OLED.

| Distance | Display |
|---|---|
| Over 15 in | Gengar |
| 8–15 in | Live distance readout |
| Under 8 in | `Roman! Too Close!` |

Both thresholds are one-line constants at the top of the sketch.

## Hardware

| Part | Notes |
|---|---|
| ESP32 DevKit V1 (WROOM-32) | 30-pin, USB-C |
| HC-SR04 ultrasonic sensor | 5 V part |
| SSD1306 OLED, 128×64 | I²C version, 4 pins |
| Breadboard + jumper wires | |

### Wiring

| From | To |
|---|---|
| HC-SR04 `VCC` | ESP32 `VIN` (5 V) |
| HC-SR04 `Gnd` | `GND` |
| HC-SR04 `Trig` | `D12` |
| HC-SR04 `Echo` | `D13` |
| OLED `VCC` | ESP32 `3V3` (**not** `VIN`) |
| OLED `GND` | `GND` |
| OLED `SDA` | `D21` |
| OLED `SCL` | `D22` |

Two things worth knowing:

- **The OLED runs at 3.3 V, not 5 V.** Its built-in pull-ups would otherwise drag
  the I²C lines to 5 V and into 3.3 V-rated ESP32 pins.
- **`Echo` has no voltage divider in this build.** The sensor drives 5 V into a
  3.3 V pin. It works, but it stresses the pin. The proper fix is
  `Echo → 1 kΩ → D13` with `2 kΩ` from `D13` to ground. See `HARDWARE.md`.

## Running it

1. Install the Arduino IDE and the ESP32 board package (`esp32` by Espressif, **3.x**)
2. Install libraries: `Adafruit GFX`, `Adafruit SSD1306`
3. Open `garage_sensor/garage_sensor.ino`
4. Board: **ESP32 Dev Module**
5. Upload, then open Serial Monitor at **115200**

`GETTING_STARTED.md` walks through all of this from scratch.

## How the measurement works

A single HC-SR04 reading jitters by a few tenths of an inch and occasionally
returns complete nonsense — a 300-inch spike with nothing in the room. Left raw,
that noise fires the proximity warning at random.

So each reading is the **median of three pings**. One bad ping cannot outvote two
good ones. Readings outside 1–120 inches are discarded before the vote, and the
echo timeout is capped at 30 ms so a missing echo costs milliseconds instead of
stalling the loop for a full second.

## Repo layout

| Path | What it is |
|---|---|
| `garage_sensor/` | **The working sketch.** Start here. |
| `HARDWARE.md` | Full parts list, power rails, wiring reference |
| `GETTING_STARTED.md` | Setup guide for a first Arduino project |
| `bringup/` | Staged test sketches used to debug the build |
| `parking_assistant/` | A larger modular design — see below |
| `docs/superpowers/` | Design spec and implementation plan |

## The larger design

`parking_assistant/` and `docs/` hold a more ambitious version that is **designed
but not yet built**: a lateral clearance assistant that tracks the *minimum* gap
during a pass, compensates for temperature, drives a 5-LED bar via a 74HC595, and
beeps at a rising rate.

The idea behind it: a side-facing sensor never sees a static distance. As a car
moves past, the gap traces a profile — bumper, fender, wheel, **mirror**, door. The
mirror usually sticks out furthest and passes in a fraction of a second, so what
matters is the tightest moment, not the current reading.

That firmware compiles and is split into four units (`Ranger`, `Thermo`, `Tracker`,
`Presenter`), but it has never run on assembled hardware. The sketch in
`garage_sensor/` is the part that actually works today.

## Debugging notes

`bringup/` holds the sketches used to find faults during the build, and they are
worth keeping:

- `07_diagnose.txt` — I²C scan plus raw echo-pin state
- `08_pintest.txt` — enables internal pulldowns to prove a GPIO isn't damaged
- Loopback test — writes one pin and reads another to clear the whole signal path

Most of the build time went into wiring faults, not code: a power rail shorted to
ground, a trigger pin that did not match the sketch, and grounds spread across
three unconnected rails. Every one was found by measuring rather than guessing.

The single most useful lesson: **breadboard holes are connected when they share a
number, not a letter.** `f21` and `j21` are the same electrical point. `f21` and
`f22` are not.

## License

MIT
