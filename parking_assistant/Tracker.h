// Tracker - pass detection, minimum-hold, and threshold bands.
//
// A side-mounted sensor never sees a static distance. The car moves past
// it and the sensor sees a profile: air, bumper corner, fender, wheel,
// MIRROR, door, rear quarter. The mirror usually protrudes furthest and
// passes in a fraction of a second. So the tightest gap observed during a
// pass is held, not just the live reading.
//
// This unit performs no I/O. It can be reasoned about with the board off.

#pragma once
#include <Arduino.h>
#include "config.h"

enum Band {
  BAND_NONE,      // nothing within PASS_ACTIVE_INCHES
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
  int   ledCount() const;        // 0-5, lights from the bottom of the bar

  float currentInches() const { return _current; }
  bool  passActive() const    { return _passActive; }
  bool  hasMin() const        { return _hasMin; }
  float minInches() const     { return _min; }

  // D_ref is "the tightest clearance I am willing to accept", not a
  // target to hit. All bands are offsets above it.
  float dRef() const     { return _dRef; }
  void  setDRef(float d) { _dRef = d; }

  void  clearMin()       { _hasMin = false; }

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
