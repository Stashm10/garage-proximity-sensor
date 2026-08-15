// Presenter - all output. LED bar via 74HC595, passive buzzer, OLED.
//
// Consumes Tracker state only; knows nothing about ultrasonics. This is
// the seam a future two-board ESP-NOW version would split along.

#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "Tracker.h"

class Presenter {
public:
  void begin();

  // Returns false if the OLED does not answer. The device must keep
  // working without it, so this is not a fatal condition.
  bool beginDisplay();

  // Lights `count` LEDs from the bottom of the bar. `on == false` blanks
  // the bar without forgetting the count, which is how DANGER flashes.
  void setBar(int count, bool on);

  // Call every loop. Manages beep timing internally; never blocks.
  void updateSound(Band band, float inches, float dRef);

  void draw(Band band, float inches, bool hasTarget,
            bool hasMin, float minInches, float dRef, float tempC);

  // Calibration feedback. These DO block for a few hundred ms, which is
  // acceptable because they run only on an explicit button press while
  // the car is already stopped - never during a pass.
  void chirpSaved();
  void chirpRefused();

private:
  void writeBits(uint8_t bits);

  Adafruit_SSD1306 _oled{128, 64, &Wire, -1};
  bool     _haveOled    = false;
  uint32_t _lastDraw    = 0;
  bool     _beepOn      = false;
  uint32_t _beepChanged = 0;
};
