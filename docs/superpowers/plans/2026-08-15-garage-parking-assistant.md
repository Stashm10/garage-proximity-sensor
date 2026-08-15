# Lateral Clearance Parking Assistant — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a portable, battery-powered device that measures lateral clearance between a car's flank and a garage wall, and warns the driver with an LED bar and rising-rate beeping before contact.

**Architecture:** Four decoupled units — `Thermo` (temperature → speed of sound), `Ranger` (ping → filtered distance), `Tracker` (pass detection, minimum-hold, threshold bands), `Presenter` (LED bar, buzzer, OLED). The sketch's `loop()` wires them together and owns nothing else. `Ranger` and `Presenter` never reference each other, which is the seam a future two-board ESP-NOW version would split along.

**Tech Stack:** Arduino IDE 2.x, ESP32 Arduino core **3.x**, C++. Libraries: Adafruit GFX, Adafruit SSD1306, Adafruit DHT sensor library, Adafruit Unified Sensor. ESP32 `Preferences` (NVS) for calibration persistence.

## Global Constraints

- **Board:** ESP32-WROOM-32, selected in Arduino IDE as **"ESP32 Dev Module"**.
- **ESP32 Arduino core must be 3.x.** Core 2.x does not provide `tone()`/`noTone()` on ESP32 and Task 6 will not compile. If stuck on 2.x, substitute `ledcSetup`/`ledcAttachPin`/`ledcWriteTone`.
- **HC-SR04 ECHO must go through the 1 kΩ / 2 kΩ divider.** Never connect ECHO directly to a GPIO. This is a plain HC-SR04, not the 3.3 V "P" variant.
- **74HC595 VCC = 3.3 V, never 5 V.** At 5 V a 74HC part needs V_IH = 3.5 V and the ESP32's 3.3 V logic falls below threshold.
- **HC-SR04 VCC = 5 V** from the ESP32's VIN pin (present whenever USB-powered).
- **Disconnect USB power before changing any wiring.**
- Pin assignments are fixed in `config.h` and must not be changed ad hoc — they were chosen to avoid strapping pins (0, 2, 5, 12, 15), flash pins (6–11), and input-only pins (34–39).
- All distances are **inches**, all times **milliseconds**, unless a name says otherwise.

## File Structure

All files live in a single Arduino sketch folder. In Arduino IDE, each file appears as a **tab**; create them with the **▾ button below the serial monitor icon → New Tab**. The folder name and the `.ino` filename must match exactly.

```
parking_assistant/
  parking_assistant.ino   Wires the units together. setup() + loop() only.
  config.h                Every pin number and tunable constant. Single source of truth.
  Thermo.h / Thermo.cpp   DHT11 → speed of sound in inches/µs. Non-blocking.
  Ranger.h / Ranger.cpp   TRIG/ECHO → rolling median-of-3 filtered distance.
  Tracker.h / Tracker.cpp Pass state, minimum-hold, threshold bands. Pure logic, no I/O.
  Button.h / Button.cpp   Debounced short/long press detection.
  Presenter.h / Presenter.cpp  74HC595 LED bar, buzzer, OLED. Output only.
```

`Tracker` is deliberately free of hardware calls — it is the one unit whose behavior can be reasoned about without the board powered on.

---

### Task 0: Toolchain and bare-board verification

Nothing is wired in this task. The goal is to prove the IDE, the board package, the USB cable, and the board itself all work before any component can be blamed. **A charge-only USB cable that does not carry data is the single most common failure at this stage.**

**Files:**
- Create: `parking_assistant/parking_assistant.ino`

- [ ] **Step 1: Install Arduino IDE 2.x**

Download from `arduino.cc/en/software`. Install and launch.

- [ ] **Step 2: Add the ESP32 board package**

Open **Settings → Additional boards manager URLs** and paste:

```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Then **Tools → Board → Boards Manager**, search `esp32`, install **"esp32 by Espressif Systems"**. Confirm the installed version is **3.x**.

- [ ] **Step 3: Install libraries**

**Tools → Manage Libraries**, install each and accept the "install dependencies" prompt when offered:

- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `DHT sensor library` (by Adafruit)
- `Adafruit Unified Sensor`

- [ ] **Step 4: Select board and port**

**Tools → Board → esp32 → ESP32 Dev Module.** Plug the board in via USB-A-to-C and pick the new port under **Tools → Port**.

If no new port appears, the board's USB-serial chip needs a driver. Look at the small chip near the USB connector: `CP2102` needs the Silicon Labs VCP driver; `CH340`/`CH9102` needs the WCH driver. If a port still never appears, try a different USB cable before anything else.

- [ ] **Step 5: Write the blink sketch**

```cpp
void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT);
}

void loop() {
  digitalWrite(2, HIGH);
  Serial.println("on");
  delay(500);
  digitalWrite(2, LOW);
  Serial.println("off");
  delay(500);
}
```

- [ ] **Step 6: Upload and verify**

Click Upload. Open **Serial Monitor** and set the baud dropdown to **115200**.

Expected: the small blue LED on the board blinks once per second, and the monitor prints alternating `on` / `off`.

If upload fails with `Failed to connect ... Wrong boot mode detected`, hold the **BOOT** button on the board while the IDE prints "Connecting…", then release.

- [ ] **Step 7: Commit**

```bash
git add parking_assistant/parking_assistant.ino
git commit -m "chore: verify toolchain with blink sketch"
```

---

### Task 1: Raw ultrasonic reading

**Files:**
- Create: `parking_assistant/config.h`
- Modify: `parking_assistant/parking_assistant.ino`

**Interfaces:**
- Produces: `config.h` defining all `PIN_*` constants and ranging constants used by every later task.

- [ ] **Step 1: Wire the sensor**

Power off. Seat the ESP32 across the breadboard's center trench.

| From | To |
|---|---|
| ESP32 `VIN` | breadboard `+` rail (this rail is 5 V) |
| ESP32 `GND` | breadboard `−` rail |
| HC-SR04 `VCC` | `+` rail (5 V) |
| HC-SR04 `GND` | `−` rail |
| HC-SR04 `TRIG` | ESP32 GPIO 17 |
| HC-SR04 `ECHO` | one leg of a **1 kΩ** resistor |
| 1 kΩ other leg | ESP32 GPIO 16 — **and** one leg of a **2 kΩ** resistor |
| 2 kΩ other leg | `−` rail |

The two resistors form the divider that drops ECHO's 5 V to 3.33 V. Double-check this before applying power.

- [ ] **Step 2: Create `config.h`**

```cpp
#pragma once
#include <Arduino.h>

// ---- Pins (ESP32-WROOM-32) ----
constexpr int PIN_TRIG      = 17;
constexpr int PIN_ECHO      = 16;
constexpr int PIN_DHT       = 4;
constexpr int PIN_595_DATA  = 26;
constexpr int PIN_595_LATCH = 27;
constexpr int PIN_595_CLOCK = 13;
constexpr int PIN_BUZZER    = 25;
constexpr int PIN_BUTTON    = 33;

// ---- Ranging ----
constexpr uint32_t PING_INTERVAL_MS = 20;
constexpr uint32_t ECHO_TIMEOUT_US  = 12000;   // ~81 in one-way
constexpr float    MIN_VALID_INCHES = 1.5f;
constexpr float    MAX_VALID_INCHES = 80.0f;

// ---- Temperature ----
constexpr uint32_t DHT_INTERVAL_MS = 5000;
constexpr float    DEFAULT_TEMP_C  = 20.0f;

// ---- Pass tracking ----
constexpr float    PASS_ACTIVE_INCHES = 40.0f;
constexpr uint32_t PASS_END_MS        = 3000;
constexpr uint32_t MIN_HOLD_MS        = 30000;

// ---- Threshold bands (offsets above D_ref) ----
constexpr float BAND_SAFE_OFFSET    = 12.0f;
constexpr float BAND_OK_OFFSET      = 8.0f;
constexpr float BAND_CAUTION_OFFSET = 4.0f;
constexpr float BAND_WARN_OFFSET    = 1.5f;

// ---- Calibration ----
constexpr float    DEFAULT_D_REF_INCHES = 8.0f;
constexpr uint32_t BUTTON_LONG_PRESS_MS = 2000;
constexpr uint32_t BUTTON_DEBOUNCE_MS   = 50;

// ---- Buzzer ----
constexpr uint32_t BEEP_INTERVAL_SLOW_MS = 800;
constexpr uint32_t BEEP_INTERVAL_FAST_MS = 120;
constexpr int      BEEP_FREQ_HZ          = 2200;
```

- [ ] **Step 3: Replace the sketch with a raw-reading test**

```cpp
#include "config.h"

void setup() {
  Serial.begin(115200);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
  Serial.println("raw ranger test");
}

void loop() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(3);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long dur = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);

  if (dur == 0) {
    Serial.println("no target");
  } else {
    float inches = (dur * 0.0135205f) / 2.0f;
    Serial.print(dur);
    Serial.print(" us  ");
    Serial.print(inches, 2);
    Serial.println(" in");
  }
  delay(200);
}
```

- [ ] **Step 4: Verify against a tape measure**

Upload. Stand a book flat and face-on to the sensor. Measure with a tape and compare at **4, 8, 12, 24, and 36 inches**.

Expected: each printed value within **±0.5 in** of the tape. Readings will visibly jitter by a few tenths — that is the noise Task 2 removes, not a fault.

Point the sensor at open air across the room. Expected: `no target`, printed promptly at the same ~200 ms cadence. If the `no target` lines are visibly slower than the ranged lines, `ECHO_TIMEOUT_US` is not being applied — check Step 3.

- [ ] **Step 5: Commit**

```bash
git add parking_assistant/config.h parking_assistant/parking_assistant.ino
git commit -m "feat: raw HC-SR04 reading with level shifting and echo timeout"
```

---

### Task 2: Ranger — rolling median filter

**Files:**
- Create: `parking_assistant/Ranger.h`, `parking_assistant/Ranger.cpp`
- Modify: `parking_assistant/parking_assistant.ino`

**Interfaces:**
- Consumes: `config.h` constants.
- Produces: `class Ranger` with `void begin()`, `bool update(float inchesPerUs)`, `float inches() const`, `bool hasTarget() const`.

- [ ] **Step 1: Create `Ranger.h`**

```cpp
#pragma once
#include <Arduino.h>
#include "config.h"

class Ranger {
public:
  void begin();

  // Fires one ping if PING_INTERVAL_MS has elapsed.
  // Returns true when a ping was taken this call (not that a target was seen).
  bool update(float inchesPerUs);

  float inches() const    { return _filtered; }
  bool  hasTarget() const { return _hasTarget; }

private:
  float    _window[3] = {0, 0, 0};
  uint8_t  _count     = 0;
  uint8_t  _idx       = 0;
  float    _filtered  = 0.0f;
  bool     _hasTarget = false;
  uint32_t _lastPing  = 0;

  float pingInches(float inchesPerUs);
};
```

- [ ] **Step 2: Create `Ranger.cpp`**

```cpp
#include "Ranger.h"

static float median3(float a, float b, float c) {
  float t;
  if (a > b) { t = a; a = b; b = t; }
  if (b > c) { t = b; b = c; c = t; }
  if (a > b) { t = a; a = b; b = t; }
  return b;
}

void Ranger::begin() {
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
}

float Ranger::pingInches(float inchesPerUs) {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(3);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long dur = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);
  if (dur == 0) return -1.0f;

  float in = (dur * inchesPerUs) / 2.0f;
  if (in < MIN_VALID_INCHES || in > MAX_VALID_INCHES) return -1.0f;
  return in;
}

bool Ranger::update(float inchesPerUs) {
  uint32_t now = millis();
  if (now - _lastPing < PING_INTERVAL_MS) return false;
  _lastPing = now;

  float d = pingInches(inchesPerUs);

  if (d < 0.0f) {
    // Invalidate the window so a reappearing target refills it cleanly
    // rather than blending stale samples with fresh ones.
    _count = 0;
    _idx = 0;
    _hasTarget = false;
    return true;
  }

  _window[_idx] = d;
  _idx = (_idx + 1) % 3;
  if (_count < 3) _count++;

  if (_count < 3) {
    _hasTarget = false;   // not enough samples to trust yet
    return true;
  }

  _filtered = median3(_window[0], _window[1], _window[2]);
  _hasTarget = true;
  return true;
}
```

- [ ] **Step 3: Update the sketch to compare raw vs filtered**

```cpp
#include "config.h"
#include "Ranger.h"

Ranger ranger;
uint32_t lastPrint = 0;

void setup() {
  Serial.begin(115200);
  ranger.begin();
  Serial.println("filtered ranger test");
}

void loop() {
  ranger.update(0.0135205f);   // fixed constant until Task 3

  if (millis() - lastPrint >= 200) {
    lastPrint = millis();
    if (ranger.hasTarget()) {
      Serial.println(ranger.inches(), 2);
    } else {
      Serial.println("no target");
    }
  }
}
```

- [ ] **Step 4: Verify filtering actually reduces jitter**

Upload. Hold a book **fixed** at 12 in — clamp it or lean it against something so it genuinely does not move. Watch the Serial Monitor for 20 seconds.

Expected: the spread between the highest and lowest printed value is **smaller than it was in Task 1** at the same distance, and there are **no isolated wild values** (e.g. a lone `3.1` among `12.x` readings). Rejecting those outliers is the entire point of the median.

Then use **Tools → Serial Plotter** (also at 115200) and wave the book slowly toward and away. Expected: a smooth trace that tracks the motion without visible lag.

- [ ] **Step 5: Commit**

```bash
git add parking_assistant/Ranger.h parking_assistant/Ranger.cpp parking_assistant/parking_assistant.ino
git commit -m "feat: rolling median-of-3 filter with outlier rejection"
```

---

### Task 3: Thermo — temperature-corrected speed of sound

**Files:**
- Create: `parking_assistant/Thermo.h`, `parking_assistant/Thermo.cpp`
- Modify: `parking_assistant/parking_assistant.ino`

**Interfaces:**
- Produces: `class Thermo` with `void begin()`, `void update()`, `float inchesPerMicrosecond() const`, `float temperatureC() const`, `bool hasReading() const`.

- [ ] **Step 1: Wire the DHT11**

Power off. The DHT11 module (3-pin, on a small PCB) has pins usually labelled `+`/`out`/`−` or `VCC`/`DATA`/`GND`:

| From | To |
|---|---|
| DHT11 `VCC` | breadboard **3.3 V** rail |
| DHT11 `GND` | `−` rail |
| DHT11 `DATA` | ESP32 GPIO 4 |
| 10 kΩ resistor | between GPIO 4 and 3.3 V (pull-up) |

Run a jumper from the ESP32's `3V3` pin to a second `+` rail if you have not already — the 5 V rail from Task 1 must stay separate and feed only the HC-SR04.

Most DHT11 breakout boards already include the pull-up resistor. Adding a second one in parallel is harmless.

- [ ] **Step 2: Create `Thermo.h`**

```cpp
#pragma once
#include <Arduino.h>
#include "config.h"

class Thermo {
public:
  void begin();

  // Non-blocking: performs a real DHT11 read only every DHT_INTERVAL_MS.
  void update();

  float inchesPerMicrosecond() const;
  float temperatureC() const { return _tempC; }
  bool  hasReading() const   { return _hasReading; }

private:
  float    _tempC      = DEFAULT_TEMP_C;
  bool     _hasReading = false;
  uint32_t _lastRead   = 0;
};
```

- [ ] **Step 3: Create `Thermo.cpp`**

```cpp
#include "Thermo.h"
#include <DHT.h>

static DHT dht(PIN_DHT, DHT11);

void Thermo::begin() {
  dht.begin();
  _lastRead = millis() - DHT_INTERVAL_MS;   // force a read on the first update()
}

void Thermo::update() {
  uint32_t now = millis();
  if (now - _lastRead < DHT_INTERVAL_MS) return;
  _lastRead = now;

  float t = dht.readTemperature();          // Celsius
  if (isnan(t)) return;                     // keep the last good value
  _tempC = t;
  _hasReading = true;
}

float Thermo::inchesPerMicrosecond() const {
  // Speed of sound in dry air: c = 331.3 + 0.606 * T(C), metres/second.
  float c_mps = 331.3f + 0.606f * _tempC;
  return c_mps * 39.3701f / 1000000.0f;
}
```

- [ ] **Step 4: Update the sketch**

```cpp
#include "config.h"
#include "Ranger.h"
#include "Thermo.h"

Ranger ranger;
Thermo thermo;
uint32_t lastPrint = 0;

void setup() {
  Serial.begin(115200);
  ranger.begin();
  thermo.begin();
  Serial.println("temperature-compensated ranger");
}

void loop() {
  thermo.update();
  ranger.update(thermo.inchesPerMicrosecond());

  if (millis() - lastPrint >= 500) {
    lastPrint = millis();
    Serial.print(thermo.temperatureC(), 1);
    Serial.print("C ");
    Serial.print(thermo.hasReading() ? "(live) " : "(fallback) ");
    if (ranger.hasTarget()) {
      Serial.print(ranger.inches(), 2);
      Serial.println(" in");
    } else {
      Serial.println("no target");
    }
  }
}
```

- [ ] **Step 5: Verify live reading and fallback**

Upload. Expected within 5 seconds: the line reads `(live)` with a plausible room temperature (roughly 18–26 C indoors).

Cup your hands around the DHT11 and breathe on it for 15 seconds. Expected: the temperature climbs several degrees, and the reported distance to a **fixed** target rises very slightly — roughly 0.1 in per 10 C at 24 in. That small shift, in the correct direction, is the whole feature.

Now power off, **disconnect the DHT11 data wire**, and power back on. Expected: the line reads `(fallback)` at `20.0C` and ranging continues normally. A dead temperature sensor must never stop the device from measuring distance.

Reconnect the data wire before continuing.

- [ ] **Step 6: Commit**

```bash
git add parking_assistant/Thermo.h parking_assistant/Thermo.cpp parking_assistant/parking_assistant.ino
git commit -m "feat: DHT11 temperature compensation with safe fallback"
```

---

### Task 4: Tracker — pass detection, minimum-hold, bands

This unit touches no hardware, so verify it by driving it from the serial monitor before trusting it with a car.

**Files:**
- Create: `parking_assistant/Tracker.h`, `parking_assistant/Tracker.cpp`
- Modify: `parking_assistant/parking_assistant.ino`

**Interfaces:**
- Produces: `enum Band { BAND_NONE, BAND_SAFE, BAND_OK, BAND_CAUTION, BAND_WARN, BAND_DANGER }` and `class Tracker` with `void begin(float dRef)`, `void update(bool hasTarget, float inches)`, `Band band() const`, `int ledCount() const`, `float currentInches() const`, `bool hasMin() const`, `float minInches() const`, `float dRef() const`, `void setDRef(float)`, `void clearMin()`, `bool passActive() const`.

- [ ] **Step 1: Create `Tracker.h`**

```cpp
#pragma once
#include <Arduino.h>
#include "config.h"

enum Band {
  BAND_NONE,     // nothing within PASS_ACTIVE_INCHES
  BAND_SAFE,
  BAND_OK,
  BAND_CAUTION,
  BAND_WARN,
  BAND_DANGER
};

class Tracker {
public:
  void begin(float dRef);
  void update(bool hasTarget, float inches);

  Band  band() const;
  int   ledCount() const;

  float currentInches() const { return _current; }
  bool  passActive() const    { return _passActive; }
  bool  hasMin() const        { return _hasMin; }
  float minInches() const     { return _min; }

  float dRef() const          { return _dRef; }
  void  setDRef(float d)      { _dRef = d; }
  void  clearMin()            { _hasMin = false; }

private:
  float    _dRef        = DEFAULT_D_REF_INCHES;
  float    _current     = 0.0f;
  bool     _hasTarget   = false;
  bool     _passActive  = false;
  bool     _hasMin      = false;
  float    _min         = 0.0f;
  uint32_t _lastInRange = 0;
  uint32_t _passEnded   = 0;
};
```

- [ ] **Step 2: Create `Tracker.cpp`**

```cpp
#include "Tracker.h"

void Tracker::begin(float dRef) {
  _dRef = dRef;
}

void Tracker::update(bool hasTarget, float inches) {
  uint32_t now = millis();
  _hasTarget = hasTarget;
  if (hasTarget) _current = inches;

  bool inRange = hasTarget && inches < PASS_ACTIVE_INCHES;

  if (inRange) {
    _lastInRange = now;
    if (!_passActive) {
      // A new pass begins: seed the minimum with this first reading.
      _passActive = true;
      _min = inches;
      _hasMin = true;
    }
    if (inches < _min) _min = inches;
  } else if (_passActive && (now - _lastInRange >= PASS_END_MS)) {
    _passActive = false;
    _passEnded = now;
  }

  // Expire the held minimum only after the pass has fully ended.
  if (!_passActive && _hasMin && (now - _passEnded >= MIN_HOLD_MS)) {
    _hasMin = false;
  }
}

Band Tracker::band() const {
  if (!_hasTarget || _current >= PASS_ACTIVE_INCHES) return BAND_NONE;
  if (_current >= _dRef + BAND_SAFE_OFFSET)    return BAND_SAFE;
  if (_current >= _dRef + BAND_OK_OFFSET)      return BAND_OK;
  if (_current >= _dRef + BAND_CAUTION_OFFSET) return BAND_CAUTION;
  if (_current >= _dRef + BAND_WARN_OFFSET)    return BAND_WARN;
  return BAND_DANGER;
}

int Tracker::ledCount() const {
  switch (band()) {
    case BAND_SAFE:    return 5;
    case BAND_OK:      return 4;
    case BAND_CAUTION: return 3;
    case BAND_WARN:    return 2;
    case BAND_DANGER:  return 1;
    default:           return 0;
  }
}
```

- [ ] **Step 3: Update the sketch to report tracker state**

```cpp
#include "config.h"
#include "Ranger.h"
#include "Thermo.h"
#include "Tracker.h"

Ranger  ranger;
Thermo  thermo;
Tracker tracker;
uint32_t lastPrint = 0;

static const char* bandName(Band b) {
  switch (b) {
    case BAND_SAFE:    return "SAFE";
    case BAND_OK:      return "OK";
    case BAND_CAUTION: return "CAUTION";
    case BAND_WARN:    return "WARN";
    case BAND_DANGER:  return "DANGER";
    default:           return "-";
  }
}

void setup() {
  Serial.begin(115200);
  ranger.begin();
  thermo.begin();
  tracker.begin(DEFAULT_D_REF_INCHES);
  Serial.println("tracker test (D_ref = 8.0 in)");
}

void loop() {
  thermo.update();
  if (ranger.update(thermo.inchesPerMicrosecond())) {
    tracker.update(ranger.hasTarget(), ranger.inches());
  }

  if (millis() - lastPrint >= 250) {
    lastPrint = millis();
    Serial.print(bandName(tracker.band()));
    Serial.print(" leds=");
    Serial.print(tracker.ledCount());
    Serial.print(" cur=");
    Serial.print(tracker.currentInches(), 1);
    Serial.print(" min=");
    if (tracker.hasMin()) Serial.print(tracker.minInches(), 1);
    else                  Serial.print("--");
    Serial.print(tracker.passActive() ? " [pass]" : "");
    Serial.println();
  }
}
```

- [ ] **Step 4: Verify the band boundaries**

With `D_ref = 8.0`, the boundaries are at 20, 16, 12, and 9.5 inches. Hold the book at each and confirm the reported band:

| Book at | Expected band | Expected `leds=` |
|---|---|---|
| 24 in | `SAFE` | 5 |
| 18 in | `OK` | 4 |
| 14 in | `CAUTION` | 3 |
| 10 in | `WARN` | 2 |
| 8 in | `DANGER` | 1 |
| across the room | `-` | 0 |

- [ ] **Step 5: Verify minimum-hold — the behavior this whole device exists for**

Start with nothing in range. Sweep the book past the sensor in one smooth motion, dipping to roughly 6 inches at the closest point, then take it fully away.

Expected: `min=` settles at approximately 6 and **stays there** while `cur=` climbs back up. This is the mirror-catching behavior: the closest approach is remembered even though it lasted a fraction of a second.

Expected: `[pass]` disappears about 3 seconds after the book leaves, and `min=` reverts to `--` about 30 seconds after that.

- [ ] **Step 6: Commit**

```bash
git add parking_assistant/Tracker.h parking_assistant/Tracker.cpp parking_assistant/parking_assistant.ino
git commit -m "feat: pass detection, minimum-hold and threshold bands"
```

---

### Task 5: LED bar via 74HC595

**Files:**
- Create: `parking_assistant/Presenter.h`, `parking_assistant/Presenter.cpp`
- Modify: `parking_assistant/parking_assistant.ino`

**Interfaces:**
- Produces: `class Presenter` with `void begin()`, `void setBar(int count, bool on)`. Extended in Tasks 6 and 7.

- [ ] **Step 1: Wire the 74HC595**

Power off. Seat the chip across the center trench. **Pin 1 is to the left of the half-moon notch when the notch faces up**; numbering runs counter-clockwise from there.

| 74HC595 chip pin | Name | Connect to |
|---|---|---|
| 16 | VCC | **3.3 V rail** |
| 8 | GND | `−` rail |
| 13 | OE | `−` rail |
| 10 | MR | 3.3 V rail |
| 14 | DS (data) | ESP32 GPIO 26 |
| 12 | ST_CP (latch) | ESP32 GPIO 27 |
| 11 | SH_CP (clock) | ESP32 GPIO 13 |

Chip pin 13 (OE) and ESP32 GPIO 13 (clock) are unrelated despite sharing a number. OE goes to ground; GPIO 13 goes to chip pin 11.

Now the LEDs. Each LED's **long leg (anode)** goes to its 595 output, **short leg (cathode)** goes through its resistor to the `−` rail. Mount them in a vertical line, Q0 at the bottom:

| 595 output | chip pin | LED colour | Resistor | Bar position |
|---|---|---|---|---|
| Q0 | 15 | red | 100 Ω | bottom |
| Q1 | 1 | yellow | 100 Ω | |
| Q2 | 2 | yellow | 100 Ω | |
| Q3 | 3 | green | 100 Ω | |
| Q4 | 4 | green | 100 Ω | top |

One resistor value for the whole bar, because the kit has 100 Ω and 220 Ω but no 150 Ω.
Worst-case package current with all five lit is ~59 mA, under the 74HC595's 70 mA limit.

Q5–Q7 (chip pins 5, 6, 7) stay unconnected.

- [ ] **Step 2: Create `Presenter.h`**

```cpp
#pragma once
#include <Arduino.h>
#include "config.h"

class Presenter {
public:
  void begin();

  // Lights `count` LEDs from the bottom of the bar. `on == false` blanks
  // the bar without forgetting the count, for flashing.
  void setBar(int count, bool on);

private:
  void writeBits(uint8_t bits);
};
```

- [ ] **Step 3: Create `Presenter.cpp`**

```cpp
#include "Presenter.h"

void Presenter::begin() {
  pinMode(PIN_595_DATA,  OUTPUT);
  pinMode(PIN_595_LATCH, OUTPUT);
  pinMode(PIN_595_CLOCK, OUTPUT);
  writeBits(0);
}

void Presenter::writeBits(uint8_t bits) {
  digitalWrite(PIN_595_LATCH, LOW);
  // MSBFIRST: the first bit shifted out lands on Q7, so byte bit N -> Q N.
  shiftOut(PIN_595_DATA, PIN_595_CLOCK, MSBFIRST, bits);
  digitalWrite(PIN_595_LATCH, HIGH);
}

void Presenter::setBar(int count, bool on) {
  uint8_t bits = 0;
  if (on) {
    if (count > 5) count = 5;
    for (int i = 0; i < count; i++) bits |= (1 << i);
  }
  writeBits(bits);
}
```

- [ ] **Step 4: Sweep test the bar**

Temporarily replace `loop()` in the sketch with a sweep, keeping the existing includes and `setup()` but adding `presenter.begin();` to `setup()`:

```cpp
void loop() {
  for (int n = 0; n <= 5; n++) {
    presenter.setBar(n, true);
    Serial.println(n);
    delay(400);
  }
}
```

Also add `Presenter presenter;` near the other globals and `#include "Presenter.h"`.

Expected: the bar fills from the **bottom** (red first), one LED at a time, up to all five, then repeats. If it fills from the top, Q0 and Q4 are swapped — recheck the LED wiring table. If nothing lights, check that chip pin 16 is on **3.3 V** and pin 13 (OE) is on **ground**.

Green LEDs will be noticeably dimmer than red and yellow. This is expected — their forward voltage is close to the 3.3 V supply. If a green LED is unusably dim, drop its resistor to 68 Ω or substitute a yellow.

- [ ] **Step 5: Drive the bar from the tracker**

Restore `loop()` to the Task 4 version and add this before the print block:

```cpp
  presenter.setBar(tracker.ledCount(), true);
```

Expected: moving the book toward the sensor drops the bar from 5 lit down to 1 lit, matching the `leds=` value printed on serial.

- [ ] **Step 6: Commit**

```bash
git add parking_assistant/Presenter.h parking_assistant/Presenter.cpp parking_assistant/parking_assistant.ino
git commit -m "feat: 5-LED bar graph driven by 74HC595"
```

---

### Task 6: Buzzer with rising beep rate

**Files:**
- Modify: `parking_assistant/Presenter.h`, `parking_assistant/Presenter.cpp`, `parking_assistant/parking_assistant.ino`

**Interfaces:**
- Produces: `void Presenter::updateSound(Band band, float inches, float dRef)` — called every loop, manages beeping internally with no blocking delays.

- [ ] **Step 1: Wire the buzzer through a PN2222**

Power off. The PN2222 in a TO-92 package, **flat face toward you, legs down**, is Emitter–Base–Collector left to right.

| From | To |
|---|---|
| ESP32 GPIO 25 | one leg of a **1 kΩ** resistor |
| 1 kΩ other leg | PN2222 **base** (middle leg) |
| PN2222 **emitter** (left leg) | `−` rail |
| PN2222 **collector** (right leg) | passive buzzer **−** leg |
| passive buzzer **+** leg | **3.3 V** rail |

The passive buzzer is the one **without** a sticker on top and with an open PCB underside. Do not use the active buzzer here — it has its own internal oscillator and will ignore the frequency.

- [ ] **Step 2: Add the sound interface to `Presenter.h`**

Add `#include "Tracker.h"` at the top, and inside the class:

```cpp
public:
  void updateSound(Band band, float inches, float dRef);

private:
  bool     _beepOn      = false;
  uint32_t _beepChanged = 0;
```

- [ ] **Step 3: Implement `updateSound` in `Presenter.cpp`**

```cpp
void Presenter::updateSound(Band band, float inches, float dRef) {
  // Silent when nothing is in range or there is plenty of room.
  if (band == BAND_NONE || band == BAND_SAFE) {
    if (_beepOn) { noTone(PIN_BUZZER); _beepOn = false; }
    return;
  }

  // Closest band is a continuous tone, not a beep.
  if (band == BAND_DANGER) {
    if (!_beepOn) { tone(PIN_BUZZER, BEEP_FREQ_HZ); _beepOn = true; }
    return;
  }

  // Interval shrinks linearly from SLOW at (dRef + SAFE_OFFSET)
  // to FAST at (dRef + WARN_OFFSET).
  float span = BAND_SAFE_OFFSET - BAND_WARN_OFFSET;
  float x = (inches - dRef - BAND_WARN_OFFSET) / span;
  x = constrain(x, 0.0f, 1.0f);
  uint32_t interval = (uint32_t)(BEEP_INTERVAL_FAST_MS +
                      x * (BEEP_INTERVAL_SLOW_MS - BEEP_INTERVAL_FAST_MS));

  uint32_t now = millis();
  if (now - _beepChanged >= interval) {
    _beepChanged = now;
    _beepOn = !_beepOn;
    if (_beepOn) tone(PIN_BUZZER, BEEP_FREQ_HZ);
    else         noTone(PIN_BUZZER);
  }
}
```

- [ ] **Step 4: Call it from the sketch**

Add next to the `setBar` call:

```cpp
  presenter.updateSound(tracker.band(), tracker.currentInches(), tracker.dRef());
```

- [ ] **Step 5: Verify the rate rises smoothly**

Upload. Move the book slowly from 24 in toward the sensor.

Expected, in order: silence above 20 in; slow beeping begins around 20 in; the beeps get audibly faster as you close in; below about 9.5 in the sound becomes one continuous tone. Backing away reverses it cleanly with no stuck tone.

Confirm there is **no blocking**: while beeping, the serial output must keep updating at its normal 250 ms cadence. If the print rate stutters in time with the beeps, a `delay()` has crept into the sound path.

If nothing sounds, confirm you used the **passive** buzzer and that the ESP32 core version is 3.x — `tone()` does not exist on ESP32 core 2.x.

- [ ] **Step 6: Commit**

```bash
git add parking_assistant/Presenter.h parking_assistant/Presenter.cpp parking_assistant/parking_assistant.ino
git commit -m "feat: non-blocking rising-rate beeping via passive buzzer"
```

---

### Task 7: Button and persistent calibration

**Files:**
- Create: `parking_assistant/Button.h`, `parking_assistant/Button.cpp`
- Modify: `parking_assistant/parking_assistant.ino`

**Interfaces:**
- Produces: `class Button` with `void begin()`, `void update()`, `bool consumeShortPress()`, `bool consumeLongPress()`. Each `consume*` returns true at most once per physical press.

- [ ] **Step 1: Wire the button**

Power off. A tactile button's legs are connected in pairs across the gap, so straddle the breadboard trench.

| From | To |
|---|---|
| button leg (one side) | ESP32 GPIO 33 |
| button leg (other side) | `−` rail |

No resistor: the internal pull-up is enabled in software, so the pin reads HIGH when open and LOW when pressed.

- [ ] **Step 2: Create `Button.h`**

```cpp
#pragma once
#include <Arduino.h>
#include "config.h"

class Button {
public:
  void begin();
  void update();

  bool consumeShortPress();
  bool consumeLongPress();

private:
  bool     _stable       = false;  // true == pressed
  bool     _lastRaw      = false;
  uint32_t _lastChange   = 0;
  uint32_t _pressedAt    = 0;
  bool     _longFired    = false;
  bool     _shortPending = false;
  bool     _longPending  = false;
};
```

- [ ] **Step 3: Create `Button.cpp`**

```cpp
#include "Button.h"

void Button::begin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
}

void Button::update() {
  uint32_t now = millis();
  bool raw = (digitalRead(PIN_BUTTON) == LOW);   // LOW == pressed

  if (raw != _lastRaw) {
    _lastRaw = raw;
    _lastChange = now;
  }

  if (now - _lastChange >= BUTTON_DEBOUNCE_MS && raw != _stable) {
    _stable = raw;
    if (_stable) {
      _pressedAt = now;
      _longFired = false;
    } else if (!_longFired) {
      // Released before the long-press threshold.
      _shortPending = true;
    }
  }

  // Fire the long press while still held, so it feels immediate.
  if (_stable && !_longFired && (now - _pressedAt >= BUTTON_LONG_PRESS_MS)) {
    _longFired = true;
    _longPending = true;
  }
}

bool Button::consumeShortPress() {
  if (!_shortPending) return false;
  _shortPending = false;
  return true;
}

bool Button::consumeLongPress() {
  if (!_longPending) return false;
  _longPending = false;
  return true;
}
```

- [ ] **Step 4: Wire calibration into the sketch**

Add `#include <Preferences.h>` and `#include "Button.h"`, then these globals:

```cpp
Button      button;
Preferences prefs;
```

In `setup()`, replace the `tracker.begin(...)` line with:

```cpp
  button.begin();
  prefs.begin("parkassist", false);
  float saved = prefs.getFloat("dref", DEFAULT_D_REF_INCHES);
  tracker.begin(saved);
  Serial.print("loaded D_ref = ");
  Serial.println(saved, 1);
```

In `loop()`, after `tracker.update(...)`:

```cpp
  button.update();

  if (button.consumeLongPress()) {
    if (ranger.hasTarget()) {
      float d = ranger.inches();
      tracker.setDRef(d);
      prefs.putFloat("dref", d);
      Serial.print("saved D_ref = ");
      Serial.println(d, 1);
      presenter.chirpSaved();
    } else {
      Serial.println("calibration refused: no target in view");
      presenter.chirpRefused();
    }
  }

  if (button.consumeShortPress()) {
    tracker.clearMin();
    Serial.println("min cleared");
  }
```

Add the two chirp methods to `Presenter.h` (public) and `Presenter.cpp`:

```cpp
void Presenter::chirpSaved() {
  tone(PIN_BUZZER, 1800); delay(120);
  tone(PIN_BUZZER, 2600); delay(120);
  noTone(PIN_BUZZER);
  // Reset the beep state machine so updateSound() does not think a tone
  // is still playing and skip restarting it.
  _beepOn = false;
  _beepChanged = millis();
}

void Presenter::chirpRefused() {
  tone(PIN_BUZZER, 700); delay(400);
  noTone(PIN_BUZZER);
  _beepOn = false;
  _beepChanged = millis();
}
```

The chirps must live in `Presenter` rather than being raw `tone()` calls in the sketch. `Presenter` tracks buzzer state in `_beepOn`; driving the buzzer from outside leaves that flag stale, and the next approach beep can then fail to start.

The blocking `delay()` calls inside them are acceptable: they run only on an explicit button press while the car is already stopped, never during a pass.

- [ ] **Step 5: Verify persistence across a power cycle**

Upload. Hold the book at a measured **14 inches** and hold the button for 2 seconds.

Expected: a two-tone chirp, and serial prints `saved D_ref = 14.0`.

Now **unplug the USB entirely**, wait five seconds, and plug it back in. Expected: serial prints `loaded D_ref = 14.0`. Distance bands are now measured from 14 in, so the book at 14 in should read `DANGER`.

Point the sensor at empty air and hold the button for 2 seconds. Expected: a single low buzz and `calibration refused: no target in view`. Saving a garbage reference is worse than not saving one.

Press and release the button quickly during a pass. Expected: `min cleared`.

Finally, set the reference back to something realistic for testing: hold the book at 8 in and long-press.

- [ ] **Step 6: Commit**

```bash
git add parking_assistant/Button.h parking_assistant/Button.cpp parking_assistant/parking_assistant.ino
git commit -m "feat: debounced button with NVS-persisted calibration"
```

---

### Task 8: OLED display

**Files:**
- Modify: `parking_assistant/Presenter.h`, `parking_assistant/Presenter.cpp`, `parking_assistant/parking_assistant.ino`

**Interfaces:**
- Produces: `bool Presenter::beginDisplay()` returning false if the OLED does not respond, and `void Presenter::draw(Band band, float inches, bool hasTarget, bool hasMin, float minInches, float dRef, float tempC)`.

- [ ] **Step 1: Wire the OLED**

Power off.

| From | To |
|---|---|
| OLED `VCC` | **3.3 V** rail |
| OLED `GND` | `−` rail |
| OLED `SDA` | ESP32 GPIO 21 |
| OLED `SCL` | ESP32 GPIO 22 |

- [ ] **Step 2: Extend `Presenter.h`**

Add at the top:

```cpp
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
```

and inside the class:

```cpp
public:
  bool beginDisplay();
  void draw(Band band, float inches, bool hasTarget,
            bool hasMin, float minInches, float dRef, float tempC);

private:
  Adafruit_SSD1306 _oled{128, 64, &Wire, -1};
  bool     _haveOled   = false;
  uint32_t _lastDraw   = 0;
```

- [ ] **Step 3: Implement in `Presenter.cpp`**

```cpp
bool Presenter::beginDisplay() {
  _haveOled = _oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (_haveOled) {
    _oled.clearDisplay();
    _oled.setTextColor(SSD1306_WHITE);
    _oled.display();
  }
  return _haveOled;
}

void Presenter::draw(Band band, float inches, bool hasTarget,
                     bool hasMin, float minInches, float dRef, float tempC) {
  if (!_haveOled) return;

  // Redrawing the whole 128x64 buffer over I2C takes ~25 ms, which would
  // starve the ranging loop. 10 Hz is far faster than anyone can read.
  uint32_t now = millis();
  if (now - _lastDraw < 100) return;
  _lastDraw = now;

  _oled.clearDisplay();

  _oled.setTextSize(1);
  _oled.setCursor(0, 0);
  if (hasTarget) _oled.print("CLEARANCE");
  else           _oled.print("NO TARGET");

  _oled.setTextSize(3);
  _oled.setCursor(0, 14);
  if (hasTarget) _oled.print(inches, 1);
  else           _oled.print("--");

  _oled.setTextSize(1);
  _oled.setCursor(0, 44);
  _oled.print("MIN ");
  if (hasMin) _oled.print(minInches, 1);
  else        _oled.print("--");

  _oled.setCursor(0, 54);
  _oled.print("REF ");
  _oled.print(dRef, 1);
  _oled.print("  ");
  _oled.print(tempC, 0);
  _oled.print("C");

  _oled.display();
}
```

- [ ] **Step 4: Call it from the sketch**

In `setup()`, after `presenter.begin()`:

```cpp
  if (!presenter.beginDisplay()) {
    Serial.println("OLED not found at 0x3C - continuing without display");
  }
```

In `loop()`, next to the other presenter calls:

```cpp
  presenter.draw(tracker.band(), tracker.currentInches(), ranger.hasTarget(),
                 tracker.hasMin(), tracker.minInches(),
                 tracker.dRef(), thermo.temperatureC());
```

- [ ] **Step 5: Verify the display and that it degrades gracefully**

Upload. Expected: large live distance, with `MIN`, `REF` and temperature on the bottom two lines, all updating smoothly.

Confirm the ranging loop is not starved: sweep the book past quickly and check that `MIN` still catches the closest approach as reliably as it did in Task 4. If it now misses fast passes, the redraw throttle is not working.

Power off, **unplug the OLED's SDA wire**, power on. Expected: serial prints `OLED not found at 0x3C - continuing without display`, and the LED bar and buzzer keep working normally. Reconnect afterward.

If the display stays blank but is detected, some modules use address `0x3D` — change both occurrences of `0x3C` and retry.

- [ ] **Step 6: Commit**

```bash
git add parking_assistant/Presenter.h parking_assistant/Presenter.cpp parking_assistant/parking_assistant.ino
git commit -m "feat: throttled OLED readout with graceful absence handling"
```

---

### Task 9: DANGER flash and full-system bench test

**Files:**
- Modify: `parking_assistant/parking_assistant.ino`

- [ ] **Step 1: Add the DANGER flash**

Replace the `presenter.setBar(...)` call with:

```cpp
  bool barOn = true;
  if (tracker.band() == BAND_DANGER) {
    barOn = ((millis() / 150) % 2) == 0;   // ~3.3 Hz flash
  }
  presenter.setBar(tracker.ledCount(), barOn);
```

- [ ] **Step 2: Verify the complete loop**

Upload. Walk the book from across the room to touching the sensor and back out, twice.

Expected, all at once and all consistent with each other:

| Distance (D_ref = 8) | Bar | Sound | OLED |
|---|---|---|---|
| > 40 in | dark | silent | `NO TARGET`, `--` |
| 20–40 in | 5 lit | silent | live number |
| 16–20 in | 4 lit | slow beep | live number |
| 12–16 in | 3 lit | medium beep | live number |
| 9.5–12 in | 2 lit | fast beep | live number |
| < 9.5 in | 1 flashing | solid tone | live number |

Then confirm `MIN` on the OLED holds the closest approach after the book is withdrawn, and clears about 33 seconds later.

- [ ] **Step 3: Confirm loop timing is still healthy**

Add this temporarily at the top of `loop()`:

```cpp
  static uint32_t lastLoop = 0, worst = 0;
  uint32_t t = millis();
  if (lastLoop && (t - lastLoop) > worst) {
    worst = t - lastLoop;
    Serial.print("worst loop ms: "); Serial.println(worst);
  }
  lastLoop = t;
```

Run for a minute with the book moving.

Expected: worst loop time settles **below 40 ms**. Above that, the device can miss a mirror at parking speed — the likely cause is an unthrottled OLED redraw or a stray `delay()`. Remove this block once satisfied.

- [ ] **Step 4: Commit**

```bash
git add parking_assistant/parking_assistant.ino
git commit -m "feat: DANGER flash and verified end-to-end timing"
```

---

### Task 10: Enclosure, mounting, and calibration with the car

- [ ] **Step 1: Build the enclosure**

Any small cardboard or plastic box. Cut:

- **two round holes** for the HC-SR04's transducer barrels — they must poke through cleanly and face straight out. A recessed sensor bounces its own pulse off the box interior and produces phantom close readings.
- **a vertical slot or five holes** for the LED bar, red at the bottom.
- **a rectangular window** for the OLED.
- **a hole** for the button, reachable from outside.
- **a notch** for the USB cable.

The power bank sits inside.

- [ ] **Step 2: Find the mounting height**

Park the car normally. With a tape measure, find the height of the **widest point of the car's flank** — usually the door crease or the base of the mirror. Measure from the floor. Note the number.

The sensor must sit at this height, aimed **perpendicular to the car's path**. Its beam is a roughly 30° cone, so aiming even slightly down will catch the floor and slightly up will catch the ceiling.

- [ ] **Step 3: Mount it**

Command strips (the Velcro variety) on the garage entrance wall, on the side the car passes first, at the height from Step 2. Confirm the sensor face is parallel to the wall — pointing straight across at the car, not angled along the wall.

- [ ] **Step 4: Calibrate against the car**

Power on from the power bank. Expected: it boots within two seconds and the OLED lights.

Pull the car in and stop it in the position where its side clearance is **as tight as you are willing to accept**. Get out and read the OLED. Hold the button for two seconds.

Expected: two-tone chirp, `REF` on the OLED updates to that distance.

- [ ] **Step 5: Verify against a tape measure — the real test**

Back the car out fully. Wait for `MIN` to clear.

Drive in at normal speed. Stop. Read `MIN` on the OLED, then measure the actual gap at the tightest point with a tape measure.

Expected: `MIN` is within **±0.5 in** of the tape.

If `MIN` reads much *smaller* than reality, the sensor is clipping the wall, floor, or a door frame — re-aim it. If `MIN` reads much *larger*, it likely missed the mirror; re-check the mounting height and the loop timing from Task 9 Step 3.

Repeat three times. The three `MIN` values should agree within about an inch of each other.

- [ ] **Step 6: Tune the reference if needed**

Drive in normally a few times over a few days. If it alarms while the gap is still comfortable, `D_ref` is too large — re-park at a tighter position and long-press again. If it stays quiet when the gap feels tight, `D_ref` is too small.

Remember the accuracy budget: the device is good to ±0.5 in, so leave that much margin in whatever reference you save.

- [ ] **Step 7: Commit final configuration**

```bash
git add -A
git commit -m "docs: record measured mounting height and calibration results"
```

---

## Verification Summary

Each task's gate, in one place:

| Task | Gate |
|---|---|
| 0 | Onboard LED blinks; serial prints at 115200 |
| 1 | Raw distance within ±0.5 in of tape at 4/8/12/24/36 in; `no target` returns promptly |
| 2 | Jitter visibly reduced vs Task 1; no isolated wild values |
| 3 | Reads `(live)` at room temperature; falls back to 20.0C with the sensor unplugged |
| 4 | Bands match the boundary table; `MIN` holds a fast sweep's closest approach |
| 5 | Bar fills from the bottom, red first; tracks `leds=` |
| 6 | Beep rate rises smoothly; goes solid in DANGER; serial cadence unaffected |
| 7 | `D_ref` survives a full power cycle; refuses to save with no target |
| 8 | OLED updates smoothly; system still works with the OLED unplugged |
| 9 | All four outputs agree; worst loop time under 40 ms |
| 10 | `MIN` within ±0.5 in of a tape measure on three consecutive real drive-ins |
