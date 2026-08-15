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
      // A new pass begins; seed the minimum with this first reading.
      _passActive = true;
      _min = inches;
      _hasMin = true;
    }
    if (inches < _min) _min = inches;
  } else if (_passActive && (now - _lastInRange >= PASS_END_MS)) {
    _passActive = false;
    _passEnded = now;
  }

  // A parked car keeps the pass open indefinitely, which is correct - the
  // held minimum stays readable. It expires only once the car has left.
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
