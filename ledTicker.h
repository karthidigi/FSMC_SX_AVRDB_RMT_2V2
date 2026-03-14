bool tickYelLedState = 0;
bool tickYelLedVars = 0;

// BLUE LED (PA3) — LoRa activity flash (active HIGH, 150 ms pulse)
static unsigned long blueOnMs = 0;   // when non-zero: LED is on, records when it was lit

void blueLedFlash() {
  digitalWrite(PIN_LED_BLUE, HIGH);
  blueOnMs = millis();
  if (!blueOnMs) blueOnMs = 1;   // guard against 0 sentinel
}

void BlueLedTickerFunc() {
  if (blueOnMs && millis() - blueOnMs >= 150UL) {
    digitalWrite(PIN_LED_BLUE, LOW);
    blueOnMs = 0;
  }
}

bool tickRedLedState = 0;
bool tickRedLedVars = 0;

void YellowLedTickerFunc() {
  static unsigned long timerMillis = 0;
  if (tickYelLedVars && (millis() - timerMillis > 100)) {
    tickYelLedState = !tickYelLedState;
    digitalWrite(PIN_LED_YELLOW, tickYelLedState);
    timerMillis = millis();
  }
}

void RedLedTickerFunc() {
  static unsigned long timerMillis = 0;
  if (tickRedLedVars && (millis() - timerMillis > 400)) {
    tickRedLedState = !tickRedLedState;
    // Alternate RED and GREEN at 400 ms each for high visibility during fault
    if (tickRedLedState) { pa2RedOn(); } else { pa2GreenOn(); }
    timerMillis = millis();
  }
}