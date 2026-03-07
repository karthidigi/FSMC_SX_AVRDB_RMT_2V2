// ============================================================
//  protSettings.h  –  Protection Settings Menu
//  Starter: AVR128DB48 | FW: fmcv2
//
//  Adds a "PROT.SET" sub-menu under SETTINGS (menuPos 3000).
//  Navigation sits in the 3x00 space that was previously
//  unused.  Existing RESET DEVICE entries (3100/3200/3110)
//  are kept untouched.
//
//  Menu tree (appended inside menuUi() switch-case):
//
//  3000  SETTINGS  ──►  3100  ->RESET DEVICE   (existing)
//                  │    3200     [ BACK ]       (existing)
//                  │
//                  └──► 3400  ->PROT.SET
//                            3500     [ BACK ]
//
//  3400 enters PROT.SET sub-menu (mlvl2 scope 3400–3500):
//
//    3410  ->OVERCURRENT A     (float, 1–30 A)
//    3420    DRY RUN     A     (float, 1–30 A)
//    3430    UNDER VOLT  V     (int,  330–400 V)
//    3440    OVER  VOLT  V     (int,  420–480 V)
//    3450    [ BACK ]
//
//  Each leaf (3411 / 3421 / 3431 / 3441) calls the existing
//  get1VaFloatFun() / get1ValFunc() helpers from menu.h,
//  writes directly to storage, calls savecon(), and returns
//  to the main UI exactly as the PRESET entries do.
//
//  ──────────────────────────────────────────────────────────
//  HOW TO INTEGRATE
//  ──────────────────────────────────────────────────────────
//  1.  #include "protSettings.h"  in the main .ino AFTER
//      all other includes (it uses menu.h helpers, storage,
//      savecon, loadModeVal, uiFunc, lcd_modeShow).
//
//  2.  Inside menuUi() in menu.h, add the two calls at the
//      bottom of the switch, BEFORE the default: case:
//
//          protSettingsMenu();   // ← paste this line
//          protSettingsLeafs();  // ← paste this line
//
//  3.  Extend the mlvl2 guard in menuUi() so menuPos % 1000
//      up to 500 is accepted (it is already set to > 600, so
//      the 3400–3500 range is fine with no change needed).
//
//  4.  Extend the top-level SETTINGS node (case 3000) to
//      show the new PROT.SET option – replace the existing
//      case 3000 and case 3300/3100 block with the version
//      shown in the PATCH SECTION below.
// ============================================================

#ifndef PROT_SETTINGS_H
#define PROT_SETTINGS_H

// ============================================================
//  PATCH SECTION – replace these cases inside menuUi()
//  (copy-paste over the existing case 3000 / 3300 blocks)
// ============================================================
//
//  ┌─ case 3000 ────────────────────────────────────────────┐
//  │  lcd line 0 : " -> SETTINGS    "                       │
//  │  lcd line 1 : "    [ HOME ]    "                       │
//  │  (no change – keeps existing layout)                   │
//  └────────────────────────────────────────────────────────┘
//
//  ┌─ case 3300 / case 3100 ────────────────────────────────┐
//  │  Becomes three sub-items instead of two:               │
//  │   3100  -> RESET DEVICE                                │
//  │   3400     PROT.SET                                    │
//  │   3500     [ BACK ]                                    │
//  └────────────────────────────────────────────────────────┘
//
//  Full replacement for those cases (paste into menu.h):
//
//    case 3300:
//      mlvl2 = 1;
//      menuPos = 3100;
//    case 3100:
//      lcd_set_cursor(0, 0);
//      lcd_print("->RESET DEVICE  ");
//      lcd_set_cursor(1, 0);
//      lcd_print("  PROT.SET      ");
//      break;
//    case 3200:
//      lcd_set_cursor(0, 0);
//      lcd_print("  RESET DEVICE  ");
//      lcd_set_cursor(1, 0);
//      lcd_print("->PROT.SET      ");
//      break;
//    case 3500:                        ← NEW "back" leaf
//      lcd_set_cursor(0, 0);
//      lcd_print("  PROT.SET      ");
//      lcd_set_cursor(1, 0);
//      lcd_print("->[ BACK ]      ");
//      break;
//
// ============================================================


// ============================================================
//  protSettingsMenu()
//  Shows the PROT.SET navigation rows (display only).
//  Call from inside menuUi() switch before default:.
// ============================================================
void protSettingsMenu() {

  // ── Entry into PROT.SET sub-menu ──────────────────────────
  // Triggered when user navigates from 3200 → push → 3210
  // We re-route 3210 into 3400 (first PROT.SET item).
  if (menuPos == 3210) {
    menuPos = 3400;
    mlvl2   = 1;
    mlvl3   = 0;
    mlvl4   = 0;
    mlvl1   = 0;
  }

  switch (menuPos) {

    // ── PROT.SET row 1: OVERCURRENT ─────────────────────────
    //   LCD line 0: "->OVERCURRENT A "   (16 chars)
    //   LCD line 1: "  DRY RUN     A "
    case 3400:
      lcd_set_cursor(0, 0);
      lcd_print("->OVERCURRENT A ");
      lcd_set_cursor(1, 0);
      lcd_print("  DRY RUN     A ");
      break;

    // ── PROT.SET row 2: DRY RUN ─────────────────────────────
    //   LCD line 0: "  OVERCURRENT A "
    //   LCD line 1: "->DRY RUN     A "
    case 3410:
      lcd_set_cursor(0, 0);
      lcd_print("  OVERCURRENT A ");
      lcd_set_cursor(1, 0);
      lcd_print("->DRY RUN     A ");
      break;

    // ── PROT.SET row 3: UNDER VOLT ──────────────────────────
    //   LCD line 0: "->UNDER VOLT  V "
    //   LCD line 1: "  OVER  VOLT  V "
    case 3420:
      lcd_set_cursor(0, 0);
      lcd_print("->UNDER VOLT  V ");
      lcd_set_cursor(1, 0);
      lcd_print("  OVER  VOLT  V ");
      break;

    // ── PROT.SET row 4: OVER VOLT ───────────────────────────
    //   LCD line 0: "  UNDER VOLT  V "
    //   LCD line 1: "->OVER  VOLT  V "
    case 3430:
      lcd_set_cursor(0, 0);
      lcd_print("  UNDER VOLT  V ");
      lcd_set_cursor(1, 0);
      lcd_print("->OVER  VOLT  V ");
      break;

    // ── PROT.SET row 5: BACK ────────────────────────────────
    //   LCD line 0: "  OVER  VOLT  V "
    //   LCD line 1: "->[ BACK ]      "
    case 3440:
      lcd_set_cursor(0, 0);
      lcd_print("  OVER  VOLT  V ");
      lcd_set_cursor(1, 0);
      lcd_print("->[ BACK ]      ");
      break;

  }  // end switch
}


// ============================================================
//  protSettingsLeafs()
//  Handles the edit actions triggered when the user pushes
//  the rotary encoder on a highlighted row (mlvl3 → push →
//  mlvl4 → push → leaf case ends in digit 1).
//  Call from inside menuUi() switch before default:.
// ============================================================
void protSettingsLeafs() {

  switch (menuPos) {

    // ── Leaf: OVERCURRENT (ovLRunM1) ────────────────────────
    //  Range  : 1.0 – 30.0 A  (same as PRESET SET OVRLOAD)
    //  Unit   : Amperes (A)
    //  Stored : storage.ovLRunM1  (float)
    //  Default: 10.0 A
    case 3401: {
      float tmpFloat = storage.ovLRunM1;   // pre-load current value
      if (get1VaFloatFun("OvrCurr", tmpFloat, 'A', 10.0, 30.0)) {
        storage.ovLRunM1 = tmpFloat;
        iotSerial.println("<{\"AT\":{\"a1ovl\":\"z" + String(storage.ovLRunM1) + "\"}}>");
        Serial3.println(F("[PROT] OvrCurr saved"));
        Serial3.println(storage.ovLRunM1);
        savecon();
        loadModeVal();
        uiFunc(1);
        menuUiFunc = 0;
        lcd_modeShow();
      }
      break;
    }

    // ── Leaf: DRY RUN (dryRunM1) ────────────────────────────
    //  Range  : 1.0 – 30.0 A
    //  Unit   : Amperes (A)
    //  Stored : storage.dryRunM1  (float)
    //  Default: 0.1 A
    case 3411: {
      float tmpFloat = storage.dryRunM1;   // pre-load current value
      if (get1VaFloatFun("DryRun ", tmpFloat, 'A', 2.0, 10.0)) {
        storage.dryRunM1 = tmpFloat;
        iotSerial.println("<{\"AT\":{\"a1dry\":\"z" + String(storage.dryRunM1) + "\"}}>");
        Serial3.println(F("[PROT] DryRun  saved"));
        Serial3.println(storage.dryRunM1);
        savecon();
        loadModeVal();
        uiFunc(1);
        menuUiFunc = 0;
        lcd_modeShow();
      }
      break;
    }

    // ── Leaf: UNDER VOLTAGE (undVol) ────────────────────────
    //  Range  : 330 – 400 V  (line-to-line, 3-phase)
    //  Unit   : Volts (V)
    //  Stored : storage.undVol  (uint16_t)
    //  Default: 200 V  →  recommend setting to 360 V in field
    case 3421: {
      int tmpInt = (int)storage.undVol;    // pre-load current value
      if (get1ValFunc("UndVolt", tmpInt, 'V', 280, 350)) {
        storage.undVol = (uint16_t)tmpInt;
        iotSerial.println("<{\"AT\":{\"lvTh\":\"z" + String(storage.undVol) + "\"}}>");
        Serial3.println(F("[PROT] UndVolt saved"));
        Serial3.println(storage.undVol);
        savecon();
        loadModeVal();
        uiFunc(1);
        menuUiFunc = 0;
        lcd_modeShow();
      }
      break;
    }

    // ── Leaf: OVER VOLTAGE (ovrVol) ─────────────────────────
    //  Range  : 420 – 480 V  (line-to-line, 3-phase)
    //  Unit   : Volts (V)
    //  Stored : storage.ovrVol  (uint16_t)
    //  Default: 500 V  →  recommend setting to 440 V in field
    case 3431: {
      int tmpInt = (int)storage.ovrVol;    // pre-load current value
      if (get1ValFunc("OvrVolt", tmpInt, 'V', 350, 500)) {
        storage.ovrVol = (uint16_t)tmpInt;
        iotSerial.println("<{\"AT\":{\"hvTh\":\"z" + String(storage.ovrVol) + "\"}}>");
        Serial3.println(F("[PROT] OvrVolt saved"));
        Serial3.println(storage.ovrVol);
        savecon();
        loadModeVal();
        uiFunc(1);
        menuUiFunc = 0;
        lcd_modeShow();
      }
      break;
    }

    // ── Leaf: BACK (return to SETTINGS top) ─────────────────
    case 3441:
      menuPos   = 3000;
      mlvl1     = 1;
      mlvl2 = mlvl3 = mlvl4 = 0;
      break;

  }  // end switch
}

#endif  // PROT_SETTINGS_H
