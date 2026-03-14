#include "Arduino.h"
bool mlvl1 = 1;
bool mlvl2 = 0;
bool mlvl3 = 0;
bool mlvl4 = 0;


//bool menuUiFunc = 0;
int menuPos = 1000;
unsigned long menuLastInput = 0;  // global idle timer — updated by menuUi() and all sub-menu functions


char lcd_buf1[17];
char lcd_buf2[17];
char tmpVal[17];

int var1 = 0, var2 = 0;
float var3 = 0.0;

void rotaryVal() {

  if (rotValPlus) {
    if (mlvl1) {
      menuPos = menuPos + 1000;
    } else if (mlvl2) {
      menuPos = menuPos + 100;
    } else if (mlvl3) {
      menuPos = menuPos + 10;
    } else if (mlvl4) {
      menuPos = menuPos + 1;
    }
    Serial3.println(menuPos);
    rotValPlus = 0;
  }
  if (rotValMinus) {
    if (mlvl1) {
      menuPos = menuPos - 1000;
    } else if (mlvl2) {
      menuPos = menuPos - 100;
    } else if (mlvl3) {
      menuPos = menuPos - 10;
    } else if (mlvl4) {
      menuPos = menuPos - 1;
    }
    Serial3.println(menuPos);
    rotValMinus = 0;
  }

  if (rotPush) {
    if (mlvl1) {
      mlvl1 = 0;
      mlvl2 = 1;
      menuPos = menuPos + 100;
      mlvl3 = 0;
      mlvl4 = 0;
      Serial3.println("L2");
    } else if (mlvl2) {
      mlvl1 = 0;
      mlvl2 = 0;
      mlvl3 = 1;
      menuPos = menuPos + 10;
      mlvl4 = 0;
      Serial3.println("L3");
    } else if (mlvl3) {
      mlvl1 = 0;
      mlvl2 = 0;
      mlvl3 = 0;
      mlvl4 = 1;
      menuPos = menuPos + 1;
      Serial3.println("L4");
    } else if (mlvl4) {
      mlvl1 = 1;
      mlvl2 = 0;
      mlvl3 = 0;
      mlvl4 = 0;
      Serial3.println("L1");
    }
    rotPush = 0;
  }
}





void mainMenuShow() {
  delay(800);
  menuUiFunc = 1;
  menuPos = 1000;
  mlvl4 = 0;
  mlvl3 = 0;
  mlvl2 = 0;
  mlvl1 = 1;
}


bool get2ValFunc(String ValName, int &t1, int &t2, char x, int xT, char y, int yT) {
  menuUiFunc = 0;
  bool t1Selec = 1;
  bool t2Selec = 0;
  bool okSelec = 0;
  bool canSelec = 0;
  bool blinker = 0;
  int tVal1 = t1;
  int tVal2 = t2;
  int bpCounter = 0;
  unsigned long lastmillis = millis();
  unsigned long selfDestruct = millis();

  snprintf(lcd_buf1, sizeof(lcd_buf1), "                 ");
  snprintf(lcd_buf2, sizeof(lcd_buf2), "                 ");
  while (1) {
    wdt_reset();
    if (millis() - selfDestruct >= 30000) {
      mainMenuShow();
      return 0;
    }

    rotaryFunc();
    if (takeMenuPress()) {
      mainMenuShow();
      return 0;
    }

    // --- Increment / Decrement ---
    if (rotValPlus || rotValMinus) {
      menuLastInput = selfDestruct = millis();  // reset both idle timers on activity
      int delta = (rotValMinus ? 1 : -1);

      if (t1Selec) {
        //tVal1 = constrain(tVal1 + delta, 0, xT);
        tVal1 = (tVal1 + delta + (xT + 1)) % (xT + 1);
      }
      if (t2Selec) {
        //tVal2 = constrain(tVal2 + delta, 0, yT);
        tVal2 = (tVal2 + delta + (yT + 1)) % (yT + 1);
      }

      rotValPlus = rotValMinus = 0;
    }

    if (rotPush) {
      menuLastInput = selfDestruct = millis();  // reset both idle timers on activity
      if (t1Selec) {
        t1Selec = 0;
        t2Selec = 1;
        okSelec = 0;
        canSelec = 0;
      } else if (t2Selec) {
        t1Selec = 0;
        t2Selec = 0;
        okSelec = 1;
        canSelec = 0;
      } else if (okSelec) {
        t1Selec = 0;
        t2Selec = 0;
        okSelec = 0;
        canSelec = 1;
      } else if (canSelec) {
        t1Selec = 1;
        t2Selec = 0;
        okSelec = 0;
        canSelec = 0;
      }
      rotPush = 0;
    }

    if (millis() - lastmillis > 300) {
      blinker = !blinker;
      lastmillis = millis();

      if (t1Selec) {
        if (blinker) {
          snprintf(lcd_buf1, sizeof(lcd_buf1), "%s:   %c:%02d%c", ValName.c_str(), x, tVal2, y);
        } else {
          snprintf(lcd_buf1, sizeof(lcd_buf1), "%s: %02d%c:%02d%c", ValName.c_str(), tVal1, x, tVal2, y);
        }
      }
      if (t2Selec) {
        if (blinker) {
          snprintf(lcd_buf1, sizeof(lcd_buf1), "%s: %02d%c:  %c", ValName.c_str(), tVal1, x, y);
        } else {
          snprintf(lcd_buf1, sizeof(lcd_buf1), "%s: %02d%c:%02d%c", ValName.c_str(), tVal1, x, tVal2, y);
        }
      }

      if (okSelec) {
        if (blinker) {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "-> OK     CANCEL");  // arrow blinks on OK
        } else {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
        }
      }

      if (canSelec) {
        if (blinker) {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK  -> CANCEL");  // arrow blinks on CANCEL
        } else {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
        }
      }
    }

    if (!t1Selec && !t2Selec) {
      snprintf(lcd_buf1, sizeof(lcd_buf1), "%s: %02d%c:%02d%c", ValName.c_str(), tVal1, x, tVal2, y);
    }
    if (!okSelec && !canSelec) {
      snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
    }

    lcd_set_cursor(0, 0);
    lcd_print(lcd_buf1);
    lcd_set_cursor(1, 0);
    lcd_print(lcd_buf2);

    bpCounter = 0;
    while ((okSelec || canSelec) && isEnterButtonDown()) {
      wdt_reset();
      rotaryFunc();
      //Serial3.println(bpCounter);
      delay(5);
      bpCounter = bpCounter + 1;
      if (bpCounter >= BPCOUNTER) {
        if (okSelec) {
          lcd_set_cursor(1, 0);
          lcd_print("   Applying...  ");
          t1 = tVal1;
          t2 = tVal2;
          mainMenuShow();
          return 1;
        }
        if (canSelec) {
          lcd_set_cursor(1, 0);
          lcd_print(" X  Aborting  X ");
          mainMenuShow();
          return 0;
        }
      }
      // rotPush = 1;  // REMOVED: was wrongly advancing okSelec→canSelec every hold iteration
    }
  }
}



bool get1VaFloatFun(String ValName, float &v1, char x, float xT, float yT) {
  menuUiFunc = 0;
  bool v1Selec = 1;
  bool okSelec = 0;
  bool canSelec = 0;
  bool blinker = 0;
  float tVal1 = v1;  // pre-load caller's current value so editor opens at stored setting
  int bpCounter = 0;
  unsigned long lastmillis = millis();
  unsigned long selfDestruct = millis();

  snprintf(lcd_buf1, sizeof(lcd_buf1), "                 ");
  snprintf(lcd_buf2, sizeof(lcd_buf2), "                 ");
  while (1) {
    wdt_reset();
    if (millis() - selfDestruct >= 30000) {
      mainMenuShow();
      return 0;
    }

    rotaryFunc();
    if (takeMenuPress()) {
      mainMenuShow();
      return 0;
    }

    // --- Increment / Decrement ---
    if (rotValPlus || rotValMinus) {
      menuLastInput = selfDestruct = millis();  // reset both idle timers on activity
      float delta = (rotValMinus ? 0.10 : -0.10);

      if (v1Selec) {
        tVal1 += delta;

        if (tVal1 < xT) {
          tVal1 = yT;
        }
        if (tVal1 > yT) {
          tVal1 = xT;
        }
      }
      rotValPlus = rotValMinus = 0;
    }

    if (rotPush) {
      menuLastInput = selfDestruct = millis();  // reset both idle timers on activity
      if (v1Selec) {
        v1Selec = 0;
        okSelec = 1;
        canSelec = 0;
      } else if (okSelec) {
        v1Selec = 0;
        okSelec = 0;
        canSelec = 1;
      } else if (canSelec) {
        v1Selec = 1;
        okSelec = 0;
        canSelec = 0;
      }
      rotPush = 0;
    }

    if (millis() - lastmillis > 300) {
      blinker = !blinker;
      lastmillis = millis();

      if (v1Selec) {
        if (blinker) {
          snprintf(lcd_buf1, sizeof(lcd_buf1), "%s:       %c ", ValName.c_str(), x);
        } else {
          char bufx[8];
          // break float into integer and decimal parts// multiply by 100, round, then split
          int scaled = (int)round(tVal1 * 100);

          int intPart = scaled / 100;
          int fracPart = scaled % 100;
          snprintf(bufx, sizeof(bufx), "%02d.%02d", intPart, fracPart);
          snprintf(lcd_buf1, sizeof(lcd_buf1), "%s: %s %c ", ValName.c_str(), bufx, x);
        }
      }
      if (okSelec) {
        if (blinker) {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "-> OK     CANCEL");  // arrow blinks on OK
        } else {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
        }
      }

      if (canSelec) {
        if (blinker) {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK  -> CANCEL");  // arrow blinks on CANCEL
        } else {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
        }
      }
    }

    if (!v1Selec) {
      char bufx[8];
      // break float into integer and decimal parts
      int intPart = (int)tVal1;
      int fracPart = (int)((tVal1 - intPart) * 100 + 0.5);  // 2 decimals
      snprintf(bufx, sizeof(bufx), "%02d.%02d", intPart, fracPart);
      snprintf(lcd_buf1, sizeof(lcd_buf1), "%s: %s %c ", ValName.c_str(), bufx, x);
    }
    if (!okSelec && !canSelec) {
      snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
    }

    lcd_set_cursor(0, 0);
    lcd_print(lcd_buf1);
    lcd_set_cursor(1, 0);
    lcd_print(lcd_buf2);

    bpCounter = 0;
    while ((okSelec || canSelec) && isEnterButtonDown()) {
      wdt_reset();
      rotaryFunc();
      //Serial3.println(bpCounter);
      delay(5);
      bpCounter = bpCounter + 1;
      if (bpCounter >= BPCOUNTER) {

        if (okSelec) {
          lcd_set_cursor(1, 0);
          lcd_print("   Applying...  ");
          v1 = tVal1;
          mainMenuShow();
          return 1;
        }
        if (canSelec) {
          lcd_set_cursor(1, 0);
          lcd_print(" X  Aborting  X ");
          mainMenuShow();
          return 0;
        }
      }
      // rotPush = 1;  // REMOVED: was wrongly advancing okSelec→canSelec every hold iteration
    }
  }
}




bool get1ValFunc(String ValName, int &v1, char x, int xT, int yT) {
  menuUiFunc = 0;
  bool v1Selec = 1;
  bool okSelec = 0;
  bool canSelec = 0;
  bool blinker = 0;
  int tVal1 = v1;  // pre-load caller's current value so editor opens at stored setting
  int bpCounter = 0;
  unsigned long lastmillis = millis();
  unsigned long selfDestruct = millis();
  snprintf(lcd_buf1, sizeof(lcd_buf1), "                 ");
  snprintf(lcd_buf2, sizeof(lcd_buf2), "                 ");
  while (1) {
    wdt_reset();
    if (millis() - selfDestruct >= 30000) {
      mainMenuShow();
      return 0;
    }

    rotaryFunc();
    if (takeMenuPress()) {
      mainMenuShow();
      return 0;
    }

    // --- Increment / Decrement ---
    if (rotValPlus || rotValMinus) {
      menuLastInput = selfDestruct = millis();  // reset both idle timers on activity
      int delta = (rotValMinus ? 10 : -10);

      if (v1Selec) {
        tVal1 = tVal1 + delta;
        if (tVal1 < xT) {
          tVal1 = yT;
        }
        if (tVal1 > yT) {
          tVal1 = xT;
        }
      }
      rotValPlus = rotValMinus = 0;
    }

    if (rotPush) {
      menuLastInput = selfDestruct = millis();  // reset both idle timers on activity
      if (v1Selec) {
        v1Selec = 0;
        okSelec = 1;
        canSelec = 0;
      } else if (okSelec) {
        v1Selec = 0;
        okSelec = 0;
        canSelec = 1;
      } else if (canSelec) {
        v1Selec = 1;
        okSelec = 0;
        canSelec = 0;
      }
      rotPush = 0;
    }

    if (millis() - lastmillis > 300) {
      blinker = !blinker;
      lastmillis = millis();

      if (v1Selec) {
        if (blinker) {
          snprintf(lcd_buf1, sizeof(lcd_buf1), "%s:     %c  ", ValName.c_str(), x);
        } else {
          snprintf(lcd_buf1, sizeof(lcd_buf1), "%s: %03d %c  ", ValName.c_str(), tVal1, x);
        }
      }
      if (okSelec) {
        if (blinker) {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "-> OK     CANCEL");  // arrow blinks on OK
        } else {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
        }
      }

      if (canSelec) {
        if (blinker) {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK  -> CANCEL");  // arrow blinks on CANCEL
        } else {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
        }
      }
    }

    if (!v1Selec) {
      snprintf(lcd_buf1, sizeof(lcd_buf1), "%s: %03d %c  ", ValName.c_str(), tVal1, x);
    }
    if (!okSelec && !canSelec) {
      snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
    }

    lcd_set_cursor(0, 0);
    lcd_print(lcd_buf1);
    lcd_set_cursor(1, 0);
    lcd_print(lcd_buf2);

    bpCounter = 0;
    while ((okSelec || canSelec) && isEnterButtonDown()) {
      wdt_reset();
      rotaryFunc();
      //Serial3.println(bpCounter);
      delay(5);
      bpCounter = bpCounter + 1;
      if (bpCounter >= BPCOUNTER) {
        if (okSelec) {
          lcd_set_cursor(1, 0);
          lcd_print("   Applying...  ");
          v1 = tVal1;
          mainMenuShow();
          return 1;
        }
        if (canSelec) {
          lcd_set_cursor(1, 0);
          lcd_print(" X  Aborting  X ");
          mainMenuShow();
          return 0;
        }
      }
      // rotPush = 1;  // REMOVED: was wrongly advancing okSelec→canSelec every hold iteration
    }
  }
}


// Step-1 variant of get1ValFunc — used for relay latch (2–5 s, 1 s resolution)
bool get1ValFuncS1(String ValName, uint8_t &v1, char x, int xT, int yT) {
  menuUiFunc = 0;
  bool v1Selec = 1;
  bool okSelec = 0;
  bool canSelec = 0;
  bool blinker = 0;
  int tVal1 = (int)v1;
  int bpCounter = 0;
  unsigned long lastmillis = millis();
  unsigned long selfDestruct = millis();
  snprintf(lcd_buf1, sizeof(lcd_buf1), "                 ");
  snprintf(lcd_buf2, sizeof(lcd_buf2), "                 ");
  while (1) {
    wdt_reset();
    if (millis() - selfDestruct >= 30000) {
      mainMenuShow();
      return 0;
    }
    rotaryFunc();
    if (takeMenuPress()) {
      mainMenuShow();
      return 0;
    }
    if (rotValPlus || rotValMinus) {
      menuLastInput = selfDestruct = millis();  // reset both idle timers on activity
      int delta = (rotValMinus ? 1 : -1);
      if (v1Selec) {
        tVal1 = tVal1 + delta;
        if (tVal1 < xT) tVal1 = yT;
        if (tVal1 > yT) tVal1 = xT;
      }
      rotValPlus = rotValMinus = 0;
    }
    if (rotPush) {
      menuLastInput = selfDestruct = millis();  // reset both idle timers on activity
      if (v1Selec) {
        v1Selec = 0;
        okSelec = 1;
        canSelec = 0;
      } else if (okSelec) {
        v1Selec = 0;
        okSelec = 0;
        canSelec = 1;
      } else if (canSelec) {
        v1Selec = 1;
        okSelec = 0;
        canSelec = 0;
      }
      rotPush = 0;
    }
    if (millis() - lastmillis > 300) {
      blinker = !blinker;
      lastmillis = millis();
      if (v1Selec) {
        if (blinker) snprintf(lcd_buf1, sizeof(lcd_buf1), "%s:       %c ", ValName.c_str(), x);
        else snprintf(lcd_buf1, sizeof(lcd_buf1), "%s:  %02d  %c ", ValName.c_str(), tVal1, x);
      }
      if (okSelec) {
        snprintf(lcd_buf2, sizeof(lcd_buf2), blinker ? "-> OK     CANCEL" : "   OK     CANCEL");
      }
      if (canSelec) {
        snprintf(lcd_buf2, sizeof(lcd_buf2), blinker ? "   OK  -> CANCEL" : "   OK     CANCEL");
      }
    }
    if (!v1Selec) snprintf(lcd_buf1, sizeof(lcd_buf1), "%s:  %02d  %c ", ValName.c_str(), tVal1, x);
    if (!okSelec && !canSelec) snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
    lcd_set_cursor(0, 0);
    lcd_print(lcd_buf1);
    lcd_set_cursor(1, 0);
    lcd_print(lcd_buf2);
    bpCounter = 0;
    while ((okSelec || canSelec) && isEnterButtonDown()) {
      wdt_reset();
      rotaryFunc();
      delay(5);
      bpCounter++;
      if (bpCounter >= BPCOUNTER) {
        if (okSelec) {
          lcd_set_cursor(1, 0);
          lcd_print("   Applying...  ");
          v1 = (uint8_t)tVal1;
          mainMenuShow();
          return 1;
        }
        if (canSelec) {
          lcd_set_cursor(1, 0);
          lcd_print(" X  Aborting  X ");
          mainMenuShow();
          return 0;
        }
      }
    }
  }
}

static int maxMlvl2OffsetForGroup(int groupBase) {
  if (groupBase == 1000) return 700;  // MODES:        1100..1700
  if (groupBase == 2000) return 600;  // PRESET:       2100..2600
  if (groupBase == 3000) return 800;  // SETTINGS:     3100..3800 (factory reset removed)
  if (groupBase == 4000) return 100;  // CONTACT:      4100
  if (groupBase == 5000) return 100;  // ABOUT DEVICE: 5100
  if (groupBase == 6000) return 100;  // [HOME]:       6100
  return 100;
}

static int maxMlvl3OffsetForHundreds(int hundredsBase) {
  return 10;  // all branches: single leaf at x10
}

static int maxMlvl4OffsetForTens(int tensBase) {
  return 0;  // mlvl4 not used in current structure
}



// ── About Device: static info stored in flash (PROGMEM) ─────────────────────
// Strings live in flash only — zero RAM cost.
// __DATE__ is the compiler-injected build date: "Mmm DD YYYY" (always 11 chars).
static const char PROGMEM about_company[] = "HorizonGrid     ";  // 16 chars
static const char PROGMEM about_fwver[] = FIRMWARE_VERSION;      // e.g. "AVR128DB48"
static const char PROGMEM about_mfg_lbl[] = "Manufactured:   ";  // 16 chars
static const char PROGMEM about_mfg_date[] = __DATE__;           // "Mmm DD YYYY"

void menuUi() {
  static uint8_t aboutDevPage = 0;  // current About Device info page (0-1)
  static int prevMenuPos = -1;      // menuPos at end of previous menuUi() call

  if (!menuUiFunc) {
    menuLastInput = 0;
    return;
  }

  if (menuUiFunc) {
    rotaryFunc();
    unsigned long now = millis();

    if (menuLastInput == 0) {
      menuLastInput = now;
    }
    if (rotValPlus || rotValMinus || rotPush || btnMenuPress || btnModePress) {
      menuLastInput = now;
    }
    if ((now - menuLastInput) >= 30000UL) {
      menuUiFunc = 0;
      clearNavEvents();
      menuLastInput = 0;
      return;
    }

    // MODE key is reserved for home hotkey behavior.
    takeModePress();

    if (takeMenuPress()) {
      if (mlvl4) {
        mlvl4 = 0;
        mlvl3 = 1;
        menuPos = (menuPos / 10) * 10;
      } else if (mlvl3) {
        mlvl3 = 0;
        mlvl2 = 1;
        menuPos = (menuPos / 100) * 100;
      } else if (mlvl2) {
        mlvl2 = 0;
        mlvl1 = 1;
        menuPos = (menuPos / 1000) * 1000;
      } else {
        menuUiFunc = 0;
        return;
      }
    }

    if (mlvl1) {
      if (menuPos < 999 || menuPos > 6000) {
        menuPos = 1000;
        Serial3.print("after condition 1 - ");
        Serial3.println(menuPos);
      }
    }
    if (mlvl2) {
      int base = (menuPos / 1000) * 1000;
      int offset = menuPos % 1000;
      int maxOffset = maxMlvl2OffsetForGroup(base);
      if (offset <= 0 || offset > maxOffset || (offset % 100) != 0) {
        menuPos = base + 100;
        Serial3.print("after condition 2 - ");
        Serial3.println(menuPos);
      }
    }
    if (mlvl3) {
      int base = (menuPos / 100) * 100;
      int offset = menuPos % 100;
      int maxOffset = maxMlvl3OffsetForHundreds(base);
      if (offset <= 0 || offset > maxOffset || (offset % 10) != 0) {
        menuPos = base + 10;
        Serial3.print("after condition 3 - ");
        Serial3.println(menuPos);
      }
    }
    if (mlvl4) {
      int base = (menuPos / 10) * 10;
      int offset = menuPos % 10;
      int maxOffset = maxMlvl4OffsetForTens(base);
      if (offset < 0 || offset > maxOffset) {
        menuPos = base;
      }
    }


    // if (menuPos >= 1000 && menuPos < 3000) {
    //   menuPos = 1000;
    // else if (menuPos >= 2000 && menuPos < 3000) {
    //   menuPos = 2000;
    // } else if (menuPos >= 3000 && menuPos < 4000) {
    //   menuPos = 3000;
    // } else if (menuPos > 3000 || menuPos < 999) {
    //   menuPos = 1000;
    // }


    switch (menuPos) {
      case 1000:
        lcd_set_cursor(0, 0);
        lcd_print(" -> MODES       ");
        lcd_set_cursor(1, 0);
        lcd_print("    PRESET      ");
        break;
      case 1100:
        lcd_set_cursor(0, 0);
        lcd_print("-> SET NORMAL   ");
        lcd_set_cursor(1, 0);
        lcd_print("   SET AUTO     ");
        break;
      case 1110:
        storage.modeM1 = 0;
        markLocalModeChange();
        Serial3.print("menu mode normal: ");
        Serial3.println(storage.modeM1);
        savecon();
        loadModeVal();
        uiFunc(1);
        menuUiFunc = 0;
        lcd_modeShow();
        break;

      case 1200:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("   SET NORMAL   ");
        lcd_set_cursor(1, 0);
        lcd_print("-> SET AUTO     ");
        break;

      case 1210:
        var1 = String(storage.autoM1).substring(1, 3).toInt();
        var2 = String(storage.autoM1).substring(3, 5).toInt();
        if (get2ValFunc("OnDelay", var1, var2, 'm', 59, 's', 59)) {
          Serial3.println("applied");
          Serial3.println(var1);
          Serial3.println(var2);

          /////////////////////////////////////////////////////////////////
          /////////////////////////////////////////////////////////////////
          storage.modeM1 = 1;
          markLocalModeChange();
          // format into val with leading zeros, always 2 digits
          snprintf(tmpVal, sizeof(tmpVal), "z%02d%02d", var1, var2);
          strncpy(storage.autoM1, tmpVal, sizeof(storage.autoM1) - 1);
          storage.autoM1[sizeof(storage.autoM1) - 1] = '\0';  // ensure null-termination
          Serial3.print("autoM1: ");
          Serial3.println(storage.autoM1);
          iotSerial.println("<{\"AT\":{\"a1auDly\":\"" + String(storage.autoM1) + "\"}}>");
          delay(500);
          iotSerial.println("<{\"TS\":{\"n\":\"2\"}}>");
          Serial3.println("<{\"AT\":{\"a1auDly\":\"" + String(storage.autoM1) + "\"}}>");
          savecon();
          // Stop motor FIRST so the OFF-relay pulse starts before the countdown
          // timer is stamped — mirrors the fix in toggleNormalAutoMode().
          if (m1StaVars) {
            m1Off();
          }
          loadModeVal();  // reloads all mode vars from storage
          autoM1Trigd = 0;
          autoM1BuffMillis = millis();  // re-stamp AFTER m1Off() so delay is accurate
          atLastTrigdMillis = 0;
          uiFunc(1);
          menuUiFunc = 0;
          lcd_modeShow();
          /////////////////////////////////////////////////////////////////
          /////////////////////////////////////////////////////////////////

        } else {
          Serial3.println("failed");
          Serial3.println(var1);
          Serial3.println(var2);
        }
        break;

      case 1300:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("-> SET CYCLIC   ");
        lcd_set_cursor(1, 0);
        lcd_print("   SET COUNTDOWN");
        break;
      case 1310:
        var1 = String(storage.cyclicM1).substring(1, 3).toInt();
        var2 = String(storage.cyclicM1).substring(3, 5).toInt();
        if (get2ValFunc(" On Dur", var1, var2, 'h', 23, 'm', 59)) {
          Serial3.println("applied");
          Serial3.println(var1);
          Serial3.println(var2);
          /////////////////////////////////////////////////////////////////
          /////////////////////////////////////////////////////////////////
          // Build the ON-duration portion of the setting string but do NOT
          // commit storage.modeM1 yet — the user must also confirm the OFF
          // duration before the mode change takes effect.
          snprintf(tmpVal, sizeof(tmpVal), "z%02d%02d", var1, var2);
          Serial3.println(tmpVal);
          delay(2000);
          /////////////////////////////////////////////////////////////////
          /////////////////////////////////////////////////////////////////
          var1 = String(storage.cyclicM1).substring(5, 7).toInt();
          var2 = String(storage.cyclicM1).substring(7, 9).toInt();
          if (get2ValFunc("Off Dur", var1, var2, 'h', 23, 'm', 59)) {
            Serial3.println("applied");
            /////////////////////////////////////////////////////////////////
            /////////////////////////////////////////////////////////////////
            // Both durations confirmed — now commit mode and settings.
            storage.modeM1 = 2;
            markLocalModeChange();
            snprintf(&tmpVal[5], sizeof(tmpVal) - 5, "%02d%02d", var1, var2);
            Serial3.println(tmpVal);
            strncpy(storage.cyclicM1, tmpVal, sizeof(storage.cyclicM1) - 1);
            storage.cyclicM1[sizeof(storage.cyclicM1) - 1] = '\0';  // ensure null-termination
            savecon();
            iotSerial.println("<{\"AT\":{\"a1cyc\":\"" + String(storage.cyclicM1) + "\"}}>");
            delay(500);
            iotSerial.println("<{\"TS\":{\"n\":\"4\"}}>");
            Serial3.println("<{\"AT\":{\"a1cyc\":\"" + String(storage.cyclicM1) + "\"}}>");
            loadModeVal();
            uiFunc(1);
            menuUiFunc = 0;
            lcd_modeShow();
            /////////////////////////////////////////////////////////////////
            /////////////////////////////////////////////////////////////////
          } else {
            Serial3.println("failed");
          }
        } else {
          Serial3.println("failed");
        }
        break;

      case 1400:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("   SET CYCLIC   ");
        lcd_set_cursor(1, 0);
        lcd_print("-> SET COUNTDOWN");
        break;

      case 1410:
        var1 = String(storage.countM1).substring(1, 3).toInt();
        var2 = String(storage.countM1).substring(3, 5).toInt();
        if (get2ValFunc("CounTmr", var1, var2, 'h', 23, 'm', 59)) {
          Serial3.println("applied");
          Serial3.println(var1);
          Serial3.println(var2);
          storage.modeM1 = 3;
          markLocalModeChange();
          //////////////////////////////
          /////////////////////////////
          // format into val with leading zeros, always 2 digits
          snprintf(tmpVal, sizeof(tmpVal), "z%02d%02d", var1, var2);
          strncpy(storage.countM1, tmpVal, sizeof(storage.countM1) - 1);
          storage.countM1[sizeof(storage.countM1) - 1] = '\0';  // ensure null-termination
          Serial3.print("a1Ctmr: ");
          Serial3.println(storage.countM1);
          savecon();
          iotSerial.println("<{\"AT\":{\"a1cTmr\":\"" + String(storage.countM1) + "\"}}>");
          delay(500);
          iotSerial.println("<{\"TS\":{\"n\":\"3\"}}>");
          Serial3.println("<{\"AT\":{\"a1cTmr\":\"" + String(storage.countM1) + "\"}}>");
          loadModeVal();
          uiFunc(1);
          menuUiFunc = 0;
          lcd_modeShow();
        } else {
          Serial3.println("failed");
          mainMenuShow();  // return to main menu when countdown hotkey is cancelled
        }
        break;
      case 1500:
        lcd_set_cursor(0, 0);
        lcd_print("-> SET SCHEDULE ");
        lcd_set_cursor(1, 0);
        lcd_print("   SET LAST REM ");
        break;

      case 1510:
        // Schedule mode: show info message, set mode=4, save, exit
        storage.modeM1 = 4;
        markLocalModeChange();
        savecon();
        loadModeVal();
        iotSerial.println("<{\"TS\":{\"n\":\"6\"}}>");
        lcd_set_cursor(0, 0);
        lcd_print("To set schedule,");
        lcd_set_cursor(1, 0);
        lcd_print("please use app. ");
        uiFunc(1);        // hold "please use app" message for 3 s non-blocking
        menuUiFunc = 0;
        break;

      case 1600:
        lcd_set_cursor(0, 0);
        lcd_print("   SET SCHEDULE ");
        lcd_set_cursor(1, 0);
        lcd_print("-> SET LAST REM ");
        break;

      case 1610:
        var1 = String(storage.lasRemM1).substring(1, 3).toInt();
        var2 = String(storage.lasRemM1).substring(3, 5).toInt();
        if (get2ValFunc("OnDelay", var1, var2, 'm', 59, 's', 59)) {
          Serial3.println("lastrem applied");
          storage.modeM1 = 5;
          markLocalModeChange();
          snprintf(tmpVal, sizeof(tmpVal), "z%02d%02d", var1, var2);
          strncpy(storage.lasRemM1, tmpVal, sizeof(storage.lasRemM1) - 1);
          storage.lasRemM1[sizeof(storage.lasRemM1) - 1] = '\0';
          Serial3.print("lasRemM1: ");
          Serial3.println(storage.lasRemM1);
          iotSerial.println("<{\"AT\":{\"a1asRem\":\"" + String(storage.lasRemM1) + "\"}}>");
          delay(500);
          iotSerial.println("<{\"TS\":{\"n\":\"5\"}}>");
          savecon();
          loadModeVal();
          uiFunc(1);
          menuUiFunc = 0;
          lcd_modeShow();
        } else {
          Serial3.println("lastrem failed");
        }
        break;

      case 1700:
        lcd_set_cursor(0, 0);
        lcd_print("   SET LAST REM ");
        lcd_set_cursor(1, 0);
        lcd_print("-> [ BACK ]     ");
        break;

      case 1710:
        menuUiFunc = 0;
        uiFunc(0);
        return;
        break;

      case 2000:
        lcd_set_cursor(0, 0);
        lcd_print("    MODES       ");
        lcd_set_cursor(1, 0);
        lcd_print(" -> PRESET      ");
        break;

      case 2100:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("->SET DRY RUN Ah");
        lcd_set_cursor(1, 0);
        lcd_print("  SET OVRLOAD Ah");
        break;
      case 2110:
        var3 = storage.dryRunM1;  // pre-load current stored value
        if (get1VaFloatFun("DryRun ", var3, 'A', 2.0, 15.0)) {
          storage.dryRunM1 = var3;
          savecon();
          iotSerial.println("<{\"AT\":{\"a1dry\":\"z" + String(storage.dryRunM1) + "\"}}>");
          Serial3.println("<{\"AT\":{\"a1dry\":\"z" + String(storage.dryRunM1) + "\"}}>");
          loadModeVal();
          uiFunc(1);
          menuUiFunc = 0;
          lcd_modeShow();
        }
        break;

      case 2200:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("  SET DRY RUN Ah");
        lcd_set_cursor(1, 0);
        lcd_print("->SET OVRLOAD Ah");
        break;
      case 2210:
        var3 = storage.ovLRunM1;  // pre-load current stored value
        if (get1VaFloatFun("OverRun", var3, 'A', 5.0, 25.0)) {
          storage.ovLRunM1 = var3;
          iotSerial.println("<{\"AT\":{\"a1ovl\":\"z" + String(storage.ovLRunM1) + "\"}}>");
          Serial3.println("<{\"AT\":{\"a1ovl\":\"z" + String(storage.ovLRunM1) + "\"}}>");
          savecon();
          loadModeVal();
          uiFunc(1);
          menuUiFunc = 0;
          lcd_modeShow();
        }
        break;
      case 2300:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("->SET Under Volt");
        lcd_set_cursor(1, 0);
        lcd_print("  SET Over  Volt");
        break;
      case 2310:
        var1 = (int)storage.undVol;  // pre-load current stored value
        if (get1ValFunc("UnderVol", var1, 'V', 280, 350)) {
          storage.undVol = var1;
          iotSerial.println("<{\"AT\":{\"lvTh\":\"z" + String(storage.undVol) + "\"}}>");
          Serial3.println("<{\"AT\":{\"lvTh\":\"z" + String(storage.undVol) + "\"}}>");
          savecon();
          loadModeVal();
          uiFunc(1);
          menuUiFunc = 0;
          lcd_modeShow();
        }
        break;
      case 2400:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("  SET Under Volt");
        lcd_set_cursor(1, 0);
        lcd_print("->SET Over  Volt");
        break;
      case 2410:
        var1 = (int)storage.ovrVol;  // pre-load current stored value
        if (get1ValFunc("OverVol", var1, 'V', 400, 500)) {
          storage.ovrVol = var1;
          iotSerial.println("<{\"AT\":{\"hvTh\":\"z" + String(storage.ovrVol) + "\"}}>");
          Serial3.println("<{\"AT\":{\"hvTh\":\"z" + String(storage.ovrVol) + "\"}}>");
          savecon();
          loadModeVal();
          uiFunc(1);
          menuUiFunc = 0;
          lcd_modeShow();
        }
        break;
      case 2500:
        lcd_set_cursor(0, 0);
        lcd_print("-> SENSE TIME   ");
        lcd_set_cursor(1, 0);
        lcd_print("   [ BACK ]     ");
        break;
      case 2510:
        {
          var1 = (int)storage.senseTime;
          if (get1ValFunc("SensTim", var1, 's', 10, 120)) {
            storage.senseTime = (uint16_t)var1;
            savecon();
            uiFunc(1);
            menuUiFunc = 0;
            lcd_modeShow();
          }
          break;
        }
      case 2600:
        lcd_set_cursor(0, 0);
        lcd_print("   SENSE TIME   ");
        lcd_set_cursor(1, 0);
        lcd_print("-> [ BACK ]     ");
        break;
      case 2610:
        menuUiFunc = 0;
        uiFunc(0);
        return;
        break;


      case 3000:
        lcd_set_cursor(0, 0);
        lcd_print(" -> SETTINGS    ");
        lcd_set_cursor(1, 0);
        lcd_print("    CONTACT     ");
        break;

      case 4000:
        lcd_set_cursor(0, 0);
        lcd_print("    SETTINGS    ");
        lcd_set_cursor(1, 0);
        lcd_print(" -> CONTACT     ");
        break;

      case 5000:
        lcd_set_cursor(0, 0);
        lcd_print(" -> ABOUT DVC   ");
        lcd_set_cursor(1, 0);
        lcd_print("    [ HOME ]    ");
        break;

      // ── SETTINGS mlvl2 rows ────────────────────────────────
      case 3100:
        lcd_set_cursor(0, 0);
        lcd_print("-> DATE & TIME  ");
        lcd_set_cursor(1, 0);
        lcd_print("   CONFIGURE DEV");
        break;
      case 3200:
        lcd_set_cursor(0, 0);
        lcd_print("   DATE & TIME  ");
        lcd_set_cursor(1, 0);
        lcd_print("-> CONFIGURE DEV");
        break;
      case 3300:
        lcd_set_cursor(0, 0);
        lcd_print("-> RESET WIFI   ");
        lcd_set_cursor(1, 0);
        lcd_print("   ON RELAY LAT ");
        break;
      case 3400:
        lcd_set_cursor(0, 0);
        lcd_print("   RESET WIFI   ");
        lcd_set_cursor(1, 0);
        lcd_print("-> ON RELAY LAT ");
        break;
      case 3500:
        lcd_set_cursor(0, 0);
        lcd_print("-> OFF RELAY LAT");
        lcd_set_cursor(1, 0);
        lcd_print("   AI MODE      ");
        break;
      case 3600:
        lcd_set_cursor(0, 0);
        lcd_print("   OFF RELAY LAT");
        lcd_set_cursor(1, 0);
        lcd_print("-> AI MODE      ");
        break;
      case 3700:
        lcd_set_cursor(0, 0);
        lcd_print("   AI MODE      ");
        lcd_set_cursor(1, 0);
        lcd_print("-> RESET DEVICE ");
        break;
      case 3800:
        lcd_set_cursor(0, 0);
        lcd_print("   RESET DEVICE ");
        lcd_set_cursor(1, 0);
        lcd_print("-> [ BACK ]     ");
        break;

      // ── SETTINGS mlvl3: DATE & TIME leaf ──────────────────
      case 3110:
        {
          // HH:MM — get2ValFunc, step-1, range 00-23 / 00-59
          int rtcHH = constrain((int)tHrs, 0, 23);
          int rtcMM = constrain((int)tMin, 0, 59);
          if (!get2ValFunc(" Time ", rtcHH, rtcMM, 'h', 23, 'm', 59)) break;

          // Seconds — get1ValFuncS1, step-1, range 00-59
          uint8_t rtcSS = (uint8_t)constrain((int)tSec, 0, 59);
          if (!get1ValFuncS1("  Secs", rtcSS, 's', 0, 59)) break;

          // Date — get1ValFuncS1, step-1, range 01-31 (no zero day)
          uint8_t rtcDD = (uint8_t)constrain((int)tDate, 1, 31);
          if (!get1ValFuncS1("  Date", rtcDD, 'd', 1, 31)) break;

          // Month — get1ValFuncS1, step-1, range 01-12 (no zero month)
          uint8_t rtcMON = (uint8_t)constrain((int)tMonth, 1, 12);
          if (!get1ValFuncS1(" Month", rtcMON, 'm', 1, 12)) break;

          // Year — get1ValFunc, step-10, range 00-99, default 26 (= 2026)
          // Accept 25-50 as valid (2025-2050); catches tYear=99 firmware-init and tYear=0 fresh RTC
          int rtcYY = (int)tYear;
          if (rtcYY < 25 || rtcYY > 50) rtcYY = 26;
          if (!get1ValFunc("  Year", rtcYY, 'y', 0, 99)) break;

          ds3231_SetDateTime(rtcYY, (int)rtcMON, (int)rtcDD, rtcHH, rtcMM, (int)rtcSS, 1);
          rtcFetchTimeFunc();
          lcd_set_cursor(0, 0);
          lcd_print("  RTC Updated!  ");
          lcd_set_cursor(1, 0);
          lcd_print("                ");
          wdt_reset();
          delay(2000);
          uiFunc(1);
          menuUiFunc = 0;
          break;
        }

      // ── SETTINGS mlvl3: CONFIGURE DEVICE sub-menu ─────────
      case 3210:
        {
          // 4-item scrolling sub-menu under "CONFIGURE DEV"
          static const char *const cfgLabels[4] = {
            "-> PAIR REMOTE  ",
            "-> PAIR GATEWAY ",
            "-> CONFIG WIFI  ",
            "-> [BACK]       "
          };
          uint8_t cfgSel = 0;
          const uint8_t CFG_N = 4;
          unsigned long cfgTo = millis();

          while (1) {
            wdt_reset();
            rotaryFunc();

            if (millis() - cfgTo >= 30000UL) {
              mainMenuShow();
              break;
            }
            if (takeMenuPress()) {
              mainMenuShow();
              break;
            }

            if (rotValPlus) {
              cfgSel = (cfgSel + 1) % CFG_N;
              rotValPlus = 0;
              cfgTo = millis();
            }
            if (rotValMinus) {
              cfgSel = (cfgSel == 0 ? CFG_N - 1 : cfgSel - 1);
              rotValMinus = 0;
              cfgTo = millis();
            }

            lcd_set_cursor(0, 0);
            lcd_print("CONFIGURE DEV   ");
            lcd_set_cursor(1, 0);
            lcd_print(cfgLabels[cfgSel]);

            if (!isEnterButtonDown()) {
              delay(10);
              continue;
            }
            // Debounce: hold 500 ms
            {
              unsigned long _ps = millis();
              while (millis() - _ps < 500UL) {
                wdt_reset();
                delay(5);
              }
            }
            if (!isEnterButtonDown()) continue;

            // ── PAIR REMOTE ──────────────────────────────────────────
            if (cfgSel == 0) {
              storage.remote[0] = '\0';
              storage.rf_locked = 1;
              savecon();
              enterRemPairMode();  // LCD → "REM PAIRING... scanning..."
              {
                unsigned long pairStart = millis();
                while (pairRemState != PAIR_REM_IDLE) {
                  wdt_reset();
                  sx1268Func();
                  pairRemoteTick();
                  if (takeMenuPress()) {
                    switchToOperationalChannel();
                    pairRemState = PAIR_REM_IDLE;
                    break;
                  }
                  if (millis() - pairStart >= 90000UL) {
                    switchToOperationalChannel();
                    pairRemState = PAIR_REM_IDLE;
                    break;
                  }
                  delay(5);
                }
              }
              wdt_reset();
              delay(2000);
              mainMenuShow();
              break;

              // ── PAIR GATEWAY (independent — remote unaffected) ───────
            } else if (cfgSel == 1) {
              storage.gateway[0] = '\0';  // clear gateway only; remote stays intact
              storage.rf_locked = 0;
              savecon();
              enterPairNodeMode();  // LCD → "GW  PAIRING... beaconing..."
              {
                unsigned long pairStart = millis();
                bool cancelled = false;
                while (pairNodeState != PAIR_NODE_IDLE) {
                  wdt_reset();
                  sx1268Func();
                  pairNodeTick();
                  if (takeMenuPress()) {
                    cancelled = true;
                    break;
                  }
                  if (millis() - pairStart >= 90000UL) {
                    cancelled = true;
                    break;
                  }
                  delay(5);
                }
                if (cancelled) {
                  switchToOperationalChannel();
                  pairNodeState = PAIR_NODE_IDLE;
                  wdt_reset();
                  delay(1000);
                  mainMenuShow();
                  break;
                }
              }
              wdt_reset();
              delay(2000);
              mainMenuShow();
              break;

              // ── CONFIG WIFI (send CP then reset ESP32) ───────────────
            } else if (cfgSel == 2) {
              lcd_set_cursor(0, 0);
              lcd_print(" Config WiFi... ");
              lcd_set_cursor(1, 0);
              lcd_print("  Please wait   ");
              iotSerial.println("<{\"TS\":{\"n\":\"CP\"}}>");
              Serial3.println("[MENU] Config WiFi sent");
              wdt_reset();
              delay(2000);
              iotSerial.println("@rst");           // restart ESP32 to apply new config
              Serial3.println("[MENU] ESP32 @rst sent");
              wdt_reset();
              delay(1000);
              mainMenuShow();
              break;

              // ── [BACK] ───────────────────────────────────────────────
            } else {
              mainMenuShow();
              break;
            }
          }
          break;
        }

      // ── SETTINGS mlvl3: RESET WIFI confirmation ───────────
      case 3310:
        {
          bool wfYes = false;
          bool wfBlink = false;
          unsigned long wfLast = millis();
          unsigned long wfTimeout = millis();
          lcd_set_cursor(0, 0);
          lcd_print("  Reset WiFi?   ");
          while (1) {
            wdt_reset();
            rotaryFunc();
            if (millis() - wfTimeout >= 30000UL) {
              mainMenuShow();
              break;
            }
            if (takeMenuPress()) {
              mainMenuShow();
              break;
            }
            if (rotValPlus || rotValMinus) {
              wfYes = !wfYes;
              rotValPlus = rotValMinus = 0;
            }
            if (millis() - wfLast >= 400) {
              wfBlink = !wfBlink;
              wfLast = millis();
            }
            lcd_set_cursor(1, 0);
            lcd_print(wfYes ? (wfBlink ? "< NO  -> YES >  " : "< NO     YES >  ")
                            : (wfBlink ? "< NO <-  YES >  " : "< NO     YES >  "));
            if (isEnterButtonDown()) {
              {
                unsigned long _ps = millis();
                while (millis() - _ps < 600) {
                  wdt_reset();
                  rotaryFunc();
                  delay(5);
                }
              }
              if (isEnterButtonDown()) {
                if (wfYes) {
                  lcd_set_cursor(0, 0);
                  lcd_print("  WiFi Reset!   ");
                  lcd_set_cursor(1, 0);
                  lcd_print("                ");
                  iotSerial.println("<{\"TS\":{\"n\":\"RW\"}}>");
                  Serial3.println("[MENU] WiFi Reset sent");
                  wdt_reset();
                  delay(2000);
                }
                mainMenuShow();
                break;
              }
            }
          }
          break;
        }

      // ── SETTINGS mlvl3: ON RELAY LATCH editor ─────────────
      case 3410:
        if (get1ValFuncS1("ON Lat ", storage.onRelay, 's', 2, 5)) {
          savecon();
          uiFunc(1);
          menuUiFunc = 0;
          lcd_modeShow();
        }
        break;

      // ── SETTINGS mlvl3: OFF RELAY LATCH editor ────────────
      case 3510:
        if (get1ValFuncS1("OFF Lat", storage.offRelay, 's', 2, 5)) {
          savecon();
          uiFunc(1);
          menuUiFunc = 0;
          lcd_modeShow();
        }
        break;

      // ── SETTINGS mlvl3: AI MODE leaf ──────────────────────
      case 3610:
        lcd_set_cursor(0, 0);
        lcd_print("  AI Calibrate  ");
        lcd_set_cursor(1, 0);
        lcd_print(" Use Mobile App ");
        wdt_reset();
        delay(3000);
        mainMenuShow();
        break;

      // ── SETTINGS mlvl3: RESET DEVICE confirmation ────────
      case 3710:
        {
          bool rdYes = false;
          bool rdBlink = false;
          unsigned long rdLast = millis();
          unsigned long rdTimeout = millis();
          lcd_set_cursor(0, 0);
          lcd_print(" Reset Device?  ");
          while (1) {
            wdt_reset();
            rotaryFunc();
            if (millis() - rdTimeout >= 30000UL) {
              mainMenuShow();
              break;
            }
            if (takeMenuPress()) {
              mainMenuShow();
              break;
            }
            if (rotValPlus || rotValMinus) {
              rdYes = !rdYes;
              rotValPlus = rotValMinus = 0;
            }
            if (millis() - rdLast >= 400) {
              rdBlink = !rdBlink;
              rdLast = millis();
            }
            lcd_set_cursor(1, 0);
            lcd_print(rdYes ? (rdBlink ? "< NO  -> YES >  " : "< NO     YES >  ")
                            : (rdBlink ? "< NO <-  YES >  " : "< NO     YES >  "));
            if (isEnterButtonDown()) {
              {
                unsigned long _ps = millis();
                while (millis() - _ps < 600) {
                  wdt_reset();
                  rotaryFunc();
                  delay(5);
                }
              }
              if (isEnterButtonDown()) {
                if (rdYes) {
                  lcd_set_cursor(0, 0);
                  lcd_print("  RESETTING...  ");
                  lcd_set_cursor(1, 0);
                  lcd_print("                ");
                  storage.modeM1 = 0;
                  markLocalModeChange();
                  Serial3.println("[MENU] Reset Device");
                  savecon();
                  loadModeVal();
                  wdt_reset();
                  delay(2000);
                  uiFunc(1);
                  menuUiFunc = 0;
                  lcd_modeShow();
                } else {
                  mainMenuShow();
                }
                break;
              }
            }
          }
          break;
        }

      // ── SETTINGS mlvl3: [BACK] leaf ───────────────────────
      case 3810:
        menuUiFunc = 0;
        uiFunc(0);
        return;
        break;

      case 6000:
        lcd_set_cursor(0, 0);
        lcd_print("    ABOUT DVC   ");
        lcd_set_cursor(1, 0);
        lcd_print(" -> [ HOME ]    ");
        break;
      case 6100:
        menuUiFunc = 0;
        uiFunc(0);
        return;
        break;

      // ── CONTACT main menu group ────────────────────────────
      case 4100:
        lcd_set_cursor(0, 0);
        lcd_print("Contact:        ");
        lcd_set_cursor(1, 0);
        lcd_print("+91 99626 78788 ");
        // Stays on screen; MENU/ESC handled by back-press logic above
        break;
      case 4110:
        // ENTER from CONTACT screen → return to main menu
        mainMenuShow();
        break;

      // ── ABOUT DEVICE main menu group ──────────────────────
      case 5100:
        // Reset page so info display always starts at page 0 on each entry.
        aboutDevPage = 0;
        lcd_set_cursor(0, 0);
        lcd_print("-> SHOW INFO    ");
        lcd_set_cursor(1, 0);
        lcd_print("                ");
        break;

      case 5110:
        {
          // Non-blocking multi-page About Device info display.
          // All strings are read from PROGMEM (flash) — no RAM overhead.
          //
          // Page 0 : Company name + Firmware version (from PROGMEM)
          // Page 1 : Manufacture date = compiler __DATE__ (from PROGMEM)
          //
          // Rotate pages with rotary knob (UP / DOWN / push all cycle).
          // MENU back-press is handled by the top-level handler above and
          // navigates back to case 5100 normally.
          // The 30 s idle timeout in menuUi() acts as the overall exit guard.
          const uint8_t ABOUT_PAGES = 2;

          // Detect fresh entry
          bool isFirstEntry = (prevMenuPos != 5110);
          if (isFirstEntry) aboutDevPage = 0;

          // Consume rotary events so rotaryVal() does NOT advance menuPos.
          bool pageChanged = isFirstEntry;
          if (rotValPlus) {
            rotValPlus = 0;
            aboutDevPage = (aboutDevPage + 1) % ABOUT_PAGES;
            pageChanged = true;
          }
          if (rotValMinus) {
            rotValMinus = 0;
            aboutDevPage = (aboutDevPage + ABOUT_PAGES - 1) % ABOUT_PAGES;
            pageChanged = true;
          }
          if (rotPush) {
            rotPush = 0;
            aboutDevPage = (aboutDevPage + 1) % ABOUT_PAGES;
            pageChanged = true;
          }

          if (pageChanged) {
            char pmBuf[17];  // scratch buffer for PROGMEM → RAM copy
            char b[17];      // formatted LCD line

            switch (aboutDevPage) {

              case 0:  // ── Company + Firmware version ────────────────────────
                // Row 0: company name (stored in flash)
                strncpy_P(pmBuf, about_company, 16);
                pmBuf[16] = '\0';
                lcd_set_cursor(0, 0);
                lcd_print(pmBuf);

                // Row 1: "FW: <version>" padded to 16 chars
                strncpy_P(pmBuf, about_fwver, 16);
                pmBuf[16] = '\0';
                snprintf(b, sizeof(b), "FW: %-12s", pmBuf);
                lcd_set_cursor(1, 0);
                lcd_print(b);
                break;

              case 1:  // ── Manufacture date (compile-time __DATE__) ─────────
                // Row 0: label (stored in flash)
                strncpy_P(pmBuf, about_mfg_lbl, 16);
                pmBuf[16] = '\0';
                lcd_set_cursor(0, 0);
                lcd_print(pmBuf);

                // Row 1: date string padded to 16 chars, e.g. "Mar 13 2026     "
                strncpy_P(pmBuf, about_mfg_date, 16);
                pmBuf[16] = '\0';
                snprintf(b, sizeof(b), "%-16s", pmBuf);
                lcd_set_cursor(1, 0);
                lcd_print(b);
                break;
            }
          }
          // Stays here every loop until MENU is pressed or idle timeout fires.
          break;
        }

      default:
        menuPos = 1000;
        mlvl4 = 0;
        mlvl3 = 0;
        mlvl2 = 0;
        mlvl1 = 1;
        break;
    }
    prevMenuPos = menuPos;  // record for isFirstEntry detection next call
  }
}
