// // =============================================
// // llcc68Main.h – MAXIMUM RANGE CONFIGURATION (SF10+BW250)
// // =============================================
// // Configuration: SF10, BW250, CR4/8, Preamble=16, Sync=0x12
// // Expected Range: ~4-6 km (line of sight, optimal conditions)
// // Time-on-Air: ~800ms per 32-byte packet
// // Data Rate: ~390 bps
// // NOTE: SF10/SF11 + BW125 UNSUPPORTED, SF11+BW250 ALSO UNSUPPORTED!

// #ifndef LLCC68_MAIN_H
// #define LLCC68_MAIN_H

// #include <SPI.h>
// #include "src/llcc68.h"
// #include "src/llcc68_hal.h"
// volatile bool dio1_triggered = false;
// // ────────────────────────────────────────────────
// // Forward declarations (fixes "not declared in this scope")
// // ────────────────────────────────────────────────
// void send_lora_data(const uint8_t* data, uint8_t length);
// void llcc68Init();
// void llcc68Func();

// // ────────────────────────────────────────────────
// // Radio state machine enum – MUST be BEFORE usage
// // ────────────────────────────────────────────────
// enum RadioState {
//     STATE_IDLE,
//     STATE_TX_SETUP,
//     STATE_TX_WAIT,
//     STATE_RX_SETUP,
//     STATE_RX_WAIT
// };

// // ────────────────────────────────────────────────
// // Globals
// // ────────────────────────────────────────────────
// static uint8_t radioBuf[32];           // Shared TX/RX buffer (32 bytes max)
// #define payload     radioBuf           // alias for TX
// #define bufferRadio radioBuf           // alias for RX

// extern volatile bool dio1_triggered;   // MUST be extern here

// static unsigned long state_start_time = 0;
// static const unsigned long TX_TIMEOUT_MS = 3500UL;  // SF10+BW250: ~800ms per packet + margin
// static const unsigned long WATCHDOG_PERIOD = 120000UL;

// static RadioState radio_state = STATE_IDLE;  // now declared here

// static volatile bool stop_transmission = false;

// typedef struct {
//     uint8_t dummy;
// } llcc68_context_t;

// static llcc68_context_t llcc68_context;

// // ────────────────────────────────────────────────
// // ISR – PORTC (PC4 = LLCC68_DIO1 rising edge)
// // ────────────────────────────────────────────────
// ISR(PORTC_PORT_vect) {
//     uint8_t flags = PORTC.INTFLAGS;
//     PORTC.INTFLAGS = flags;             // clear ALL flags at once (recommended)

//     if (flags & PIN4_bm) {              // PC4 = LLCC68_DIO1
//         dio1_triggered = true;
//     }
// }

// // ────────────────────────────────────────────────
// // HAL functions (unchanged)
// // ────────────────────────────────────────────────
// llcc68_hal_status_t llcc68_hal_write(const void* context,
//                                      const uint8_t* command, const uint16_t command_length,
//                                      const uint8_t* data, const uint16_t data_length) {
//     (void)context; // unused
//     digitalWrite(LLCC68_NSS, LOW);
//     for (uint16_t i = 0; i < command_length; i++) SPI.transfer(command[i]);
//     for (uint16_t i = 0; i < data_length; i++) SPI.transfer(data[i]);
//     digitalWrite(LLCC68_NSS, HIGH);
//     return LLCC68_HAL_STATUS_OK;
// }

// llcc68_hal_status_t llcc68_hal_read(const void* context,
//                                     const uint8_t* command, const uint16_t command_length,
//                                     uint8_t* data, const uint16_t data_length) {
//     (void)context; // unused
//     digitalWrite(LLCC68_NSS, LOW);
//     for (uint16_t i = 0; i < command_length; i++) SPI.transfer(command[i]);
//     for (uint16_t i = 0; i < data_length; i++) data[i] = SPI.transfer(0x00);
//     digitalWrite(LLCC68_NSS, HIGH);
//     return LLCC68_HAL_STATUS_OK;
// }

// llcc68_hal_status_t llcc68_hal_reset(const void* context) {
//     (void)context; // unused
//     digitalWrite(LLCC68_RESET, LOW);
//     delay(10);
//     digitalWrite(LLCC68_RESET, HIGH);
//     delay(10);
//     return LLCC68_HAL_STATUS_OK;
// }

// llcc68_hal_status_t llcc68_hal_wakeup(const void* context) {
//     (void)context; // unused
//     digitalWrite(LLCC68_NSS, LOW);
//     delayMicroseconds(200);
//     digitalWrite(LLCC68_NSS, HIGH);
//     return LLCC68_HAL_STATUS_OK;
// }

// // ────────────────────────────────────────────────
// // Non-blocking send
// // ────────────────────────────────────────────────
// void send_lora_data(const uint8_t* data, uint8_t length) {
//     if (length > 32) {
//         DEBUG_PRINTN("Send Error: Data too long (>32 bytes)");
//         return;
//     }

//     if (!bit_is_set(SREG, 7)) {
//         DEBUG_PRINTN(F("Interrupts OFF → enabling"));
//         sei();
//     }

//     memset(payload, 0, sizeof(payload));
//     memcpy(payload, data, length);

//     radio_state = STATE_TX_SETUP;
//     state_start_time = millis();
//     DEBUG_PRINTN(" TX requested");
// }

// // ────────────────────────────────────────────────
// // Init error handler
// // ────────────────────────────────────────────────
// void handleInitError(const char* errorMsg) {
//     DEBUG_PRINTN("Init Error: ");
//     DEBUG_PRINTN(errorMsg);
// }

// // ────────────────────────────────────────────────
// // RADIO INIT – now safe (enum + globals declared above)
// // ────────────────────────────────────────────────
// void llcc68Init() {
//     DEBUG_PRINTN("LLCC68 Transceiver Start");

//     pinMode(LLCC68_NSS,   OUTPUT);
//     pinMode(LLCC68_RESET, OUTPUT);
//     pinMode(LLCC68_BUSY,  INPUT);
//     pinMode(LLCC68_DIO1,  INPUT);

//     digitalWrite(LLCC68_NSS,   HIGH);
//     digitalWrite(LLCC68_RESET, HIGH);

//     SPI.begin();
//     SPI.setClockDivider(SPI_CLOCK_DIV16);

//     llcc68_hal_status_t hal_status = llcc68_hal_reset(&llcc68_context);
//     if (hal_status != LLCC68_HAL_STATUS_OK) handleInitError("Reset Fail");
//     delay(10);
//     DEBUG_PRINTN("Reset OK");

//     llcc68_status_t status;
//     status = llcc68_set_standby(&llcc68_context, LLCC68_STANDBY_CFG_RC);
//     if (status != LLCC68_STATUS_OK) handleInitError("Standby Fail");
//     DEBUG_PRINTN("Standby OK");

//     status = llcc68_set_pkt_type(&llcc68_context, LLCC68_PKT_TYPE_LORA);
//     if (status != LLCC68_STATUS_OK) handleInitError("Pkt Type Fail");
//     DEBUG_PRINTN("LoRa OK");

//     status = llcc68_set_rf_freq(&llcc68_context, 867000000);
//     if (status != LLCC68_STATUS_OK) handleInitError("Freq Fail");
//     DEBUG_PRINTN("867 MHz OK");

//     // SF10 + BW250 for MAXIMUM SUPPORTED RANGE (4-6 km)
//     // HARDWARE LIMITATIONS (llcc68.c:672-677):
//     // ❌ UNSUPPORTED: SF10+BW125, SF11+BW125, SF11+BW250
//     // ✅ SUPPORTED: SF10+BW250 (best available range)
//     llcc68_mod_params_lora_t mod_params = {
//         .sf = LLCC68_LORA_SF10,        // SF10 (max supported with long range)
//         .bw = LLCC68_LORA_BW_250,      // 250 kHz (MUST use BW250 with SF10/SF11)
//         .cr = LLCC68_LORA_CR_4_8,      // CR 4/8 (better error correction)
//         .ldro = 1                      // LDRO enabled (mandatory for SF10+)
//     };
//     status = llcc68_set_lora_mod_params(&llcc68_context, &mod_params);
//     if (status != LLCC68_STATUS_OK) {
//         handleInitError("Mod Params Fail - CRITICAL");
//         DEBUG_PRINTN("ERROR: Unsupported SF/BW combo!");
//         return;  // Do not proceed with broken config
//     }
//     DEBUG_PRINTN("SF10 BW250 CR4/8 OK");

//     // Packet parameters optimized for long range
//     llcc68_pkt_params_lora_t pkt_params = {
//         .preamble_len_in_symb = 16,            // 16 symbols (better sync)
//         .header_type = LLCC68_LORA_PKT_EXPLICIT,
//         .pld_len_in_bytes = 32,
//         .crc_is_on = 1,                        // CRC enabled
//         .invert_iq_is_on = 0
//     };
//     status = llcc68_set_lora_pkt_params(&llcc68_context, &pkt_params);
//     if (status != LLCC68_STATUS_OK) handleInitError("Pkt Params Fail");
//     DEBUG_PRINTN("Pkt OK (Preamble=16)");

//     // Set LoRa Sync Word (0x12 = private network, 0x34 = public)
//     status = llcc68_set_lora_sync_word(&llcc68_context, 0x12);
//     if (status != LLCC68_STATUS_OK) handleInitError("Sync Word Fail");
//     DEBUG_PRINTN("Sync Word=0x12 OK");

//     llcc68_pa_cfg_params_t pa_params = {
//         .pa_duty_cycle = 0x04,
//         .hp_max = 0x07,
//         .device_sel = 0x00,
//         .pa_lut = 0x01
//     };
//     status = llcc68_set_pa_cfg(&llcc68_context, &pa_params);
//     if (status != LLCC68_STATUS_OK) handleInitError("PA Fail");
//     DEBUG_PRINTN("PA OK");

//     status = llcc68_set_tx_params(&llcc68_context, 22, LLCC68_RAMP_200_US);
//     if (status != LLCC68_STATUS_OK) handleInitError("TX Params Fail");
//     DEBUG_PRINTN("TX OK");

//     status = llcc68_set_buffer_base_address(&llcc68_context, 0, 0);
//     if (status != LLCC68_STATUS_OK) handleInitError("Buf Fail");
//     DEBUG_PRINTN("Buf OK");

//     llcc68_irq_mask_t irq_mask =
//         LLCC68_IRQ_TX_DONE | LLCC68_IRQ_RX_DONE |
//         LLCC68_IRQ_TIMEOUT | LLCC68_IRQ_HEADER_ERROR |
//         LLCC68_IRQ_CRC_ERROR;

//     status = llcc68_set_dio_irq_params(&llcc68_context, irq_mask, irq_mask, 0, 0);
//     if (status != LLCC68_STATUS_OK) handleInitError("IRQ Fail");
//     DEBUG_PRINTN("IRQ OK");

//     // DIO1 interrupt setup (PC4 = LLCC68_DIO1)
//     PORTC.DIRCLR = PIN4_bm;
//     PORTC.PIN4CTRL = PORT_ISC_RISING_gc;

//     DEBUG_PRINTN("Int OK");
//     DEBUG_PRINTN("LLCC68 Init Complete!");

//     radio_state = STATE_IDLE;
//     state_start_time = millis();
//     stop_transmission = false;
//     dio1_triggered = false;
// }

// // ────────────────────────────────────────────────
// // STATE MACHINE
// // ────────────────────────────────────────────────
// void llcc68Func() {
//     llcc68_status_t status;
//     static unsigned long last_radio_activity = millis();

//     switch (radio_state) {
//         case STATE_IDLE:
//             // FAST RX RETURN: Immediately return to RX mode
//             // This minimizes the time when radio is not listening
//             radio_state = STATE_RX_SETUP;
//             last_radio_activity = millis();
//             break;

//         case STATE_TX_SETUP:
//             memcpy(bufferRadio, payload, 32);
//             status = llcc68_write_buffer(&llcc68_context, 0, bufferRadio, 32);
//             if (status != LLCC68_STATUS_OK) {
//                 DEBUG_PRINTN("Buf Wr Fail");
//                 radio_state = STATE_RX_SETUP;
//                 break;
//             }

//             dio1_triggered = false;
//             status = llcc68_set_tx(&llcc68_context, 0);
//             if (status != LLCC68_STATUS_OK) {
//                 DEBUG_PRINTN("TX Fail");
//                 radio_state = STATE_RX_SETUP;
//                 break;
//             }

//             radio_state = STATE_TX_WAIT;
//             state_start_time = millis();
//             break;

//         case STATE_TX_WAIT:
//             if (dio1_triggered) {
//                 DEBUG_PRINTN("TX Done");
//                 llcc68_clear_irq_status(&llcc68_context, LLCC68_IRQ_ALL);
//                 dio1_triggered = false;
//                 radio_state = STATE_RX_SETUP;
//             }
//             else if (millis() - state_start_time >= TX_TIMEOUT_MS) {
//                 DEBUG_PRINTN("TX Timeout");
//                 llcc68_clear_irq_status(&llcc68_context, LLCC68_IRQ_ALL);
//                 radio_state = STATE_RX_SETUP;
//             }
//             break;

//         case STATE_RX_SETUP:
//             dio1_triggered = false;
//             status = llcc68_set_rx(&llcc68_context, 0);  // continuous RX
//             if (status != LLCC68_STATUS_OK) {
//                 DEBUG_PRINTN("RX Start Fail");
//                 radio_state = STATE_IDLE;
//                 break;
//             }
//             radio_state = STATE_RX_WAIT;
//             DEBUG_PRINTN("RX Continuous Start");
//             break;

//         case STATE_RX_WAIT:
//             if (dio1_triggered) {
//                 llcc68_irq_mask_t irq_status;
//                 status = llcc68_get_irq_status(&llcc68_context, &irq_status);
//                 if (status == LLCC68_STATUS_OK) {
//                     if (irq_status & LLCC68_IRQ_RX_DONE) {
//                         llcc68_rx_buffer_status_t rx_status;
//                         status = llcc68_get_rx_buffer_status(&llcc68_context, &rx_status);
//                         if (status == LLCC68_STATUS_OK) {
//                             uint8_t rx_length = rx_status.pld_len_in_bytes;
//                             uint8_t rx_buffer[32];
//                             status = llcc68_read_buffer(&llcc68_context,
//                                                        rx_status.buffer_start_pointer,
//                                                        rx_buffer, rx_length);
//                             if (status == LLCC68_STATUS_OK) {
//                                 DEBUG_PRINT(F("RX raw: ["));
//                                 for (uint8_t i = 0; i < rx_length; ++i)
//                                     DEBUG_PRINT((char)rx_buffer[i]);
//                                 DEBUG_PRINTN("]");
//                                 decryptNFunc(rx_buffer, rx_length);

//                                 // OPTIMIZATION: Go to IDLE instead of RX_SETUP to allow
//                                 // faster TX response when loraAck is set
//                                 // This reduces latency for ACK transmission
//                                 llcc68_clear_irq_status(&llcc68_context, LLCC68_IRQ_ALL);
//                                 dio1_triggered = false;
//                                 radio_state = STATE_IDLE;
//                                 last_radio_activity = millis();
//                                 break;  // Exit early to process ACK faster
//                             }
//                         }
//                     }
//                     else if (irq_status & (LLCC68_IRQ_HEADER_ERROR |
//                                            LLCC68_IRQ_CRC_ERROR |
//                                            LLCC68_IRQ_TIMEOUT)) {
//                         DEBUG_PRINTN("RX Error IRQ");
//                     }
//                 }
//                 else {
//                     DEBUG_PRINTN("IRQ read fail");
//                 }

//                 llcc68_clear_irq_status(&llcc68_context, LLCC68_IRQ_ALL);
//                 dio1_triggered = false;
//                 radio_state = STATE_RX_SETUP;
//             }
//             break;
//     }

//     // Watchdog recovery
//     if (millis() - last_radio_activity > WATCHDOG_PERIOD) {
//         DEBUG_PRINTN("Radio watchdog reset");
//         llcc68_hal_reset(&llcc68_context);
//         llcc68Init();
//         last_radio_activity = millis();
//     }
// }

// #endif // LLCC68_MAIN_H
