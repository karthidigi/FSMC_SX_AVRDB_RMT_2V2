// pairRemote.h — Starter-side: Starter ↔ Remote pairing (Starter as master/scanner)
//
// FLOW:
//   pairRemoteTick()       → runs SM; IDLE does nothing (no auto-enter)
//   enterRemPairMode()     → switchToPairChannel() → PAIR_REM_SCAN
//   pairRemHandleReq()     → on 0x0D: store rem serial, queue 0x0E ACK → WAIT_DONE
//   pairRemoteTick/WAIT_DONE → TX [0x0E], retry on timeout
//   pairRemHandleDone()    → on 0x0F: save storage.remote, switchToOperationalChannel()
//
// ALSO defines dispatchPairPkt() — the single binary-packet entry point on the Starter.
//
// Requires (included before via .ino):
//   sx1268Main.h  → switchToPairChannel() / switchToOperationalChannel() / send_lora_data()
//                   g_operSyncMsb / g_operSyncLsb (dynamic sync word)
//   storage.h     → storage.remote / savecon()
//   lcdDisp.h     → lcd_set_cursor() / lcd_print()
//   debug.h       → DEBUG_PRINT / DEBUG_PRINTN
//   aesMain.h     → hwSerialKey[21]

#ifndef PAIR_REMOTE_H
#define PAIR_REMOTE_H

// ── State enum ────────────────────────────────────────────────────────────────
typedef enum {
    PAIR_REM_IDLE = 0,
    PAIR_REM_SCAN,        // on pair channel, waiting for 0x0D from Remote
    PAIR_REM_WAIT_DONE,   // sent 0x0E, waiting for 0x0F; retry on timeout
} PairRemState_t;

static PairRemState_t pairRemState      = PAIR_REM_IDLE;
static unsigned long  pairRemTimer      = 0;    // scan-window / retry timer
static char           pairRemSerial[21] = {0};  // remote serial from 0x0D
static bool           pairRemAckPending = false; // true = 0x0E not yet sent
static uint8_t        pairRemRetries    = 0;

// ── LCD helpers ───────────────────────────────────────────────────────────────
static void pairRemLcdScanning() {
    lcd_set_cursor(0, 0);
    lcd_print("REM PAIRING...  ");
    lcd_set_cursor(1, 0);
    lcd_print("scanning...     ");
}

static void pairRemLcdFound(const char* remSerial) {
    char buf[17];
    lcd_set_cursor(0, 0);
    lcd_print("REM FOUND!      ");
    lcd_set_cursor(1, 0);
    snprintf(buf, sizeof(buf), "REM:%-12s", remSerial + 12);
    lcd_print(buf);
}

static void pairRemLcdPaired() {
    lcd_set_cursor(0, 0);
    lcd_print("REM PAIRED      ");
    lcd_set_cursor(1, 0);
    lcd_print("OK              ");
}

static void pairRemLcdTimeout() {
    lcd_set_cursor(0, 0);
    lcd_print("REM TIMEOUT     ");
    lcd_set_cursor(1, 0);
    lcd_print("retry later     ");
}

// ── Build and send 0x0E REM_PAIR_ACK ─────────────────────────────────────────
// [0x0E][starter_id:20][sf:1][bwCode:1][cr:1][preamble:1][txPow:1][syncMsb:1][syncLsb:1] = 28 bytes
// Bytes 26-27 carry the dynamic operational sync word so the Remote can apply
// the same channel as this Starter without any hardcoded constant.
static void pairRemSendAck() {
    uint8_t pkt[28];
    pkt[0]  = PKT_REM_PAIR_ACK;           // 0x0E
    memcpy(&pkt[1], hwSerialKey, 20);      // starter chip serial as starter_id
    pkt[21] = OPER_SF;
    pkt[22] = 1;                           // bwCode=1 → 125 kHz
    pkt[23] = OPER_CR;
    pkt[24] = OPER_PREAMBLE;
    pkt[25] = OPER_TX_POWER;
    pkt[26] = g_operSyncMsb;              // dynamic sync word MSB
    pkt[27] = g_operSyncLsb;              // dynamic sync word LSB
    send_lora_data(pkt, 28);
    DEBUG_PRINTN("PairRem: REM_PAIR_ACK sent (dyn sync)");
}

// ── enterRemPairMode() ────────────────────────────────────────────────────────
// Call to begin Remote pairing.
// Called from LCD menu "PAIR REMOTE" option, or automatically after GW pairing completes.
// Never auto-triggered from pairRemoteTick() IDLE.
void enterRemPairMode() {
    DEBUG_PRINTN("PairRem: entering Remote pair mode");
    switchToPairChannel();
    pairRemState      = PAIR_REM_SCAN;
    pairRemTimer      = millis();
    pairRemRetries    = 0;
    pairRemAckPending = false;
    pairRemLcdScanning();
}

// ── pairRemHandleReq() ────────────────────────────────────────────────────────
// Called from dispatchPairPkt() when rx_buffer[0] == PKT_REM_PAIR_REQ (0x0D).
// Packet: [0x0D][rem_serial:20] = 21 bytes minimum
void pairRemHandleReq(const uint8_t* buf, uint8_t len) {
    if (len < 21) {
        DEBUG_PRINTN("PairRem: REQ too short, ignored");
        return;
    }
    if (pairRemState != PAIR_REM_SCAN) {
        // Accept REQ also in WAIT_DONE (duplicate beacon) — re-send ACK
        if (pairRemState != PAIR_REM_WAIT_DONE) {
            DEBUG_PRINTN("PairRem: REQ not expected in this state");
            return;
        }
    }

    memcpy(pairRemSerial, &buf[1], 20);
    pairRemSerial[20] = '\0';
    DEBUG_PRINT("PairRem: 0x0D REQ from: ");
    DEBUG_PRINTN(pairRemSerial);

    // Provision remote serial as soon as REQ is validated so a missing 0x0F
    // does not leave operational AES peer unset.
    if (strncmp(storage.remote, pairRemSerial, 20) != 0) {
        memcpy(storage.remote, pairRemSerial, 21);
        savecon();
        DEBUG_PRINT("PairRem: provisioned remote from REQ: ");
        DEBUG_PRINTN(storage.remote);
    }

    pairRemLcdFound(pairRemSerial);
    pairRemAckPending = true;           // tick will call send_lora_data after 250 ms
    pairRemState      = PAIR_REM_WAIT_DONE;
    pairRemTimer      = millis();
    pairRemRetries    = 0;
    DEBUG_PRINTN("PairRem: ACK pending — will TX 0x0E after 250 ms hold-off");
}

// ── pairRemHandleDone() ───────────────────────────────────────────────────────
// Called from dispatchPairPkt() when rx_buffer[0] == PKT_REM_PAIR_DONE (0x0F).
// Packet: [0x0F][serial_echo:4] = 5 bytes minimum
void pairRemHandleDone(const uint8_t* buf, uint8_t len) {
    if (len < 5) {
        DEBUG_PRINTN("PairRem: DONE too short, ignored");
        return;
    }
    if (pairRemState != PAIR_REM_WAIT_DONE) {
        DEBUG_PRINTN("PairRem: DONE not expected in this state");
        return;
    }

    // Soft echo check — log mismatch but don't abort
    if (memcmp(&buf[1], pairRemSerial, 4) != 0) {
        DEBUG_PRINTN("PairRem: DONE echo mismatch (proceeding anyway)");
    }

    // Persist remote serial to storage
    memcpy(storage.remote, pairRemSerial, 21);
    savecon();
    DEBUG_PRINT("PairRem: Remote paired & saved: ");
    DEBUG_PRINTN(storage.remote);

    pairRemLcdPaired();
    switchToOperationalChannel();   // SF11/BW125/sync=0x3444
    pairRemState = PAIR_REM_IDLE;
}

// ── pairRemoteTick() ──────────────────────────────────────────────────────────
// Call every loop(). Must be placed AFTER sx1268Func() and pairNodeTick().
// Pairing is started ONLY via enterRemPairMode() from the LCD menu or after GW pairing.
// No auto-entry on empty storage — user must trigger via "PAIR REMOTE" menu item.
void pairRemoteTick() {
    unsigned long now = millis();

    switch (pairRemState) {

        case PAIR_REM_IDLE:
            // Nothing — pairing starts only when called from the LCD menu
            // (or automatically chained after GW pairing completes in pairNodeTick).
            break;

        case PAIR_REM_SCAN: {
            // Heartbeat every 5 s — confirms scanner is alive and loop is running.
            // If this stops printing the MCU is blocked; if it runs but no REQ
            // arrives the Remote is not in pair mode (trigger: hold M1_OFF+STA 5s).
            static unsigned long _scanBeat = 0;
            if (now - _scanBeat >= 5000UL) {
                _scanBeat = now;
                unsigned long elapsed = now - pairRemTimer;
                unsigned long remain  = (elapsed < PAIR_SCAN_WINDOW_MS)
                                        ? (PAIR_SCAN_WINDOW_MS - elapsed) / 1000UL : 0;
                DEBUG_PRINT(F("PairRem: scanning... "));
                DEBUG_PRINT(remain);
                DEBUG_PRINTN(F("s left — Remote: hold M1_OFF+STA 5s to pair"));
            }
            // Scan window timeout
            if (now - pairRemTimer >= PAIR_SCAN_WINDOW_MS) {
                DEBUG_PRINTN("PairRem: scan timeout");
                pairRemLcdTimeout();
                switchToOperationalChannel();
                pairRemState = PAIR_REM_IDLE;
            }
            break;
        }

        case PAIR_REM_WAIT_DONE:
            // Send pending ACK (set by pairRemHandleReq).
            // IMPORTANT: Wait 250 ms before sending 0x0E so the Remote has time
            // to finish its own 0x0D TX and fully enter RX mode.  Without this
            // guard the 0x0E preamble starts while the Remote is still in
            // STATE_TX_WAIT → STATE_RX_SETUP, causing SX1268 preamble-lock failure.
            // pairRemTimer is set to millis() inside pairRemHandleReq().
            if (pairRemAckPending) {
                if (now - pairRemTimer < 250UL) break;  // hold-off: wait for Remote RX
                pairRemSendAck();
                pairRemAckPending = false;
                pairRemTimer      = now;
                break;
            }

            // Retry timeout: re-send 0x0E if no 0x0F received
            if (now - pairRemTimer >= PAIR_ACK_TIMEOUT_MS) {
                if (pairRemRetries >= PAIR_ACK_MAX_RETRIES) {
                    DEBUG_PRINTN("PairRem: max retries reached, giving up");
                    pairRemLcdTimeout();
                    switchToOperationalChannel();
                    pairRemState = PAIR_REM_IDLE;
                } else {
                    pairRemRetries++;
                    DEBUG_PRINT("PairRem: retry ");
                    DEBUG_PRINTN(pairRemRetries);
                    pairRemSendAck();
                    pairRemTimer = now;
                }
            }
            break;
    }
}

// ── dispatchPairPkt() ─────────────────────────────────────────────────────────
// Single entry point for Remote pairing packets on the STARTER.
// Called from sx1268Main.h STATE_RX_WAIT when rx_buffer[0] is in 0x0A-0x0F range.
void dispatchPairPkt(const uint8_t* buf, uint8_t len) {
    switch (buf[0]) {
        case PKT_REM_PAIR_REQ:   // 0x0D — Remote → Starter
            pairRemHandleReq(buf, len);
            break;
        case PKT_REM_PAIR_DONE:  // 0x0F — Remote → Starter
            pairRemHandleDone(buf, len);
            break;
        default:
            DEBUG_PRINT("PairDisp: unknown binary pkt 0x");
            DEBUG_PRINTN(buf[0]);
            break;
    }
}

#endif // PAIR_REMOTE_H
