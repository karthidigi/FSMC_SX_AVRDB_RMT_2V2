// userRow.h
// AVR128DB48 User Row (USERROW) management — RESERVED FOR FUTURE USE
//
// The USERROW is 32 bytes of non-volatile memory that survives a chip erase.
// Intended use: store first-flash date/time so production units can be identified.
//
// Layout (32 bytes):
//   [0..11]  Build date string from __DATE__  e.g. "Mar 13 2026\0"  (12 bytes)
//   [12..20] Build time string from __TIME__  e.g. "12:30:45\0"     ( 9 bytes)
//   [21]     Valid marker = 0xA5                                     ( 1 byte)
//   [22..31] Reserved (0xFF)
//
// ── DISABLED — uncomment when needed ────────────────────────────────────────

/*

#pragma once
#include <Arduino.h>
#include <avr/io.h>

// ── USERROW layout constants ─────────────────────────────────────────────────
#define UROW_DATE_OFFSET    0u
#define UROW_DATE_LEN      12u   // "Mmm DD YYYY\0"  (__DATE__ is always 11 chars)
#define UROW_TIME_OFFSET   12u
#define UROW_TIME_LEN       9u   // "HH:MM:SS\0"     (__TIME__ is always  8 chars)
#define UROW_VALID_OFFSET  21u
#define UROW_VALID_MARKER  0xA5u

// Defensive fallback in case old toolchain headers lack the gc-enum names
#ifndef NVMCTRL_CMD_EEERWR_gc
  #define NVMCTRL_CMD_EEERWR_gc   ((uint8_t)0x06)
#endif
#ifndef NVMCTRL_CMD_NOCMD_gc
  #define NVMCTRL_CMD_NOCMD_gc    ((uint8_t)0x00)
#endif

// ── Private helpers ──────────────────────────────────────────────────────────

static inline void _urow_waitBusy() {
  while (NVMCTRL.STATUS & NVMCTRL_EEBUSY_bm) { }
}

// Write a single byte to USERROW[offset] using the atomic EEPROM Erase+Write
// command. Safe to call byte-by-byte; each call performs its own erase-write.
static void _urowWriteByte(uint8_t offset, uint8_t val) {
  if (offset >= 32u) return;
  volatile uint8_t* dest = (volatile uint8_t*)&USERROW + offset;
  _urow_waitBusy();
  _PROTECTED_WRITE(NVMCTRL.CTRLA, NVMCTRL_CMD_EEERWR_gc);
  *dest = val;
  _urow_waitBusy();
  _PROTECTED_WRITE(NVMCTRL.CTRLA, NVMCTRL_CMD_NOCMD_gc);
}

// ── Public API ───────────────────────────────────────────────────────────────

inline bool isUserRowValid() {
  const uint8_t* ur = (const uint8_t*)&USERROW;
  return ur[UROW_VALID_OFFSET] == UROW_VALID_MARKER;
}

// Write build date/time to USERROW once at first boot after flashing.
// Subsequent boots return immediately — preserves original manufacture date.
void initUserRow() {
  if (isUserRowValid()) return;

  const char* buildDate = __DATE__;
  const char* buildTime = __TIME__;

  for (uint8_t i = 0; i < UROW_DATE_LEN; i++) {
    _urowWriteByte(UROW_DATE_OFFSET + i, (i < 11u) ? (uint8_t)buildDate[i] : 0u);
  }
  for (uint8_t i = 0; i < UROW_TIME_LEN; i++) {
    _urowWriteByte(UROW_TIME_OFFSET + i, (i < 8u)  ? (uint8_t)buildTime[i] : 0u);
  }
  _urowWriteByte(UROW_VALID_OFFSET, UROW_VALID_MARKER);
}

// Erase valid marker (use for rework / test jigs to force a date re-write).
void clearUserRow() {
  _urowWriteByte(UROW_VALID_OFFSET, 0xFFu);
}

// Copy stored build-date into buf (caller: at least 12 bytes).
void getUserRowDate(char* buf, uint8_t bufLen) {
  if (bufLen == 0) return;
  if (!isUserRowValid()) {
    strncpy(buf, "--DATE N/A--", bufLen - 1u);
    buf[bufLen - 1u] = '\0';
    return;
  }
  const uint8_t* ur = (const uint8_t*)&USERROW;
  uint8_t n = (bufLen - 1u < UROW_DATE_LEN) ? (bufLen - 1u) : UROW_DATE_LEN;
  for (uint8_t i = 0; i < n; i++) {
    buf[i] = (char)ur[UROW_DATE_OFFSET + i];
    if (buf[i] == '\0') { n = i; break; }
  }
  buf[n] = '\0';
}

// Copy stored build-time into buf (caller: at least 9 bytes).
void getUserRowTime(char* buf, uint8_t bufLen) {
  if (bufLen == 0) return;
  if (!isUserRowValid()) {
    strncpy(buf, "--:--:--", bufLen - 1u);
    buf[bufLen - 1u] = '\0';
    return;
  }
  const uint8_t* ur = (const uint8_t*)&USERROW;
  uint8_t n = (bufLen - 1u < UROW_TIME_LEN) ? (bufLen - 1u) : UROW_TIME_LEN;
  for (uint8_t i = 0; i < n; i++) {
    buf[i] = (char)ur[UROW_TIME_OFFSET + i];
    if (buf[i] == '\0') { n = i; break; }
  }
  buf[n] = '\0';
}

*/  // end of USERROW block — uncomment to re-enable
