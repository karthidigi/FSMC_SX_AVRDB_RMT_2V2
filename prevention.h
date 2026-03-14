bool overloadVars = 0;
bool dryRunVars   = 0;

bool powerLoss  = 0;
bool phaseFault = 0;

bool unVolVars = 0;
bool hiVolVars = 0;

bool openWireVars = 0;   // current open-wire (phase conductor open)
bool leakageVars  = 0;   // current imbalance / leakage fault

// Open-wire and leakage detection constants (hardcoded per spec)
#define OPEN_WIRE_THRESHOLD_A  1.0f   // phase current <= this => open wire (if Iavg > min)
#define OPEN_WIRE_IAVG_MIN_A   2.0f   // minimum Iavg to enable open-wire check
#define LEAKAGE_IMBALANCE_PCT  30.0f  // unbalance % threshold for leakage fault
#define LEAKAGE_IAVG_MIN_A     2.0f   // minimum Iavg to enable leakage check

// Dry run uses its own filtered-current threshold (not m1StaVars / STA_MIN_AMPHS).
// Avoids false-disables when actual current sits at or near the STA_MIN_AMPHS boundary
// which causes m1StaVars to flicker and continuously resets the sense timer.
#define DRY_RUN_MOTOR_MIN_A    0.5f   // filtered Iavg must exceed this to treat motor as running

// 5-sample moving-average filter for phase currents
#define CURR_FILTER_LEN  5
static float filt_R_buf[CURR_FILTER_LEN] = {0};
static float filt_Y_buf[CURR_FILTER_LEN] = {0};
static float filt_B_buf[CURR_FILTER_LEN] = {0};
static uint8_t filt_idx = 0;
float curRFilt = 0, curYFilt = 0, curBFilt = 0;   // filtered values used by all check fns

void updateCurrentFilter() {
  filt_R_buf[filt_idx] = curR;
  filt_Y_buf[filt_idx] = curY;
  filt_B_buf[filt_idx] = curBM1;
  filt_idx = (filt_idx + 1) % CURR_FILTER_LEN;
  float sR = 0, sY = 0, sB = 0;
  for (uint8_t i = 0; i < CURR_FILTER_LEN; i++) {
    sR += filt_R_buf[i]; sY += filt_Y_buf[i]; sB += filt_B_buf[i];
  }
  curRFilt = sR / CURR_FILTER_LEN;
  curYFilt = sY / CURR_FILTER_LEN;
  curBFilt = sB / CURR_FILTER_LEN;
}

// Fault-duration timers -- set on first trip; cleared when fault clears
unsigned long voltFaultStartMs   = 0;
bool          voltFaultTimerRun  = false;

unsigned long phaseFaultStartMs  = 0;   // used for Phase Cut only
bool          phaseFaultTimerRun = false;

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

  float Imax = max(curRFilt, max(curYFilt, curBFilt));

  if (m1StaVars && (millis() - m1OnBuffMillis >= 1000)) {
    if (Imax >= storage.ovLRunM1) {
      overloadVars = 1;
      tickRedLedVars = 1;
      if (olStartMs == 0) { olStartMs = millis(); if (!olStartMs) olStartMs = 1; }
      if (millis() - olStartMs >= senseWindowMs()) {
        if (!m1OffActive) {
          m1Off();
          a1et = 29;
          if (storage.modeM1 != 0) {
            storage.modeM1 = 0;
            markLocalModeChange();
            savecon();
            loadModeVal();
          }
        }
        olStartMs = 0;
      }
    } else {
      overloadVars = 0;
      olStartMs    = 0;
    }
  } else {
    overloadVars = 0;
    olStartMs    = 0;
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
  static unsigned long drOnMs    = 0;   // when filtered current first exceeded min threshold
  static bool          drPrevOn  = false;

  float Iavg    = (curRFilt + curYFilt + curBFilt) / 3.0f;
  bool  motorOn = (Iavg > DRY_RUN_MOTOR_MIN_A);

  if (!motorOn) {
    // Motor not running — clear everything
    drPrevOn   = false;
    drStartMs  = 0;
    dryRunVars = 0;
    return;
  }

  // Rising edge: record when motor current first stabilised above min threshold
  if (!drPrevOn) {
    drOnMs   = millis();
    drPrevOn = true;
  }

  // Skip first second after motor detected running (inrush / filter warmup)
  if (millis() - drOnMs < 1000UL) {
    drStartMs = 0;
    return;
  }

  if (Iavg < storage.dryRunM1) {
    dryRunVars = 1;
    tickRedLedVars = 1;
    if (drStartMs == 0) { drStartMs = millis(); if (!drStartMs) drStartMs = 1; }
    if (millis() - drStartMs >= senseWindowMs()) {
      if (!m1OffActive) {
        m1Off();
        a1et = 28;
        if (storage.modeM1 != 0) {
          storage.modeM1 = 0;
          markLocalModeChange();
          savecon();
          loadModeVal();
        }
      }
      drStartMs = 0;
    }
  } else {
    dryRunVars = 0;
    drStartMs  = 0;
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

  float Iavg = (curRFilt + curYFilt + curBFilt) / 3.0f;
  bool anyLow = (curRFilt <= OPEN_WIRE_THRESHOLD_A ||
                 curYFilt <= OPEN_WIRE_THRESHOLD_A ||
                 curBFilt <= OPEN_WIRE_THRESHOLD_A);

  if (m1StaVars && (millis() - m1OnBuffMillis >= 1000)) {
    if (anyLow && Iavg > OPEN_WIRE_IAVG_MIN_A) {
      openWireVars = 1;
      tickRedLedVars = 1;
      if (owStartMs == 0) { owStartMs = millis(); if (!owStartMs) owStartMs = 1; }
      if (millis() - owStartMs >= senseWindowMs()) {
        if (!m1OffActive) {
          m1Off();
          a1et = 27;
          if (storage.modeM1 != 0) {
            storage.modeM1 = 0;
            markLocalModeChange();
            savecon();
            loadModeVal();
          }
        }
        owStartMs = 0;
      }
    } else {
      openWireVars = 0;
      owStartMs    = 0;
    }
  } else {
    openWireVars = 0;
    owStartMs    = 0;
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

  float Iavg = (curRFilt + curYFilt + curBFilt) / 3.0f;

  if (m1StaVars && (millis() - m1OnBuffMillis >= 1000) && Iavg > LEAKAGE_IAVG_MIN_A) {
    float Imax   = max(curRFilt, max(curYFilt, curBFilt));
    float Imin   = min(curRFilt, min(curYFilt, curBFilt));
    // Use max deviation (high OR low phase) for proper 3-phase unbalance detection
    float maxDev       = max(Imax - Iavg, Iavg - Imin);
    float imbalancePct = (maxDev / Iavg) * 100.0f;
    if (imbalancePct > LEAKAGE_IMBALANCE_PCT) {
      leakageVars = 1;
      tickRedLedVars = 1;
      if (lkStartMs == 0) { lkStartMs = millis(); if (!lkStartMs) lkStartMs = 1; }
      if (millis() - lkStartMs >= senseWindowMs()) {
        if (!m1OffActive) {
          m1Off();
          a1et = 26;
          if (storage.modeM1 != 0) {
            storage.modeM1 = 0;
            markLocalModeChange();
            savecon();
            loadModeVal();
          }
        }
        lkStartMs = 0;
      }
    } else {
      leakageVars = 0;
      lkStartMs   = 0;
    }
  } else {
    leakageVars = 0;
    lkStartMs   = 0;
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
      unVolVars = 1;
      elst      = 3;
    }
    uvStableStartMs = 0;

  } else {
    uvSenseMs = 0;   // condition cleared — reset sense window
  }

  if (!anyBelowTrip && unVolVars) {
    // FAULT ACTIVE, VOLTAGE HAS RISEN
    if (allAboveClear) {
      if (uvStableStartMs == 0) {
        uvStableStartMs = millis(); if (!uvStableStartMs) uvStableStartMs = 1;
      }
      if (millis() - uvStableStartMs >= (atMins + atSec) * 1000UL) {
        unVolVars         = 0;
        uvStableStartMs   = 0;
        voltFaultTimerRun = false;
      } else {
        elst = 3;
      }
    } else {
      // In dead-band (above trip but below clear) - reset stability, keep fault
      uvStableStartMs = 0;
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
      hiVolVars = 1;
      elst      = 4;
    }
    ovStableStartMs = 0;

  } else {
    ovSenseMs = 0;   // condition cleared — reset sense window
  }

  if (!anyAboveTrip && hiVolVars) {
    // FAULT ACTIVE, VOLTAGE HAS DROPPED
    if (allBelowClear) {
      if (ovStableStartMs == 0) {
        ovStableStartMs = millis(); if (!ovStableStartMs) ovStableStartMs = 1;
      }
      if (millis() - ovStableStartMs >= (atMins + atSec) * 1000UL) {
        hiVolVars         = 0;
        ovStableStartMs   = 0;
        voltFaultTimerRun = false;
      } else {
        elst = 4;
      }
    } else {
      ovStableStartMs = 0;
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

  } else {
    // NO PHASE LOW

    // Power loss: clear immediately
    if (powerLoss) {
      powerLoss = 0;
    }

    // Phase cut: stability timer before clearing
    if (phaseFault) {
      if (phStableStartMs == 0) {
        phStableStartMs = millis(); if (!phStableStartMs) phStableStartMs = 1;
      }
      if (millis() - phStableStartMs >= (atMins + atSec) * 1000UL) {
        phaseFault         = 0;
        phStableStartMs    = 0;
        phaseFaultTimerRun = false;
      } else {
        elst = 2;
      }
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
