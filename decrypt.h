// Receiver-side: decrypt incoming LoRa buffer

char decBuf[MAX_MESSAGE_LEN + 1];

static inline void decryptData(const uint8_t* rx_buf, uint8_t rx_len) {

  if (rx_len == 0 || rx_len > 64) return;

  char hexBuf[65];
  uint8_t copyLen = (rx_len < sizeof(hexBuf) - 1) ? rx_len : sizeof(hexBuf) - 1;
  memcpy(hexBuf, rx_buf, copyLen);
  hexBuf[copyLen] = '\0';

  // ---- Parse idx + rnd ----
  if (strlen(hexBuf) < 6) {
    DEBUG_PRINTN(F("RX too short for idx+rnd"));
    return;
  }

  // Extract idx (2 hex chars → byte)
  // uint8_t idx = (fromHexChar(hexBuf[0]) << 4) | fromHexChar(hexBuf[1]);

  // Extract rnd (next 4 hex chars → uint16_t)
  uint16_t rnd = (fromHexChar(hexBuf[2]) << 12) | (fromHexChar(hexBuf[3]) << 8) | (fromHexChar(hexBuf[4]) << 4) | (fromHexChar(hexBuf[5]));

  // Remaining ciphertext (ASCII hex)
  const char* cipherHex = hexBuf + 6;

  // ---- Build AES key from *this receiver’s* chip serial ----
  char selfSerial[21];
  getChipSerial(selfSerial, sizeof(selfSerial));
  DEBUG_PRINTN(selfSerial);

  uint8_t key[KEY_LEN] = { 0 };
  const char* last12 = selfSerial + 8;  // last 12 chars = 6 bytes
   DEBUG_PRINTN(last12);
  for (uint8_t i = 0; i < 6; i++) {
    char hexPair[3] = { last12[i * 2], last12[i * 2 + 1], 0 };
    key[(KEY_LEN - 8) + i] = (uint8_t)strtoul(hexPair, NULL, 16);
  }

  // Append rnd into last 2 bytes
  key[KEY_LEN - 2] = rnd >> 8;
  key[KEY_LEN - 1] = rnd & 0xFF;

     DEBUG_PRINT(F("Key: ["));
    for (uint8_t i = 0; i < KEY_LEN; i++) {
  #ifdef SERIAL_DEBUG
      if (key[i] < 0x10) Serial.print('0');
      Serial3.print(key[i], HEX);
      Serial3.print(' ');
  #else
       DEBUG_PRINT(key[i]);  // fallback, but will look weird
  #endif
    }
     DEBUG_PRINTN("]");

  //---- Decrypt ----
  char decBuf[MAX_MESSAGE_LEN + 1];
  if (decryptWithIdx(cipherHex, key, selfSerial, decBuf, sizeof(decBuf))) {
     DEBUG_PRINT("Decrypted: ");
     DEBUG_PRINTN(decBuf);
  } else {
     DEBUG_PRINTN("Decrypt failed");
  }



  if (decryptWithIdx(cipherHex, key, selfSerial, decBuf, sizeof(decBuf))) {
    char* start = strchr(decBuf, '[');
    char* end = strchr(decBuf, ']');

    if (start && end && end > start) {
      // Extract text between [ and ]
      size_t len = end - start - 1;  // content length
      if (len < sizeof(encapData)) {
        strncpy(encapData, start + 1, len);
        encapData[len] = '\0';  // null terminate

        DEBUG_PRINT(F("Extracted: "));
        DEBUG_PRINTN(encapData);
      } else {
        delay(10);
        DEBUG_PRINTN(F("Content too long"));
      }
    } else {
      
      delay(10);
      DEBUG_PRINTN(F("Invalid message (no brackets)"));
    }
  } else {
    DEBUG_PRINTN(F("Decrypt failed"));\
    delay(200);
  }
}


void decryptNFunc(const uint8_t* rx_buf, uint8_t rx_len) {
  decryptData(rx_buf, rx_len);
  rxFunc();
}