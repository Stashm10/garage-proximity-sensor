// Thermo - DHT11 temperature into a speed-of-sound figure.
//
// The speed of sound changes about 0.1% per degree C. Over a garage's
// yearly range that is a ~6% swing, which is most of an inch at close
// range. This unit removes it.

#pragma once
#include <Arduino.h>
#include "config.h"

class Thermo {
public:
  void begin();

  // Non-blocking. Performs a real DHT11 read only every DHT_INTERVAL_MS;
  // returns immediately otherwise. Must never sit inside the ranging path.
  void update();

  float inchesPerMicrosecond() const;
  float temperatureC() const { return _tempC; }

  // False until the first successful read. When false, the value returned
  // by temperatureC() is the DEFAULT_TEMP_C fallback.
  bool hasReading() const { return _hasReading; }

private:
  float    _tempC      = DEFAULT_TEMP_C;
  bool     _hasReading = false;
  uint32_t _lastRead   = 0;
};
