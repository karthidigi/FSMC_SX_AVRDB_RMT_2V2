#include "src/aes.h"

#define MAX_MESSAGE_LEN 32
#define KEY_LEN 16
#define BLOCK_SIZE 16

char hwSerialKey[21];  // for encryption purpose
// ------------------------------------------------------
// Convert a nibble (0–15) to hex char
// ------------------------------------------------------
static inline char toHexNibble(uint8_t v) {
  return (v < 10) ? ('0' + v) : ('A' + v - 10);
}

// ------------------------------------------------------
// Bytes → hex string
// ------------------------------------------------------
static inline void bytesToHex(const uint8_t *in, size_t len, char *out) {
  for (size_t i = 0; i < len; i++) {
    out[i * 2] = toHexNibble(in[i] >> 4);
    out[i * 2 + 1] = toHexNibble(in[i] & 0x0F);
  }
  out[len * 2] = '\0';
}

// ------------------------------------------------------
// Hex string → bytes
// ------------------------------------------------------
static inline uint8_t fromHexChar(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;  // fallback
}

static inline bool hexToBytes(const char *hex, uint8_t *out, size_t *outLen) {
  size_t hexLen = strlen(hex);
  if (hexLen % 2 != 0) return false;
  size_t len = hexLen / 2;
  for (size_t i = 0; i < len; i++) {
    out[i] = (fromHexChar(hex[i * 2]) << 4) | fromHexChar(hex[i * 2 + 1]);
  }
  *outLen = len;
  return true;
}

// ------------------------------------------------------
// Generate nonce dynamically (saves flash vs static table)
// ------------------------------------------------------
static inline void getNonce(uint8_t *nonce, uint8_t idx) {
  for (uint8_t i = 0; i < BLOCK_SIZE; i++) {
    nonce[i] = (uint8_t)(idx * 31 + i * 17 + 0x55);
  }
}

// ------------------------------------------------------
// Read chip serial into hex string (20 chars + null)
// ------------------------------------------------------
static inline void getChipSerial(char *out, size_t outLen) {
  if (outLen < 21) return;
  for (uint8_t i = 0; i < 10; i++) {
    uint8_t serByte = *((uint8_t *)&SIGROW.SERNUM0 + i);
    out[i * 2] = toHexNibble(serByte >> 4);
    out[i * 2 + 1] = toHexNibble(serByte & 0x0F);
  }
  out[20] = '\0';
}


// ------------------------------------------------------
// Deterministic Nonce Generator (shared)
// ------------------------------------------------------
static inline void getNonce(uint8_t *nonce, uint8_t idx, uint16_t sessionSeed) {
  uint8_t base = (uint8_t)(sessionSeed ^ idx);

  for (uint8_t i = 0; i < BLOCK_SIZE; i++) {
    uint8_t v = base + i * 13 + (idx * 7);
    switch (idx & 0x0F) {
      case 0: v ^= sessionSeed; break;
      case 1: v = ~v; break;
      case 2: v = (v << 1) | (v >> 7); break;  // rotate left
      case 3: v = (v >> 1) | (v << 7); break;  // rotate right
      case 4: v += (sessionSeed & 0xFF); break;
      case 5: v -= (sessionSeed >> 8); break;
      case 6: v ^= (sessionSeed >> 3); break;
      case 7: v ^= (sessionSeed << 2); break;
      case 8: v = v * 17 + 0x53; break;
      case 9: v = v * 29 + 0xA7; break;
      case 10: v = (v ^ 0xAA) + (sessionSeed & 0x0F); break;
      case 11: v = (v ^ 0x55) + (sessionSeed >> 4); break;
      case 12: v = (v * 3) ^ idx; break;
      case 13: v = (v * 7) ^ (sessionSeed & 0x3F); break;
      case 14: v = (v + i) ^ (sessionSeed >> 2); break;
      case 15: v = (v - i) ^ (sessionSeed << 1); break;
    }
    nonce[i] = v;
  }
}

// ------------------------------------------------------
// Session seed (derived from chip serial + key)
// ------------------------------------------------------
static inline uint16_t deriveSessionSeed(const char *chipSerial, const uint8_t *key) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < 20; i++) crc ^= chipSerial[i];
  for (uint8_t i = 0; i < KEY_LEN; i++) crc ^= key[i];
  return crc;
}

// ------------------------------------------------------
// Encrypt: produces [idx (1 byte)] + [hex ciphertext]
// ------------------------------------------------------
bool encryptWithIdx(const char *plainText,
                    const uint8_t *key,
                    const char *chipSerial,
                    uint8_t idx,
                    char *outHex,
                    size_t outHexLen) {
  size_t len = strlen(plainText);
  if (len > MAX_MESSAGE_LEN) len = MAX_MESSAGE_LEN;

  // Output must hold: 2*len hex chars + 2 (idx in hex) + 1 null
  if (outHexLen < (len * 2 + 3)) return false;

  uint8_t data[MAX_MESSAGE_LEN];
  memcpy(data, plainText, len);

  // Compute sessionSeed & nonce
  uint16_t sessionSeed = deriveSessionSeed(chipSerial, key);
  uint8_t nonce[BLOCK_SIZE];
  getNonce(nonce, idx, sessionSeed);

  // Encrypt
  AES_ctx ctx;
  AES_init_ctx_iv(&ctx, key, nonce);
  AES_CTR_xcrypt_buffer(&ctx, data, len);

  // Store idx (2 hex chars at start)
  outHex[0] = toHexNibble(idx >> 4);
  outHex[1] = toHexNibble(idx & 0x0F);

  // Store ciphertext
  bytesToHex(data, len, outHex + 2);

  return true;
}

// ------------------------------------------------------
// Decrypt: extracts idx, regenerates nonce, decrypts
// ------------------------------------------------------
bool decryptWithIdx(const char *inHex,
                    const uint8_t *key,
                    const char *chipSerial,
                    char *outPlain,
                    size_t outPlainLen) {
  if (!inHex) return false;
  size_t inLen = strlen(inHex);
  if (inLen < 2) return false;

  // First 2 hex chars = idx
  uint8_t idx = (fromHexChar(inHex[0]) << 4) | fromHexChar(inHex[1]);
  DEBUG_PRINTN(idx);

  const char *cipherHex = inHex + 2;

  uint8_t data[MAX_MESSAGE_LEN];
  size_t dataLen;


  if (!hexToBytes(cipherHex, data, &dataLen)) return false;
  if (dataLen > MAX_MESSAGE_LEN || outPlainLen < dataLen + 1) return false;

  // Compute same nonce
  uint16_t sessionSeed = deriveSessionSeed(chipSerial, key);
  uint8_t nonce[BLOCK_SIZE];
  getNonce(nonce, idx, sessionSeed);

  // Decrypt
  AES_ctx ctx;
  AES_init_ctx_iv(&ctx, key, nonce);
  AES_CTR_xcrypt_buffer(&ctx, data, dataLen);

  memcpy(outPlain, data, dataLen);
  outPlain[dataLen] = '\0';
  return true;
}

// ------------------------------------------------------
// Test using idx-based nonce (sends only 1 byte idx)
// ------------------------------------------------------
void aesInit(const char *msg) {
  DEBUG_PRINTN(F("----- AES TEST -----"));
  //DEBUG_PRINT(F("Msg: "));
  DEBUG_PRINTN(msg);

  //Get chip serial
  //char hwSerialKey[21];
  //getChipSerial(hwSerialKey, sizeof(hwSerialKey));

  //DEBUG_PRINT(F("Chip Serial: "));
  DEBUG_PRINTN(hwSerialKey);

  // Build AES key (same as before)
  uint8_t key[KEY_LEN] = { 0 };
  const char *last12 = hwSerialKey + 8;  // last 12 chars of 20-char serial
  memcpy(key + (KEY_LEN - 14), last12, 12);
  uint16_t rnd = (uint16_t)rand();
  rnd = 1;
  key[KEY_LEN - 2] = rnd >> 8;
  key[KEY_LEN - 1] = rnd & 0xFF;

  // Choose message index (simulate increment per packet)

  uint8_t idx = rand() % 32;
  //DEBUG_PRINT(F("Using idx: "));
  DEBUG_PRINTN(idx);

  // Encrypt → outHex = [idx(2 hex chars)] + [ciphertext]
  char encBuf[MAX_MESSAGE_LEN * 2 + 3];
  if (!encryptWithIdx(msg, key, hwSerialKey, idx, encBuf, sizeof(encBuf))) {
    DEBUG_PRINTN(F("Encrypt failed"));
    return;
  }
  //DEBUG_PRINT(F("Encrypted: "));
  DEBUG_PRINTN(encBuf);

  // Decrypt → extracts idx, regenerates nonce internally
  char decBuf[MAX_MESSAGE_LEN + 1];
  if (decryptWithIdx(encBuf, key, hwSerialKey, decBuf, sizeof(decBuf))) {
    //DEBUG_PRINT(F("Decrypted: "));
    DEBUG_PRINTN(decBuf);
  } else {
    DEBUG_PRINTN(F("Decrypt failed"));
  }

  DEBUG_PRINTN(F("--------------------"));
}
