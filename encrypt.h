/////////////////////////////////////////////////////////
#define HEX_BUFFER_LEN (MAX_MESSAGE_LEN * 2 + 8)
static char txBuffer[HEX_BUFFER_LEN + 4];

static inline void encryptData(const char *msg) {

  // Read remote's paired serial from storage (set by LoRa pairing)
  char peerSerialKey[21];
  strncpy(peerSerialKey, storage.remote, sizeof(peerSerialKey) - 1);
  peerSerialKey[sizeof(peerSerialKey) - 1] = '\0';

  // Build AES key: 6 bytes from last 12 chars of peer serial + 2-byte rnd
  uint8_t key[KEY_LEN] = { 0 };
  const char *last12 = peerSerialKey + 8;  // last 12 chars of 20-char serial
  for (uint8_t i = 0; i < 6; i++) {
    char hexPair[3] = { last12[i * 2], last12[i * 2 + 1], 0 };
    key[(KEY_LEN - 8) + i] = (uint8_t)strtoul(hexPair, NULL, 16);
  }

  randomSeed(millis());
  uint16_t rnd = random(1, 0xFFFF);
  key[KEY_LEN - 2] = rnd >> 8;
  key[KEY_LEN - 1] = rnd & 0xFF;

  uint8_t idx = random(0, 16);

  char encBuf[MAX_MESSAGE_LEN * 2 + 3];
  if (!encryptWithIdx(msg, key, peerSerialKey, idx, encBuf, sizeof(encBuf))) {
    DEBUG_PRINTN(F("Encrypt failed"));
    return;
  }

  // Build final TX buffer: idx(2 hex) + rnd(4 hex) + encBuf
  char finalBuf[HEX_BUFFER_LEN + 16];
  snprintf(finalBuf, sizeof(finalBuf), "%02X%04X%s", idx, rnd, encBuf);

  strncpy(txBuffer, finalBuf, sizeof(txBuffer));
  txBuffer[sizeof(txBuffer) - 1] = '\0';

  DEBUG_PRINT(F("TX: "));
  DEBUG_PRINTN(txBuffer);
}
