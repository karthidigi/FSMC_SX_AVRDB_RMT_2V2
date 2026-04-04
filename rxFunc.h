#include "hwSer.h"   // defines iotSerial (Serial1 stub) and debugSerial (Serial3)
///////////////////////////////////////
char encapData[MAX_MESSAGE_LEN];
bool loraAck              = 0;
bool offAckPending        = 0;   // set when [1F] received while motor running; cleared by loraTxFunc after currents < 0.5A
bool pendingLastStateSave = false; // deferred savecon() — set by 1N/1F handlers in modeM1==5, flushed by loraTxFunc after ACK TX
// Forward declarations — defined in files included after rxFunc.h
void encryptNTx(const char* msg);
void loraTxFaultStatus();   // on-demand D0/D1/D2 reply (rfData.h)
///////////////////////////////////////
void rxFunc() {
  DEBUG_PRINT(F("RX CMD: "));
  DEBUG_PRINTN(encapData);

  if ((strcmp(encapData, "1N") == 0) ||
      (strcmp(encapData, "M11") == 0) ||
      (strcmp(encapData, "M1N") == 0)) {
    DEBUG_PRINTN(F("M1 ON received"));

    // ── Remote ON block: bypass switch or active fault ──────────────
    if (bypassActive) {
      remBlockReason = 1;
      a1et = 40;
      lcd_set_cursor(0, 0); lcd_print(" REMOTE : BLOCK ");
      lcd_set_cursor(1, 0); lcd_print(" BYPASS  ACTIVE ");
      uiFunc(1);
      iotSerial.println("<{\"TS\":{\"n\":\"40\"}}>");
      encryptNTx("[B1]");   // immediate response — bypass active
    } else if (elst > 0) {
      remBlockReason = 2;
      a1et = 41;
      lcd_set_cursor(0, 0); lcd_print(" REMOTE : BLOCK ");
      lcd_set_cursor(1, 0); lcd_print(" VOLTAGE  FAULT ");
      uiFunc(1);
      iotSerial.println("<{\"TS\":{\"n\":\"41\"}}>");
      encryptNTx("[F1]");   // immediate response — voltage fault active
    } else {
      // ── Normal remote ON ──────────────────────────────────────────
      remBlockReason = 0;
      a1et = 8;
      lcd_set_cursor(0, 0);
      lcd_print(" MOTOR  CONTROL ");
      lcd_set_cursor(1, 0);
      lcd_print(" REMOTE :  ON   ");
      uiFunc(1);   // hold LCD message visible for 3 s (non-blocking block of uiFunc redraw)
      // Cancel any active OFF latch so REMOTE ON is never blocked by the off delay
      if (m1OffActive) {
        stopOffTimer();
        digitalWrite(PIN_M1OFF, HIGH);
        m1OffActive = false;
      }
      m1On();
      loraAck = 1;
      // Update state fields now; defer savecon() until after ACK TX to avoid
      // blocking the radio state machine before the remote's ACK window closes.
      if (storage.modeM1 == 5) {
        laStaAppStaSavShow = 1;
        laStaRemM1remTrigrd = 1;
        laStaRemM1Trigd = 1;
        digitalWrite(PIN_LED_YELLOW, LOW);
        storage.app1Run = 1;
        pendingLastStateSave = true;
      }
    }

  } else if ((strcmp(encapData, "1F") == 0) ||
             (strcmp(encapData, "M10") == 0) ||
             (strcmp(encapData, "M1F") == 0)) {
    DEBUG_PRINTN(F("M1 OFF received"));
    a1et = 9;
    lcd_set_cursor(0, 0);
    lcd_print(" MOTOR  CONTROL ");
    lcd_set_cursor(1, 0);
    lcd_print(" REMOTE :  OFF  ");
    uiFunc(1);   // hold LCD message visible for 3 s (non-blocking block of uiFunc redraw)
    m1Off();
    // If motor was running, defer ACK until currents confirm it has stopped.
    // If already stopped, ACK immediately through the normal path.
    if (m1StaVars) {
      offAckPending = 1;
    } else {
      loraAck = 1;
    }

    if (storage.modeM1 == 5) {
      laStaAppStaSavShow = 1;
      storage.app1Run = 0;
      digitalWrite(PIN_LED_YELLOW, HIGH);
      pendingLastStateSave = true;  // flushed by loraTxFunc after ACK TX
    } else if (storage.modeM1 != 0 && !isLocalModeChangeGuardActive()) {
      // Only reset mode when no recent local menu change is in progress.
      // isLocalModeChangeGuardActive() stays true for 5 s after the user
      // sets a mode via the LCD — prevents a gateway OFF command from
      // wiping a freshly configured mode before the LoRa/cloud round-trip.
      storage.modeM1 = 0;
      savecon();
    }
    //delay(1000);


  } else if ((strcmp(encapData, "2N") == 0) ||
             (strcmp(encapData, "M21") == 0) ||
             (strcmp(encapData, "M2N") == 0)) {
    DEBUG_PRINTN(F("LiREL ON received"));
    a1et = 34;
    lcd_set_cursor(0, 0); lcd_print(" SWITCH CONTROL ");
    lcd_set_cursor(1, 0); lcd_print(" REMOTE :  ON   ");
    uiFunc(1);
    digitalWrite(PIN_LiREL, HIGH);
    if (storage.app2Run != 1) { storage.app2Run = 1; savecon(); }
    loraAck = 1;

  } else if ((strcmp(encapData, "2F") == 0) ||
             (strcmp(encapData, "M20") == 0) ||
             (strcmp(encapData, "M2F") == 0)) {
    DEBUG_PRINTN(F("LiREL OFF received"));
    a1et = 35;
    lcd_set_cursor(0, 0); lcd_print(" SWITCH CONTROL ");
    lcd_set_cursor(1, 0); lcd_print(" REMOTE :  OFF  ");
    uiFunc(1);
    digitalWrite(PIN_LiREL, LOW);
    if (storage.app2Run != 0) { storage.app2Run = 0; savecon(); }
    loraAck = 1;

  // ── L1 / L0 (remote light-switch toggle via long press) ──────────────────
  // Reserved for future use — commented out until fully implemented.
  // } else if (strcmp(encapData, "L1") == 0) {
  //   DEBUG_PRINTN(F("LiREL L1 ON received"));
  //   a1et = 34;
  //   lcd_set_cursor(0, 0); lcd_print("  Light Switch  ");
  //   lcd_set_cursor(1, 0); lcd_print("   Turned  ON   ");
  //   uiFunc(1);
  //   digitalWrite(PIN_LiREL, HIGH);
  //   if (storage.app2Run != 1) { storage.app2Run = 1; savecon(); }
  //   iotDataSendVol(1);
  //   loraAck = 1;
  // } else if (strcmp(encapData, "L0") == 0) {
  //   DEBUG_PRINTN(F("LiREL L0 OFF received"));
  //   a1et = 35;
  //   lcd_set_cursor(0, 0); lcd_print("  Light Switch  ");
  //   lcd_set_cursor(1, 0); lcd_print("   Turned OFF   ");
  //   uiFunc(1);
  //   digitalWrite(PIN_LiREL, LOW);
  //   if (storage.app2Run != 0) { storage.app2Run = 0; savecon(); }
  //   iotDataSendVol(1);
  //   loraAck = 1;

  } else if ((strcmp(encapData, "S?") == 0) ||
             (strcmp(encapData, "M00") == 0)) {
    lcd_set_cursor(0, 0);
    lcd_print(" MOTOR  STATUS  ");
    lcd_set_cursor(1, 0);
    lcd_print(" REMOTE :  ACK  ");
    uiFunc(1);
    DEBUG_PRINTN(F("S? received"));
    // Brief hold: remote is already in RX before starter finishes processing
    // (SF11 air time ~3 s >> processing time). 50 ms is a safe guard margin.
    delay(50);
    if (bypassActive) {
      encryptNTx("[D2]");                          // bypass override active
      DEBUG_PRINTN(F("S? reply: D2 (bypass)"));
    } else if (elst > 0) {
      encryptNTx("[D1]");                          // voltage / phase fault active
      DEBUG_PRINTN(F("S? reply: D1 (fault)"));
    } else if (m1StaVars) {
      encryptNTx("[1N]");                          // no fault — motor running
      DEBUG_PRINTN(F("S? reply: 1N (motor ON)"));
    } else {
      encryptNTx("[1F]");                          // no fault — motor stopped
      DEBUG_PRINTN(F("S? reply: 1F (motor OFF)"));
    }
    // loraAck intentionally NOT set — single immediate reply only, no deferred follow-up

  } else {
    DEBUG_PRINTN(F("CMD not found received"));
  }
  encapData[0] = '\0';
}
