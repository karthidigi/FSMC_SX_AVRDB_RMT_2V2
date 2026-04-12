#include <Arduino.h>

enum ButtonId : uint8_t {
  BTN_NONE = 0,
  BTN_MENU,
  BTN_UP,
  BTN_DOWN,
  BTN_ENTER,
  BTN_MODE
};

// ── Navigation event flags ───────────────────────────────────────────────────
// Set by btnScan() on button press; consumed (cleared) by the caller.
// navDn  : DOWN button pressed  → next item / increment value
// navUp  : UP   button pressed  → prev item / decrement value
// navEnter: ENTER clicked (press+release ≤ 500 ms) → confirm / advance field
volatile bool navDn        = 0;
volatile bool navUp        = 0;
volatile bool navEnter     = 0;
volatile bool btnMenuPress = 0;
volatile bool btnModePress = 0;

// ── Scroll-activity tracking ─────────────────────────────────────────────────
// btnScrolled   : set whenever UP or DOWN fires; triggers secondary info display
// btnScrollMillis: timestamp of last UP/DOWN press
// navDnCount    : running DOWN-press counter (0–5); used by home-screen display
int  navDnCount      = 0;
bool btnScrolled     = 0;
unsigned long btnScrollMillis = 0;

bool menuUiFunc        = 0;
bool modeHoldInProgress = false;   // pauses emon ADC ISR during MODE hold window

extern volatile uint16_t ladderBtnAdcRaw;
void refreshButtonAdcSample();

static ButtonId   btnCandidate      = BTN_NONE;
static ButtonId   btnStable         = BTN_NONE;
static unsigned long btnDebounceMillis = 0;
static unsigned long enterPressMillis  = 0;
static unsigned long modePressMillis   = 0;
static uint16_t   btnLastAdc        = 4095;
static const uint16_t ENTER_CLICK_MAX_MS = 500;
static const uint16_t MODE_CLICK_MAX_MS  = 1000;

// ADC_NONE_MIN: threshold above which the ladder reads as "no button pressed".
// Hardware idle floor is ~3870–4004 (measured). Set to 3750 to stay safely
// below the floor while remaining well above MODE_MAX (3590).
static const uint16_t ADC_NONE_MIN     = 3750;
static const uint16_t ADC_MENU_MAX     = 400;   // MENU reads ~0    (measured)
static const uint16_t ADC_UP_CENTER    = 821;   // UP    reads ~821  (measured)
static const uint16_t ADC_DOWN_CENTER  = 1693;  // DOWN  reads ~1693 (measured)
static const uint16_t ADC_ENTER_CENTER = 2505;  // ENTER reads ~2505 (measured)
static const uint16_t ADC_MODE_CENTER  = 3290;  // MODE  (update if detection unreliable)

// Per-button tolerances — VDD variation shifts readings proportionally to their
// absolute voltage, so ENTER and MODE need wider windows than UP/DOWN.
static const uint16_t ADC_UP_TOL    = 220;
static const uint16_t ADC_DOWN_TOL  = 220;
static const uint16_t ADC_ENTER_TOL = 300;
static const uint16_t ADC_MODE_TOL  = 300;
static const uint16_t ADC_TOL       = 220;   // fallback
static const uint16_t ADC_HYS       = 320;   // hysteresis while held

// ── RC-filter settling ───────────────────────────────────────────────────────
// 47 nF cap on PD6 slows the ADC rise after release.  60 ms is enough for the
// cap to charge past ADC_NONE_MIN from any button voltage.
static const uint16_t BTN_SETTLE_MS  = 60;
static unsigned long  btnSettleUntil = 0;

// ── Hold-repeat acceleration constants ──────────────────────────────────────
static const uint16_t HOLD_START_MS  = 150;  // gate before first auto-repeat
static const uint16_t REPEAT_SLOW_MS = 250;  // initial repeat interval
static const uint16_t REPEAT_FAST_MS =  40;  // minimum repeat interval (25/sec)
static const uint16_t REPEAT_STEP_MS =  30;  // interval reduction per tier
static const uint8_t  ACCEL_EVERY_N  =   4;  // repeats per acceleration tier

uint16_t getButtonAdcRaw() { return btnLastAdc; }

bool isEnterButtonDown() { return btnStable == BTN_ENTER; }
bool isModeButtonDown()  { return btnStable == BTN_MODE;  }
bool isUpButtonDown()    { return btnStable == BTN_UP;    }
bool isDownButtonDown()  { return btnStable == BTN_DOWN;  }

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
  navDn        = 0;
  navUp        = 0;
  navEnter     = 0;
  btnMenuPress = 0;
  btnModePress = 0;
  enterPressMillis = 0;
  modePressMillis  = 0;
}

static uint16_t buttonCenter(ButtonId b) {
  if (b == BTN_UP)    return ADC_UP_CENTER;
  if (b == BTN_DOWN)  return ADC_DOWN_CENTER;
  if (b == BTN_ENTER) return ADC_ENTER_CENTER;
  if (b == BTN_MODE)  return ADC_MODE_CENTER;
  return 0;
}

static uint16_t buttonTol(ButtonId b) {
  if (b == BTN_ENTER) return ADC_ENTER_TOL;
  if (b == BTN_MODE)  return ADC_MODE_TOL;
  if (b == BTN_UP)    return ADC_UP_TOL;
  if (b == BTN_DOWN)  return ADC_DOWN_TOL;
  return ADC_TOL;
}

static uint16_t absDiff(uint16_t a, uint16_t b) {
  return (a > b) ? (a - b) : (b - a);
}

static ButtonId decodeButton(uint16_t adcVal) {
  if (adcVal >= ADC_NONE_MIN) return BTN_NONE;
  if (adcVal <= ADC_MENU_MAX) return BTN_MENU;

  ButtonId best = BTN_NONE;
  uint16_t bestDiff = 0xFFFF;
  const ButtonId candidates[4] = { BTN_UP, BTN_DOWN, BTN_ENTER, BTN_MODE };
  for (uint8_t i = 0; i < 4; i++) {
    ButtonId b = candidates[i];
    uint16_t d = absDiff(adcVal, buttonCenter(b));
    if (d <= buttonTol(b) && d < bestDiff) {
      best     = b;
      bestDiff = d;
    }
  }

  // Hysteresis: keep current stable button while near its window edge.
  if (best == BTN_NONE && btnStable != BTN_NONE) {
    if (btnStable == BTN_MENU && adcVal <= (ADC_MENU_MAX + 120))
      return BTN_MENU;
    if (btnStable != BTN_MENU && absDiff(adcVal, buttonCenter(btnStable)) <= ADC_HYS)
      return btnStable;
  }
  return best;
}

static void onButtonPressed(ButtonId button) {
  if (button == BTN_UP) {
    navUp    = 1;
    navDn    = 0;
    if (navDnCount > 0) navDnCount--;
    btnScrolled     = 1;
    btnScrollMillis = millis();
    return;
  }
  if (button == BTN_DOWN) {
    navDn    = 1;
    navUp    = 0;
    navDnCount++;
    if (navDnCount > 5) navDnCount = 5;
    btnScrolled     = 1;
    btnScrollMillis = millis();
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
    if ((now - enterPressMillis) <= ENTER_CLICK_MAX_MS)
      navEnter = 1;
    enterPressMillis = 0;
    return;
  }
  if (button == BTN_MODE) {
    if ((now - modePressMillis) <= MODE_CLICK_MAX_MS)
      btnModePress = 1;
    modePressMillis = 0;
  }
}

// ── btnInit ──────────────────────────────────────────────────────────────────
void btnInit() {
  btnCandidate      = BTN_NONE;
  btnStable         = BTN_NONE;
  btnDebounceMillis = millis();
  enterPressMillis  = 0;
  modePressMillis   = 0;
  btnLastAdc        = 4095;
  btnSettleUntil    = 0;
  clearNavEvents();
}

// ── btnScan ───────────────────────────────────────────────────────────────────
// Reads the ADC button ladder and updates navDn/navUp/navEnter/btnMenuPress.
// Call once per loop iteration (or inner editor loop).
// Press fires immediately on first non-NONE sample (after the settle gate).
// Release is debounced for 30 ms to prevent glitch-early-release.
void btnScan() {
  refreshButtonAdcSample();

  uint16_t raw     = (uint16_t)ladderBtnAdcRaw;
  btnLastAdc       = raw;
  ButtonId sampled = decodeButton(raw);
  unsigned long now = millis();

  // ── PRESS: immediate on first non-NONE sample ──────────────────────────
  // No press-side debounce: the main loop can be 30–80 ms between calls, so
  // a timed debounce would reset its counter when the button is released
  // between calls, losing the press entirely.  The settle gate (BTN_SETTLE_MS)
  // already blocks RC-filter phantom presses after a release.
  if (btnStable == BTN_NONE && sampled != BTN_NONE) {
    if ((long)(btnSettleUntil - now) > 0) return;  // RC gate active
    btnStable         = sampled;
    btnCandidate      = sampled;
    btnDebounceMillis = now;
    onButtonPressed(sampled);
    return;
  }

  // ── RELEASE: debounce NONE for 30 ms ───────────────────────────────────
  if (btnStable != BTN_NONE) {
    if (sampled == BTN_NONE) {
      if (btnCandidate != BTN_NONE) {
        btnCandidate      = BTN_NONE;
        btnDebounceMillis = now;
      }
      if ((now - btnDebounceMillis) >= 30) {
        ButtonId prev  = btnStable;
        btnStable      = BTN_NONE;
        btnCandidate   = BTN_NONE;
        btnSettleUntil = now + BTN_SETTLE_MS;
        onButtonReleased(prev, now);
      }
    } else if (sampled != btnStable) {
      // Button-to-button without NONE: RC cap still settling — release only.
      ButtonId prev  = btnStable;
      btnStable      = BTN_NONE;
      btnCandidate   = BTN_NONE;
      btnSettleUntil = now + BTN_SETTLE_MS;
      onButtonReleased(prev, now);
    }
    // else: same button still held — nothing to do
  }
}

// ── checkHoldRepeat ───────────────────────────────────────────────────────────
// Call once per editor inner-loop iteration, BEFORE the navDn/navUp check.
// Pass true only when a value field is selected (not the OK/CANCEL row).
//
// When UP or DOWN is held beyond HOLD_START_MS, this function synthetically
// sets navUp or navDn at an accelerating rate — starting at REPEAT_SLOW_MS
// and ramping down to REPEAT_FAST_MS.  The editor's existing navDn/navUp
// block handles both real and synthetic events identically.
//
// Acceleration tiers (interval drops by REPEAT_STEP_MS every ACCEL_EVERY_N repeats):
//   Repeats  1–4  : 250 ms  (~4 steps/sec)
//   Repeats  5–8  : 220 ms
//   ...
//   Repeats 29+   :  40 ms  (~25 steps/sec, floor)
inline void checkHoldRepeat(bool valueFieldSelected) {
  static unsigned long holdStartMs  = 0;
  static unsigned long lastRepeatMs = 0;
  static uint8_t       repeatCount  = 0;
  static ButtonId      holdButton   = BTN_NONE;

  unsigned long now = millis();

  // Reset on release or direction change
  if (btnStable == BTN_NONE ||
      (holdButton != BTN_NONE && btnStable != holdButton)) {
    holdStartMs = lastRepeatMs = 0;
    repeatCount = 0;
    holdButton  = BTN_NONE;
    return;
  }

  // Only repeat when a value field is active and a direction key is held
  if (!valueFieldSelected ||
      (btnStable != BTN_UP && btnStable != BTN_DOWN)) {
    holdStartMs = lastRepeatMs = 0;
    repeatCount = 0;
    holdButton  = BTN_NONE;
    return;
  }

  // Arm hold timer on first qualifying call
  if (holdButton == BTN_NONE) {
    holdButton   = btnStable;
    holdStartMs  = now;
    lastRepeatMs = now;
    return;  // first call = the real press already handled by btnScan
  }

  // Wait for initial hold gate
  if ((now - holdStartMs) < HOLD_START_MS) return;

  // Compute current interval for this acceleration tier
  uint16_t interval = REPEAT_SLOW_MS
                    - (uint16_t)(repeatCount / ACCEL_EVERY_N) * REPEAT_STEP_MS;
  if (interval < REPEAT_FAST_MS) interval = REPEAT_FAST_MS;

  // Fire a synthetic step when the interval has elapsed
  if ((now - lastRepeatMs) >= interval) {
    lastRepeatMs = now;
    if (repeatCount < 255) repeatCount++;
    // Mirror onButtonPressed() semantics
    if (holdButton == BTN_UP)   navUp = 1;
    if (holdButton == BTN_DOWN) navDn = 1;
  }
}
