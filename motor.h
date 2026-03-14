// ================= motor.h =================

// -------- Existing variables (UNCHANGED) --------
bool m1StaVars = 0;
bool m1LastAckSta = 0;
unsigned long m1OnBuffMillis = 0;
int m1StaAckVars = 0;
int a1et = 0;

bool motSimulation = 0;
bool manOnHapp = 1;
bool manOffHap = 1;

// -------- State (Hardware timer-based) --------
// Note: m1OnTimerActive and m1OffTimerActive are in relayTimer.h
bool m1OffActive = false;

// ------------------------------------------------------
// M1 ON command (Hardware timer-based)
// ------------------------------------------------------
void m1On() {
  manOnHapp = 0;

  // OFF dominates
  if (m1OffActive) return;

  // Start ON pulse if not already active
  if (!m1OnTimerActive) {
    digitalWrite(PIN_M1ON, HIGH);
    startOnTimer();  // TCB1 handles pulse — storage.onRelay s (LCD: ON RELAY LAT)
  }

  motSimulation = 1;
}

// ------------------------------------------------------
// M1 OFF command (Hardware timer-based)
// ------------------------------------------------------
void m1Off() {
  manOffHap = 0;

  // Cancel ON immediately
  digitalWrite(PIN_M1ON, LOW);
  stopOnTimer();

  // De-energise OFF relay (NO opens → stop circuit activates)
  digitalWrite(PIN_M1OFF, LOW);
  m1OffActive = true;

  // Start TCB3 timer — releases after storage.offRelay s (LCD: OFF RELAY LAT)
  if (!m1OffTimerActive) {
    startOffTimer();
  }

  motSimulation = 0;
}

// ------------------------------------------------------
// Relay service (Simplified - timers are hardware-based)
// ------------------------------------------------------
void motorRelayService() {
  // Hardware timers handle all relay timing:
  //   TCB1 ISR: de-asserts PIN_M1ON  after storage.onRelay  s (LCD: ON RELAY LAT)
  //   TCB3 ISR: re-energises PIN_M1OFF after storage.offRelay s (LCD: OFF RELAY LAT),
  //             only when elst == 0 (no electrical fault)
  // No polling needed.
}

// ------------------------------------------------------
// Motor status check (UNCHANGED)
// ------------------------------------------------------
void motorStaCheck() {
  if (curR >= STA_MIN_AMPHS || curY >= STA_MIN_AMPHS || curBM1 >= STA_MIN_AMPHS ) {

    m1StaVars = 1;
    if (elst == 0) pa2GreenOn();  // GREEN on; skip if fault blink is active

    if (!m1LastAckSta) {
      m1OnBuffMillis = millis();
      m1StaAckVars = 1;
      m1LastAckSta = 1;

      if (manOnHapp) {
        a1et = 6;
      } else {
        manOnHapp = 1;
      }
    }

  } else {

    m1StaVars = 0;
    if (elst == 0) pa2LedOff();   // both off; skip if fault blink is active

    if (m1LastAckSta) {
      m1StaAckVars = 1;
      m1LastAckSta = 0;

      if (manOffHap) {
        a1et = 7;
      } else {
        manOffHap = 1;
      }
    }
  }
}
