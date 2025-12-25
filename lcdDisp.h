#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

// ========== I2C Addresses ==========
#define LCD_ADDR 0x27

// ========== LCD Bit Definitions ==========
#define LCD_RS 0x01
#define LCD_EN 0x04
#define LCD_BL 0x08

// ========== Globals ==========
bool backlightOn = true;
char lcd_buf[17];

// ========== LCD Functions ==========
void lcd_send_nibble(uint8_t nibble, uint8_t rs) {
  uint8_t data = (nibble << 4) | (rs ? LCD_RS : 0) | (backlightOn ? LCD_BL : 0);
  i2c1_start(LCD_ADDR, false);
  i2c1_write(data | LCD_EN);
  i2c1_stop();
  _delay_us(1);
  i2c1_start(LCD_ADDR, false);
  i2c1_write(data & ~LCD_EN);
  i2c1_stop();
  _delay_us(50);
}

void lcd_send_byte(uint8_t byte, uint8_t rs) {
  lcd_send_nibble(byte >> 4, rs);
  lcd_send_nibble(byte & 0x0F, rs);
}

void lcd_cmd(uint8_t cmd) {
  lcd_send_byte(cmd, 0);
  if (cmd == 0x01 || cmd == 0x02) _delay_ms(2);
}

void lcd_data(uint8_t data) {
  lcd_send_byte(data, 1);
}

void lcdInit() {
  _delay_ms(50);
  lcd_send_nibble(0x03, 0);
  _delay_ms(5);
  lcd_send_nibble(0x03, 0);
  _delay_us(150);
  lcd_send_nibble(0x03, 0);
  _delay_us(150);
  lcd_send_nibble(0x02, 0);
  _delay_us(50);

  lcd_cmd(0x28);  // 4-bit, 2-line
  lcd_cmd(0x0C);  // Display ON
  lcd_cmd(0x06);  // Entry mode
  lcd_cmd(0x01);  // Clear
}

void lcd_set_cursor(uint8_t row, uint8_t col) {
  lcd_cmd(0x80 + (row == 0 ? 0x00 : 0x40) + col);
}

void lcd_print(const char* str) {
  while (*str) lcd_data(*str++);
}

void lcd_backlight(bool on) {
  backlightOn = on;
  lcd_set_cursor(0, 0);
}

void lcd_clockFunc() {
  static uint32_t lastUpdate = 0;
  uint32_t now = millis();

  if (now - lastUpdate >= 1000) {

    rtcFetchTimeFunc();
    snprintf(lcd_buf, sizeof(lcd_buf), "  %02d-%02d-%02d %s   ", tDate, tMonth, tYear, sDoW);
    lcd_set_cursor(0, 0);
    lcd_print(lcd_buf);

    snprintf(lcd_buf, sizeof(lcd_buf), "  %02d:%02d:%02d %s   ", tHrs, tMin, tSec, tAmPm);
    lcd_set_cursor(1, 0);
    lcd_print(lcd_buf);

    lastUpdate = now;
  }
}

void lcd_PowFunc() {
  static unsigned long timerMillis1 = 0;
  if (millis() - timerMillis1 > 2000) {
    ////////////////////////////////////////////////////////
    snprintf(lcd_buf, sizeof(lcd_buf), "V %03d  %03d  %03d ", volRY, volYB, volBR);
    lcd_set_cursor(0, 0);
    lcd_print(lcd_buf);
    ///////////////////////////////////////////////////////
    if (powerLoss) {
      lcd_set_cursor(0, 0);
      lcd_print("V  0    0    0 ");
      lcd_set_cursor(1, 0);
      lcd_print("   POWER LOSS!    ");
    } else if (phaseFault) {
      lcd_set_cursor(1, 0);
      lcd_print("  PHASE FAULT!    ");
    } else if (unVolVars) {
      lcd_set_cursor(1, 0);
      lcd_print(" UNDER VOLTAGE!  ");
    } else if (hiVolVars) {
      lcd_set_cursor(1, 0);
      lcd_print(" OVER VOLTAGE!   ");
    } else {
      char bufR[8], bufY[8], bufBM1[8];
      if (curR <= 0.5) {
        strcpy(bufR, " 0 ");
      } else {
        dtostrf(curR, 3, 1, bufR);  // 1 decimal
      }

      if (curY <= 0.5) {
        strcpy(bufY, " 0 ");
      } else {
        dtostrf(curY, 3, 1, bufY);
      }

      if (curBM1 <= 0.5) {
        strcpy(bufBM1, " 0 ");
      } else {
        dtostrf(curBM1, 3, 1, bufBM1);
      }

      snprintf(lcd_buf, sizeof(lcd_buf), "I %s  %s  %s ", bufR, bufY, bufBM1);
      lcd_set_cursor(1, 0);
      lcd_print(lcd_buf);
    }
    timerMillis1 = millis();
  }
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
    } else if (!((((atMins + atSec) * 1000UL) + autoM1BuffMillis) - millis() < 1)) {
      lcd_set_cursor(0, 0);
      lcd_print("AUTOSTART:DELAY ");

      unsigned long x = (((atMins + atSec) * 1000UL) + autoM1BuffMillis) - millis();
      if ((long)x < 0) {
        x = 0;  // safety, no negative countdown
      }

      unsigned long seconds = x / 1000;      // total seconds
      unsigned long minutes = seconds / 60;  // total minutes
      unsigned long hours = minutes / 60;    // total hours

      seconds %= 60;
      minutes %= 60;

      if (hours > 0) {
        snprintf(lcd_buf, sizeof(lcd_buf), "Start in %02lu:%02lu:%02lu", hours, minutes, seconds);
      } else {
        snprintf(lcd_buf, sizeof(lcd_buf), "Start in %02lu:%02lu  ", minutes, seconds);
      }

      lcd_set_cursor(1, 0);
      lcd_print(lcd_buf);
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
      unsigned long offMillis = ((((cTHrs + cTMins) * 1000) + countM1BuffMills)) - millis();
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
      if (storage.app1Run) {
        lcd_set_cursor(0, 0);
        lcd_print("  SAVING STATE  ");
        lcd_set_cursor(1, 0);
        lcd_print("MOTOR is Running");
      } else {
        lcd_set_cursor(0, 0);
        lcd_print("  SAVING STATE  ");
        lcd_set_cursor(1, 0);
        lcd_print("MOTOR is Stopped");
      }
      laStaAppStaSavShow = 0;
      wdt_reset();
      delay(1000);
    }else if (m1StaVars) {
      lcd_set_cursor(0, 0);
      lcd_print("LAST STATE MODE ");
      lcd_set_cursor(1, 0);
      lcd_print("MOTOR is RUNNING");
    } else if (!((((laStaRemMins + laStaRemSec) * 1000UL) + laStaRemM1BuffMillis) - millis() < 1) && !m1StaVars && storage.app1Run) {
      lcd_set_cursor(0, 0);
      lcd_print("LAST STATE:DELAY");

      unsigned long x = (((laStaRemMins + laStaRemSec) * 1000UL) + laStaRemM1BuffMillis) - millis();
      if ((long)x < 0) {
        x = 0;  // safety, no negative countdown
      }

      unsigned long seconds = x / 1000;      // total seconds
      unsigned long minutes = seconds / 60;  // total minutes
      unsigned long hours = minutes / 60;    // total hours

      seconds %= 60;
      minutes %= 60;

      if (hours > 0) {
        snprintf(lcd_buf, sizeof(lcd_buf), "Start in %02lu:%02lu:%02lu", hours, minutes, seconds);
      } else {
        snprintf(lcd_buf, sizeof(lcd_buf), "Start in %02lu:%02lu  ", minutes, seconds);
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




void lcdUiUpdate() {
  switch (RotCounter) {
    case 0:
      lcd_activeModeShow();
      break;
    case 1:
      lcd_PowFunc();
      break;
    case 2:
      lcd_volPrevShow(0);
      break;
    case 3:
      lcd_curPrevShow(0);
      break;
    case 4:
      lcd_nameShow(0);
      break;
    case 5:
      lcd_clockFunc();
      break;
    default:
      RotCounter = 0;
  }
}


void uiFunc(bool lx) {
  static unsigned long blockUntil = 0;  // remembers value across calls
  static unsigned long timerMillis = 0;



  // Run only if not blocked
  if (millis() >= blockUntil) {
    rotaryFunc();
    lcdUiUpdate();
  }

  if (lx) {
    blockUntil = millis() + 3000;
    return;
  }


  if (powerLoss || phaseFault || unVolVars || hiVolVars) {
    RotCounter = 1;
    lcdUiUpdate();
    return;
  } else if (millis() - knobRollMillis > 15000) {
    if (millis() - timerMillis > 5000) {
      if (RotCounter == 0) {
        RotCounter = 1;
      } else {
        RotCounter = 0;
      }
      timerMillis = millis();
    }
  }
}
