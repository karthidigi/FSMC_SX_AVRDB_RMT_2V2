void encryptNTx(const char *msg) {

  encryptData(msg);
  send_lora_data((uint8_t *)txBuffer, 32);
  //send_lora_data((uint8_t *)txBuffer, strlen(txBuffer));
  DEBUG_PRINTN(txBuffer);
}


// ──────────────────────────────────────────────────────────────────────────────
// Fault-status reply — D0 / D1 / D2
//
//   D0 = normal operation  (no voltage fault, no bypass)
//   D1 = voltage fault active (elst > 0 and bypass is OFF)
//   D2 = bypass switch active
//
// Called only on demand when the remote sends an "S?" status query.
// Never auto-transmits; no periodic timer.
// ──────────────────────────────────────────────────────────────────────────────
void loraTxFaultStatus() {
  if (bypassActive)  { encryptNTx("[D2]"); DEBUG_PRINTN(F("FaultStatus TX: D2")); }
  else if (elst > 0) { encryptNTx("[D1]"); DEBUG_PRINTN(F("FaultStatus TX: D1")); }
  else               { encryptNTx("[D0]"); DEBUG_PRINTN(F("FaultStatus TX: D0")); }
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
  static unsigned long offAckStartMs    = 0;
  static bool          offCurrentForced = false;

  static unsigned long ackTime = 0;
  static bool ackReadyToSend = false;
  static bool currentUpdateForced = false;

  // --- Remote OFF ACK: hold response until relay latch completes AND Iavg < 0.5A ---
  if (offAckPending) {
    if (offAckStartMs == 0) {
      offAckStartMs = millis();
      if (!offAckStartMs) offAckStartMs = 1;
      offCurrentForced = false;
    }

    // As soon as the off-relay latch finishes, force a fresh current sample
    if (!m1OffActive && !offCurrentForced) {
      forceCurrentUpdate();
      offCurrentForced = true;
    }

    bool  motorStopped  = (!m1OffActive && curIavgFilt < 0.5f);
    bool  timedOut      = (millis() - offAckStartMs >= 3000UL); // 3 s — must be < remote retry window (~6 s)

    if (motorStopped || timedOut) {
      encryptNTx("[1F]");
      offAckPending    = 0;
      offAckStartMs    = 0;
      offCurrentForced = false;
      // TX is now queued — safe to write EEPROM without delaying the ACK
      if (pendingLastStateSave) { savecon(); pendingLastStateSave = false; }
    }
    return;   // Block normal ACK path while waiting
  }
  offAckStartMs    = 0;
  offCurrentForced = false;

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

      // TX is now queued — safe to write EEPROM without delaying the ACK
      if (pendingLastStateSave) { savecon(); pendingLastStateSave = false; }

      // Reset state
      loraAck = 0;
      ackTime = 0;
      ackReadyToSend = false;
    }
  }
}

