#include "Thermo.h"
#include <DHT.h>

static DHT dht(PIN_DHT, DHT11);

void Thermo::begin() {
  dht.begin();
  // Unsigned wraparound is intentional and correct here: it makes the very
  // first update() call perform a read immediately rather than waiting 5s.
  _lastRead = millis() - DHT_INTERVAL_MS;
}

void Thermo::update() {
  uint32_t now = millis();
  if (now - _lastRead < DHT_INTERVAL_MS) return;
  _lastRead = now;

  float t = dht.readTemperature();   // Celsius
  if (isnan(t)) return;              // keep the last good value

  _tempC = t;
  _hasReading = true;
}

float Thermo::inchesPerMicrosecond() const {
  // Speed of sound in dry air: c = 331.3 + 0.606 * T(C), metres/second.
  // At 20C this yields 0.0135205 in/us.
  float c_mps = 331.3f + 0.606f * _tempC;
  return c_mps * 39.3701f / 1000000.0f;
}
