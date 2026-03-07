// pairNode.h — Starter-side: Gateway ↔ Starter pairing (Starter as node/client)
//
// FLOW:
//   pairNodeTick()        → runs SM; IDLE does nothing (no auto-enter)
//   enterPairNodeMode()   → called from LCD menu → switchToPairChannel() → PAIR_NODE_BEACONING
//   BEACONING             → TX [0x0A][hwSerialKey:20] every 2 s
//   pairNodeHandleAck()   → on 0x0B: store GW info, schedule 0x0C TX → WAIT_TX_DONE
//   WAIT_TX_DONE (tick)   → send 0x0C, wait 1 s, switchToOperationalChannel()
//                         → IDLE (Remote pairing triggered independently via LCD menu)
//
// Requires (included before this header via .ino):
//   sx1268Main.h  → switchToPairChannel() / switchToOperationalChannel() / send_lora_data()
//   storage.h     → storage.gateway / storage.rf_locked / savecon()
//   lcdDisp.h     → lcd_set_cursor() / lcd_print()
//   debug.h       → DEBUG_PRINT / DEBUG_PRINTN
//   aesMain.h     → hwSerialKey[21]

#ifndef PAIR_NODE_H
#define PAIR_NODE_H

// forward declaration — defined in pairRemote.h (included after this file)
void enterRemPairMode();

// ── State enum ────────────────────────────────────────────────────────────────
typedef enum {
    PAIR_NODE_IDLE = 0,
    PAIR_NODE_BEACONING,     // TX [0x0A] every 2 s, listening for 0x0B
    PAIR_NODE_WAIT_TX_DONE,  // got 0x0B, 0x0C TX pending; waiting ~1 s before channel switch
} PairNodeState_t;

static PairNodeState_t pairNodeState      = PAIR_NODE_IDLE;
static unsigned long   pairNodeBeaconMs   = 0;    // timestamp of last beacon
static unsigned long   pairNodeDoneTxMs   = 0;    // timestamp when 0x0C was sent
static bool            pairNodeDonePending = false; // true until 0x0C TX is dispatched

static char  pairNodeGwId[21]   = {0};   // GW ID received in 0x0B
static uint8_t pairNodeAckSf    = OPER_SF;
static uint8_t pairNodeAckBwCode = 1;     // 1=125kHz
static uint8_t pairNodeAckCr    = OPER_CR;
static uint8_t pairNodeAckPre   = OPER_PREAMBLE;
static uint8_t pairNodeAckPwr   = OPER_TX_POWER;

// ── bwCode → SX1268 BW constant ──────────────────────────────────────────────
static uint8_t pairBwCodeToBw(uint8_t bwCode) {
    switch (bwCode) {
        case 1:  return SX1268_BW_125000;
        case 2:  return SX1268_BW_250000;
        case 3:  return SX1268_BW_500000;
        default: return SX1268_BW_125000;
    }
}

// ── LDRO: ON when symbol time >= 16 ms ───────────────────────────────────────
static uint8_t pairCalcLdro(uint8_t sf, uint8_t bwCode) {
    // BW125k: sf>=11 → LDRO=1; BW250k: sf>=12; BW500k: sf>=13
    if (bwCode == 1 && sf >= 11) return 1;
    if (bwCode == 2 && sf >= 12) return 1;
    if (bwCode == 3 && sf >= 13) return 1;
    return 0;
}

// ── LCD helpers ───────────────────────────────────────────────────────────────
static void pairNodeLcdBeaconing() {
    lcd_set_cursor(0, 0);
    lcd_print("GW  PAIRING...  ");
    lcd_set_cursor(1, 0);
    lcd_print("beaconing...    ");
}

static void pairNodeLcdFound(const char* gwId) {
    char buf[17];
    lcd_set_cursor(0, 0);
    lcd_print("GW  FOUND!      ");
    lcd_set_cursor(1, 0);
    // show last 8 chars of the 20-char GW ID
    snprintf(buf, sizeof(buf), "GW:%-13s", gwId + 12);
    lcd_print(buf);
}

static void pairNodeLcdPaired() {
    lcd_set_cursor(0, 0);
    lcd_print("GW  PAIRED      ");
    lcd_set_cursor(1, 0);
    lcd_print("OK              ");
}

// ── enterPairNodeMode() ───────────────────────────────────────────────────────
// Call to begin Gateway pairing.
// Called from LCD menu "PAIR GATEWAY" option only — never auto-triggered.
void enterPairNodeMode() {
    DEBUG_PRINTN("PairNode: entering GW pair mode");
    switchToPairChannel();
    pairNodeBeaconMs   = 0;          // trigger immediate first beacon
    pairNodeDonePending = false;
    pairNodeState      = PAIR_NODE_BEACONING;
    pairNodeLcdBeaconing();
}

// ── pairNodeHandleAck() ───────────────────────────────────────────────────────
// Called from dispatchPairPkt() when rx_buffer[0] == PKT_PAIR_ACK (0x0B).
// Packet: [0x0B][gw_id:20][sf:1][bwCode:1][cr:1][preamble:1][txPow:1] = 26 bytes
void pairNodeHandleAck(const uint8_t* buf, uint8_t len) {
    if (len < 26) {
        DEBUG_PRINTN("PairNode: PAIR_ACK too short, ignored");
        return;
    }
    if (pairNodeState != PAIR_NODE_BEACONING) {
        DEBUG_PRINTN("PairNode: PAIR_ACK not expected in this state");
        return;
    }

    // Extract 20-char GW ID
    memcpy(pairNodeGwId, &buf[1], 20);
    pairNodeGwId[20] = '\0';

    // Extract RF params sent by Gateway
    pairNodeAckSf     = buf[21];
    pairNodeAckBwCode = buf[22];
    pairNodeAckCr     = buf[23];
    pairNodeAckPre    = buf[24];
    pairNodeAckPwr    = buf[25];

    DEBUG_PRINT("PairNode: ACK from GW: ");
    DEBUG_PRINTN(pairNodeGwId);
    pairNodeLcdFound(pairNodeGwId);

    // Flag: pairNodeTick() will send 0x0C PAIR_DONE on the next loop pass
    pairNodeDonePending = true;
    pairNodeState       = PAIR_NODE_WAIT_TX_DONE;
}

// ── pairNodeTick() ────────────────────────────────────────────────────────────
// Call every loop(). Must be placed AFTER sx1268Func() in the loop body.
// Pairing is started ONLY via enterPairNodeMode() called from the LCD menu.
// No auto-entry on empty storage — user must trigger via "PAIR GATEWAY" menu item.
void pairNodeTick() {
    unsigned long now = millis();

    switch (pairNodeState) {

        case PAIR_NODE_IDLE:
            // Nothing — pairing starts only when called from the LCD menu.
            break;

        case PAIR_NODE_BEACONING:
            // Re-beacon every PAIR_BEACON_INTERVAL_MS
            if (now - pairNodeBeaconMs >= PAIR_BEACON_INTERVAL_MS) {
                uint8_t pkt[21];
                pkt[0] = PKT_PAIR_REQ;                    // 0x0A
                memcpy(&pkt[1], hwSerialKey, 20);          // starter chip serial
                send_lora_data(pkt, 21);
                pairNodeBeaconMs = now;
                DEBUG_PRINTN("PairNode: PAIR_REQ beacon sent");
            }
            break;

        case PAIR_NODE_WAIT_TX_DONE:
            // Step 1: send 0x0C PAIR_DONE on the pair channel (GW still listening here)
            if (pairNodeDonePending) {
                uint8_t pkt[5];
                pkt[0] = PKT_PAIR_DONE;                   // 0x0C
                memcpy(&pkt[1], hwSerialKey, 4);           // first 4 bytes as echo
                send_lora_data(pkt, 5);
                pairNodeDonePending = false;
                pairNodeDoneTxMs    = now;
                DEBUG_PRINTN("PairNode: PAIR_DONE sent");
                break;
            }

            // Step 2: wait ~1 s for TX to finish at SF7 (~10 ms ToA), then switch channel
            if (now - pairNodeDoneTxMs >= 1000UL) {
                // Save GW ID and mark RF as locked on operational channel
                memcpy(storage.gateway, pairNodeGwId, 21);
                storage.rf_locked = 1;
                savecon();
                DEBUG_PRINT("PairNode: GW paired & saved: ");
                DEBUG_PRINTN(storage.gateway);

                uint8_t operBw = pairBwCodeToBw(pairNodeAckBwCode);
                DEBUG_PRINT("PairNode: applying GW RF sf=");
                DEBUG_PRINT(pairNodeAckSf);
                DEBUG_PRINT(" bwCode=");
                DEBUG_PRINT(pairNodeAckBwCode);
                DEBUG_PRINT(" cr=");
                DEBUG_PRINT(pairNodeAckCr);
                DEBUG_PRINT(" pre=");
                DEBUG_PRINT(pairNodeAckPre);
                DEBUG_PRINT(" pwr=");
                DEBUG_PRINTN(pairNodeAckPwr);
                switchToOperationalChannelParams(pairNodeAckSf, operBw, pairNodeAckCr, pairNodeAckPre, pairNodeAckPwr);
                pairNodeLcdPaired();
                pairNodeState = PAIR_NODE_IDLE;
                // Remote pairing is independent — user triggers via LCD "PAIR REMOTE" if needed.
            }
            break;
    }
}

#endif // PAIR_NODE_H
