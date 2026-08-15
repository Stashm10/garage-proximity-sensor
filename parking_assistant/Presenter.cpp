#include "Presenter.h"

// ---------------------------------------------------------------- LED bar

void Presenter::begin() {
  pinMode(PIN_595_DATA,  OUTPUT);
  pinMode(PIN_595_LATCH, OUTPUT);
  pinMode(PIN_595_CLOCK, OUTPUT);
  writeBits(0);
}

void Presenter::writeBits(uint8_t bits) {
  digitalWrite(PIN_595_LATCH, LOW);
  // MSBFIRST: the first bit shifted out ends up on Q7 after eight clocks,
  // so byte bit N lands on output Q N. Q0 is the bottom (red) LED.
  shiftOut(PIN_595_DATA, PIN_595_CLOCK, MSBFIRST, bits);
  digitalWrite(PIN_595_LATCH, HIGH);
}

void Presenter::setBar(int count, bool on) {
  uint8_t bits = 0;
  if (on) {
    if (count > 5) count = 5;
    for (int i = 0; i < count; i++) bits |= (1 << i);
  }
  writeBits(bits);
}

// ----------------------------------------------------------------- Buzzer

void Presenter::updateSound(Band band, float inches, float dRef) {
  // Silent when nothing is in range or there is plenty of room.
  if (band == BAND_NONE || band == BAND_SAFE) {
    if (_beepOn) { noTone(PIN_BUZZER); _beepOn = false; }
    return;
  }

  // The closest band is a continuous tone, not a beep.
  if (band == BAND_DANGER) {
    if (!_beepOn) { tone(PIN_BUZZER, BEEP_FREQ_HZ); _beepOn = true; }
    return;
  }

  // Interval shrinks linearly from SLOW at (dRef + SAFE_OFFSET) to FAST at
  // (dRef + WARN_OFFSET). A rising rate reads while driving; a fixed tone
  // does not.
  float span = BAND_SAFE_OFFSET - BAND_WARN_OFFSET;
  float x = (inches - dRef - BAND_WARN_OFFSET) / span;
  x = constrain(x, 0.0f, 1.0f);
  uint32_t interval = (uint32_t)(BEEP_INTERVAL_FAST_MS +
                      x * (BEEP_INTERVAL_SLOW_MS - BEEP_INTERVAL_FAST_MS));

  uint32_t now = millis();
  if (now - _beepChanged >= interval) {
    _beepChanged = now;
    _beepOn = !_beepOn;
    if (_beepOn) tone(PIN_BUZZER, BEEP_FREQ_HZ);
    else         noTone(PIN_BUZZER);
  }
}

void Presenter::chirpSaved() {
  tone(PIN_BUZZER, 1800); delay(120);
  tone(PIN_BUZZER, 2600); delay(120);
  noTone(PIN_BUZZER);
  // Reset the beep state machine so updateSound() does not think a tone
  // is still playing and skip restarting it.
  _beepOn = false;
  _beepChanged = millis();
}

void Presenter::chirpRefused() {
  tone(PIN_BUZZER, 700); delay(400);
  noTone(PIN_BUZZER);
  _beepOn = false;
  _beepChanged = millis();
}

// ------------------------------------------------------------------- OLED

bool Presenter::beginDisplay() {
  _haveOled = _oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS);
  if (_haveOled) {
    _oled.clearDisplay();
    _oled.setTextColor(SSD1306_WHITE);
    _oled.display();
  }
  return _haveOled;
}

void Presenter::draw(Band band, float inches, bool hasTarget,
                     bool hasMin, float minInches, float dRef, float tempC) {
  (void)band;   // reserved for a future band-specific readout
  if (!_haveOled) return;

  // A full 128x64 redraw over I2C costs ~25ms, which would starve the
  // ranging loop and lose fast passes. 10Hz is far quicker than anyone
  // can read anyway.
  uint32_t now = millis();
  if (now - _lastDraw < OLED_REDRAW_MS) return;
  _lastDraw = now;

  _oled.clearDisplay();

  _oled.setTextSize(1);
  _oled.setCursor(0, 0);
  _oled.print(hasTarget ? "CLEARANCE" : "NO TARGET");

  _oled.setTextSize(3);
  _oled.setCursor(0, 14);
  if (hasTarget) _oled.print(inches, 1);
  else           _oled.print("--");

  _oled.setTextSize(1);
  _oled.setCursor(0, 44);
  _oled.print("MIN ");
  if (hasMin) _oled.print(minInches, 1);
  else        _oled.print("--");

  _oled.setCursor(0, 54);
  _oled.print("REF ");
  _oled.print(dRef, 1);
  _oled.print("  ");
  _oled.print(tempC, 0);
  _oled.print("C");

  _oled.display();
}
