// Lateral Clearance Parking Assistant
//
// Measures the gap between a car's flank and a garage wall as the car
// drives past, and warns before contact. Mounted on the garage entrance
// wall at the height of the widest point of the car's side.
//
// Board:  ESP32-WROOM-32  ("ESP32 Dev Module")
// Core:   ESP32 Arduino core 3.x REQUIRED - core 2.x has no tone()
// Libs:   Adafruit GFX, Adafruit SSD1306, DHT sensor library,
//         Adafruit Unified Sensor
//
// Button: hold 2s = save current distance as the reference
//         tap    = clear the held minimum
//
// See docs/superpowers/plans/ for the wiring tables.

#include <Preferences.h>
#include "config.h"
#include "Thermo.h"
#include "Ranger.h"
#include "Tracker.h"
#include "Button.h"
#include "Presenter.h"

Thermo      thermo;
Ranger      ranger;
Tracker     tracker;
Button      button;
Presenter   presenter;
Preferences prefs;

static void handleButton() {
  if (button.consumeLongPress()) {
    if (ranger.hasTarget()) {
      float d = ranger.inches();
      tracker.setDRef(d);
      prefs.putFloat("dref", d);
      Serial.print("saved D_ref = ");
      Serial.println(d, 1);
      presenter.chirpSaved();
    } else {
      // Saving a garbage reference is worse than not saving one.
      Serial.println("calibration refused: no target in view");
      presenter.chirpRefused();
    }
  }

  if (button.consumeShortPress()) {
    tracker.clearMin();
    Serial.println("min cleared");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  ranger.begin();
  thermo.begin();
  button.begin();
  presenter.begin();

  if (!presenter.beginDisplay()) {
    Serial.println("OLED not found - continuing without display");
  }

  prefs.begin("parkassist", false);
  float saved = prefs.getFloat("dref", DEFAULT_D_REF_INCHES);
  tracker.begin(saved);

  Serial.print("loaded D_ref = ");
  Serial.println(saved, 1);
}

void loop() {
  thermo.update();

  if (ranger.update(thermo.inchesPerMicrosecond())) {
    tracker.update(ranger.hasTarget(), ranger.inches());
  }

  button.update();
  handleButton();

  // Flash the bar in the closest band; steady everywhere else.
  bool barOn = true;
  if (tracker.band() == BAND_DANGER) {
    barOn = ((millis() / 150) % 2) == 0;   // ~3.3Hz
  }
  presenter.setBar(tracker.ledCount(), barOn);

  presenter.updateSound(tracker.band(), tracker.currentInches(), tracker.dRef());

  presenter.draw(tracker.band(), tracker.currentInches(), ranger.hasTarget(),
                 tracker.hasMin(), tracker.minInches(),
                 tracker.dRef(), thermo.temperatureC());
}
