// Receiver-side: decrypt incoming LoRa buffer

char decBuf[MAX_MESSAGE_LEN + 1];

static inline uint16_t parseHex16(const char* p) {
  return (fromHexChar(p[0]) << 12) |
         (fromHexChar(p[1]) << 8)  |
         (fromHexChar(p[2]) << 4)  |
         (fromHexChar(p[3]));
}

static inline bool decryptTryFrame(const char* inHex,
                                   uint16_t rnd,
                                   const char* selfSerial,
                                   char* outPlain,
                                   size_t outPlainLen) {
  if (!inHex || !selfSerial || !outPlain || outPlainLen == 0) return false;

  uint8_t key[KEY_LEN] = {0};
  const char* last12 = selfSerial + 8;  // last 12 chars = 6 bytes
  for (uint8_t i = 0; i < 6; i++) {
    char hexPair[3] = { last12[i * 2], last12[i * 2 + 1], 0 };
    key[(KEY_LEN - 8) + i] = (uint8_t)strtoul(hexPair, NULL, 16);
  }
  key[KEY_LEN - 2] = rnd >> 8;
  key[KEY_LEN - 1] = rnd & 0xFF;

  return decryptWithIdx(inHex, key, selfSerial, outPlain, outPlainLen);
}

static inline bool extractCommandFromPlain(const char* plain) {
  if (!plain) return false;

  const char* start = strchr(plain, '[');
  const char* end = strchr(plain, ']');

  if (start && end && end > start) {
    size_t len = (size_t)(end - start - 1);
    if (len > 0 && len < sizeof(encapData)) {
      strncpy(encapData, start + 1, len);
      encapData[len] = '\0';
      DEBUG_PRINT(F("Extracted: "));
      DEBUG_PRINTN(encapData);
      return true;
    }
  }

  // Legacy/plain command fallback.
  if ((strcmp(plain, "1N") == 0) ||
      (strcmp(plain, "1F") == 0) ||
      (strcmp(plain, "2N") == 0) ||
      (strcmp(plain, "2F") == 0) ||
      (strcmp(plain, "S?") == 0)) {
    strncpy(encapData, plain, sizeof(encapData) - 1);
    encapData[sizeof(encapData) - 1] = '\0';
    DEBUG_PRINT(F("Extracted (legacy): "));
    DEBUG_PRINTN(encapData);
    return true;
  }

  return false;
}

static inline void decryptData(const uint8_t* rx_buf, uint8_t rx_len) {
  if (rx_len == 0 || rx_len > 64) return;

  char hexBuf[65];
  uint8_t copyLen = (rx_len < (sizeof(hexBuf) - 1)) ? rx_len : (sizeof(hexBuf) - 1);
  memcpy(hexBuf, rx_buf, copyLen);
  hexBuf[copyLen] = '\0';

  size_t hexLen = strlen(hexBuf);
  if (hexLen < 2) return;

  // Plaintext compatibility fallback (for legacy remotes without AES frame).
  if (extractCommandFromPlain(hexBuf)) {
    DEBUG_PRINTN(F("Plain RX fallback"));
    return;
  }

  char selfSerial[21];
  getChipSerial(selfSerial, sizeof(selfSerial));

  bool ok = false;
  char plain[MAX_MESSAGE_LEN + 1];
  plain[0] = '\0';

  // Format A: [idx][rnd][idx+cipher] (current starter TX layout)
  if (!ok && hexLen >= 6) {
    uint16_t rnd = parseHex16(hexBuf + 2);
    ok = decryptTryFrame(hexBuf + 6, rnd, selfSerial, plain, sizeof(plain));
  }

  // Format B: [idx][rnd][cipher] (single idx only)
  if (!ok && hexLen >= 6) {
    uint16_t rnd = parseHex16(hexBuf + 2);
    char inHex[65];
    size_t bodyLen = hexLen - 6;
    if ((2 + bodyLen) < sizeof(inHex)) {
      inHex[0] = hexBuf[0];
      inHex[1] = hexBuf[1];
      memcpy(inHex + 2, hexBuf + 6, bodyLen);
      inHex[2 + bodyLen] = '\0';
      ok = decryptTryFrame(inHex, rnd, selfSerial, plain, sizeof(plain));
    }
  }

  // Format C: [rnd][idx+cipher]
  if (!ok && hexLen >= 4) {
    uint16_t rnd = parseHex16(hexBuf);
    ok = decryptTryFrame(hexBuf + 4, rnd, selfSerial, plain, sizeof(plain));
  }

  // Format D: [idx+cipher] with fixed rnd=1
  if (!ok) {
    ok = decryptTryFrame(hexBuf, 1, selfSerial, plain, sizeof(plain));
  }

  // Format E: [idx][cipher] with fixed rnd=1
  if (!ok && hexLen >= 2) {
    ok = decryptTryFrame(hexBuf + 2, 1, selfSerial, plain, sizeof(plain));
  }

  if (!ok) {
    DEBUG_PRINT(F("Decrypt failed for RX: "));
    DEBUG_PRINTN(hexBuf);
    return;
  }

  DEBUG_PRINT(F("Decrypted: "));
  DEBUG_PRINTN(plain);

  if (!extractCommandFromPlain(plain)) {
    DEBUG_PRINTN(F("Invalid message payload"));
  }
}

void decryptNFunc(const uint8_t* rx_buf, uint8_t rx_len) {
  decryptData(rx_buf, rx_len);
  rxFunc();
}
