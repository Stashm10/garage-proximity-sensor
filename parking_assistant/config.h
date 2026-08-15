// config.h - every pin number and tunable constant, in one place.
//
// Board: ESP32-WROOM-32 (select "ESP32 Dev Module" in Arduino IDE).
// Pins were chosen to avoid strapping pins (0, 2, 5, 12, 15),
// flash pins (6-11) and input-only pins (34-39). Do not reassign casually.

#pragma once
#include <Arduino.h>

// ---- Pins ----
constexpr int PIN_TRIG      = 17;
constexpr int PIN_ECHO      = 16;   // via 1k/2k divider - NEVER direct from HC-SR04
constexpr int PIN_DHT       = 4;
constexpr int PIN_595_DATA  = 26;   // 74HC595 chip pin 14 (DS)
constexpr int PIN_595_LATCH = 27;   // 74HC595 chip pin 12 (ST_CP)
constexpr int PIN_595_CLOCK = 13;   // 74HC595 chip pin 11 (SH_CP)
constexpr int PIN_BUZZER    = 25;   // passive buzzer, via PN2222
constexpr int PIN_BUTTON    = 33;   // to GND, uses internal pull-up

// ---- Ranging ----
constexpr uint32_t PING_INTERVAL_MS = 20;
constexpr uint32_t ECHO_TIMEOUT_US  = 12000;   // ~81 in one-way; also prevents
                                               // the 1-second block of an
                                               // untimed pulseIn()
constexpr float    MIN_VALID_INCHES = 1.5f;    // below the HC-SR04's ~0.8in floor
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

// ---- Display ----
constexpr uint8_t  OLED_I2C_ADDRESS = 0x3C;   // some modules use 0x3D
constexpr uint32_t OLED_REDRAW_MS   = 100;    // a full 128x64 redraw costs ~25ms
