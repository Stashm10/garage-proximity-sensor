# Bring-up sketches

The module files (`config.h`, `Ranger`, `Thermo`, `Tracker`, `Button`, `Presenter`)
are final from the moment they are written and never change. Only
`parking_assistant.ino` evolves as you add subsystems.

These files are the intermediate contents of `parking_assistant.ino` for each
verification stage in the plan.

## How to use

1. Open `parking_assistant/parking_assistant.ino` in Arduino IDE.
2. Select all, delete, and paste in the contents of the stage file you are on.
3. Upload, run the stage's verification from the plan, and confirm it passes.
4. Move to the next stage.
5. After stage 5, restore the real `parking_assistant.ino` — keep a copy first,
   or re-copy it from `bringup/99_final.txt`.

They are `.txt` on purpose. Arduino IDE only compiles files in the sketch folder
itself, so nothing here can interfere with a build.

## Stages

| File | Plan task | Verifies |
|---|---|---|
| `01_raw_sensor.txt` | Task 1, Step 4 | Raw distance within ±0.5in of a tape measure; prompt `no target` |
| `02_filtered.txt` | Task 2, Step 4 | Median filter reduces jitter and rejects wild values |
| `03_temperature.txt` | Task 3, Step 5 | DHT11 reads live; falls back safely when unplugged |
| `04_tracker.txt` | Task 4, Steps 4–5 | Band boundaries; minimum-hold catches a fast sweep |
| `05_led_sweep.txt` | Task 5, Step 4 | Bar fills from the bottom, red first |
| `99_final.txt` | Task 9 | Copy of the finished sketch |

Stages 1–4 need only the serial monitor at **115200 baud**. Stage 4 is the one
worth spending time on — the minimum-hold is the behavior the whole device
exists for, and it is far easier to trust after watching it catch a book swept
past by hand than after watching it catch a wing mirror.
