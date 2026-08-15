// Button - one tactile switch, two gestures.
//
//   long press (2s)  -> save the current distance as D_ref
//   short press      -> clear the held minimum
//
// Wired to GND with the internal pull-up enabled, so LOW means pressed.

#pragma once
#include <Arduino.h>
#include "config.h"

class Button {
public:
  void begin();
  void update();

  // Each returns true at most once per physical press.
  bool consumeShortPress();
  bool consumeLongPress();

private:
  bool     _stable       = false;   // true == pressed
  bool     _lastRaw      = false;
  uint32_t _lastChange   = 0;
  uint32_t _pressedAt    = 0;
  bool     _longFired    = false;
  bool     _shortPending = false;
  bool     _longPending  = false;
};
