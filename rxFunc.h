///////////////////////////////////////
char encapData[MAX_MESSAGE_LEN];
bool loraAck = 0;
///////////////////////////////////////
void rxFunc() {
  DEBUG_PRINT(F("RX CMD: "));
  DEBUG_PRINTN(encapData);

  if ((strcmp(encapData, "1N") == 0) ||
      (strcmp(encapData, "M11") == 0) ||
      (strcmp(encapData, "M1N") == 0)) {
    DEBUG_PRINTN(F("M1 ON received"));
    a1et = 8;
    lcd_set_cursor(0, 0);
    lcd_print(" MOTOR  CONTROL ");
    lcd_set_cursor(1, 0);
    lcd_print(" REMOTE :  ON   ");
    // Cancel any active OFF latch so REMOTE ON is never blocked by the off delay
    if (m1OffActive) {
      stopOffTimer();
      digitalWrite(PIN_M1OFF, HIGH);
      m1OffActive = false;
    }
    m1On();
    loraAck = 1;
    // Save EEPROM state only when Last State mode is active
    if (storage.modeM1 == 5) {
      laStaAppStaSavShow = 1;
      laStaRemM1remTrigrd = 1;
      laStaRemM1Trigd = 1;
      digitalWrite(PIN_LED_YELLOW, LOW);
      storage.app1Run = 1;
      savecon();
    }
    delay(100);

  } else if ((strcmp(encapData, "1F") == 0) ||
             (strcmp(encapData, "M10") == 0) ||
             (strcmp(encapData, "M1F") == 0)) {
    DEBUG_PRINTN(F("M1 OFF received"));
    a1et = 9;
    lcd_set_cursor(0, 0);
    lcd_print(" MOTOR  CONTROL ");
    lcd_set_cursor(1, 0);
    lcd_print(" REMOTE :  OFF  ");
    m1Off();
    loraAck = 1;

    if (storage.modeM1 == 5) {
      laStaAppStaSavShow = 1;
      storage.app1Run = 0;
      digitalWrite(PIN_LED_YELLOW, HIGH);
      savecon();
    } else if (storage.modeM1 != 0) {
      storage.modeM1 = 0;
      savecon();
    }
    //delay(1000);


  } else if ((strcmp(encapData, "2N") == 0) ||
             (strcmp(encapData, "M21") == 0) ||
             (strcmp(encapData, "M2N") == 0)) {
    DEBUG_PRINTN(F("M2 ON received"));
    loraAck = 1;

  } else if ((strcmp(encapData, "2F") == 0) ||
             (strcmp(encapData, "M20") == 0) ||
             (strcmp(encapData, "M2F") == 0)) {
    DEBUG_PRINTN(F("M2 OFF received"));
    loraAck = 1;

  } else if ((strcmp(encapData, "S?") == 0) ||
             (strcmp(encapData, "M00") == 0)) {
    lcd_set_cursor(0, 0);
    lcd_print(" MOTOR  STATUS  ");
    lcd_set_cursor(1, 0);
    lcd_print(" REMOTE :  ACK  ");
    DEBUG_PRINTN(F("S? received"));
    loraAck = 1;
    delay(1000);
  } else {
    DEBUG_PRINTN(F("CMD not found received"));
  }
  encapData[0] = '\0';
}
