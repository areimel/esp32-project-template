// Temporary placeholder while the firmware modules move into lib/ (replaced in a later task).
#include <Arduino.h>
#include "generated_config.h"

void setup() {
  Serial.begin(115200);  // never wait for a USB serial host
  Serial.printf("%s on %s\n", AppConfig::PROJECT_NAME, BOARD_LABEL);
}

void loop() { delay(1); }
