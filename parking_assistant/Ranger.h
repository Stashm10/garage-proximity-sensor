// Ranger - HC-SR04 into a filtered distance in inches.
//
// Raw HC-SR04 output jitters roughly +/-0.3-0.5in and intermittently
// returns complete garbage. A ROLLING median-of-3 (emit on every ping,
// not once per three) keeps latency at ~20ms, which is about 0.7in of
// car travel at 2mph. A batch median-of-5 would cost ~125ms, enough to
// miss a wing mirror entirely.

#pragma once
#include <Arduino.h>
#include "config.h"

class Ranger {
public:
  void begin();

  // Fires one ping if PING_INTERVAL_MS has elapsed.
  // Returns true when a ping was taken this call - NOT that a target
  // was seen. Check hasTarget() for that.
  bool update(float inchesPerUs);

  float inches() const    { return _filtered; }
  bool  hasTarget() const { return _hasTarget; }

private:
  float    _window[3] = {0.0f, 0.0f, 0.0f};
  uint8_t  _count     = 0;
  uint8_t  _idx       = 0;
  float    _filtered  = 0.0f;
  bool     _hasTarget = false;
  uint32_t _lastPing  = 0;

  // Returns -1.0f on timeout or an out-of-range reading.
  float pingInches(float inchesPerUs);
};
