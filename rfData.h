void encryptNTx(const char *msg) {

  encryptData(msg);
  send_lora_data((uint8_t *)txBuffer, 32);
  //send_lora_data((uint8_t *)txBuffer, strlen(txBuffer));
  DEBUG_PRINTN(txBuffer);
}



void loraTx10sec() {
  static unsigned long timerMillis = 0;

  if (millis() - timerMillis > 30000) {
    // List of possible messages
    const char* messages[] = {"[2N]", "[2F]", "[00]" };
    // const char *messages[] = { "[M11]",  "[M21]" };
    const size_t numMessages = sizeof(messages) / sizeof(messages[0]);

    // Pick a random index
    int idx = random(0, numMessages);

    // Send the random message
    encryptNTx(messages[idx]);

    DEBUG_PRINT(F("Random Msg Sent: "));
    DEBUG_PRINTLN(messages[idx]);

    // Reset timer
    timerMillis = millis();
  }
}


void loraTxFunc() {
  static unsigned long ackTime = 0;
  static bool ackReadyToSend = false;
  static bool currentUpdateForced = false;

  if (loraAck) {

    //loraTx10sec();
    // First time ack detected → start timer
    if (ackTime == 0) {
      ackTime = millis();
      ackReadyToSend = false;
      currentUpdateForced = false;
    }

    // OPTIMIZED DELAY: 1500ms to allow motor current to stabilize
    // Motor current takes ~1000ms to reach steady state
    // emonFunc() updates every 1000ms
    // This ensures m1StaVars reflects actual motor state
    if (millis() - ackTime >= 1200 && !currentUpdateForced) {
      // Force immediate current reading update at 1200ms
      // This ensures we have fresh data before sending ACK at 1500ms
      forceCurrentUpdate();
      currentUpdateForced = true;
    }

    if (millis() - ackTime >= 1500 && !ackReadyToSend) {
      ackReadyToSend = true;
    }

    // Send ACK after delay AND ready flag
    if (ackReadyToSend) {
      // Double-check motor status for accuracy
      // m1StaVars is updated by motorStaCheck() based on current readings
      if (m1StaVars) {
        encryptNTx("[1N]");
        DEBUG_PRINTN(F("ACK: Motor ON"));
      } else {
        encryptNTx("[1F]");
        DEBUG_PRINTN(F("ACK: Motor OFF"));
      }

    // const char* messages[] = { "[M11]", "[M10]", "[M21]", "[M20]", "[M00]" };
    // // const char *messages[] = { "[M11]",  "[M21]" };
    // const size_t numMessages = sizeof(messages) / sizeof(messages[0]);

    // // Pick a random index
    // int idx = random(0, numMessages);

    // // Send the random message
    // sendLoraData(messages[idx]);

    // DEBUG_PRINT(F("Random Msg Sent: "));
    // DEBUG_PRINTLN(messages[idx]);

      // Reset state
      loraAck = 0;
      ackTime = 0;
      ackReadyToSend = false;
    }
  }
}

