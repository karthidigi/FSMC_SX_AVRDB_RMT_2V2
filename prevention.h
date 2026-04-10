bool overloadVars = 0;
bool dryRunVars   = 0;

bool powerLoss  = 0;
bool phaseFault = 0;

bool unVolVars = 0;
bool hiVolVars = 0;

bool openWireVars = 0;   // current open-wire (phase conductor open)
bool leakageVars  = 0;   // current imbalance / leakage fault

// Open-wire and leakage detection: trip thresholds are user-configurable via EEPROM
// (storage.openWireThA, storage.leakagePct).  The Iavg minimums are physics-based
// preconditions — not user-settable — kept as internal constants.
#define OPEN_WIRE_IAVG_MIN_A   2.0f   // minimum Iavg to enable open-wire check
#define LEAKAGE_IAVG_MIN_A     2.0f   // minimum Iavg to enable leakage check

// Dry run uses its own filtered-current threshold (not m1StaVars / STA_MIN_AMPHS).
// 0.5A was too high — a pump running dry may draw less, keeping motorOn=false and
// preventing fault detection. 0.1A is above ADC noise floor when motor is fully stopped.
#define DRY_RUN_MOTOR_MIN_A    0.1f   // filtered Iavg must exceed this to treat motor as running

// 5-sample moving-average filter for phase currents
#define CURR_FILTER_LEN  5
static float filt_R_buf[CURR_FILTER_LEN] = {0};
static float filt_Y_buf[CURR_FILTER_LEN] = {0};
static float filt_B_buf[CURR_FILTER_LEN] = {0};
static uint8_t filt_idx = 0;
float curRFilt   = 0, curYFilt = 0, curBFilt = 0;  // filtered values used by all check fns
float curIavgFilt = 0;  // pre-computed average of the three filtered phases — avoids
                         // recomputing (curRFilt+curYFilt+curBFilt)/3 in every check fn

void updateCurrentFilter() {
  filt_R_buf[filt_idx] = curR;
  filt_Y_buf[filt_idx] = curY;
  filt_B_buf[filt_idx] = curBM1;
  filt_idx = (filt_idx + 1) % CURR_FILTER_LEN;
  float sR = 0, sY = 0, sB = 0;
  for (uint8_t i = 0; i < CURR_FILTER_LEN; i++) {
    sR += filt_R_buf[i]; sY += filt_Y_buf[i]; sB += filt_B_buf[i];
  }
  curRFilt   = sR / CURR_FILTER_LEN;
  curYFilt   = sY / CURR_FILTER_LEN;
  curBFilt   = sB / CURR_FILTER_LEN;
  curIavgFilt = (curRFilt + curYFilt + curBFilt) / 3.0f;  // computed once per filter update
}

// Fault-duration timers -- set on first trip; cleared when fault clears
unsigned long voltFaultStartMs   = 0;
bool          voltFaultTimerRun  = false;

unsigned long phaseFaultStartMs  = 0;   // used for Phase Cut only
bool          phaseFaultTimerRun = false;

// Recovery window state — set when fault clears and stability countdown begins.
// Exposed globally so lcdDisp.h can show "UV/OV/PH Recovry MM:SS".
bool          uvRecovering   = false;
unsigned long uvRecovStartMs = 0;
bool          ovRecovering   = false;
unsigned long ovRecovStartMs = 0;
bool          phRecovering   = false;
unsigned long phRecovStartMs = 0;

// Separate hysteresis for Under Voltage and Over Voltage.
//
// UV — percentage added ABOVE the trip threshold to form the clear level:
//   uvClear = undVol × (100 + underVOLTAGE_HYSTERESIS_PCT) / 100
//   Default 10 % → e.g. trip ≤300 V, clear >330 V
//
// OV — multiplier applied to the trip threshold to form the clear level:
//   ovClear = ovrVol × overVOLTAGE_HYSTERESIS_PCT
//   0.98 → 2 % hysteresis → e.g. trip ≥440 V, clear <431.2 V
//
#define underVOLTAGE_HYSTERESIS_PCT  10      // UV clear band above trip (integer %)
#define overVOLTAGE_HYSTERESIS_PCT   0.98f   // OV clear level as fraction of trip (float)

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
  static unsigned long olClearMs = 0;

  float Imax = max(curRFilt, max(curYFilt, curBFilt));

  if (m1StaVars && (millis() - m1OnBuffMillis >= 1000)) {
    if (Imax >= storage.ovLRunM1) {
      olClearMs = 0;
      overloadVars = 1;
      tickRedLedVars = 1;
      if (olStartMs == 0) { olStartMs = millis(); if (!olStartMs) olStartMs = 1; }
      if (millis() - olStartMs >= senseWindowMs()) {
        m1Off();
        a1et = 29;
        if (storage.modeM1 != 0) {
          if (storage.modeM1 == 5) { storage.app1Run = 0; }
          storage.modeM1 = 0;
          markLocalModeChange();
          savecon();
          loadModeVal();
        }
        olStartMs = 0;
      }
    } else {
      if (olClearMs == 0) { olClearMs = millis(); if (!olClearMs) olClearMs = 1; }
      if (millis() - olClearMs >= 2000UL) {
        overloadVars = 0;
        olStartMs    = 0;
        olClearMs    = 0;
      }
    }
  } else {
    overloadVars = 0;
    olStartMs    = 0;
    olClearMs    = 0;
  }
}

//////////////////////////////////////
// CURRENT FAULT — Dry Run
// Behaviour: stop motor + switch to NORMAL MODE. No auto restart.
//
// Uses filtered Iavg (not m1StaVars) to determine motor-running state.
// m1StaVars is set from raw current at STA_MIN_AMPHS (1.5A); when actual
// current is near that boundary, noise causes m1StaVars to flicker 0/1,
// continuously resetting drStartMs and preventing fault detection.
// Using filtered Iavg > DRY_RUN_MOTOR_MIN_A (0.5A) is stable at any
// running current and does not flicker.
//////////////////////////////////////
void dryRunCheck() {
  static unsigned long drStartMs = 0;
  static unsigned long drOnMs    = 0;
  static unsigned long drClearMs = 0;
  static bool          drPrevOn  = false;

  float Iavg    = curIavgFilt;
  bool  motorOn = (Iavg > DRY_RUN_MOTOR_MIN_A);

  if (!motorOn) {
    drPrevOn   = false;
    drStartMs  = 0;
    drClearMs  = 0;
    dryRunVars = 0;
    return;
  }

  if (!drPrevOn) {
    drOnMs   = millis();
    drPrevOn = true;
  }

  if (millis() - drOnMs < 1000UL) {
    drStartMs = 0;
    drClearMs = 0;
    return;
  }

  if (Iavg < storage.dryRunM1) {
    drClearMs = 0;
    dryRunVars = 1;
    tickRedLedVars = 1;
    if (drStartMs == 0) { drStartMs = millis(); if (!drStartMs) drStartMs = 1; }
    if (millis() - drStartMs >= senseWindowMs()) {
      m1Off();
      a1et = 28;
      if (storage.modeM1 != 0) {
        if (storage.modeM1 == 5) { storage.app1Run = 0; }
        storage.modeM1 = 0;
        markLocalModeChange();
        savecon();
        loadModeVal();
      }
      drStartMs = 0;
    }
  } else {
    if (drClearMs == 0) { drClearMs = millis(); if (!drClearMs) drClearMs = 1; }
    if (millis() - drClearMs >= 2000UL) {
      dryRunVars = 0;
      drStartMs  = 0;
      drClearMs  = 0;
    }
  }
}

//////////////////////////////////////
// CURRENT FAULT -- Open Wire
// One phase drops to near zero while motor is running and Iavg is significant.
// Threshold 1.0A is below STA_MIN_AMPHS (1.5A): phases between 1.0-1.5A are
// partial faults caught by leakageCheck, not open wire.
// Priority: highest current fault (spec priority 1).
//////////////////////////////////////
void openWireCheck() {
  static unsigned long owStartMs = 0;
  static unsigned long owClearMs = 0;

  float Iavg = curIavgFilt;
  // Clamp owTh to a physically valid range (0.1–1.5 A).
  // Upper bound 1.5 A is below any normal running current — this prevents
  // garbage EEPROM bytes (from old firmware layouts) from setting owTh at
  // or above the actual phase current and causing false open-wire trips.
  float owTh = 1.0f;
  if (storage.openWireThA > 0.0f && storage.openWireThA < 1.5f) {
    owTh = storage.openWireThA;
  }
  float Imin   = min(curRFilt, min(curYFilt, curBFilt));
  bool anyLow  = (Imin < owTh);   // strict < : phase at exactly threshold is not an open wire

  if (m1StaVars && (millis() - m1OnBuffMillis >= 1000)) {
    if (anyLow && Iavg > OPEN_WIRE_IAVG_MIN_A) {
      owClearMs = 0;
      openWireVars = 1;
      tickRedLedVars = 1;
      if (owStartMs == 0) { owStartMs = millis(); if (!owStartMs) owStartMs = 1; }
      if (millis() - owStartMs >= senseWindowMs()) {
        m1Off();
        a1et = 27;
        if (storage.modeM1 != 0) {
          if (storage.modeM1 == 5) { storage.app1Run = 0; }
          storage.modeM1 = 0;
          markLocalModeChange();
          savecon();
          loadModeVal();
        }
        owStartMs = 0;
      }
    } else {
      if (owClearMs == 0) { owClearMs = millis(); if (!owClearMs) owClearMs = 1; }
      if (millis() - owClearMs >= 2000UL) {
        openWireVars = 0;
        owStartMs    = 0;
        owClearMs    = 0;
      }
    }
  } else {
    openWireVars = 0;
    owStartMs    = 0;
    owClearMs    = 0;
  }
}

//////////////////////////////////////
// CURRENT FAULT -- Leakage / Current Imbalance
// Unequal phase currents indicate winding or leakage fault.
// Unbalance% = max(Imax-Iavg, Iavg-Imin) / Iavg * 100  (catches high AND low phase)
// Priority: 3rd current fault (after Open Wire and Overload).
//////////////////////////////////////
void leakageCheck() {
  static unsigned long lkStartMs = 0;
  static unsigned long lkClearMs = 0;

  float Iavg = curIavgFilt;

  if (m1StaVars && (millis() - m1OnBuffMillis >= 1000) && Iavg > LEAKAGE_IAVG_MIN_A) {
    float Imax   = max(curRFilt, max(curYFilt, curBFilt));
    float Imin   = min(curRFilt, min(curYFilt, curBFilt));
    float maxDev       = max(Imax - Iavg, Iavg - Imin);
    float imbalancePct = (maxDev / Iavg) * 100.0f;
    float lkTh = (storage.leakagePct > 0.0f) ? storage.leakagePct : 30.0f;
    if (imbalancePct > lkTh) {
      lkClearMs = 0;
      leakageVars = 1;
      tickRedLedVars = 1;
      if (lkStartMs == 0) { lkStartMs = millis(); if (!lkStartMs) lkStartMs = 1; }
      if (millis() - lkStartMs >= senseWindowMs()) {
        m1Off();
        a1et = 26;
        if (storage.modeM1 != 0) {
          if (storage.modeM1 == 5) { storage.app1Run = 0; }
          storage.modeM1 = 0;
          markLocalModeChange();
          savecon();
          loadModeVal();
        }
        lkStartMs = 0;
      }
    } else {
      if (lkClearMs == 0) { lkClearMs = millis(); if (!lkClearMs) lkClearMs = 1; }
      if (millis() - lkClearMs >= 2000UL) {
        leakageVars = 0;
        lkStartMs   = 0;
        lkClearMs   = 0;
      }
    }
  } else {
    leakageVars = 0;
    lkStartMs   = 0;
    lkClearMs   = 0;
  }
}

//////////////////////////////////////
// VOLTAGE FAULT — Under Voltage
// Behaviour: set elst=3 (PowChecks stops motor).
//            Recovery: AUTO/LAST STATE auto restarts; NORMAL stays OFF.
//////////////////////////////////////
void underVoltageCheck() {
  static unsigned long uvSenseMs      = 0;   // sense-window: condition must persist 2 s before trip
  static unsigned long uvStableStartMs = 0;

  // Clear level = trip + 10% (voltage must rise this far before fault resets)
  const uint16_t uvClear = (uint32_t)storage.undVol * (100 + underVOLTAGE_HYSTERESIS_PCT) / 100;
  bool anyBelowTrip  = (volRY <= storage.undVol || volYB <= storage.undVol || volBR <= storage.undVol);
  bool allAboveClear = (volRY > uvClear && volYB > uvClear && volBR > uvClear);

  if (anyBelowTrip) {
    // Start sense-window timer on first detection
    if (uvSenseMs == 0) { uvSenseMs = millis(); if (!uvSenseMs) uvSenseMs = 1; }

    // Only confirm fault after condition persists for 2 s (prevents single-reading
    // false trips from ADC restart after menu exit)
    if (millis() - uvSenseMs >= 2000UL) {
      if (!unVolVars) {
        voltFaultStartMs  = millis(); if (!voltFaultStartMs) voltFaultStartMs = 1;
        voltFaultTimerRun = true;
      }
      unVolVars    = 1;
      elst         = 3;
      uvRecovering = false;   // fault is active, not in recovery
    }
    uvStableStartMs = 0;

  } else {
    uvSenseMs = 0;   // condition cleared — reset sense window
  }

  if (!anyBelowTrip && unVolVars) {
    // FAULT ACTIVE, VOLTAGE HAS RISEN
    if (allAboveClear) {
      if (uvStableStartMs == 0) {
        uvStableStartMs  = millis(); if (!uvStableStartMs) uvStableStartMs = 1;
        uvRecovering     = true;    // start recovery countdown display
        uvRecovStartMs   = uvStableStartMs;
      }
      if (millis() - uvStableStartMs >= (atMins + atSec) * 1000UL) {
        unVolVars         = 0;
        uvStableStartMs   = 0;
        voltFaultTimerRun = false;
        uvRecovering      = false;  // recovery complete
      } else {
        elst = 3;
      }
    } else {
      // In dead-band (above trip but below clear) — reset stability, keep fault
      uvStableStartMs = 0;
      uvRecovering    = false;
      elst            = 3;
    }
  }
}

//////////////////////////////////////
// VOLTAGE FAULT — Over Voltage
// Behaviour: set elst=4 (PowChecks stops motor).
//            Recovery: AUTO/LAST STATE auto restarts; NORMAL stays OFF.
//////////////////////////////////////
void highVoltageCheck() {
  static unsigned long ovSenseMs      = 0;   // sense-window: condition must persist 2 s before trip
  static unsigned long ovStableStartMs = 0;

  // Clear level = trip × 0.98 (voltage must drop to 98 % of trip before fault resets)
  const uint16_t ovClear = (uint16_t)((float)storage.ovrVol * overVOLTAGE_HYSTERESIS_PCT);
  bool anyAboveTrip  = (volRY >= storage.ovrVol || volYB >= storage.ovrVol || volBR >= storage.ovrVol);
  bool allBelowClear = (volRY < ovClear && volYB < ovClear && volBR < ovClear);

  if (anyAboveTrip) {
    // Start sense-window timer on first detection
    if (ovSenseMs == 0) { ovSenseMs = millis(); if (!ovSenseMs) ovSenseMs = 1; }

    // Only confirm fault after condition persists for 2 s
    if (millis() - ovSenseMs >= 2000UL) {
      if (!hiVolVars) {
        voltFaultStartMs  = millis(); if (!voltFaultStartMs) voltFaultStartMs = 1;
        voltFaultTimerRun = true;
      }
      hiVolVars    = 1;
      elst         = 4;
      ovRecovering = false;   // fault is active, not in recovery
    }
    ovStableStartMs = 0;

  } else {
    ovSenseMs = 0;   // condition cleared — reset sense window
  }

  if (!anyAboveTrip && hiVolVars) {
    // FAULT ACTIVE, VOLTAGE HAS DROPPED
    if (allBelowClear) {
      if (ovStableStartMs == 0) {
        ovStableStartMs  = millis(); if (!ovStableStartMs) ovStableStartMs = 1;
        ovRecovering     = true;    // start recovery countdown display
        ovRecovStartMs   = ovStableStartMs;
      }
      if (millis() - ovStableStartMs >= (atMins + atSec) * 1000UL) {
        hiVolVars         = 0;
        ovStableStartMs   = 0;
        voltFaultTimerRun = false;
        ovRecovering      = false;  // recovery complete
      } else {
        elst = 4;
      }
    } else {
      ovStableStartMs = 0;
      ovRecovering    = false;
      elst            = 4;
    }
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
  static unsigned long phStableStartMs = 0;

  bool anyPhaseLow  = (volRY <= PHASE_VOLAGE_MINIMUM ||
                       volYB <= PHASE_VOLAGE_MINIMUM ||
                       volBR <= PHASE_VOLAGE_MINIMUM);
  bool allPhasesLow = (volRY <= PHASE_VOLAGE_MINIMUM &&
                       volYB <= PHASE_VOLAGE_MINIMUM &&
                       volBR <= PHASE_VOLAGE_MINIMUM);

  if (allPhasesLow) {
    // POWER LOSS TRIP - immediate trip and clear, no stability timer
    powerLoss          = 1;
    phaseFault         = 0;
    phaseFaultTimerRun = false;   // cancel phase-cut timer if transitioning from phase-cut
    elst               = 1;
    phStableStartMs    = 0;

  } else if (anyPhaseLow) {
    // PHASE CUT TRIP - immediate trip
    if (!phaseFault) {
      phaseFaultStartMs  = millis(); if (!phaseFaultStartMs) phaseFaultStartMs = 1;
      phaseFaultTimerRun = true;
    }
    powerLoss       = 0;
    phaseFault      = 1;
    elst            = 2;
    phStableStartMs = 0;
    phRecovering    = false;   // fault is active, not in recovery

  } else {
    // NO PHASE LOW

    // Power loss: clear immediately
    if (powerLoss) {
      powerLoss = 0;
    }

    // Phase cut: stability timer before clearing
    if (phaseFault) {
      if (phStableStartMs == 0) {
        phStableStartMs  = millis(); if (!phStableStartMs) phStableStartMs = 1;
        phRecovering     = true;    // start recovery countdown display
        phRecovStartMs   = phStableStartMs;
      }
      if (millis() - phStableStartMs >= (atMins + atSec) * 1000UL) {
        phaseFault         = 0;
        phStableStartMs    = 0;
        phaseFaultTimerRun = false;
        phRecovering       = false;  // recovery complete
      } else {
        elst = 2;
      }
    }
  }
}


//////////////////////////////////////
// Bypass switch check.
// Called first in every loop() iteration.
// When PD5 is HIGH (bypass ON): clears all fault states, forces M1OFF relay ON,
// cancels any running off-timer, and blocks protection from stopping the motor.
// When PD5 is LOW: bypassActive is false and normal protection resumes.
//////////////////////////////////////
void bypassCheck() {
  bypassActive = (digitalRead(Bypass) == HIGH);

  if (bypassActive) {
    // Cancel any pending off-sequence so relay stays energised
    if (m1OffActive) {
      stopOffTimer();
      m1OffActive = false;
    }
    // Keep M1OFF relay energised (NO closed = run circuit intact)
    digitalWrite(PIN_M1OFF, HIGH);

    // Clear every fault flag so LCD and IoT show no fault while bypassed
    elst          = 0;
    unVolVars     = 0;  hiVolVars    = 0;
    overloadVars  = 0;  dryRunVars   = 0;
    openWireVars  = 0;  leakageVars  = 0;
    powerLoss     = 0;  phaseFault   = 0;
    tickRedLedVars = 0;
  }
}

//////////////////////////////////////
// Main voltage protection entry point.
// Called every main-loop cycle after 5 s boot settle.
// Resets elst=0 then lets the three check functions reassert it
// if a confirmed fault is present.
//////////////////////////////////////
void PowChecks() {
  if (bypassActive) return;   // bypass switch overrides all voltage protection

  if (millis() > 5000) {
    elst = 0;
    underVoltageCheck();
    highVoltageCheck();
    phaseChecks();

    if (elst > 0) {
      tickRedLedVars = 1;
      // Use m1Off() so the M1ON relay is also cancelled immediately — prevents the
      // ON-relay pulse from continuing during a voltage fault (e.g. fault trips while
      // the motor contactor is still being energised).  m1Off() is idempotent: the
      // stopOnTimer / stopOffTimer guards inside it prevent double-starts.
      if (!m1OffActive) {
        m1Off();
      }
    } else {
      // Only clear RED blink if no current faults are still active
      if (!overloadVars && !dryRunVars && !openWireVars && !leakageVars) {
        tickRedLedVars  = 0;
        tickRedLedState = 0;
        pa2LedOff();  // both LEDs off; motorStaCheck restores GREEN if motor is running
      }
      // DO NOT touch PIN_M1OFF here — let hardware timer ISR handle release
    }
  }
}
