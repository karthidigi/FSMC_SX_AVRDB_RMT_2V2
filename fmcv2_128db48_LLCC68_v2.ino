#include "zSettings.h"
#include "hwSer.h"
#include "debug.h"
#include "hwPins.h"
#include "avrWDT.h"
#include "ledTicker.h"
#include "rotary.h"
#include "i2cSetup.h"
#include "ds3231.h"
#include "epochHandler.h"
#include "storage.h"
#include "emon.h"
#include "motor.h"
#include "prevention.h"
#include "mode.h"
#include "lcdDisp.h"
#include "menu.h"
#include "aesMain.h"
#include "devId.h"
#include "encrypt.h"
#include "rxFunc.h"
#include "decrypt.h"
#include "llcc68Main.h"
#include "iotData.h"
#include "rfData.h"
#include "simulaton.h"
#include "serReader.h"

/////////////////////////////////////////////
/////////////////////////////////////////////
void setup() {
  delay(100);
  initHwPins();
  hwSerialInit();
  rotaryInit();
  i2c1Init();
  lcdInit();
  emonInit();
  loadcon();
  loadModeVal();
  getDeviceSerId();
  aesInit("[horizon]");
  llcc68Init();
  wdtInit();
  wdt_reset();
  delay(1000);
  lcd_backlight(1);
  lcd_set_cursor(0, 0);
  lcd_print("  HORIZONGRID   ");
  lcd_set_cursor(1, 0);
  lcd_print("FW : 3P2MV1.01  ");

  if (storage.app2Run) {
    digitalWrite(PIN_M2ON, HIGH);
  } else {
    digitalWrite(PIN_M2ON, LOW);
  }
}

void loop() {
  //Serial3.println("qqqq");
  readHWSerial();
  RedLedTickerFunc();
  overLoadCheck();
  dryRunCheck();
  emonFunc();
  Simulation();
  motorStaCheck();
  modeM1();
  PowChecks();
  readIotSerial();
  iotDataSendVol(0);
  AttribIotDatSendSend();
  llcc68Func();
  //motStatus2send(m1StaVars);
  ackElst();
  rtcFetchTimeFunc();
  //ds3231_getTemperature();
  ////////////////
  if (!menuUiFunc) {
    uiFunc(0);
  } else {
    menuUi();
  }
  ////////////////
  rotaryFunc();
  rotaryVal();
  loraTxFunc();
  wdt_reset();

  //loraTx10sec();
}