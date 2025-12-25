#include "Arduino.h"
bool mlvl1 = 1;
bool mlvl2 = 0;
bool mlvl3 = 0;
bool mlvl4 = 0;


//bool menuUiFunc = 0;
int menuPos = 1000;


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
  delay(2000);
  menuUiFunc = 1;
  menuPos = 0;
  mlvl4 = 1;
  mlvl3 = 0;
  mlvl2 = 0;
  mlvl1 = 0;
}


bool get2ValFunc(String ValName, int &t1, int &t2, char x, int xT, char y, int yT) {
  menuUiFunc = 0;
  bool t1Selec = 1;
  bool t2Selec = 0;
  bool okSelec = 0;
  bool canSelec = 0;
  bool blinker = 0;
  int tVal1 = 0;
  int tVal2 = 0;
  int bpCounter = 0;
  unsigned long lastmillis = millis();
  unsigned long selfDestruct = millis();

  snprintf(lcd_buf1, sizeof(lcd_buf1), "                 ");
  snprintf(lcd_buf2, sizeof(lcd_buf2), "                 ");
  while (1) {
    wdt_reset();
    if (millis() - selfDestruct >= 300000) {
      mainMenuShow();
      return 0;
    }

    rotaryFunc();

    // --- Increment / Decrement ---
    if (rotValPlus || rotValMinus) {
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
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
        } else {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "          CANCEL");
        }
      }

      if (canSelec) {
        if (blinker) {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
        } else {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK           ");
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
    while ((okSelec || canSelec) && digitalRead(PIN_RSW) == LOW) {
      wdt_reset();
      //Serial3.println(bpCounter);
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
      rotPush = 1;
    }
  }
}



bool get1VaFloatFun(String ValName, float &v1, char x, float xT, float yT) {
  menuUiFunc = 0;
  bool v1Selec = 1;
  bool okSelec = 0;
  bool canSelec = 0;
  bool blinker = 0;
  float tVal1 = 01.0;
  int bpCounter = 0;
  unsigned long lastmillis = millis();
  unsigned long selfDestruct = millis();

  snprintf(lcd_buf1, sizeof(lcd_buf1), "                 ");
  snprintf(lcd_buf2, sizeof(lcd_buf2), "                 ");
  while (1) {
    wdt_reset();
    if (millis() - selfDestruct >= 300000) {
      mainMenuShow();
      return 0;
    }

    rotaryFunc();

    // --- Increment / Decrement ---
    if (rotValPlus || rotValMinus) {
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
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
        } else {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "          CANCEL");
        }
      }

      if (canSelec) {
        if (blinker) {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
        } else {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK           ");
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
    while ((okSelec || canSelec) && digitalRead(PIN_RSW) == LOW) {
      wdt_reset();
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
      rotPush = 1;
    }
  }
}




bool get1ValFunc(String ValName, int &v1, char x, int xT, int yT) {
  menuUiFunc = 0;
  bool v1Selec = 1;
  bool okSelec = 0;
  bool canSelec = 0;
  bool blinker = 0;
  int tVal1 = 0;
  int bpCounter = 0;
  unsigned long lastmillis = millis();
  unsigned long selfDestruct = millis();
  snprintf(lcd_buf1, sizeof(lcd_buf1), "                 ");
  snprintf(lcd_buf2, sizeof(lcd_buf2), "                 ");
  while (1) {
    wdt_reset();
    if (millis() - selfDestruct >= 300000) {
      mainMenuShow();
      return 0;
    }

    rotaryFunc();

    // --- Increment / Decrement ---
    if (rotValPlus || rotValMinus) {
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
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
        } else {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "          CANCEL");
        }
      }

      if (canSelec) {
        if (blinker) {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK     CANCEL");
        } else {
          snprintf(lcd_buf2, sizeof(lcd_buf2), "   OK           ");
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
    while ((okSelec || canSelec) && digitalRead(PIN_RSW) == LOW) {
      wdt_reset();
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
      rotPush = 1;
    }
  }
}




void menuUi() {

  if (menuUiFunc) {
    if (mlvl1) {
      if (menuPos < 999 || menuPos > 4000) {
        menuPos = 1000;
        Serial3.print("after condition 1 - ");
        Serial3.println(menuPos);
      }
    }
    if (mlvl2) {
      if (menuPos % 1000 <= 0 || menuPos % 1000 > 600) {
        menuPos = (menuPos / 1000) * 1000 + 100;
        Serial3.print("after condition 2 - ");
        Serial3.println(menuPos);
      }
    }
    if (mlvl3) {
      if (menuPos % 100 < 0 || menuPos % 100 > 70) {
        menuPos = (menuPos / 100) * 100 + 10;
        Serial3.print("after condition 3 - ");
        Serial3.println(menuPos);
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
        //d_print("1234567890123456");
        lcd_print(" -> MODE        ");
        lcd_set_cursor(1, 0);
        lcd_print("    PRESET      ");
        break;
      case 1700:
      case 1100:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("-> SET NORMAL   ");
        lcd_set_cursor(1, 0);
        lcd_print("   SET AUTO     ");
        break;
      case 1110:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print(" mode change to ");
        lcd_set_cursor(1, 0);
        lcd_print("     Normal     ");
        wdt_reset();
        delay(3000);
        storage.modeM1 = 0;
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
        var1 = 0;
        var2 = 0;
        if (get2ValFunc("OnDelay", var1, var2, 'm', 59, 's', 59)) {
          Serial3.println("applied");
          Serial3.println(var1);
          Serial3.println(var2);

          /////////////////////////////////////////////////////////////////
          /////////////////////////////////////////////////////////////////
          storage.modeM1 = 1;
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
          loadModeVal();
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
        var1 = 0;
        var2 = 0;
        if (get2ValFunc(" On Dur", var1, var2, 'h', 23, 'm', 59)) {
          Serial3.println("applied");
          Serial3.println(var1);
          Serial3.println(var2);
          /////////////////////////////////////////////////////////////////
          /////////////////////////////////////////////////////////////////
          storage.modeM1 = 2;
          // format into val with leading zeros, always 2 digits
          snprintf(tmpVal, sizeof(tmpVal), "z%02d%02d", var1, var2);
          Serial3.println(tmpVal);
          delay(2000);
          /////////////////////////////////////////////////////////////////
          /////////////////////////////////////////////////////////////////

          if (get2ValFunc("Off Dur", var1, var2, 'h', 23, 'm', 59)) {
            Serial3.println("applied");
            /////////////////////////////////////////////////////////////////
            /////////////////////////////////////////////////////////////////

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
        if (get2ValFunc("CounTmr", var1, var2, 'h', 23, 'm', 59)) {
          Serial3.println("applied");
          Serial3.println(var1);
          Serial3.println(var2);
          storage.modeM1 = 3;
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
        }
        break;
      case 1510:
        var1 = 0;
        var2 = 0;
        if (get2ValFunc("OnDelay", var1, var2, 'm', 59, 's', 59)) {
          Serial3.println("applied");
          Serial3.println(var1);
          Serial3.println(var2);

          /////////////////////////////////////////////////////////////////
          /////////////////////////////////////////////////////////////////
          storage.modeM1 = 5;
          // format into val with leading zeros, always 2 digits
          snprintf(tmpVal, sizeof(tmpVal), "z%02d%02d", var1, var2);
          strncpy(storage.lasRemM1, tmpVal, sizeof(storage.lasRemM1) - 1);
          storage.lasRemM1[sizeof(storage.lasRemM1) - 1] = '\0';  // ensure null-termination
          Serial3.print("lasRemM1: ");
          Serial3.println(storage.lasRemM1);
          iotSerial.println("<{\"AT\":{\"a1asRem\":\"" + String(storage.lasRemM1) + "\"}}>");
          delay(500);
          iotSerial.println("<{\"TS\":{\"n\":\"5\"}}>");
          Serial3.println("<{\"AT\":{\"a1lasRem\":\"" + String(storage.lasRemM1) + "\"}}>");
          savecon();
          loadModeVal();
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


      case 1500:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("-> SET LAST REM ");
        lcd_set_cursor(1, 0);
        lcd_print("   [ BACK ]     ");
        //lcd_print("                ");
        break;
      case 1600:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("   SET LAST REM ");
        lcd_set_cursor(1, 0);
        lcd_print("-> [ BACK ]     ");
        //lcd_print("                ");
        break;

      case 2000:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("    MODE        ");
        lcd_set_cursor(1, 0);
        lcd_print(" -> PRESET      ");
        break;

      case 2600:
      case 2100:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("->SET DRY RUN Ah");
        lcd_set_cursor(1, 0);
        lcd_print("  SET OVRLOAD Ah");
        break;
      case 2110:
        get1VaFloatFun("DryRun ", var3, 'A', 1.0, 30.0);
        storage.dryRunM1 = var3;
        savecon();
        iotSerial.println("<{\"AT\":{\"a1dry\":\"z" + String(storage.dryRunM1) + "\"}}>");
        Serial3.println("<{\"AT\":{\"a1dry\":\"z" + String(storage.dryRunM1) + "\"}}>");
        loadModeVal();
        uiFunc(1);
        menuUiFunc = 0;
        lcd_modeShow();
        break;

      case 2200:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("  SET DRY RUN Ah");
        lcd_set_cursor(1, 0);
        lcd_print("->SET OVRLOAD Ah");
        break;
      case 2210:
        get1VaFloatFun("OverRun", var3, 'A', 1.0, 30.0);
        storage.ovLRunM1 = var3;
        iotSerial.println("<{\"AT\":{\"a1ovl\":\"z" + String(storage.ovLRunM1) + "\"}}>");
        Serial3.println("<{\"AT\":{\"a1ovl\":\"z" + String(storage.ovLRunM1) + "\"}}>");
        savecon();
        loadModeVal();
        uiFunc(1);
        menuUiFunc = 0;
        lcd_modeShow();
        break;
      case 2300:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("->SET Under Volt");
        lcd_set_cursor(1, 0);
        lcd_print("  SET Over  Volt");
        break;
      case 2310:
        get1ValFunc("UnderVol", var1, 'V', 330, 400);
        storage.undVol = var1;
        iotSerial.println("<{\"AT\":{\"lvTh\":\"z" + String(storage.undVol) + "\"}}>");
        Serial3.println("<{\"AT\":{\"lvTh\":\"z" + String(storage.undVol) + "\"}}>");
        savecon();
        loadModeVal();
        uiFunc(1);
        menuUiFunc = 0;
        lcd_modeShow();
        break;
      case 2400:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("  SET Under Volt");
        lcd_set_cursor(1, 0);
        lcd_print("->SET Over  Volt");
        break;
      case 2410:
        get1ValFunc("OverVol", var1, 'V', 420, 480);
        storage.ovrVol = var1;
        iotSerial.println("<{\"AT\":{\"hvTh\":\"z" + String(storage.ovrVol) + "\"}}>");
        Serial3.println("<{\"AT\":{\"hvTh\":\"z" + String(storage.ovrVol) + "\"}}>");
        savecon();
        loadModeVal();
        uiFunc(1);
        menuUiFunc = 0;
        lcd_modeShow();
        break;
      case 2500:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print(" -> [ BACK ]    ");
        lcd_set_cursor(1, 0);
        lcd_print("                ");
        break;


      case 3000:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print(" -> SETTINGS    ");
        lcd_set_cursor(1, 0);
        lcd_print("    [ HOME ]    ");
        break;
      case 3300:
        mlvl2 = 1;
        menuPos = 3100;
      case 3100:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("->RESET DEVICE");
        lcd_set_cursor(1, 0);
        lcd_print("   [ BACK ]     ");
        break;
      case 3200:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("  RESET DEVICE");
        lcd_set_cursor(1, 0);
        lcd_print("-> [ BACK ]     ");
        break;

      case 3110:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("  RESETTING...  ");
        lcd_set_cursor(1, 0);
        lcd_print("                ");
        delay(3000);
        storage.modeM1 = 0;
        Serial3.print("Reset ");
        Serial3.println(storage.modeM1);
        loadModeVal();
        uiFunc(1);
        menuUiFunc = 0;
        lcd_modeShow();
        break;

      case 4000:
        lcd_set_cursor(0, 0);
        //d_print("1234567890123456");
        lcd_print("    SETTINGS    ");
        lcd_set_cursor(1, 0);
        lcd_print(" -> [ HOME ]    ");
        break;
      case 4100:
        menuUiFunc = 0;
        uiFunc(0);
        return;
        break;
      default:
        menuPos = 1000;
        mlvl4 = 0;
        mlvl3 = 0;
        mlvl2 = 0;
        mlvl1 = 1;
        break;
    }
  }
}