#include "zSettings.h"
#include "hwSer.h"
#include "debug.h"
#include "hwPins.h"
#include "avrWDT.h"
#include "ledTicker.h"
#include "buttin.h"
#include "i2cSetup.h"
#include "ds3231.h"
#include "epochHandler.h"
#include "storage.h"
#include "emon.h"
#include "relayTimer.h"
#include "motor.h"
#include "mode.h"
#include "prevention.h"
#include "lcdDisp.h"
#include "aesMain.h"
#include "devId.h"
#include "encrypt.h"
#include "rxFunc.h"
#include "decrypt.h"
#include "sx1268Main.h"
#include "pairRemote.h"  // Remote-side pairing + dispatchPairPkt  (defines: pairRemoteTick, enterRemPairMode)
// #include "userRow.h"  // AVR128DB User Row (DISABLED — uncomment to re-enable USERROW write)
#include "menu.h"        // LCD menu (after pairing headers so it can access pair state vars)
#include "rfData.h"
#include "simulaton.h"

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
  relayTimerInit();
  loadcon();
  loadModeVal();
  getDeviceSerId();
  // initUserRow();  // USERROW disabled — uncomment together with #include "userRow.h"
  aesInit("[horizon]");
  sx1268Init();
  wdtInit();
  wdt_reset();
 
  lcd_set_cursor(0, 0);
  lcd_print("Horizongrid     ");
  lcd_set_cursor(1, 0);
  lcd_print("+91- 99626 78788");
  // Keep boot splash visible before loop UI refresh starts.
  unsigned long splashStart = millis();
  while (millis() - splashStart < 1500UL) {
    wdt_reset();
    delay(10);
  }
 delay(1000);
  if (storage.app2Run) {
    digitalWrite(PIN_LiREL, HIGH);
  } else {
    digitalWrite(PIN_LiREL, LOW);
  }
}

void loop() {
  bypassCheck();               // FIRST: read bypass switch; clears faults and blocks m1Off when HIGH
  //Serial3.println("qqqq");
  motorRelayService();
  readHWSerial();
  RedLedTickerFunc();
  BlueLedTickerFunc();             // BLUE: extinguish LoRa activity flash after 150 ms
  RemBlockLedFunc();               // Remote block warning LED pattern (200/500 ms, 5 s auto-clear)
  updateCurrentFilter();   // refresh 5-sample MA once per loop; all fault checks use result
  if (!bypassActive) {
    overLoadCheck();
    dryRunCheck();
    openWireCheck();   // Open wire: any phase <= 1A while Iavg > 2A
    leakageCheck();    // Current imbalance > 30%
  }
  emonFunc();
  Simulation();
  motorStaCheck();
  modeM1();
  PowChecks();
  sx1268Func();
  pairRemoteTick();  // Remote pairing scan / retry (must run after sx1268Func)
  rtcFetchTimeFunc();
  //ds3231_getTemperature();
  ////////////////
  if (!menuUiFunc) {
    uiFunc(0);
  } else {
    menuUi();
  }
  ////////////////
  rotaryVal();
  loraTxFunc();
  wdt_reset();

//  loraTx10sec();
}
