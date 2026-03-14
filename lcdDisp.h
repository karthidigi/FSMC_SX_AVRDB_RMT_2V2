#include <Arduino.h>
#include <stdio.h>

// Number of items in the scrollable status screen.
// When ENABLE_RTC is defined a Date/Time item (index 7) is appended.
#ifdef ENABLE_RTC
  #define STATUS_SCREEN_ITEMS 7   // Mode, DryRun, Overload, OverVolt, UnderVolt, Name, DateTime
#else
  #define STATUS_SCREEN_ITEMS 6   // Mode, DryRun, Overload, OverVolt, UnderVolt, Name
#endif

// ========== Globals ==========
char lcd_buf[17];
// Display State Machine constants
#define DISPLAY_HOME        0   // V+I live readings (default)
#define DISPLAY_FAULT       1   // FAULT / fault name
#define DISPLAY_AUTO_MODE   2   // AUTO MODE countdown
#define DISPLAY_LAST_STATE  3   // LAST STATE MODE countdown
#define DISPLAY_COUNTDOWN   4   // MOTOR STOPS IN countdown
#define DISPLAY_CYCLIC_ON   5   // CYCLIC MODE ON remaining ON time
#define DISPLAY_CYCLIC_OFF  6   // CYCLIC MODE OFF remaining OFF time

// File-scope statics - allow external code to force a redraw
static uint8_t        lcdDisplayState = 255;   // 255 = uninitialised -> force first render
static unsigned long  lcdDisplayMs    = 0;     // last render timestamp


// ========== LCD Functions ==========
static void lcdPulseEnable() {
  digitalWrite(PIN_LCD_EN, HIGH);
  delayMicroseconds(3);  // Important for DxCore timing
  digitalWrite(PIN_LCD_EN, LOW);
  delayMicroseconds(50);
}

static void lcdWriteNibble(uint8_t nibble) {
  digitalWrite(PIN_LCD_D4, (nibble & 0x01) ? HIGH : LOW);
  digitalWrite(PIN_LCD_D5, (nibble & 0x02) ? HIGH : LOW);
  digitalWrite(PIN_LCD_D6, (nibble & 0x04) ? HIGH : LOW);
  digitalWrite(PIN_LCD_D7, (nibble & 0x08) ? HIGH : LOW);
  lcdPulseEnable();
}

static void lcdWriteByte(uint8_t value, bool rs) {
  digitalWrite(PIN_LCD_RS, rs ? HIGH : LOW);
  lcdWriteNibble(value >> 4);
  lcdWriteNibble(value & 0x0F);
}

void lcd_cmd(uint8_t cmd) {
  lcdWriteByte(cmd, false);
  delay(2);
}

void lcd_data(uint8_t data) {
  lcdWriteByte(data, true);
  delayMicroseconds(50);
}

void lcdInit() {
  pinMode(PIN_LCD_RS, OUTPUT);
  pinMode(PIN_LCD_EN, OUTPUT);
  pinMode(PIN_LCD_D4, OUTPUT);
  pinMode(PIN_LCD_D5, OUTPUT);
  pinMode(PIN_LCD_D6, OUTPUT);
  pinMode(PIN_LCD_D7, OUTPUT);

  digitalWrite(PIN_LCD_RS, LOW);
  digitalWrite(PIN_LCD_EN, LOW);

  delay(50);

  // HD44780 reset sequence
  lcdWriteNibble(0x03);
  delay(5);
  lcdWriteNibble(0x03);
  delayMicroseconds(150);
  lcdWriteNibble(0x03);
  delayMicroseconds(150);
  lcdWriteNibble(0x02);  // 4-bit mode

  lcd_cmd(0x28);  // Function set: 4-bit, 2-line
  lcd_cmd(0x08);  // Display OFF
  lcd_cmd(0x01);  // Clear
  delay(2);
  lcd_cmd(0x06);  // Entry mode
  lcd_cmd(0x0C);  // Display ON, cursor OFF
}

void lcd_set_cursor(uint8_t row, uint8_t col) {
  lcd_cmd(0x80 + (row == 0 ? 0x00 : 0x40) + col);
}

void lcd_print(const char* str) {
  while (*str) lcd_data(*str++);
}


void lcd_volPrevShow(bool lx) {
  if (knobRoll || lx) {
    snprintf(lcd_buf, sizeof(lcd_buf), "UnderVol < %2d V", storage.undVol);
    lcd_set_cursor(0, 0);
    lcd_print(lcd_buf);
    snprintf(lcd_buf, sizeof(lcd_buf), "Over Vol > %2d V", storage.ovrVol);
    lcd_set_cursor(1, 0);
    lcd_print(lcd_buf);
    knobRoll = 0;
  }
}


void lcd_curPrevShow(bool lx) {
  if (knobRoll || lx) {
    char bufx[8];
    dtostrf(storage.dryRunM1, 3, 1, bufx);
    snprintf(lcd_buf, sizeof(lcd_buf), "DRY RUN < %s A ", bufx);
    lcd_set_cursor(0, 0);
    lcd_print(lcd_buf);
    dtostrf(storage.ovLRunM1, 3, 1, bufx);
    snprintf(lcd_buf, sizeof(lcd_buf), "OV LOAD > %s A ", bufx);
    lcd_set_cursor(1, 0);
    lcd_print(lcd_buf);
    knobRoll = 0;
  }
}


void lcd_nameShow(bool lx) {
  if (knobRoll || lx) {
    lcd_set_cursor(0, 0);
    //c_print("1234567890123456");
    lcd_print("   Device Name  ");
    lcd_set_cursor(1, 0);
    lcd_print("                ");
    lcd_set_cursor(1, (16 - String(storage.dname).length()) / 2);
    lcd_print(storage.dname);
    knobRoll = 0;
  }
}


void lcd_modeShow() {
  lcd_set_cursor(0, 0);
  //c_print("1234567890123456");
  lcd_print("Mode Changed to ");
  lcd_set_cursor(1, 0);
  lcd_print("                ");
  if (storage.modeM1 == 0) {
    lcd_set_cursor(1, 0);
    lcd_print("     NORMAL     ");
  } else if (storage.modeM1 == 1) {
    lcd_set_cursor(1, 0);
    lcd_print("     AUTO       ");
  } else if (storage.modeM1 == 2) {
    lcd_set_cursor(1, 0);
    lcd_print("     CYCLIC     ");
  } else if (storage.modeM1 == 3) {
    lcd_set_cursor(1, 0);
    lcd_print("   COUNTDOWN    ");
  } else if (storage.modeM1 == 4) {
    lcd_set_cursor(1, 0);
    lcd_print("   SCHEDULER    ");
  } else if (storage.modeM1 == 5) {
    lcd_set_cursor(1, 0);
    lcd_print("   LAST STATE   ");
  }
}

void lcd_activeModeShow() {


  if (storage.modeM1 == 0) {
    lcd_set_cursor(0, 0);
    lcd_print("  NORMAL MODE   ");
    lcd_set_cursor(1, 0);
    if (m1StaVars) {
      lcd_print("MOTOR is RUNNING");
    } else {
      lcd_print("MOTOR is STOPPED");
    }
  }

  //////////////////////////////////////////
  if (storage.modeM1 == 1) {
    if (m1StaVars) {
      lcd_set_cursor(0, 0);
      lcd_print("AUTO START MODE ");
      lcd_set_cursor(1, 0);
      lcd_print("MOTOR is RUNNING");
    } else if (autoM1Trigd) {
      lcd_set_cursor(0, 0);
      lcd_print("AUTO START MODE ");
      lcd_set_cursor(1, 0);
      lcd_print("STARTER TRIGGER ");
    } else {
      // Count-down phase.  Use explicit comparison so unsigned arithmetic
      // cannot wrap and produce a false "countdown still running" result.
      unsigned long target = ((atMins + atSec) * 1000UL) + autoM1BuffMillis;
      if (millis() < target) {
        lcd_set_cursor(0, 0);
        lcd_print("AUTOSTART:DELAY ");

        unsigned long x       = target - millis();
        unsigned long seconds = x / 1000;
        unsigned long minutes = seconds / 60;
        unsigned long hours   = minutes / 60;
        seconds %= 60;
        minutes %= 60;

        if (hours > 0) {
          snprintf(lcd_buf, sizeof(lcd_buf), "RESTART%02lu:%02lu:%02lu  ", hours, minutes, seconds);
        } else {
          snprintf(lcd_buf, sizeof(lcd_buf), "  Start In %02lu:%02lu  ", minutes, seconds);
        }
        lcd_set_cursor(1, 0);
        lcd_print(lcd_buf);
      } else {
        // Delay has expired but trigger is being held back (e.g. fault active).
        lcd_set_cursor(0, 0);
        lcd_print("AUTO START MODE ");
        lcd_set_cursor(1, 0);
        if (elst > 0) {
          lcd_print(" FAULT: WAITING ");
        } else {
          lcd_print(" READY TO START ");
        }
      }
    }
  }
  //////////////////////////////////////////
  if (storage.modeM1 == 2) {

    lcd_set_cursor(0, 0);
    lcd_print("  CYCLIC MODE   ");

    unsigned long seconds = 0;
    unsigned long minutes = 0;
    unsigned long hours = 0;

    if (cyclicM1State == 0) {
      snprintf(lcd_buf, sizeof(lcd_buf), "Preparing to Run ");
    } else if (cyclicM1State == 1) {
      unsigned long onDurMillis = cyclicM1OnDurMillis - millis();
      if ((long)onDurMillis < 0) {
        onDurMillis = 0;  // safety, no negative countdown
      }

      seconds = onDurMillis / 1000;  // total seconds
      minutes = seconds / 60;        // total minutes
      hours = minutes / 60;          // total hours
      seconds %= 60;
      minutes %= 60;

      if (hours > 0) {
        snprintf(lcd_buf, sizeof(lcd_buf), "RUN for %02lu:%02lu:%02lu", hours, minutes, seconds);
      } else {
        snprintf(lcd_buf, sizeof(lcd_buf), "RUN for %02lu:%02lu   ", minutes, seconds);
      }
    } else if (cyclicM1State == 2) {
      unsigned long offDurMillis = cyclicM1OffDurMillis - millis();
      if ((long)offDurMillis < 0) {
        offDurMillis = 0;  // safety, no negative countdown
      }

      seconds = offDurMillis / 1000;  // total seconds
      minutes = seconds / 60;         // total minutes
      hours = minutes / 60;           // total hours
      seconds %= 60;
      minutes %= 60;

      if (hours > 0) {
        snprintf(lcd_buf, sizeof(lcd_buf), "STOP for %02lu:%02lu:%02lu", hours, minutes, seconds);
      } else {
        snprintf(lcd_buf, sizeof(lcd_buf), "STOP for %02lu:%02lu   ", minutes, seconds);
      }
    }

    lcd_set_cursor(1, 0);
    lcd_print(lcd_buf);
  }
  //////////////////////////////////////////
  if (storage.modeM1 == 3) {
    lcd_set_cursor(0, 0);
    lcd_print("COUNTDOWN TIMER ");
    if (m1StaVars) {
      unsigned long offMillis = (((cTHrs + cTMins) * 1000UL) + countM1BuffMills) - millis();
      if ((long)offMillis < 0) {
        offMillis = 0;  // safety, no negative countdown
      }

      unsigned long seconds = offMillis / 1000;  // total seconds
      unsigned long minutes = seconds / 60;      // total minutes
      unsigned long hours = minutes / 60;        // total hours
      seconds %= 60;
      minutes %= 60;

      if (hours > 0) {
        snprintf(lcd_buf, sizeof(lcd_buf), "RUN for %02lu:%02lu:%02lu", hours, minutes, seconds);
      } else {
        snprintf(lcd_buf, sizeof(lcd_buf), "RUN for %02lu:%02lu   ", minutes, seconds);
      }
      lcd_set_cursor(1, 0);
      lcd_print(lcd_buf);
    } else {
      lcd_set_cursor(1, 0);
      lcd_print("MOTOR is STOPPED");
    }
  }
  if (storage.modeM1 == 4) {
    lcd_set_cursor(0, 0);
    //c_print("1234567890123456");
    lcd_print("MODE : SCHEDULER");
    lcd_set_cursor(1, 0);
    //c_print("1234567890123456");
    lcd_print(" use Mobile App ");
  }
  //////////////////////////////////////////
  // last state remembers
  // bool laStaRemM1Trigd = 0;
  // unsigned long laStaRemM1BuffMillis = 0;
  // unsigned long laStaRemSec = 0;
  // unsigned long laStaRemMins = 0;
  // unsigned long laStaRemM1TrigdMillis = 0;
  if (storage.modeM1 == 5) {
    if (laStaAppStaSavShow) {
      // Show "SAVING STATE" for 1 s without blocking the main loop.
      // A static millis() timer replaces the former delay(1000).
      static unsigned long laStaSavMs = 0;
      if (laStaSavMs == 0) {
        // First entry: render screen and start hold timer.
        lcd_set_cursor(0, 0);
        lcd_print("  SAVING STATE  ");
        lcd_set_cursor(1, 0);
        if (storage.app1Run) {
          lcd_print("MOTOR is Running");
        } else {
          lcd_print("MOTOR is Stopped");
        }
        laStaSavMs = millis();
        if (laStaSavMs == 0) laStaSavMs = 1;  // avoid zero sentinel
      }
      // After 1 s clear the flag; normal status display resumes next loop.
      if (millis() - laStaSavMs >= 1000) {
        laStaAppStaSavShow = 0;
        laStaSavMs = 0;
        wdt_reset();
      }
    } else if (m1StaVars) {
      lcd_set_cursor(0, 0);
      lcd_print("LAST STATE MODE ");
      lcd_set_cursor(1, 0);
      lcd_print("MOTOR is RUNNING");
    } else if (!m1StaVars && storage.app1Run &&
               millis() < ((laStaRemMins + laStaRemSec) * 1000UL) + laStaRemM1BuffMillis) {
      lcd_set_cursor(0, 0);
      lcd_print("MOTOR LAST STATE");

      unsigned long target = ((laStaRemMins + laStaRemSec) * 1000UL) + laStaRemM1BuffMillis;
      unsigned long x      = target - millis();

      unsigned long seconds = x / 1000;
      unsigned long minutes = seconds / 60;
      unsigned long hours   = minutes / 60;
      seconds %= 60;
      minutes %= 60;

      if (hours > 0) {
        snprintf(lcd_buf, sizeof(lcd_buf), "RESTART%02lu:%02lu:%02lu  ", hours, minutes, seconds);
      } else if (minutes > 0) {
        snprintf(lcd_buf, sizeof(lcd_buf), "Start In %02lu:%02lu  ", minutes, seconds);
      } else {
        snprintf(lcd_buf, sizeof(lcd_buf), "Start In: %2lus  ", seconds);
      }

      lcd_set_cursor(1, 0);
      lcd_print(lcd_buf);
    } else if (!m1StaVars) {
      lcd_set_cursor(0, 0);
      lcd_print("LAST STATE MODE ");
      lcd_set_cursor(1, 0);
      lcd_print("MOTOR is STOPPED");
    }
  }
  //////////////////////////////////////////
}
extern bool mlvl1;
extern bool mlvl2;
extern bool mlvl3;
extern bool mlvl4;
extern int menuPos;

static bool statusScreenActive = 0;
static uint8_t statusScreenIndex = 0;
static unsigned long statusScreenLastInput = 0;

// Live V/I data screen — single screen for Cyclic / Countdown / Scheduler modes
static bool          liveDataScreenActive = false;
static unsigned long liveDataLastInput    = 0;

static uint8_t normalizeModeForUi(uint8_t rawMode) {
  int modeVal = (int)rawMode;
  if (modeVal >= '0' && modeVal <= '5') {
    modeVal -= '0';
  } else if (modeVal == 6) {
    modeVal = 5;
  }

  if (modeVal < 0 || modeVal > 5) {
    return 0;
  }
  return (uint8_t)modeVal;
}


static void openMainMenuRoot() {
  menuUiFunc = 1;
  mlvl1 = 1;
  mlvl2 = 0;
  mlvl3 = 0;
  mlvl4 = 0;
  menuPos = 1000;
  clearNavEvents();
}

static void openCountdownHotkey() {
  menuUiFunc = 1;
  mlvl1 = 0;
  mlvl2 = 0;
  mlvl3 = 0;
  mlvl4 = 1;
  menuPos = 1410;
  clearNavEvents();
}

// Forward declarations — these are defined later in this file or in menu.h
bool get2ValFunc(String ValName, int &t1, int &t2, char x, int xT, char y, int yT);
void uiFunc(bool lx);

// Direct countdown editor — mirrors case 1410 logic exactly.
// Called from the status-screen ENTER hotkey so that get2ValFunc() runs
// immediately without relying on menuUi()/mlvl routing.
static void invokeCountdownEditor() {
  int v1 = String(storage.countM1).substring(1, 3).toInt();
  int v2 = String(storage.countM1).substring(3, 5).toInt();
  if (get2ValFunc("CounTmr", v1, v2, 'h', 23, 'm', 59)) {
    // Confirmed — save value and activate Countdown mode
    storage.modeM1 = 3;
    markLocalModeChange();
    char tVal[17];
    snprintf(tVal, sizeof(tVal), "z%02d%02d", v1, v2);
    strncpy(storage.countM1, tVal, sizeof(storage.countM1) - 1);
    storage.countM1[sizeof(storage.countM1) - 1] = '\0';
    savecon();
    iotSerial.println("<{\"AT\":{\"a1cTmr\":\"" + String(storage.countM1) + "\"}}>");
    delay(500);
    iotSerial.println("<{\"TS\":{\"n\":\"3\"}}>");
    loadModeVal();
    uiFunc(1);     // hold "mode changed" display for 3 s non-blocking
    lcd_modeShow();
  }
  // Always close menu and return to home display (cancel path arrives here too)
  menuUiFunc      = 0;
  lcdDisplayState = 255;
}

// ENTER hotkey from Status List → jumps directly to Cyclic ON-duration editor
static void openCyclicHotkey() {
  menuUiFunc = 1;
  mlvl1 = 0;
  mlvl2 = 0;
  mlvl3 = 0;
  mlvl4 = 1;
  menuPos = 1310;
  clearNavEvents();
}

static void toggleNormalAutoMode() {
  uint8_t curMode = normalizeModeForUi(storage.modeM1);

  // Button logic (fixed):
  //   Not in Normal → always go to Normal + stop motor
  //   In Normal     → always go to Auto mode (1)
  uint8_t nextMode = (curMode == 0) ? 1 : 0;

  storage.modeM1 = nextMode;
  markLocalModeChange();
  savecon();
  loadModeVal();

  if (nextMode == 0) {
    // Switched to NORMAL MODE: stop motor immediately
    m1Off();
    autoM1Trigd = 0;

  } else {
    // Switched to AUTO START: stop motor first, then begin countdown delay
    if (m1StaVars) {
      m1Off();
    }
    autoM1Trigd      = 0;
    autoM1BuffMillis = millis();
    atLastTrigdMillis = 0;
  }

  lcd_modeShow();
}
static void showStatusScreenItem(uint8_t itemIndex) {
  // Status List order (per spec v15):
  // 0 = Current Mode
  // 1 = Dry Run Setting
  // 2 = Overload Setting
  // 3 = Over Volt Setting
  // 4 = Under Volt Setting
  // 5 = Device Name
  // 6 = Date and Time (RTC, if ENABLE_RTC)
  char b1[8];

  switch (itemIndex) {
    case 0: {
      uint8_t m = normalizeModeForUi(storage.modeM1);
      lcd_set_cursor(0, 0);
      lcd_print("  CURRENT MODE  ");
      lcd_set_cursor(1, 0);
      if      (m == 0) lcd_print("  NORMAL MODE   ");
      else if (m == 1) lcd_print("AUTO START MODE ");
      else if (m == 2) lcd_print("  CYCLIC MODE   ");
      else if (m == 3) lcd_print(" COUNTDOWN MODE ");
      else if (m == 4) lcd_print(" SCHEDULE MODE  ");
      else if (m == 5) lcd_print("LAST STATE MODE ");
      else             lcd_print("  UNKNOWN MODE  ");
      break;
    }

    case 1:
      dtostrf(storage.dryRunM1, 4, 1, b1);
      lcd_set_cursor(0, 0);
      lcd_print("Dry Run Set:    ");
      lcd_set_cursor(1, 0);
      snprintf(lcd_buf, sizeof(lcd_buf), "%s A            ", b1);
      lcd_print(lcd_buf);
      break;

    case 2:
      dtostrf(storage.ovLRunM1, 4, 1, b1);
      lcd_set_cursor(0, 0);
      lcd_print("Overload Set:   ");
      lcd_set_cursor(1, 0);
      snprintf(lcd_buf, sizeof(lcd_buf), "%s A            ", b1);
      lcd_print(lcd_buf);
      break;

    case 3:
      lcd_set_cursor(0, 0);
      lcd_print("Over Volt Set:  ");
      lcd_set_cursor(1, 0);
      snprintf(lcd_buf, sizeof(lcd_buf), "%3d V           ", storage.ovrVol);
      lcd_print(lcd_buf);
      break;

    case 4:
      lcd_set_cursor(0, 0);
      lcd_print("Under Volt Set: ");
      lcd_set_cursor(1, 0);
      snprintf(lcd_buf, sizeof(lcd_buf), "%3d V           ", storage.undVol);
      lcd_print(lcd_buf);
      break;

    case 5:
    default:
      lcd_set_cursor(0, 0);
      lcd_print("Device Name:    ");
      lcd_set_cursor(1, 0);
      snprintf(lcd_buf, sizeof(lcd_buf), "%-16s", storage.dname);
      lcd_print(lcd_buf);
      break;

#ifdef ENABLE_RTC
    case 6:
      if (rtcPassVars) {
        snprintf(lcd_buf, sizeof(lcd_buf), "  %02d-%02d-%02d %s  ",
                 tDate, tMonth, tYear, sDoW);
        lcd_set_cursor(0, 0);
        lcd_print(lcd_buf);
        snprintf(lcd_buf, sizeof(lcd_buf), "  %02d:%02d:%02d %s   ",
                 tHrs, tMin, tSec, tAmPm);
        lcd_set_cursor(1, 0);
        lcd_print(lcd_buf);
      } else {
        lcd_set_cursor(0, 0);
        lcd_print("  DATE & TIME   ");
        lcd_set_cursor(1, 0);
        lcd_print(" RTC NOT FOUND  ");
      }
      break;
#endif
  }
}


// ── Display State Machine helpers ─────────────────────────────────────

static void lcd_formatTimer(char* buf, unsigned long remainMs) {
  unsigned long sec  = remainMs / 1000;
  unsigned long mins = sec / 60;
  unsigned long hrs  = mins / 60;
  sec  %= 60;
  mins %= 60;
  if (hrs > 0) {
    snprintf(buf, 17, " %02lu:%02lu:%02lu       ", hrs, mins, sec);
  } else {
    snprintf(buf, 17, "     %02lu:%02lu      ", mins, sec);
  }
}

static const char* lcd_faultNameStr() {
  // Priority: voltage/phase faults first, then current faults (Open Wire > Overload > Leakage > Dry Run)
  if (powerLoss)    return "  POWER LOSS    ";
  if (phaseFault)   return "  PHASE CUT     ";
  if (unVolVars)    return " UNDER VOLTAGE  ";
  if (hiVolVars)    return " OVER VOLTAGE   ";
  if (openWireVars) return "FAULT OPEN WIRE ";
  if (overloadVars) return "FAULT OVERLOAD  ";
  if (leakageVars)  return "FAULT LEAKAGE   ";
  if (dryRunVars)   return "FAULT DRY RUN   ";
  return "    UNKNOWN     ";
}

static void lcd_drawHome() {
  snprintf(lcd_buf, sizeof(lcd_buf), "V %3d %3d %3d V ", volRY, volYB, volBR);
  lcd_set_cursor(0, 0); lcd_print(lcd_buf);
  char bR[8], bY[8], bB[8];
  if (curR   <= 0.5) strcpy(bR, " 0.0"); else dtostrf(curR,   4, 1, bR);
  if (curY   <= 0.5) strcpy(bY, " 0.0"); else dtostrf(curY,   4, 1, bY);
  if (curBM1 <= 0.5) strcpy(bB, " 0.0"); else dtostrf(curBM1, 4, 1, bB);
  snprintf(lcd_buf, sizeof(lcd_buf), "I%s %s %sA", bR, bY, bB);
  lcd_set_cursor(1, 0); lcd_print(lcd_buf);
}

static void lcd_drawFault() {
  if (unVolVars || hiVolVars) {
    // Under / Over voltage: ROW0 = live voltages, ROW1 = fault/recovery info
    snprintf(lcd_buf, sizeof(lcd_buf), "V %3d %3d %3d V ", volRY, volYB, volBR);
    lcd_set_cursor(0, 0); lcd_print(lcd_buf);
    {
      unsigned long recovDur = (unsigned long)(atMins + atSec) * 1000UL;
      unsigned long sec, mins;
      if (uvRecovering) {
        unsigned long elapsed = millis() - uvRecovStartMs;
        unsigned long remain  = (elapsed < recovDur) ? (recovDur - elapsed) : 0;
        sec = remain / 1000; mins = sec / 60; if (mins > 99) mins = 99; sec %= 60;
        snprintf(lcd_buf, sizeof(lcd_buf), "UV Recovry %02lu:%02lu", mins, sec);
      } else if (ovRecovering) {
        unsigned long elapsed = millis() - ovRecovStartMs;
        unsigned long remain  = (elapsed < recovDur) ? (recovDur - elapsed) : 0;
        sec = remain / 1000; mins = sec / 60; if (mins > 99) mins = 99; sec %= 60;
        snprintf(lcd_buf, sizeof(lcd_buf), "OV Recovry %02lu:%02lu", mins, sec);
      } else {
        unsigned long elapsed = voltFaultTimerRun ? (millis() - voltFaultStartMs) : 0;
        sec = elapsed / 1000; mins = sec / 60; if (mins > 99) mins = 99; sec %= 60;
        const char* shortName = unVolVars ? "UNDERVOLT " : "OVERVOLT  ";
        snprintf(lcd_buf, sizeof(lcd_buf), "%s%02lu:%02lu ", shortName, mins, sec);
      }
    }
    lcd_set_cursor(1, 0); lcd_print(lcd_buf);

  } else if (phaseFault) {
    // Phase cut: ROW0 = live voltages, ROW1 = fault/recovery info
    snprintf(lcd_buf, sizeof(lcd_buf), "V %3d %3d %3d V ", volRY, volYB, volBR);
    lcd_set_cursor(0, 0); lcd_print(lcd_buf);
    {
      unsigned long sec, mins;
      if (phRecovering) {
        unsigned long recovDur = (unsigned long)(atMins + atSec) * 1000UL;
        unsigned long elapsed  = millis() - phRecovStartMs;
        unsigned long remain   = (elapsed < recovDur) ? (recovDur - elapsed) : 0;
        sec = remain / 1000; mins = sec / 60; if (mins > 99) mins = 99; sec %= 60;
        snprintf(lcd_buf, sizeof(lcd_buf), "PH Recovry %02lu:%02lu", mins, sec);
      } else {
        unsigned long elapsed = phaseFaultTimerRun ? (millis() - phaseFaultStartMs) : 0;
        sec = elapsed / 1000; mins = sec / 60; if (mins > 99) mins = 99; sec %= 60;
        snprintf(lcd_buf, sizeof(lcd_buf), "PHASE CUT %02lu:%02lu ", mins, sec);
      }
    }
    lcd_set_cursor(1, 0); lcd_print(lcd_buf);

  } else if (powerLoss) {
    // Power loss: ROW1 = live voltages, ROW2 = fault name only (no timer)
    snprintf(lcd_buf, sizeof(lcd_buf), "V %3d %3d %3d V ", volRY, volYB, volBR);
    lcd_set_cursor(0, 0); lcd_print(lcd_buf);
    lcd_set_cursor(1, 0); lcd_print("  POWER LOSS    ");

  } else {
    // Current fault (overload / dry run): ROW1 = fault name, ROW2 = phase currents
    lcd_set_cursor(0, 0); lcd_print(lcd_faultNameStr());
    char bR[8], bY[8], bB[8];
    if (curR   <= 0.5) strcpy(bR, " 0.0"); else dtostrf(curR,   4, 1, bR);
    if (curY   <= 0.5) strcpy(bY, " 0.0"); else dtostrf(curY,   4, 1, bY);
    if (curBM1 <= 0.5) strcpy(bB, " 0.0"); else dtostrf(curBM1, 4, 1, bB);
    snprintf(lcd_buf, sizeof(lcd_buf), "I%s %s %sA", bR, bY, bB);
    lcd_set_cursor(1, 0); lcd_print(lcd_buf);
  }
}

static void lcd_drawAutoMode() {
  unsigned long target = ((atMins + atSec) * 1000UL) + autoM1BuffMillis;
  unsigned long remain = (millis() < target) ? (target - millis()) : 0;
  lcd_set_cursor(0, 0); lcd_print("   AUTO MODE    ");
  unsigned long sec  = remain / 1000;
  unsigned long mins = sec / 60;
  unsigned long hrs  = mins / 60;
  sec  %= 60;
  mins %= 60;
  if (hrs > 0) {
    snprintf(lcd_buf, sizeof(lcd_buf), "  Start %02lu:%02lu:%02lu", hrs, mins, sec);
  } else {
    snprintf(lcd_buf, sizeof(lcd_buf), "  Start In %02lu:%02lu ", mins, sec);
  }
  lcd_set_cursor(1, 0); lcd_print(lcd_buf);
}

static void lcd_drawLastState() {
  if (laStaAppStaSavShow) {
    static unsigned long savMs = 0;
    if (savMs == 0) {
      lcd_set_cursor(0, 0); lcd_print("  SAVING STATE  ");
      lcd_set_cursor(1, 0);
      lcd_print(storage.app1Run ? "MOTOR is Running" : "MOTOR is Stopped");
      savMs = millis(); if (!savMs) savMs = 1;
    }
    if (millis() - savMs >= 1000) {
      laStaAppStaSavShow = 0; savMs = 0; wdt_reset();
      lcdDisplayState = 255;   // force full redraw after notification
    }
    return;
  }
  unsigned long target = ((laStaRemMins + laStaRemSec) * 1000UL) + laStaRemM1BuffMillis;
  unsigned long remain = (millis() < target) ? (target - millis()) : 0;
  lcd_set_cursor(0, 0); lcd_print("LAST STATE MODE ");
  unsigned long sec2  = remain / 1000;
  unsigned long mins2 = sec2 / 60;
  unsigned long hrs2  = mins2 / 60;
  sec2  %= 60;
  mins2 %= 60;
  if (hrs2 > 0) {
    snprintf(lcd_buf, sizeof(lcd_buf), "RESTART %02lu:%02lu:%02lu", hrs2, mins2, sec2);
  } else {
    snprintf(lcd_buf, sizeof(lcd_buf), " Start In %02lu:%02lu ", mins2, sec2);
  }
  lcd_set_cursor(1, 0); lcd_print(lcd_buf);
}

static void lcd_drawCountdown() {
  unsigned long endMs  = ((cTHrs + cTMins) * 1000UL) + countM1BuffMills;
  unsigned long remain = (millis() < endMs) ? (endMs - millis()) : 0;
  lcd_set_cursor(0, 0); lcd_print("MOTOR STOPS IN  ");
  lcd_formatTimer(lcd_buf, remain);
  lcd_set_cursor(1, 0); lcd_print(lcd_buf);
}

static void lcd_drawCyclicOn() {
  unsigned long remain = (millis() < cyclicM1OnDurMillis) ? (cyclicM1OnDurMillis - millis()) : 0;
  lcd_set_cursor(0, 0); lcd_print("CYCLIC MODE ON  ");
  lcd_formatTimer(lcd_buf, remain);
  lcd_set_cursor(1, 0); lcd_print(lcd_buf);
}

static void lcd_drawCyclicOff() {
  unsigned long remain = (millis() < cyclicM1OffDurMillis) ? (cyclicM1OffDurMillis - millis()) : 0;
  lcd_set_cursor(0, 0); lcd_print("CYCLIC MODE OFF ");
  lcd_formatTimer(lcd_buf, remain);
  lcd_set_cursor(1, 0); lcd_print(lcd_buf);
}

// ── Live data screen (Cyclic / Countdown / Scheduler) ────────────────────────
static void lcd_drawLiveData() {
  snprintf(lcd_buf, sizeof(lcd_buf), "V %3d %3d %3d V ", volRY, volYB, volBR);
  lcd_set_cursor(0, 0); lcd_print(lcd_buf);
  char bR[8], bY[8], bB[8];
  if (curR   <= 0.5) strcpy(bR, " 0.0"); else dtostrf(curR,   4, 1, bR);
  if (curY   <= 0.5) strcpy(bY, " 0.0"); else dtostrf(curY,   4, 1, bY);
  if (curBM1 <= 0.5) strcpy(bB, " 0.0"); else dtostrf(curBM1, 4, 1, bB);
  snprintf(lcd_buf, sizeof(lcd_buf), "I%s %s %sA", bR, bY, bB);
  lcd_set_cursor(1, 0); lcd_print(lcd_buf);
}

static void lcdDisplayStateMachine() {
  unsigned long now = millis();

  uint8_t ds;
  bool faultActive = (elst > 0 || overloadVars || dryRunVars || openWireVars || leakageVars);

  if (faultActive) {
    ds = DISPLAY_FAULT;
  } else if (storage.modeM1 == 1 && !autoM1Trigd && !m1StaVars
             && now < ((atMins + atSec) * 1000UL) + autoM1BuffMillis) {
    ds = DISPLAY_AUTO_MODE;
  } else if (storage.modeM1 == 5 && !m1StaVars && storage.app1Run && !laStaRemM1Trigd
             && now < ((laStaRemMins + laStaRemSec) * 1000UL) + laStaRemM1BuffMillis) {
    ds = DISPLAY_LAST_STATE;
  } else if (storage.modeM1 == 3 && m1StaVars) {
    ds = DISPLAY_COUNTDOWN;
  } else if (storage.modeM1 == 2 && cyclicM1State == 1) {
    ds = DISPLAY_CYCLIC_ON;
  } else if (storage.modeM1 == 2 && cyclicM1State == 2) {
    ds = DISPLAY_CYCLIC_OFF;
  } else {
    ds = DISPLAY_HOME;
  }

  if (laStaAppStaSavShow) {
    ds = DISPLAY_LAST_STATE;
  }

  if (ds != lcdDisplayState || (now - lcdDisplayMs >= 1000UL)) {
    lcdDisplayState = ds;
    lcdDisplayMs    = now;
    switch (ds) {
      case DISPLAY_HOME:       lcd_drawHome();       break;
      case DISPLAY_FAULT:      lcd_drawFault();      break;
      case DISPLAY_AUTO_MODE:  lcd_drawAutoMode();   break;
      case DISPLAY_LAST_STATE: lcd_drawLastState();  break;
      case DISPLAY_COUNTDOWN:  lcd_drawCountdown();  break;
      case DISPLAY_CYCLIC_ON:  lcd_drawCyclicOn();   break;
      case DISPLAY_CYCLIC_OFF: lcd_drawCyclicOff();  break;
    }
  }
}

void uiFunc(bool lx) {
  static unsigned long blockUntil = 0;
  unsigned long now = millis();

  rotaryFunc();

  if (lx) {
    blockUntil = now + 3000;
    return;
  }

  if (now < blockUntil) {
    // Drop ALL stale navigation events while blocked.  Without this, any
    // UP/DOWN/ENTER/MENU input queued during the 1.2–3 s display window
    // fires immediately after it expires, causing the status screen, the
    // menu, or the countdown hotkey to open unexpectedly.
    takeModePress();
    takeMenuPress();
    rotValPlus  = 0;
    rotValMinus = 0;
    rotPush     = 0;
    return;
  }

  // ── Bypass switch warning — HIGHEST display priority ─────────────────────
  // Flash "PASS SWITCH ON! / Turn it OFF!" for 4 s every 30 s while bypass is active.
  // Outside that 4 s window the normal priority screen is shown unchanged.
  {
    static unsigned long bypassFlashStart = 0;
    if (bypassActive) {
      unsigned long phase = (bypassFlashStart == 0) ? 30000UL : (now - bypassFlashStart);
      if (phase >= 30000UL) {
        // Start a new 30 s cycle now
        bypassFlashStart = now;
        phase = 0;
      }
      if (phase < 4000UL) {
        // Show warning for first 4 s of the cycle
        lcd_set_cursor(0, 0); lcd_print("BYPASS SWITCH ON");
        lcd_set_cursor(1, 0); lcd_print("  Turn it OFF!  ");
        lcdDisplayState = 255;   // force normal screen to redraw when flash ends
        return;
      }
      // else: remaining 26 s — fall through to normal display
    } else {
      bypassFlashStart = 0;   // reset cycle when bypass goes off
    }
  }

  if (statusScreenActive) {
    if ((now - statusScreenLastInput) >= 15000UL) {
      statusScreenActive = 0;
      clearNavEvents();
      lcdDisplayState = 255;   // force immediate redraw via state machine
      return;
    }

    if (rotValPlus) {
      statusScreenIndex = (statusScreenIndex + 1) % STATUS_SCREEN_ITEMS;
      rotValPlus = 0;
      statusScreenLastInput = now;
      showStatusScreenItem(statusScreenIndex);
      return;
    }

    if (rotValMinus) {
      statusScreenIndex = (statusScreenIndex + (STATUS_SCREEN_ITEMS - 1)) % STATUS_SCREEN_ITEMS;
      rotValMinus = 0;
      statusScreenLastInput = now;
      showStatusScreenItem(statusScreenIndex);
      return;
    }

    bool modePressed = takeModePress();

    // ENTER (rotPush) in Status List → open Countdown timer editor directly
    if (rotPush) {
      rotPush = 0;
      statusScreenActive = 0;
      clearNavEvents();
      invokeCountdownEditor();   // runs get2ValFunc() inline — no menuUi routing needed
      return;
    }

    if (takeMenuPress() || modePressed) {
      statusScreenActive = 0;
      clearNavEvents();
      if (modePressed) {
        toggleNormalAutoMode();
        blockUntil = millis() + 1200;
      } else {
        lcdDisplayState = 255;   // let state machine redraw on next call
      }
      return;
    }

    // Auto-refresh item 0 (Current Mode) every 1 s so fault/remote changes show immediately
    {
      static unsigned long modeItemRefreshMillis = 0;
      if (statusScreenIndex == 0 && (now - modeItemRefreshMillis) >= 1000UL) {
        modeItemRefreshMillis = now;
        showStatusScreenItem(0);
      }
    }

#ifdef ENABLE_RTC
    // Auto-refresh item 6 (Date/Time) every 1 s so seconds tick live
    {
      static unsigned long rtcItemRefreshMillis = 0;
      if (statusScreenIndex == 6 && (now - rtcItemRefreshMillis) >= 1000UL) {
        rtcItemRefreshMillis = now;
        showStatusScreenItem(6);
      }
    }
#endif
    return;
  }

  // ── Live data screen (Cyclic / Countdown / Scheduler modes) ─────────────────
  if (liveDataScreenActive) {
    // 30-second idle timeout — dismiss and return to mode status
    if ((now - liveDataLastInput) >= 30000UL) {
      liveDataScreenActive = false;
      clearNavEvents();
      lcdDisplayState = 255;
      return;
    }

    // UP / DOWN resets the idle timer (screen stays visible)
    if (rotValPlus || rotValMinus) {
      rotValPlus      = 0;
      rotValMinus     = 0;
      liveDataLastInput = now;
    }

    // MENU or MODE press → exit live data screen
    bool modeP = takeModePress();
    if (takeMenuPress() || modeP) {
      liveDataScreenActive = false;
      clearNavEvents();
      lcdDisplayState = 255;
      return;
    }

    if (rotPush) { rotPush = 0; }   // consume, no action

    // Refresh live V/I values every 1 s
    static unsigned long liveRefreshMs = 0;
    if (lcdDisplayState == 255 || (now - liveRefreshMs) >= 1000UL) {
      lcdDisplayState = 254;   // sentinel: liveData owns the display
      liveRefreshMs   = now;
      lcd_drawLiveData();
    }
    return;
  }

  if (takeModePress()) {
    toggleNormalAutoMode();
    blockUntil = millis() + 1200;
    return;
  }

  if (takeMenuPress()) {
    openMainMenuRoot();
    return;
  }

  if (rotPush) {
    rotPush = 0;
    invokeCountdownEditor();   // ENTER hotkey: open countdown timer editor directly
    return;
  }

  if (rotValPlus || rotValMinus) {
    uint8_t curMode = normalizeModeForUi(storage.modeM1);
    rotValPlus  = 0;
    rotValMinus = 0;
    if (curMode == 2 || curMode == 3 || curMode == 4) {
      // Cyclic / Countdown / Scheduler → live V/I data screen
      liveDataScreenActive = true;
      liveDataLastInput    = now;
      lcdDisplayState      = 255;
      lcd_drawLiveData();
    } else {
      // All other modes → settings status list
      statusScreenActive    = 1;
      statusScreenIndex     = 0;
      statusScreenLastInput = now;
      showStatusScreenItem(statusScreenIndex);
    }
    return;
  }
  lcdDisplayStateMachine();
}





