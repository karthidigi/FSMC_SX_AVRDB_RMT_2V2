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

  if (loraAck) {

    //loraTx10sec();
    // First time ack detected → start timer
    if (ackTime == 0) {
      ackTime = millis();
    }

    // If 5 seconds have passed since ack
    if (millis() - ackTime >= 3000) {

      if (m1StaVars) {
        encryptNTx("[1N]");
      } else {
        encryptNTx("[1F]");
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
    }
  }
}

