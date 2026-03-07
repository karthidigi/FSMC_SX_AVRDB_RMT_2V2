/////////////////////////////////////////////////////////
#define HEX_BUFFER_LEN (MAX_MESSAGE_LEN * 2 + 8)
static char txBuffer[HEX_BUFFER_LEN + 4];

static inline void encryptData(const char *msg) {

  DEBUG_PRINTN(F("----- AES TEST -----"));
  DEBUG_PRINT(F("Msg: "));
  DEBUG_PRINTN(msg);

  // Get chip serial
  char peerSerialKey[21];
  // Build AES key
  // Read peer serial from EEPROM instead of chip serial
  //readPeerSerial(peerSerialKey, sizeof(peerSerialKey)); // attiny
 strncpy(peerSerialKey, storage.remote, sizeof(peerSerialKey) - 1);  //avr — uses paired serial
 //strncpy(peerSerialKey, REMOTE_ID, sizeof(peerSerialKey) - 1);  //avr — old hardcoded (removed)

  peerSerialKey[sizeof(peerSerialKey) - 1] = '\0';
  DEBUG_PRINT(F("Peer Serial: "));
  DEBUG_PRINTN(peerSerialKey);
  // Build AES key
  uint8_t key[KEY_LEN] = { 0 };
  const char *last12 = peerSerialKey + 8;  // last 12 chars of 20-char serial
  //  memcpy(key + (KEY_LEN - 14), last12, 12);

  for (uint8_t i = 0; i < 6; i++) {
    char hexPair[3] = { last12[i * 2], last12[i * 2 + 1], 0 };
    key[(KEY_LEN - 8) + i] = (uint8_t)strtoul(hexPair, NULL, 16);
  }

  randomSeed(millis());
  uint16_t rnd = random(1, 0xFFFF);
  key[KEY_LEN - 2] = rnd >> 8;
  key[KEY_LEN - 1] = rnd & 0xFF;

  DEBUG_PRINT(F("Key: "));
  for (uint8_t i = 0; i < KEY_LEN; i++) {
    if (key[i] < 0x10)  
    DEBUG_PRINT('0');
#ifdef SERIAL_DEBUG
      Serial3.print(key[i], HEX);
#endif
    DEBUG_PRINT(' ');
  }
  DEBUG_PRINTN();

  // Index hardcoded
  uint8_t idx = random(0, 16);
  DEBUG_PRINTN(F("Using idx: "));
  DEBUG_PRINTN(idx);
  delay(10);
  DEBUG_PRINTN(F("Bf encr"));
  delay(10);
  char encBuf[MAX_MESSAGE_LEN * 2 + 3];

  if (!encryptWithIdx(msg, key, peerSerialKey, idx, encBuf, sizeof(encBuf))) {
    DEBUG_PRINTN(F("Encrypt failed"));
    return;
  }
  DEBUG_PRINT(F("Encrypted: "));
  DEBUG_PRINTN(encBuf);

  DEBUG_PRINTN(F("After encryption"));

  // ---- Build final TX buffer: idx + rnd + encBuf ----
  char finalBuf[HEX_BUFFER_LEN + 16];  // safety margin
  snprintf(finalBuf, sizeof(finalBuf), "%02X%04X%s", idx, rnd, encBuf);
  DEBUG_PRINT(F("finalBuf: "));
  DEBUG_PRINTN(finalBuf);

  // Copy to txBuffer for sending
  strncpy(txBuffer, finalBuf, sizeof(txBuffer));
  txBuffer[sizeof(txBuffer) - 1] = '\0';

  DEBUG_PRINT(F("TX Payload: ["));
  DEBUG_PRINT(txBuffer);
  DEBUG_PRINTN(F("]"));
  
  // Optional self-test: decrypt
  char decBuf[MAX_MESSAGE_LEN + 1];
  DEBUG_PRINTN(F("Trying decrypt..."));
  if (decryptWithIdx(encBuf, key, peerSerialKey, decBuf, sizeof(decBuf))) {
    DEBUG_PRINT(F("Decrypted: "));
    DEBUG_PRINTN(decBuf);
  } else {
    DEBUG_PRINTN(F("Decrypt failed"));
  }
  DEBUG_PRINTN(F("--------------------"));
}
