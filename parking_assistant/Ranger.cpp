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

  // Round trip, so halve it.
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
    // Invalidate the window so a target that reappears refills it with
    // fresh samples rather than blending them against stale ones.
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
