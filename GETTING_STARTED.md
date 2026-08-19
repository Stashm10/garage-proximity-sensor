# Getting started (never used Arduino before)

Read this first. It covers what to install, where everything lives, and what
order to do things in.

## 1. What to install

**One program: the Arduino IDE.** That is the only software you need.

1. Go to **https://www.arduino.cc/en/software**
2. Download the version for macOS
3. Open the downloaded file and drag Arduino IDE into your Applications folder
4. Launch it

There is no account to make and no website to sign into. Everything else below
happens inside that one program.

### Then, inside the Arduino IDE, three setup steps

These teach the IDE about your specific board. You do them once, ever.

**Step A — tell it where to find ESP32 boards**

Menu: **Arduino IDE → Settings** (or **File → Preferences** on Windows).

Find the box labelled **"Additional boards manager URLs"** and paste in:

```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Click OK.

**Step B — install the ESP32 board package**

Menu: **Tools → Board → Boards Manager**

A search panel opens on the left. Type `esp32`. Find **"esp32 by Espressif
Systems"** and click Install.

This is a large download and takes several minutes. When it finishes, check the
version number says **3.something**. If it says 2.something, the buzzer code will
not compile — see the troubleshooting note at the bottom.

**Step C — install four libraries**

Menu: **Tools → Manage Libraries**

Search for and install each of these. If a popup asks "install dependencies?",
always say yes.

- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `DHT sensor library` (the one by Adafruit)
- `Adafruit Unified Sensor`

Libraries are just other people's code that handles the fiddly parts of talking
to the screen and the temperature sensor.

## 2. Where everything is

Everything is in `Desktop/parking_project/`:

| Folder / file | What it is |
|---|---|
| **`parking_assistant/`** | The code. Open this folder in Arduino IDE. |
| **`HARDWARE.md`** | Parts list and every wiring connection. Your bench reference. |
| **`docs/superpowers/plans/`** | **The step-by-step build procedure.** The main document. |
| `docs/superpowers/specs/` | Why the thing is designed the way it is. Background reading. |
| `bringup/` | Small test programs used during the build. |

**The procedure you asked about is the file in `docs/superpowers/plans/`.** It is
called `2026-08-15-garage-parking-assistant.md`. It has 11 tasks, numbered Task 0
through Task 10. Work them in order, top to bottom.

## 3. How to open the code

In Arduino IDE: **File → Open**, navigate to
`Desktop/parking_project/parking_assistant/`, and pick **`parking_assistant.ino`**.

The other files in that folder open automatically as **tabs** across the top. That
is normal — an Arduino "sketch" is a folder of files, and the IDE shows them as
tabs. You do not need to open them separately.

## 4. The two buttons you will use constantly

Top-left of the window:

- **✓ Verify** — checks the code compiles. Does not touch the board. Use this a lot.
- **→ Upload** — compiles, then sends the program to the board.

And one more, top-right:

- **Serial Monitor** (magnifying glass icon) — a text window showing messages from
  the board. **Set the dropdown at its top-right to `115200`** or you will see
  garbage characters. Most of the early testing happens here.

## 5. What order to do everything

1. Install everything in section 1 above.
2. Plug the ESP32 into your Mac with the USB A-to-C cable. **Nothing else wired yet.**
3. Open the plan and do **Task 0**. It wires no components — it exists purely to
   prove the IDE, the board, and the cable all work. Do not skip it.
4. Continue through Tasks 1–10 in order. Each task wires one thing, tests it, and
   only then moves on.

That last point is the whole method. Wiring everything at once and then finding it
does not work gives you twenty possible causes. Wiring one part at a time gives you
one.

## 6. Common first-time problems

**No port appears under Tools → Port**

Usually the cable. Many USB cables are charge-only and carry no data. Try a
different one first — this is the single most common beginner wall.

If a different cable does not fix it, look at the small chip near the USB connector
on your ESP32 and install its driver:
- `CP2102` → Silicon Labs CP210x VCP driver
- `CH340` or `CH9102` → WCH CH34x driver

**Upload fails with "Failed to connect... Wrong boot mode detected"**

Hold the **BOOT** button on the ESP32 while the IDE prints "Connecting...", then
let go. Some boards need this every upload.

**Serial Monitor shows nonsense symbols**

The baud dropdown is wrong. Set it to **115200**.

**Error mentioning `tone` when you reach Task 6**

Your ESP32 board package is version 2.x, not 3.x. Go back to **Tools → Board →
Boards Manager**, search `esp32`, and update it.

**Something else**

Copy the full red error text and ask. Error messages look intimidating but they
are usually specific.
