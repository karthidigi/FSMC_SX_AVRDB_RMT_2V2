// Forward declaration — remBlockReason is defined in motor.h (included after ledTicker.h)
extern uint8_t remBlockReason;

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

// ── Remote block warning LED patterns ───────────────────────────────────────
// Driven by remBlockReason:
//   1 (bypass active)  → RED fast blink 200 ms — runs for 5 s then clears
//   2 (voltage fault)  → RED + YELLOW alternate 500 ms — runs for 5 s then clears
// Both patterns are independent of the main fault blink (tickRedLedVars).
void RemBlockLedFunc() {
  static unsigned long patternStartMs = 0;
  static uint8_t       lastReason     = 0;
  static unsigned long toggleMs       = 0;
  static bool          ledState       = false;

  if (remBlockReason == 0) {
    patternStartMs = 0;
    lastReason     = 0;
    return;
  }

  unsigned long now = millis();

  // Arm the 5 s window when a new block event arrives
  if (remBlockReason != lastReason) {
    patternStartMs = now; if (!patternStartMs) patternStartMs = 1;
    lastReason     = remBlockReason;
    ledState       = false;
    toggleMs       = now;
  }

  // Auto-clear after 5 s
  if (now - patternStartMs >= 5000UL) {
    remBlockReason = 0;
    lastReason     = 0;
    return;
  }

  if (remBlockReason == 1) {
    // RED fast blink — 200 ms toggle
    if (now - toggleMs >= 200UL) {
      ledState = !ledState;
      if (ledState) { pa2RedOn(); } else { pa2LedOff(); }
      toggleMs = now;
    }
  } else if (remBlockReason == 2) {
    // RED + YELLOW alternate — 500 ms toggle
    if (now - toggleMs >= 500UL) {
      ledState = !ledState;
      if (ledState) {
        pa2RedOn();
        digitalWrite(PIN_LED_YELLOW, HIGH);  // YELLOW off during RED phase
      } else {
        pa2LedOff();
        digitalWrite(PIN_LED_YELLOW, LOW);   // YELLOW on during off-RED phase
      }
      toggleMs = now;
    }
  }
}