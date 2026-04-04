// =============================================
// sx1268Main.h – SX1268 LoRa Radio Driver
// Replaces: llcc68Main.h
// =============================================
// Operational channel: SF11/BW125/CR4-5/Preamble=12/Sync=0x3444/22dBm (LDRO=1)
// Pairing channel   : SF7 /BW125/CR4-5/Preamble=8 /Sync=0x1234/14dBm (LDRO=0)
// Frequency: 900 MHz,  TX Power: 22 dBm
// SF11+BW125 Time-on-Air: ~1053 ms per 32-byte packet (SF=11,BW=125,CR=4/5,pre=12,PL=32,LDRO=1)
// SF11+BW125 Data Rate  : ~54 bps  |  Expected Range: ~10–15 km (line of sight)
//
// Unlike LLCC68, SX1268 supports all SF/BW combinations.
// SF11: symbol time 16.4 ms → LDRO must be ON.
//
// TCXO: disabled by default (same as previous LLCC68 setup).
//   If your SX1268 module has a TCXO on DIO3, uncomment the
//   three lines marked "TCXO" in sx1268Init() below.

#ifndef SX1268_MAIN_H
#define SX1268_MAIN_H

#include <SPI.h>
#include "src/SX1268_driver.h"

volatile bool dio1_triggered = false;

// ── Dynamic operational sync word ────────────────────────────────────────────
// Derived from the full 20-char chip serial using a rotate-XOR hash.
// AVR128DB48 serials all share a "4240" prefix — hashing all 10 bytes ensures
// the unique tail bytes (positions 4-19) dominate the output so each
// installation gets a different sync word and starters cannot cross-talk.
// Sent to the Remote in bytes [26-27] of the 0x0E PKT_REM_PAIR_ACK packet.
uint8_t g_operSyncMsb = OPER_SYNC_MSB;  // default until derived
uint8_t g_operSyncLsb = OPER_SYNC_LSB;

// Hash all 10 bytes of the 20-char hex serial into 2 independent bytes.
// Two accumulators rotate in opposite directions so that identical prefix
// bytes ("4240") produce different partial states in each accumulator,
// and the unique suffix bytes resolve to a distinct (msb, lsb) pair.
static void initOperSyncWord() {
    uint8_t msb = 0xA5, lsb = 0x5A;  // asymmetric non-zero seeds
    for (uint8_t i = 0; i < 20; i += 2) {
        uint8_t b = (fromHexChar(hwSerialKey[i]) << 4) | fromHexChar(hwSerialKey[i + 1]);
        msb = ((msb << 1) | (msb >> 7)) ^ b;   // rotate-left  1, XOR byte
        lsb = ((lsb >> 1) | (lsb << 7)) ^ b;   // rotate-right 1, XOR byte
    }
    // Sanitize: avoid LoRaWAN public sync (0x34,0x44) and null bytes
    if (msb == 0x34 && lsb == 0x44) { lsb ^= 0x11; }
    if (msb == 0x00) msb = 0x10;
    if (lsb == 0x00) lsb = 0x04;
    g_operSyncMsb = msb;
    g_operSyncLsb = lsb;
    DEBUG_PRINT("DynSync: 0x");
    DEBUG_PRINT(g_operSyncMsb, HEX);
    DEBUG_PRINT(" 0x");
    DEBUG_PRINTN(g_operSyncLsb, HEX);
}

// ────────────────────────────────────────────────
// Forward declarations
// ────────────────────────────────────────────────
void send_lora_data(const uint8_t* data, uint8_t length);
void sx1268Init();
void sx1268Func();
void switchToPairChannel();
void switchToOperationalChannel();
// dispatchPairPkt is defined in pairRemote.h (included after sx1268Main.h)
void dispatchPairPkt(const uint8_t* buf, uint8_t len);

// ────────────────────────────────────────────────
// Radio state machine
// ────────────────────────────────────────────────
enum RadioState {
    STATE_IDLE,
    STATE_TX_SETUP,
    STATE_TX_WAIT,
    STATE_RX_SETUP,
    STATE_RX_WAIT
};

// ────────────────────────────────────────────────
// Globals
// ────────────────────────────────────────────────
static uint8_t radioBuf[32];        // shared TX/RX buffer
#define payload     radioBuf        // alias for TX
#define bufferRadio radioBuf        // alias for RX

static unsigned long state_start_time    = 0;
static unsigned long last_radio_activity = 0;  // updated on any DIO1 IRQ; drives watchdog
static const unsigned long TX_TIMEOUT_MS   = 5000UL;   // SF11/BW125 ToA ~1.1 s + 3.9 s margin
static const unsigned long WATCHDOG_PERIOD = 90000UL;  // 60 s — see watchdog note below

static RadioState radio_state     = STATE_IDLE;
static volatile bool stop_transmission = false;
// true while on pair channel (0x1234); false in operational mode (0x3444).
// Gates the pairing-packet byte-range check in STATE_RX_WAIT to prevent
// AES-encrypted operational packets from being misrouted to dispatchPairPkt().
static bool pairing_mode = false;

// ────────────────────────────────────────────────
// ISR – PORTC (PC4 = SX1268_DIO1 rising edge)
// Direct AVR port interrupt, identical pattern to the original LLCC68 code.
// ────────────────────────────────────────────────
ISR(PORTC_PORT_vect) {
    uint8_t flags = PORTC.INTFLAGS;
    PORTC.INTFLAGS = flags;         // clear ALL PORTC interrupt flags at once
    if (flags & PIN4_bm) {          // PC4 = SX1268_DIO1
        dio1_triggered = true;
    }
}

// ────────────────────────────────────────────────
// Non-blocking send (same interface as before)
// ────────────────────────────────────────────────
void send_lora_data(const uint8_t* data, uint8_t length) {
    if (length > 32) {
        DEBUG_PRINTN("Send Error: Data too long (>32 bytes)");
        return;
    }
    if (!bit_is_set(SREG, 7)) {
        DEBUG_PRINTN(F("Interrupts OFF - enabling"));
        sei();
    }
    memset(payload, 0, sizeof(payload));
    memcpy(payload, data, length);
    radio_state      = STATE_TX_SETUP;
    state_start_time = millis();
    DEBUG_PRINTN(" TX requested");
}

// ────────────────────────────────────────────────
// Channel-switch helpers
// Called from pairNode.h / pairRemote.h tick functions.
// Puts radio in standby, applies new params, resets state machine.
// ────────────────────────────────────────────────

static uint8_t sx1268CalcLdro(uint8_t sf, uint8_t bw) {
    // LDRO must be enabled when symbol time is >=16 ms.
    if (bw == SX1268_BW_125000 && sf >= 11) return 1;
    if (bw == SX1268_BW_250000 && sf >= 12) return 1;
    if (bw == SX1268_BW_500000 && sf >= 13) return 1;
    return 0;
}

// Switch to SF7/BW125/CR4-5/pre=8/sync=0x1234/14dBm  (LDRO=0, sym_time 1ms)
void switchToPairChannel() {
    SX1268_setStandby(SX1268_STANDBY_RC);
    SX1268_setModulationParamsLoRa(PAIR_SF, PAIR_BW, PAIR_CR, 0); // LDRO=0
    SX1268_setPacketParamsLoRa(PAIR_PREAMBLE, SX1268_HEADER_EXPLICIT, 32,
                               SX1268_CRC_ON, SX1268_IQ_STANDARD);
    // SX126x errata: re-apply IQ register after every setPacketParams call
    SX1268_fixInvertedIq(SX1268_IQ_STANDARD);
    uint8_t sw[2] = { PAIR_SYNC_MSB, PAIR_SYNC_LSB };
    SX1268_writeRegister(SX1268_REG_LORA_SYNC_WORD_MSB, sw, 2);
    SX1268_setTxParams(PAIR_TX_POWER, SX1268_PA_RAMP_200U);
    SX1268_clearIrqStatus(SX1268_IRQ_ALL);
    dio1_triggered   = false;
    radio_state      = STATE_IDLE;
    state_start_time = millis();
    pairing_mode     = true;   // pairing packets (0x0A-0x0F) now expected
    DEBUG_PRINTN("Radio: switched to PAIR channel (SF7/BW125/0x1234)");
}

// Switch to operational channel using compiled-in params + dynamic sync word.
void switchToOperationalChannel() {
    uint8_t ldro = sx1268CalcLdro(OPER_SF, OPER_BW);
    SX1268_setStandby(SX1268_STANDBY_RC);
    SX1268_setModulationParamsLoRa(OPER_SF, OPER_BW, OPER_CR, ldro);
    SX1268_setPacketParamsLoRa(OPER_PREAMBLE, SX1268_HEADER_EXPLICIT, 32,
                               SX1268_CRC_ON, SX1268_IQ_STANDARD);
    // SX126x errata: re-apply IQ register after every setPacketParams call
    SX1268_fixInvertedIq(SX1268_IQ_STANDARD);
    uint8_t sw[2] = { g_operSyncMsb, g_operSyncLsb };
    SX1268_writeRegister(SX1268_REG_LORA_SYNC_WORD_MSB, sw, 2);
    SX1268_setTxParams(OPER_TX_POWER, SX1268_PA_RAMP_200U);
    SX1268_clearIrqStatus(SX1268_IRQ_ALL);
    dio1_triggered   = false;
    radio_state      = STATE_IDLE;
    state_start_time = millis();
    pairing_mode     = false;  // operational: AES-encrypted packets only
    DEBUG_PRINT("Radio: switched to OPER channel sync=0x");
    DEBUG_PRINT(g_operSyncMsb, HEX);
    DEBUG_PRINTN(g_operSyncLsb, HEX);
}

// ────────────────────────────────────────────────
// Radio init
// ────────────────────────────────────────────────
void sx1268Init() {
    DEBUG_PRINTN("SX1268 Init Start");

    // Derive dynamic sync word from this starter's chip serial (hwSerialKey must
    // already be populated by getDeviceSerId() before sx1268Init() is called).
    initOperSyncWord();

    // Select SPI pin mapping (PA4=MOSI, PA5=MISO, PA6=SCK on AVR128DB48)
    SPI.pins(SX1268_MOSI, SX1268_MISO, SX1268_SCK);

    // Register the SPI bus and NSS / BUSY pins with the driver
    SX1268_setSPI(SPI, 0);                          // 0 = keep default (F_CPU/2 on AVR)
    SX1268_setPins(SX1268_NSS, SX1268_BUSY);

    // Set up pin modes and start SPI
    SX1268_begin();

    // Hardware reset
    SX1268_reset(SX1268_RESET_PIN);
    delay(10);
    DEBUG_PRINTN("Reset OK");

    // Standby using RC oscillator (must be first after reset)
    SX1268_setStandby(SX1268_STANDBY_RC);
    DEBUG_PRINTN("Standby OK");

    // ── TCXO: E22-900M22S has a 1.8 V TCXO on DIO3 ─────────────────────
    // MUST be configured BEFORE any calibration — without it the chip has
    // no oscillator reference and will not transmit.
    SX1268_setDio3AsTcxoCtrl(SX1268_DIO3_OUTPUT_1_8, SX1268_TCXO_DELAY_10);
    // Full calibration with TCXO running (RC64k, RC13M, PLL, ADC, image)
    SX1268_calibrate(0xFF);
    // Return to standby after calibration
    SX1268_setStandby(SX1268_STANDBY_RC);
    DEBUG_PRINTN("TCXO+Cal OK");
    // ─────────────────────────────────────────────────────────────────────

    // DC-DC converter (after TCXO is stable)
    SX1268_setRegulatorMode(SX1268_REGULATOR_DC_DC);
    DEBUG_PRINTN("DC-DC OK");

    // Image calibration for 902–928 MHz band (covers 900 MHz)
    // SX1268_calibrateImage(SX1268_CAL_IMG_902, SX1268_CAL_IMG_928);
    SX1268_calibrateImage(SX1268_CAL_IMG_863, SX1268_CAL_IMG_870);
    DEBUG_PRINTN("Img Cal OK");

    // DIO2 as internal RF-switch control (E22-900M22S uses DIO2 for TXEN)
    SX1268_setDio2AsRfSwitchCtrl(SX1268_DIO2_AS_RF_SWITCH);
    DEBUG_PRINTN("DIO2 RF switch OK");

    // LoRa packet type
    SX1268_setPacketType(SX1268_LORA_MODEM);
    DEBUG_PRINTN("LoRa OK");

    // Frequency: 900 MHz
// Before
// uint32_t rfFreq = ((uint64_t)900000000UL << SX1268_RF_FREQUENCY_SHIFT) / SX1268_RF_FREQUENCY_XTAL;

// After
uint32_t rfFreq = ((uint64_t)867000000UL << SX1268_RF_FREQUENCY_SHIFT) / SX1268_RF_FREQUENCY_XTAL;

    SX1268_setRfFrequency(rfFreq);
    DEBUG_PRINTN("867 MHz OK");

    // PA config: high-power path, 22 dBm (SX1262/SX1268)
    SX1268_setPaConfig(0x04, 0x07, 0x00, 0x01);
    // OCP: MUST be set to 0x38 (140 mA) for SX1262/SX1268 HP path (datasheet §13.4.4).
    // Reset default is 0x18 (60 mA) which current-starves the PA at 22 dBm, silently
    // reducing actual TX power by several dBm and killing range.
    uint8_t ocp = 0x38;
    SX1268_writeRegister(SX1268_REG_OCP_CONFIGURATION, &ocp, 1);

    // TX params: 22 dBm, 200 µs ramp time
    SX1268_setTxParams(22, SX1268_PA_RAMP_200U);
    DEBUG_PRINTN("TX 22 dBm OK");

    // ── Modulation Parameters ────────────────────────────────────────────────
    // SF  | BW     | ToA (32B,CR4/8,preamble=16) | LDRO | Note
    // -----|--------|------------------------------|------|------------------
    // SF12 | 250kHz | ~1380 ms                    | ON   | max range
    // SF10 | 250kHz | ~345 ms                     | OFF* | faster (*LDRO=1 harmless)
    //
    // MUST MATCH REMOTE! Only one line active at a time.
    // Note: SX1268_LDRO_ON was 0x00 in original driver (bug) — using literal 1

    // SF12 – maximum range (active):
    SX1268_setModulationParamsLoRa(OPER_SF, OPER_BW, OPER_CR, sx1268CalcLdro(OPER_SF, OPER_BW));
    // SF10 – faster, comment out SF12 line above and uncomment below:
    // SX1268_setModulationParamsLoRa(10, SX1268_BW_250000, SX1268_CR_4_8, 1);
    DEBUG_PRINTN("Operational modulation profile loaded");

    // Packet params: explicit header, 32-byte payload, CRC on
    SX1268_setPacketParamsLoRa(OPER_PREAMBLE, SX1268_HEADER_EXPLICIT, 32,
                               SX1268_CRC_ON, SX1268_IQ_STANDARD);
    SX1268_fixInvertedIq(SX1268_IQ_STANDARD);
    DEBUG_PRINTN("Pkt params OK");

    // Sync word: dynamic value derived from this starter's serial (initOperSyncWord above)
    uint8_t sw[2] = { g_operSyncMsb, g_operSyncLsb };
    SX1268_writeRegister(SX1268_REG_LORA_SYNC_WORD_MSB, sw, 2);
    DEBUG_PRINTN("Operational sync loaded");

    // Buffer base: TX at 0x00, RX at 0x80 (splits the 256-byte FIFO)
    SX1268_setBufferBaseAddress(0x00, 0x80);

    // SX126x errata: fix antenna switch resistance
    SX1268_fixResistanceAntenna();

    // IRQ: all relevant events routed to DIO1 only
    uint16_t irq_mask = SX1268_IRQ_TX_DONE  | SX1268_IRQ_RX_DONE  |
                        SX1268_IRQ_TIMEOUT   | SX1268_IRQ_HEADER_ERR |
                        SX1268_IRQ_CRC_ERR;
    SX1268_setDioIrqParams(irq_mask, irq_mask,
                           SX1268_IRQ_NONE,  SX1268_IRQ_NONE);
    DEBUG_PRINTN("IRQ OK");

    // DIO1 rising-edge interrupt – direct AVR port register (PC4)
    PORTC.DIRCLR   = PIN4_bm;                  // PC4 as input
    PORTC.PIN4CTRL = PORT_ISC_RISING_gc;       // trigger on rising edge
    DEBUG_PRINTN("Int OK");

    radio_state       = STATE_IDLE;
    state_start_time  = millis();
    stop_transmission = false;
    dio1_triggered    = false;
    last_radio_activity = millis();   // seed watchdog so it doesn't fire immediately

    DEBUG_PRINTN("SX1268 Init Complete!");
}

// ────────────────────────────────────────────────
// State machine (called every loop iteration)
// ────────────────────────────────────────────────
void sx1268Func() {
    // last_radio_activity is file-scope (declared above) so sx1268Init() can seed it.

    switch (radio_state) {

        case STATE_IDLE:
            // Immediately transition to RX; minimises time radio is not listening
            radio_state = STATE_RX_SETUP;
            // Do NOT update last_radio_activity here — only DIO1 IRQ events prove
            // the radio hardware is alive; state-machine transitions do not.
            break;

        case STATE_TX_SETUP: {
            // ── FIX: Always go through standby before TX ────────────────────
            // If the radio is in continuous RX (SX1268_RX_CONTINUOUS = 0xFFFFFF),
            // issuing SetTx directly from RX is unreliable on some SX1268 modules:
            // the SX1268_transfer() BUSY check fires during the RX→standby transition
            // and silently drops the opcode.  Calling setStandby() first puts the
            // radio into a known state so SetTx is guaranteed to be accepted.
            DEBUG_PRINTN("TX Setup: entering standby before TX");
            SX1268_setStandby(SX1268_STANDBY_RC);
            // ────────────────────────────────────────────────────────────────
            SX1268_setBufferBaseAddress(0x00, 0x80);
            SX1268_writeBuffer(0x00, bufferRadio, 32);
            dio1_triggered = false;
            SX1268_clearIrqStatus(SX1268_IRQ_ALL);
            DEBUG_PRINTN("TX Setup: issuing SetTx");
            SX1268_setTx(SX1268_TX_SINGLE);    // single TX, hardware IRQ on done/timeout
            radio_state      = STATE_TX_WAIT;
            state_start_time = millis();
            DEBUG_PRINTN("TX Setup: waiting for TX Done IRQ");
            break;
        }

        case STATE_TX_WAIT: {
            if (dio1_triggered) {
                last_radio_activity = millis(); // TX Done IRQ also proves radio is alive
                uint16_t irq_status;
                SX1268_getIrqStatus(&irq_status);
                DEBUG_PRINT("TX Done IRQ status=0x");
                DEBUG_PRINTN(irq_status, HEX);
                SX1268_clearIrqStatus(SX1268_IRQ_ALL);
                dio1_triggered = false;
                blueLedFlash();                  // BLUE: TX complete
                radio_state      = STATE_RX_SETUP;
                state_start_time = millis();
            } else if (millis() - state_start_time >= TX_TIMEOUT_MS) {
                DEBUG_PRINTN("TX Timeout — no IRQ within 5 s");
                SX1268_clearIrqStatus(SX1268_IRQ_ALL);
                radio_state      = STATE_RX_SETUP;
                state_start_time = millis();
            }
            break;
        }

        case STATE_RX_SETUP: {
            dio1_triggered = false;
            SX1268_setBufferBaseAddress(0x00, 0x80);
            SX1268_clearIrqStatus(SX1268_IRQ_ALL);
            SX1268_setRx(SX1268_RX_CONTINUOUS);    // 0xFFFFFF = continuous RX
            radio_state = STATE_RX_WAIT;
            DEBUG_PRINTN("RX Continuous Start");
            break;
        }

        case STATE_RX_WAIT: {
            if (dio1_triggered) {
                // Any IRQ (valid, CRC error, header error, timeout) proves the radio
                // hardware is alive and the DIO1 ISR is working — reset the watchdog.
                last_radio_activity = millis();
                uint16_t irq_status;
                SX1268_getIrqStatus(&irq_status);
                DEBUG_PRINT("RX IRQ status=0x");
                DEBUG_PRINTN(irq_status, HEX);

                // FIX: SX1268 sets both RX_DONE and CRC_ERR on bad packets.
                // Only process when RX_DONE is set AND no CRC error.
                if ((irq_status & SX1268_IRQ_RX_DONE) &&
                    !(irq_status & SX1268_IRQ_CRC_ERR)) {
                    blueLedFlash();                  // BLUE: valid packet received
                    uint8_t rx_length, rx_start;
                    SX1268_getRxBufferStatus(&rx_length, &rx_start);
                    SX1268_fixRxTimeout();          // SX126x errata: clear stale RTC

                    DEBUG_PRINT("RX len=");
                    DEBUG_PRINTN(rx_length);

                    if (rx_length > 0 && rx_length <= 32) {
                        uint8_t rx_buffer[32];
                        SX1268_readBuffer(rx_start, rx_buffer, rx_length);

                        // Print as hex (not char) — byte 0x0D casts to CR and
                        // overwrites the serial line, hiding the pairing packet.
                        DEBUG_PRINT(F("RX pkt[0]=0x"));
                        DEBUG_PRINT(rx_buffer[0], HEX);
                        DEBUG_PRINT(F(" len="));
                        DEBUG_PRINTN(rx_length);

                        // Pairing packets (0x0A-0x0F) only expected on the pair
                        // channel (sync=0x1234, pairing_mode=true). In operational
                        // mode (sync=0x3444) the radio cannot receive pairing packets,
                        // so all packets are AES-encrypted. Without this guard an
                        // encrypted ACK whose first byte is 0x0A-0x0F would be silently
                        // misrouted to dispatchPairPkt() and rxFunc() would never run.
                        if (pairing_mode &&
                            rx_buffer[0] >= PKT_REM_PAIR_REQ &&
                            rx_buffer[0] <= PKT_REM_PAIR_DONE) {
                            DEBUG_PRINT(F("→ PAIR pkt 0x"));
                            DEBUG_PRINTN(rx_buffer[0], HEX);
                            dispatchPairPkt(rx_buffer, rx_length);
                        } else {
                            DEBUG_PRINTN(F("→ encrypted payload"));
                            decryptNFunc(rx_buffer, rx_length);
                        }
                    } else {
                        DEBUG_PRINT(F("RX bad len="));
                        DEBUG_PRINTN(rx_length);
                    }

                    SX1268_clearIrqStatus(SX1268_IRQ_ALL);
                    dio1_triggered = false;
                    // Preserve STATE_TX_SETUP if rxFunc() queued an immediate reply
                    // via encryptNTx(). Setting STATE_IDLE here would overwrite it,
                    // causing the TX to be silently dropped (IDLE → RX_SETUP, not TX).
                    if (radio_state != STATE_TX_SETUP) {
                        radio_state = STATE_IDLE;
                    }
                    break;
                }
                else if (irq_status & SX1268_IRQ_CRC_ERR) {
                    // Continuous RX: radio self-re-arms after CRC error.
                    // Apply errata fix, clear IRQ, stay in STATE_RX_WAIT.
                    // Re-issuing SetRx (STATE_RX_SETUP) would create a deaf gap
                    // and cause cascading failures at range boundary.
                    DEBUG_PRINTN("RX CRC Error");
                    SX1268_fixRxTimeout();
                    SX1268_clearIrqStatus(SX1268_IRQ_ALL);
                    dio1_triggered = false;
                    // radio_state stays STATE_RX_WAIT
                    break;
                }
                else if (irq_status & SX1268_IRQ_HEADER_ERR) {
                    // Continuous RX: radio self-re-arms after header error.
                    DEBUG_PRINTN("RX Header Error");
                    SX1268_clearIrqStatus(SX1268_IRQ_ALL);
                    dio1_triggered = false;
                    // radio_state stays STATE_RX_WAIT
                    break;
                }
                else if (irq_status & SX1268_IRQ_TIMEOUT) {
                    // Timeout should not occur in continuous RX (0xFFFFFF).
                    // If it does, the radio exited RX — must re-issue SetRx.
                    DEBUG_PRINTN("RX Timeout IRQ");
                }

                SX1268_clearIrqStatus(SX1268_IRQ_ALL);
                dio1_triggered = false;
                radio_state = STATE_RX_SETUP;
            } else {
                // Continuous RX — no IRQ yet. Do NOT update last_radio_activity here.
                // Watchdog note: last_radio_activity is updated only when DIO1 fires
                // (any IRQ event). If no IRQ fires for WATCHDOG_PERIOD (60 s) the
                // radio is considered stuck and a full re-init is triggered below.
                // In normal operation the continuous-RX radio fires DIO1 on every
                // received packet (valid or noise), so 60 s of silence means the
                // radio or its ISR has locked up.
            }
            break;
        }
    }

    // // Watchdog: full re-init if radio is silent for WATCHDOG_PERIOD
    // if (millis() - last_radio_activity > WATCHDOG_PERIOD) {
    //     DEBUG_PRINTN("Radio watchdog reset");
    //     PORTC.PIN4CTRL = PORT_ISC_INTDISABLE_gc;   // disable PC4 IRQ before reset
    //     SX1268_reset(SX1268_RESET_PIN);
    //     sx1268Init();
    //     last_radio_activity = millis();
    // }
}

#endif // SX1268_MAIN_H
