bool overloadVars = 0;
bool dryRunVars   = 0;

bool powerLoss  = 0;
bool phaseFault = 0;

bool unVolVars = 0;
bool hiVolVars = 0;

//////////////////////////////////////
// Helper: convert EEPROM sense time (seconds) to milliseconds.
// Clamps to [1, 120] s to guard against uninitialised EEPROM.
static unsigned long senseWindowMs() {
  uint16_t s = storage.senseTime;
  if (s < 1)   s = 10;
  if (s > 120) s = 120;
  return (unsigned long)s * 1000UL;
}

//////////////////////////////////////
// CURRENT FAULT — Over Current (Over Load)
// Behaviour: stop motor + switch to NORMAL MODE. No auto restart.
//////////////////////////////////////
void overLoadCheck() {
  static unsigned long olStartMs = 0;

  if (m1StaVars && (millis() - m1OnBuffMillis >= 1000)) {
    if ((curR >= storage.ovLRunM1) || (curY >= storage.ovLRunM1) || (curBM1 >= storage.ovLRunM1)) {
      // Fault condition present — show indicator immediately
      overloadVars = 1;
      // Start sense timer on first detection
      if (olStartMs == 0) { olStartMs = millis(); if (!olStartMs) olStartMs = 1; }
      // Confirm after senseTime has elapsed
      if (millis() - olStartMs >= senseWindowMs()) {
        m1Off();
        a1et = 29;
        // Current fault: always switch to NORMAL MODE (no auto restart)
        if (storage.modeM1 != 0) {
          storage.modeM1 = 0;
          markLocalModeChange();
          savecon();
          loadModeVal();
        }
        olStartMs = 0;   // reset so a new event can be counted if motor somehow restarts
      }
    } else {
      // Fault condition cleared before timer expired — ignore
      overloadVars = 0;
      olStartMs    = 0;
    }
  } else {
    // Motor not running or still in startup window
    overloadVars = 0;
    olStartMs    = 0;
  }
}

//////////////////////////////////////
// CURRENT FAULT — Dry Run
// Behaviour: stop motor + switch to NORMAL MODE. No auto restart.
//////////////////////////////////////
void dryRunCheck() {
  static unsigned long drStartMs = 0;

  if (m1StaVars && (millis() - m1OnBuffMillis >= 1000)) {
    if ((curR <= storage.dryRunM1) || (curY <= storage.dryRunM1) || (curBM1 <= storage.dryRunM1)) {
      // Fault condition present — show indicator immediately
      dryRunVars = 1;
      // Start sense timer on first detection
      if (drStartMs == 0) { drStartMs = millis(); if (!drStartMs) drStartMs = 1; }
      // Confirm after senseTime has elapsed
      if (millis() - drStartMs >= senseWindowMs()) {
        m1Off();
        a1et = 28;
        // Current fault: always switch to NORMAL MODE (no auto restart)
        if (storage.modeM1 != 0) {
          storage.modeM1 = 0;
          markLocalModeChange();
          savecon();
          loadModeVal();
        }
        drStartMs = 0;   // reset so a new event can be counted if motor somehow restarts
      }
    } else {
      // Fault condition cleared before timer expired — ignore
      dryRunVars = 0;
      drStartMs  = 0;
    }
  } else {
    // Motor not running or still in startup window
    dryRunVars = 0;
    drStartMs  = 0;
  }
}

//////////////////////////////////////
// VOLTAGE FAULT — Under Voltage
// Behaviour: set elst=3 (PowChecks stops motor).
//            Recovery: AUTO/LAST STATE auto restarts; NORMAL stays OFF.
//////////////////////////////////////
void underVoltageCheck() {
  static unsigned long unVolStartMs  = 0;
  static bool          unVolConfirmed = false;

  if (volRY <= storage.undVol || volYB <= storage.undVol || volBR <= storage.undVol) {
    if (!unVolConfirmed) {
      // Start sense timer on first detection
      if (unVolStartMs == 0) { unVolStartMs = millis(); if (!unVolStartMs) unVolStartMs = 1; }
      // Confirm after senseTime has elapsed
      if (millis() - unVolStartMs >= senseWindowMs()) {
        unVolConfirmed = true;
      }
    }
    if (unVolConfirmed) {
      // Confirmed — assert fault status every call while condition persists
      // (PowChecks resets elst=0 each cycle; we reassert it here)
      unVolVars = 1;
      elst      = 3;
    }
  } else {
    // Voltage returned to normal — clear fault
    unVolVars      = 0;
    unVolStartMs   = 0;
    unVolConfirmed = false;
  }
}

//////////////////////////////////////
// VOLTAGE FAULT — Over Voltage
// Behaviour: set elst=4 (PowChecks stops motor).
//            Recovery: AUTO/LAST STATE auto restarts; NORMAL stays OFF.
//////////////////////////////////////
void highVoltageCheck() {
  static unsigned long hiVolStartMs  = 0;
  static bool          hiVolConfirmed = false;

  if (volRY >= storage.ovrVol || volYB >= storage.ovrVol || volBR >= storage.ovrVol) {
    if (!hiVolConfirmed) {
      // Start sense timer on first detection
      if (hiVolStartMs == 0) { hiVolStartMs = millis(); if (!hiVolStartMs) hiVolStartMs = 1; }
      // Confirm after senseTime has elapsed
      if (millis() - hiVolStartMs >= senseWindowMs()) {
        hiVolConfirmed = true;
      }
    }
    if (hiVolConfirmed) {
      // Confirmed — reassert every call while condition persists
      hiVolVars = 1;
      elst      = 4;
    }
  } else {
    // Voltage returned to normal — clear fault
    hiVolVars      = 0;
    hiVolStartMs   = 0;
    hiVolConfirmed = false;
  }
}

//////////////////////////////////////
// VOLTAGE FAULT — Phase Cut & Power Loss
// Phase Cut  (elst=2): any phase below PHASE_VOLAGE_MINIMUM (not all)
// Power Loss (elst=1): all three phases below PHASE_VOLAGE_MINIMUM
//
// Both faults use senseTime before setting elst.
// phaseFault / powerLoss booleans are set immediately for LCD feedback.
//////////////////////////////////////
void phaseChecks() {
  static unsigned long phaseCutStartMs   = 0;
  static bool          phaseCutConfirmed  = false;
  static unsigned long powerLossStartMs  = 0;
  static bool          powerLossConfirmed = false;

  bool anyPhaseLow = (volRY <= PHASE_VOLAGE_MINIMUM ||
                      volYB <= PHASE_VOLAGE_MINIMUM ||
                      volBR <= PHASE_VOLAGE_MINIMUM);
  bool allPhasesLow = (volRY <= PHASE_VOLAGE_MINIMUM &&
                       volYB <= PHASE_VOLAGE_MINIMUM &&
                       volBR <= PHASE_VOLAGE_MINIMUM);

  if (allPhasesLow) {
    // ---- Power Loss: all three phases dead ----
    powerLoss = 1;   // immediate LCD indicator
    phaseFault = 0;

    if (!powerLossConfirmed) {
      if (powerLossStartMs == 0) { powerLossStartMs = millis(); if (!powerLossStartMs) powerLossStartMs = 1; }
      if (millis() - powerLossStartMs >= senseWindowMs()) {
        powerLossConfirmed = true;
      }
    }
    if (powerLossConfirmed) {
      elst = 1;   // reasserted every call while condition persists
    }

    // Power loss takes priority — reset phase-cut state
    phaseCutStartMs  = 0;
    phaseCutConfirmed = false;

  } else {
    // Power loss cleared
    powerLoss          = 0;
    powerLossStartMs   = 0;
    powerLossConfirmed = false;

    if (anyPhaseLow) {
      // ---- Phase Cut: one or two phases below minimum ----
      phaseFault = 1;   // immediate LCD indicator

      if (!phaseCutConfirmed) {
        if (phaseCutStartMs == 0) { phaseCutStartMs = millis(); if (!phaseCutStartMs) phaseCutStartMs = 1; }
        if (millis() - phaseCutStartMs >= senseWindowMs()) {
          phaseCutConfirmed = true;
        }
      }
      if (phaseCutConfirmed) {
        elst = 2;   // reasserted every call while condition persists
      }
    } else {
      // All phases normal — clear phase cut
      phaseFault        = 0;
      phaseCutStartMs   = 0;
      phaseCutConfirmed  = false;
    }
  }
}


//////////////////////////////////////
// Main voltage protection entry point.
// Called every main-loop cycle after 5 s boot settle.
// Resets elst=0 then lets the three check functions reassert it
// if a confirmed fault is present.
//////////////////////////////////////
void PowChecks() {
  if (millis() > 5000) {
    elst = 0;
    underVoltageCheck();
    highVoltageCheck();
    phaseChecks();

    if (elst > 0) {
      tickRedLedVars = 1;
      // Only activate if not already active; if already active let TCB3 run freely.
      // TCB3 ISR re-checks elst at release time and will not re-energise while fault persists.
      if (!m1OffActive) {
        digitalWrite(PIN_M1OFF, LOW);   // De-energise: NO opens → stop circuit activates
        m1OffActive = true;
        startOffTimer();                // Start TCB3 hardware timer
      }
    } else {
      tickRedLedVars = 0;
      digitalWrite(PIN_LED_RED, HIGH);
      // DO NOT touch PIN_M1OFF here — let hardware timer ISR handle release
    }
  }
}
