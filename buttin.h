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
extern volatile uint16_t ladderBtnAdcRaw;
void refreshButtonAdcSample();

static ButtonId btnCandidate = BTN_NONE;
static ButtonId btnStable = BTN_NONE;
static unsigned long btnDebounceMillis = 0;
static unsigned long enterPressMillis = 0;
static unsigned long modePressMillis = 0;
static uint16_t btnLastAdc = 4095;
static const uint16_t ENTER_CLICK_MAX_MS = 500;
static const uint16_t MODE_CLICK_MAX_MS = 1000;
static const uint16_t ADC_NONE_MIN = 3900;
static const uint16_t ADC_MENU_MAX = 400;
static const uint16_t ADC_UP_CENTER = 832;
static const uint16_t ADC_DOWN_CENTER = 1656;
static const uint16_t ADC_ENTER_CENTER = 2455;
static const uint16_t ADC_MODE_CENTER = 3290;
static const uint16_t ADC_TOL = 220;
static const uint16_t ADC_HYS = 320;

uint16_t getButtonAdcRaw() {
  return btnLastAdc;
}

bool isEnterButtonDown() {
  return btnStable == BTN_ENTER;
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
  if (button == BTN_UP) return ADC_UP_CENTER;
  if (button == BTN_DOWN) return ADC_DOWN_CENTER;
  if (button == BTN_ENTER) return ADC_ENTER_CENTER;
  if (button == BTN_MODE) return ADC_MODE_CENTER;
  return 0;
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
    if (d <= ADC_TOL && d < bestDiff) {
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
  btnCandidate = BTN_NONE;
  btnStable = BTN_NONE;
  btnDebounceMillis = millis();
  enterPressMillis = 0;
  modePressMillis = 0;
  btnLastAdc = 4095;
  clearNavEvents();
}

void rotaryFunc() {
  refreshButtonAdcSample();

  uint16_t raw = ((uint16_t)ladderBtnAdcRaw);
  btnLastAdc = raw;
  ButtonId sampled = decodeButton(raw);
  unsigned long now = millis();

  if (sampled != btnCandidate) {
    btnCandidate = sampled;
    btnDebounceMillis = now;
  }

  if ((now - btnDebounceMillis) >= 30 && sampled != btnStable) {
    ButtonId prev = btnStable;
    btnStable = sampled;

    if (prev != BTN_NONE && prev != btnStable) {
      onButtonReleased(prev, now);
    }
    if (btnStable != BTN_NONE && prev != btnStable) {
      onButtonPressed(btnStable);
    }
  }
}


