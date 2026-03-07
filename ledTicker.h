bool tickYelLedState = 0;
bool tickYelLedVars = 0;

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
  if (tickRedLedVars && (millis() - timerMillis > 100)) {
    tickRedLedState = !tickRedLedState;
    digitalWrite(PIN_LED_RED, tickRedLedState);
    timerMillis = millis();
  }
}