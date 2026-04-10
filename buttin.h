#include <Arduino.h>

enum ButtonId : uint8_t {
  BTN_NONE = 0,
  BTN_MENU,
  BTN_UP,
  BTN_DOWN,
  BTN_ENTER,
  BTN_MODE
};

int RotCounter = 0;
bool knobRoll = 0;
unsigned long knobRollMillis = 0;

volatile bool rotValPlus = 0;
volatile bool rotValMinus = 0;
volatile bool rotPush = 0;
volatile bool btnMenuPress = 0;
volatile bool btnModePress = 0;
bool menuUiFunc = 0;
bool modeHoldInProgress = false;   // set true during 5-s MODE hold; pauses emon ADC ISR
extern volatile uint16_t ladderBtnAdcRaw;
void refreshButtonAdcSample();

static ButtonId btnCandidate = BTN_NONE;
static ButtonId btnStable = BTN_NONE;
static unsigned long btnDebounceMillis = 0;
static unsigned long enterPressMillis = 0;
static unsigned long modePressMillis = 0;
static uint16_t btnLastAdc = 4095;
static const uint16_t ENTER_CLICK_MAX_MS  = 500;
static const uint16_t MODE_CLICK_MAX_MS   = 1000;
// ADC_NONE_MIN: threshold above which the ladder reads as "no button pressed".
// Hardware idle floor is ~3870–4004 (measured). Set to 3750 to stay safely
// below the floor while remaining well above MODE_MAX (3510 = 3290+220).
static const uint16_t ADC_NONE_MIN = 3750;
static const uint16_t ADC_MENU_MAX    = 400;   // MENU reads ~0   (measured)
static const uint16_t ADC_UP_CENTER   = 821;   // UP    reads ~821 (measured)
static const uint16_t ADC_DOWN_CENTER = 1693;  // DOWN  reads ~1693(measured)
static const uint16_t ADC_ENTER_CENTER= 2505;  // ENTER reads ~2505(measured)
static const uint16_t ADC_MODE_CENTER = 3290;  // MODE  not yet measured — update if detection is unreliable
// ADC detection tolerances — per-button because VDD variation shifts the
// ladder midpoints proportionally to their absolute voltage.
// ENTER (2467) and MODE (3290) sit at higher voltages so shift more counts
// per mV of VDD change than UP (832) and DOWN (1656).
//
// VDD variation budget (worst-case ±7%):
//   ENTER: 2467 × 0.07 ≈ ±173 counts → need TOL ≥ 180; using 350 for noise margin
//   MODE:  3290 × 0.07 ≈ ±230 counts → need TOL ≥ 235; using 300 for noise margin
//   UP/DOWN: lower absolute voltage — 220 is sufficient
//
// Overlap check with ±350 ENTER:  range 2117–2817
//   → gap to DOWN max (1876): 241 ✓   gap to MODE min (2990): 173 ✓
// Overlap check with ±300 MODE: range 2990–3590
//   → gap to ENTER max (2817): 173 ✓  gap to NONE (3750): 160 ✓
static const uint16_t ADC_UP_TOL    = 220;
static const uint16_t ADC_DOWN_TOL  = 220;
static const uint16_t ADC_ENTER_TOL = 300;  // ENTER 2505±300 → 2205–2805; gap to MODE min(2990)=185 ✓
static const uint16_t ADC_MODE_TOL  = 300;
static const uint16_t ADC_TOL       = 220;   // fallback (not used for ENTER/MODE)
static const uint16_t ADC_HYS       = 320;

// ── RC-filter settling support ──────────────────────────────────────────────
// A 47 nF cap on PD6 (ADC pin) slows the voltage transition when a button is
// released.  The rising voltage drifts through other buttons' ADC windows,
// potentially spending > 30 ms there and triggering false presses.
// BTN_SETTLE_MS must be ≥ the time for ADC to rise past ADC_NONE_MIN after release.
// For a ~300 kΩ Thevenin source: 5τ ≈ 70 ms.  At 60 ms the ADC is already
// above 4073 (> ADC_NONE_MIN=3750) even from the ENTER voltage (~2505).
// 60 ms balances RC safety against responsive repeated presses.
// Increase if phantom presses return; decrease if consecutive presses are missed.
static const uint16_t BTN_SETTLE_MS  = 60;
static unsigned long  btnSettleUntil = 0;   // no new press accepted before this time

uint16_t getButtonAdcRaw() {
  return btnLastAdc;
}

bool isEnterButtonDown() {
  return btnStable == BTN_ENTER;
}

bool isModeButtonDown() {
  return btnStable == BTN_MODE;
}

bool takeMenuPress() {
  bool v = btnMenuPress;
  btnMenuPress = 0;
  return v;
}

bool takeModePress() {
  bool v = btnModePress;
  btnModePress = 0;
  return v;
}

void clearNavEvents() {
  rotValPlus = 0;
  rotValMinus = 0;
  rotPush = 0;
  btnMenuPress = 0;
  btnModePress = 0;
  enterPressMillis = 0;
  modePressMillis = 0;
}

static uint16_t buttonCenter(ButtonId button) {
  if (button == BTN_UP)    return ADC_UP_CENTER;
  if (button == BTN_DOWN)  return ADC_DOWN_CENTER;
  if (button == BTN_ENTER) return ADC_ENTER_CENTER;
  if (button == BTN_MODE)  return ADC_MODE_CENTER;
  return 0;
}

static uint16_t buttonTol(ButtonId button) {
  if (button == BTN_ENTER) return ADC_ENTER_TOL;
  if (button == BTN_MODE)  return ADC_MODE_TOL;
  if (button == BTN_UP)    return ADC_UP_TOL;
  if (button == BTN_DOWN)  return ADC_DOWN_TOL;
  return ADC_TOL;
}

static uint16_t absDiff(uint16_t a, uint16_t b) {
  return (a > b) ? (a - b) : (b - a);
}

static ButtonId decodeButton(uint16_t adcVal) {
  if (adcVal >= ADC_NONE_MIN) {
    return BTN_NONE;
  }
  if (adcVal <= ADC_MENU_MAX) {
    return BTN_MENU;
  }

  ButtonId best = BTN_NONE;
  uint16_t bestDiff = 0xFFFF;
  const ButtonId candidates[4] = { BTN_UP, BTN_DOWN, BTN_ENTER, BTN_MODE };
  for (uint8_t i = 0; i < 4; i++) {
    ButtonId b = candidates[i];
    uint16_t d = absDiff(adcVal, buttonCenter(b));
    if (d <= buttonTol(b) && d < bestDiff) {
      best = b;
      bestDiff = d;
    }
  }

  // Hysteresis while held to avoid random dropouts near window edges.
  if (best == BTN_NONE && btnStable != BTN_NONE) {
    if (btnStable == BTN_MENU && adcVal <= (ADC_MENU_MAX + 120)) {
      return BTN_MENU;
    }
    if (btnStable != BTN_MENU && absDiff(adcVal, buttonCenter(btnStable)) <= ADC_HYS) {
      return btnStable;
    }
  }
  return best;
}

static void onButtonPressed(ButtonId button) {
  if (button == BTN_UP) {
    // Preserve old rotary semantics: UP -> previous item/value.
    rotValMinus = 1;
    rotValPlus = 0;
    if (RotCounter > 0) {
      RotCounter--;
    }
    knobRoll = 1;
    knobRollMillis = millis();
    return;
  }

  if (button == BTN_DOWN) {
    // Preserve old rotary semantics: DOWN -> next item/value.
    rotValPlus = 1;
    rotValMinus = 0;
    RotCounter++;
    if (RotCounter > 5) {
      RotCounter = 5;
    }
    knobRoll = 1;
    knobRollMillis = millis();
    return;
  }

  if (button == BTN_ENTER) {
    enterPressMillis = millis();
    return;
  }

  if (button == BTN_MENU) {
    btnMenuPress = 1;
    return;
  }

  if (button == BTN_MODE) {
    modePressMillis = millis();
  }
}

static void onButtonReleased(ButtonId button, unsigned long now) {
  if (button == BTN_ENTER) {
    unsigned long heldMs = now - enterPressMillis;
    if (heldMs <= ENTER_CLICK_MAX_MS) {
      rotPush = 1;
    }
    enterPressMillis = 0;
    return;
  }

  if (button == BTN_MODE) {
    unsigned long heldMs = now - modePressMillis;
    if (heldMs <= MODE_CLICK_MAX_MS) {
      btnModePress = 1;
    }
    modePressMillis = 0;
  }
}

void rotaryInit() {
  btnCandidate    = BTN_NONE;
  btnStable       = BTN_NONE;
  btnDebounceMillis = millis();
  enterPressMillis  = 0;
  modePressMillis   = 0;
  btnLastAdc        = 4095;
  btnSettleUntil    = 0;
  clearNavEvents();
}

void rotaryFunc() {
  refreshButtonAdcSample();

  uint16_t raw = (uint16_t)ladderBtnAdcRaw;
  btnLastAdc = raw;
  ButtonId sampled = decodeButton(raw);
  unsigned long now = millis();

  // ── PRESS: fire immediately on first non-NONE sample ───────────────────
  // The main loop is slow (LoRa, emon, etc.) so rotaryFunc() may only be
  // called every 30–80 ms.  A time-based debounce on the press side resets
  // its timer whenever 'sampled' changes — if the user taps and releases
  // between two calls the debounce never fires and the press is lost.
  //
  // Solution: accept any non-NONE sample as a press immediately (after the
  // settle gate clears).  The settle gate already guards against RC-filter
  // phantom presses after a release.  Hardware bounce (<5 ms) is not a
  // concern at the poll rates we operate at.
  if (btnStable == BTN_NONE && sampled != BTN_NONE) {
    if ((long)(btnSettleUntil - now) > 0) {
      return;  // RC settle gate active — ignore until cap charges back up
    }
    btnStable         = sampled;
    btnCandidate      = sampled;
    btnDebounceMillis = now;
    onButtonPressed(sampled);
    return;
  }

  // ── RELEASE: debounce NONE for 30 ms ───────────────────────────────────
  // Debounce is kept only on the release side to prevent brief ADC glitches
  // (or hysteresis wobble at the window edge) from prematurely ending a hold.
  if (btnStable != BTN_NONE) {
    if (sampled == BTN_NONE) {
      if (btnCandidate != BTN_NONE) {
        // First NONE sample after a press — start release debounce timer
        btnCandidate      = BTN_NONE;
        btnDebounceMillis = now;
      }
      if ((now - btnDebounceMillis) >= 30) {
        // NONE stable for 30 ms → confirmed release
        ButtonId prev  = btnStable;
        btnStable      = BTN_NONE;
        btnCandidate   = BTN_NONE;
        btnSettleUntil = now + BTN_SETTLE_MS;  // arm RC settle gate
        onButtonReleased(prev, now);
      }
    } else if (sampled != btnStable) {
      // Button-to-button transition without NONE: RC cap still charging.
      // Release the current button and arm the settle gate; do NOT register
      // the intermediate button as a new press.
      ButtonId prev  = btnStable;
      btnStable      = BTN_NONE;
      btnCandidate   = BTN_NONE;
      btnSettleUntil = now + BTN_SETTLE_MS;
      onButtonReleased(prev, now);
    }
    // else sampled == btnStable: still held, nothing to do
  }
}


