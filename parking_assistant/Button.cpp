#include "Button.h"

void Button::begin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
}

void Button::update() {
  uint32_t now = millis();
  bool raw = (digitalRead(PIN_BUTTON) == LOW);   // LOW == pressed

  if (raw != _lastRaw) {
    _lastRaw = raw;
    _lastChange = now;
  }

  if (now - _lastChange >= BUTTON_DEBOUNCE_MS && raw != _stable) {
    _stable = raw;
    if (_stable) {
      _pressedAt = now;
      _longFired = false;
    } else if (!_longFired) {
      // Released before the long-press threshold, so it was a short press.
      _shortPending = true;
    }
  }

  // Fire the long press while the button is still held, so it feels
  // immediate rather than waiting for release.
  if (_stable && !_longFired && (now - _pressedAt >= BUTTON_LONG_PRESS_MS)) {
    _longFired = true;
    _longPending = true;
  }
}

bool Button::consumeShortPress() {
  if (!_shortPending) return false;
  _shortPending = false;
  return true;
}

bool Button::consumeLongPress() {
  if (!_longPending) return false;
  _longPending = false;
  return true;
}
