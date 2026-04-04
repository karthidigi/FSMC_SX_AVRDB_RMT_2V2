// gwData.h — Starter → Gateway lightweight LoRa telemetry
// PKT_GW_TELEMETRY (0x20) sent by starter; PKT_GW_COMMAND (0x21) received.
// Replaces UART iotData.h path for the LoRa-Gateway channel.
//
// Include order: after storage.h, zSettings.h, sx1268Main.h, aesMain.h, encrypt.h
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  32-byte LoRa packet budget (SX1268 hard limit)                         │
// │  [PKT_GW_TELEMETRY 1B][idx_hex 2B][rnd_hex 4B][encBuf ≤24B][pad 1B]   │
// │  Overhead = 9 B  →  max plaintext = (32−9)/2 = 11 chars                │
// └─────────────────────────────────────────────────────────────────────────┘
//
// Telemetry packets (plaintext ≤ 11 chars):
//   V:%03d%03d%03d   (11 ch) — volRY, volYB, volBR [V, integer]
//   C:%03d%03d%03d   (11 ch) — curR×10, curY×10, curBM1×10 [0.1 A resolution]
//   P:%d%d%d%02d%02d  (9 ch) — m1StaVars, app2Run, modeM1, elst, a1et
//
// Trigger schedule:
//   V: → >5 V change on any phase  OR  10-min heartbeat
//   C: → motor starts (stopped→running, immediate)  OR  60-s heartbeat while running
//   P: → any status field changes   OR  5-min heartbeat

#ifndef GW_DATA_H
#define GW_DATA_H

// ── Pending-send flags (set by gwTxAll; cleared per-type after successful TX) ─
// Because SF11 rate gate allows only one packet per 1.5 s, gwTxAll() cannot
// send all three types at once.  These flags cause each type to retry on the
// next gwDataFunc() call until successfully sent.
static bool gwPendV = false;
static bool gwPendC = false;
static bool gwPendS = false;

// ── SF11 TX rate gate ─────────────────────────────────────────────────────────
// SF11+BW125 ToA ≈ 1053 ms.  A 1500 ms minimum gap between consecutive GW
// transmissions prevents send_lora_data() from overwriting radioBuf before
// TX_DONE fires.  All TX functions share this single gate via encryptNGw().
static unsigned long       gwLastTxMs      = 0;
static const unsigned long GW_MIN_TX_GAP_MS = 1500UL;

// ── encryptNGw() ──────────────────────────────────────────────────────────────
// Encrypts msg using storage.gateway (GW serial) as peerSerial, assembles a
// 32-byte PKT_GW_TELEMETRY packet, and hands it to send_lora_data().
// Returns true if the packet was queued; false if skipped (not paired, rate
// gate active, or encryption error).
//
// Packet layout on the wire:
//   byte  0   : PKT_GW_TELEMETRY (0x20)
//   bytes 1-2 : outer idx_hex
//   bytes 3-6 : rnd_hex
//   bytes 7-  : encBuf = inner_idx_hex(2) + cipherHex(plainLen×2)
//   remaining : 0x00 pad  (radioBuf is always zeroed before use)
//
// Gateway gwDecryptAndRoute() receives buf = packet+1 (type byte stripped by
// processReceived), so buf[0..1]=idx, buf[2..5]=rnd, buf[6..]=encBuf — matches
// gwBuildKey() / gw_decryptWithIdx() expectations exactly.
static bool encryptNGw(const char* msg) {
  if (!msg || msg[0] == '\0')     return false;
  if (storage.gateway[0] == '\0') return false;   // not yet paired with gateway

  if ((millis() - gwLastTxMs) < GW_MIN_TX_GAP_MS) return false;   // rate gate

  // ── Build AES key from last 12 hex chars of gateway serial + 2-byte rnd ──
  char peerSerial[21];
  strncpy(peerSerial, storage.gateway, sizeof(peerSerial) - 1);
  peerSerial[sizeof(peerSerial) - 1] = '\0';

  uint8_t key[KEY_LEN] = { 0 };
  const char* last12 = peerSerial + 8;   // bytes 8–19 of 20-char hex serial
  for (uint8_t i = 0; i < 6; i++) {
    char hx[3] = { last12[i * 2], last12[i * 2 + 1], '\0' };
    key[(KEY_LEN - 8) + i] = (uint8_t)strtoul(hx, NULL, 16);
  }
  randomSeed(millis());
  uint16_t rnd     = (uint16_t)random(1, 0xFFFF);
  key[KEY_LEN - 2] = (uint8_t)(rnd >> 8);
  key[KEY_LEN - 1] = (uint8_t)(rnd & 0xFF);
  uint8_t  idx     = (uint8_t)random(0, 16);

  // ── Encrypt ──────────────────────────────────────────────────────────────
  char encBuf[MAX_MESSAGE_LEN * 2 + 3];
  if (!encryptWithIdx(msg, key, peerSerial, idx, encBuf, sizeof(encBuf))) {
    DEBUG_PRINTN(F("GW encryptNGw: failed"));
    return false;
  }

  // ── Assemble 32-byte LoRa packet ─────────────────────────────────────────
  uint8_t pkt[32];
  memset(pkt, 0, sizeof(pkt));
  pkt[0] = PKT_GW_TELEMETRY;
  char hdr[7];
  snprintf(hdr, sizeof(hdr), "%02X%04X", (unsigned)idx, (unsigned)rnd);
  memcpy(pkt + 1, hdr, 6);                            // bytes 1-6 : idx + rnd hex
  uint8_t eLen = (uint8_t)strlen(encBuf);
  if (eLen > 24) eLen = 24;                           // safety clamp (≤11-ch plain → eLen ≤ 24)
  memcpy(pkt + 7, encBuf, eLen);                      // bytes 7-30 : encBuf

  send_lora_data(pkt, 32);
  gwLastTxMs = millis();
  DEBUG_PRINT(F("GW TX: "));
  DEBUG_PRINTN(msg);
  return true;
}

// ── gwTxVoltage() ─────────────────────────────────────────────────────────────
// Packet: "V:%03d%03d%03d"  (11 chars)  — volRY, volYB, volBR (integer volts)
// Triggers:
//   forced or gwPendV = true  → send immediately
//   >5 V change on any phase  → event-driven send
//   10-minute heartbeat       → keep gateway updated when voltage is stable
static void gwTxVoltage(bool forced) {
  if (gwPendV) forced = true;

  static unsigned long lastSentMs = 0;
  static int lastVRY = -999, lastVYB = -999, lastVBR = -999;

  bool voltChg   = (abs((int)volRY - lastVRY) > 5) ||
                   (abs((int)volYB - lastVYB) > 5) ||
                   (abs((int)volBR - lastVBR) > 5);
  bool heartbeat = (millis() - lastSentMs) >= 600000UL;   // 10 min

  if (!forced && !voltChg && !heartbeat) return;

  char msg[12];
  snprintf(msg, sizeof(msg), "V:%03d%03d%03d",
           (int)constrain((int)volRY, 0, 999),
           (int)constrain((int)volYB, 0, 999),
           (int)constrain((int)volBR, 0, 999));

  if (encryptNGw(msg)) {
    gwPendV    = false;
    lastVRY    = (int)volRY;
    lastVYB    = (int)volYB;
    lastVBR    = (int)volBR;
    lastSentMs = millis();
  }
}

// ── gwTxCurrent() ─────────────────────────────────────────────────────────────
// Packet: "C:%03d%03d%03d"  (11 chars)
//   curR×10, curY×10, curBM1×10  — 0.1 A resolution, range 0.0–99.9 A
//   e.g. "C:153124051" = R:15.3A  Y:12.4A  B:5.1A
// Triggers:
//   forced or gwPendC = true  → send immediately (motor start)
//   60-second heartbeat       → only while motor is running
static void gwTxCurrent(bool forced) {
  if (gwPendC) forced = true;

  static unsigned long lastSentMs = 0;
  bool heartbeat = (bool)m1StaVars && ((millis() - lastSentMs) >= 60000UL);

  if (!forced && !heartbeat) return;

  int cR  = (int)constrain((int)(curR   * 10.0f + 0.5f), 0, 999);
  int cY  = (int)constrain((int)(curY   * 10.0f + 0.5f), 0, 999);
  int cBM = (int)constrain((int)(curBM1 * 10.0f + 0.5f), 0, 999);

  char msg[12];
  snprintf(msg, sizeof(msg), "C:%03d%03d%03d", cR, cY, cBM);

  if (encryptNGw(msg)) {
    gwPendC    = false;
    lastSentMs = millis();
  }
}

// ── gwTxStatus() ──────────────────────────────────────────────────────────────
// Packet: "P:%d%d%d%02d%02d"  (9 chars)
//   field 1 : m1StaVars  (0/1)  motor running
//   field 2 : app2Run    (0/1)  relay/switch
//   field 3 : modeM1     (0-5)  operating mode
//   field 4 : elst       (00-99) fault/error status
//   field 5 : a1et       (00-99) app1 event type
//   e.g. "P:10002510" = motor:ON relay:OFF mode:0 elst:02 a1et:10
// Triggers:
//   forced or gwPendS = true → send immediately
//   any of the 5 fields changes → event-driven send
//   5-minute heartbeat         → keep gateway updated when idle
static void gwTxStatus(bool forced) {
  if (gwPendS) forced = true;

  static unsigned long lastSentMs = 0;
  static int lM1 = -1, lA2 = -1, lMd = -1, lEl = -1, lEt = -1;

  int cM1 = (int)m1StaVars;
  int cA2 = (int)storage.app2Run;
  int cMd = (int)storage.modeM1;
  int cEl = (int)elst;
  int cEt = (int)a1et;

  bool changed   = (cM1 != lM1) || (cA2 != lA2) || (cMd != lMd) ||
                   (cEl != lEl) || (cEt != lEt);
  bool heartbeat = (millis() - lastSentMs) >= 300000UL;   // 5 min

  if (!forced && !changed && !heartbeat) return;

  char msg[10];
  snprintf(msg, sizeof(msg), "P:%d%d%d%02d%02d",
           cM1,
           cA2,
           cMd,
           constrain(cEl, 0, 99),
           constrain(cEt, 0, 99));

  if (encryptNGw(msg)) {
    gwPendS    = false;
    lM1        = cM1;
    lA2        = cA2;
    lMd        = cMd;
    lEl        = cEl;
    lEt        = cEt;
    lastSentMs = millis();
  }
}

// ── gwTxAll() ─────────────────────────────────────────────────────────────────
// Request a forced snapshot of all three telemetry types.
// Called when gateway sends [IAK] (init-ACK) or [STA] (status request).
// Due to the SF11 rate gate (1500 ms/packet), V: sends first; C: and P:
// send on successive gwDataFunc() calls.  gwPend* flags guarantee they are
// not silently dropped while the gate is active.
void gwTxAll() {
  gwPendV = true;
  gwPendC = (bool)m1StaVars;   // C: only useful when motor is running
  gwPendS = true;
  DEBUG_PRINTN(F("GW TxAll pending"));
}

// ── gwDiag() ──────────────────────────────────────────────────────────────────
// One-shot diagnostic — prints every packet plaintext and its expected MQTT JSON
// to Serial3 using current live sensor values.
// Call once from setup() (after emonInit + loadcon) or from a debug menu entry
// to verify packet format before going to production.
//
// Expected Serial3 output example:
//   [V pkt] V:440441442      [V  TS] {"a":440,"b":441,"c":442}
//   [C pkt] C:153124051      [C  TS] {"d":"15.3","e":"12.4","f":"5.1"}
//   [P pkt] P:10002510       [P  TS] {"j":1,"l":0,"o":0,"n":10,"k":10}
//                            [P  AT] {"a1ops":"2"}
void gwDiag() {
  char buf[64];
  int cR  = (int)constrain((int)(curR   * 10.0f + 0.5f), 0, 999);
  int cY  = (int)constrain((int)(curY   * 10.0f + 0.5f), 0, 999);
  int cBM = (int)constrain((int)(curBM1 * 10.0f + 0.5f), 0, 999);
  int cEl = (int)constrain((int)elst, 0, 99);
  int cEt = (int)constrain((int)a1et, 0, 99);

  Serial3.println(F("=== gwDiag START ==="));

  // ── V: voltage packet ──────────────────────────────────────────────────────
  snprintf(buf, sizeof(buf), "V:%03d%03d%03d",
           (int)constrain((int)volRY, 0, 999),
           (int)constrain((int)volYB, 0, 999),
           (int)constrain((int)volBR, 0, 999));
  Serial3.print(F("[V pkt] ")); Serial3.println(buf);
  snprintf(buf, sizeof(buf), "{\"a\":%d,\"b\":%d,\"c\":%d}",
           (int)constrain((int)volRY, 0, 999),
           (int)constrain((int)volYB, 0, 999),
           (int)constrain((int)volBR, 0, 999));
  Serial3.print(F("[V  TS] ")); Serial3.println(buf);

  // ── C: current packet ─────────────────────────────────────────────────────
  snprintf(buf, sizeof(buf), "C:%03d%03d%03d", cR, cY, cBM);
  Serial3.print(F("[C pkt] ")); Serial3.println(buf);
  snprintf(buf, sizeof(buf), "{\"d\":\"%d.%d\",\"e\":\"%d.%d\",\"f\":\"%d.%d\"}",
           cR / 10, cR % 10, cY / 10, cY % 10, cBM / 10, cBM % 10);
  Serial3.print(F("[C  TS] ")); Serial3.println(buf);

  // ── P: status packet ──────────────────────────────────────────────────────
  snprintf(buf, sizeof(buf), "P:%d%d%d%02d%02d",
           (int)m1StaVars, (int)storage.app2Run,
           (int)storage.modeM1, cEl, cEt);
  Serial3.print(F("[P pkt] ")); Serial3.println(buf);
  snprintf(buf, sizeof(buf), "{\"j\":%d,\"l\":%d,\"o\":%d,\"n\":%d,\"k\":%d}",
           (int)m1StaVars, (int)storage.app2Run, cEl, cEt, cEt);
  Serial3.print(F("[P  TS] ")); Serial3.println(buf);
  snprintf(buf, sizeof(buf), "{\"a1ops\":\"%d\"}", (int)storage.modeM1);
  Serial3.print(F("[P  AT] ")); Serial3.println(buf);

  // ── GW paired? ────────────────────────────────────────────────────────────
  Serial3.print(F("[GW SER] "));
  Serial3.println(storage.gateway[0] ? storage.gateway : "(not paired)");
  Serial3.println(F("=== gwDiag END ==="));
}

// ── gwDataFunc() ──────────────────────────────────────────────────────────────
// Call every main loop iteration.
// Manages all event-driven and heartbeat LoRa→Gateway telemetry:
//   1. Motor start (stopped → running)  → immediate C: packet
//   2. Voltage change > 5 V             → V: packet
//   3. Any status field change           → P: packet
//   4. Heartbeat timers                  → periodic V: / C: / P: refreshes
void gwDataFunc() {
  if (storage.gateway[0] == '\0') return;   // not yet GW-paired — nothing to do

  // ── Motor start detection: stopped → running ────────────────────────────
  // User requirement: "IR IY IB to be updated immediately when motor starts"
  static bool prevM1Sta = false;
  bool nowM1Sta = (bool)m1StaVars;
  if (nowM1Sta && !prevM1Sta) {
    gwTxCurrent(true);    // immediate send — motor just started from stop
  }
  prevM1Sta = nowM1Sta;

  // ── Event-driven + heartbeat telemetry ───────────────────────────────────
  // encryptNGw() rate-gates at GW_MIN_TX_GAP_MS so at most one packet is
  // queued per gwDataFunc() call.  Pending flags retry on future calls.
  gwTxVoltage(false);
  gwTxStatus(false);
  if (nowM1Sta) {
    gwTxCurrent(false);   // 60-s heartbeat while running
  }
}

#endif // GW_DATA_H
