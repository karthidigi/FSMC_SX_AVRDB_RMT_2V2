// ---------- FILE: llcc68Main.h ----------
#include <SPI.h>
#include "src/llcc68.h"
#include "src/llcc68_hal.h"


// static uint8_t payload[32];
// static uint8_t bufferRadio[32];

static uint8_t radioBuf[32];  // Shared TX/RX buffer
#define payload radioBuf      // alias for TX
#define bufferRadio radioBuf  // alias for RX

static unsigned long state_start_time = 0;
static const unsigned long TX_TIMEOUT_MS = 500UL;
static const unsigned long RX_TIMEOUT_MS = 500UL;
static const unsigned long IDLE_DURATION_MS = 100UL;

// Flag to stop transmission (set to true to abort TX/RX cycle)
volatile bool stop_transmission = false;

// Interrupt handler for DIO1
volatile bool dio1_triggered = false;

// Context placeholder
typedef struct {
  uint8_t dummy;
} llcc68_context_t;

static llcc68_context_t llcc68_context;

static inline void dio1_isr() {
  dio1_triggered = true;
  DEBUG_PRINTN("DIO1 Triggered");
}

enum RadioState { STATE_IDLE,
                  STATE_TX_SETUP,
                  STATE_TX_WAIT,
                  STATE_RX_SETUP,
                  STATE_RX_WAIT };

static RadioState radio_state = STATE_IDLE;


// HAL write/read/reset/wakeup - minimal
llcc68_hal_status_t llcc68_hal_write(const void* context, const uint8_t* command, const uint16_t command_length,
                                     const uint8_t* data, const uint16_t data_length) {
  digitalWrite(LLCC68_NSS, LOW);
  for (uint16_t i = 0; i < command_length; i++) SPI.transfer(command[i]);
  for (uint16_t i = 0; i < data_length; i++) SPI.transfer(data[i]);
  digitalWrite(LLCC68_NSS, HIGH);
  return LLCC68_HAL_STATUS_OK;
}

llcc68_hal_status_t llcc68_hal_read(const void* context, const uint8_t* command, const uint16_t command_length,
                                    uint8_t* data, const uint16_t data_length) {
  digitalWrite(LLCC68_NSS, LOW);
  for (uint16_t i = 0; i < command_length; i++) SPI.transfer(command[i]);
  for (uint16_t i = 0; i < data_length; i++) data[i] = SPI.transfer(0x00);
  digitalWrite(LLCC68_NSS, HIGH);
  return LLCC68_HAL_STATUS_OK;
}

llcc68_hal_status_t llcc68_hal_reset(const void* context) {
  digitalWrite(LLCC68_RESET, LOW);
  delay(10);
  digitalWrite(LLCC68_RESET, HIGH);
  delay(10);
  return LLCC68_HAL_STATUS_OK;
}

llcc68_hal_status_t llcc68_hal_wakeup(const void* context) {
  digitalWrite(LLCC68_NSS, LOW);
  delayMicroseconds(200);
  digitalWrite(LLCC68_NSS, HIGH);
  return LLCC68_HAL_STATUS_OK;
}

// Non-blocking send
static inline void send_lora_data(const uint8_t* data, uint8_t length) {
  if (length > 32) {
    DEBUG_PRINTN("Send Error: Data too long (>32 bytes)");
    return;
  }
  if (!bit_is_set(SREG, 7)) {
    DEBUG_PRINTN(F("Interrupts are OFF, enabling now"));
    sei();
  }
  memset(payload, 0, sizeof(payload));
  memcpy(payload, data, length);
  radio_state = STATE_TX_SETUP;
  state_start_time = millis();
  DEBUG_PRINTN(" TX requested");
}


// Helper function for initialization error handling
void handleInitError(const char* errorMsg) {
  DEBUG_PRINTN("Init Error: ");
  DEBUG_PRINTN(errorMsg);

  // Optional: Setup error LED blinking
  // pinMode(ERROR_LED_PIN, OUTPUT);
  // while(1) {
  //   digitalWrite(ERROR_LED_PIN, HIGH); delay(200);
  //   digitalWrite(ERROR_LED_PIN, LOW); delay(200);
  // }

  //   // Enable WDT for auto-reset after ~2s
  // #ifdef __AVR_ATtiny1606__
  //   wdt_enable(WDTO_2S);  // ~2s timeout
  // #else
  //   _PROTECTED_WRITE(WDT.CTRLA, WDT_PERIOD_2KCLK_gc);  // ~2s timeout at 32kHz
  // #endif

  // while (1) {
  //   delay(1);
  // }
}

void llcc68Init() {

  //DEBUG_PRINTN("LLCC68 Transceiver Start");

  // Initialize pins
  pinMode(LLCC68_NSS, OUTPUT);
  pinMode(LLCC68_RESET, OUTPUT);
  pinMode(LLCC68_BUSY, INPUT);
  pinMode(LLCC68_DIO1, INPUT);
  digitalWrite(LLCC68_NSS, HIGH);
  digitalWrite(LLCC68_RESET, HIGH);

  // Initialize SPI
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV16);  // 1.25 MHz SPI clock (20 MHz / 16)

  // Reset radio
  llcc68_hal_status_t hal_status = llcc68_hal_reset(&llcc68_context);
  if (hal_status != LLCC68_HAL_STATUS_OK) {
    handleInitError("Reset Fail");
  }
  delay(10);
  //DEBUG_PRINTN("Reset OK");

  // Set to standby
  llcc68_status_t status = llcc68_set_standby(&llcc68_context, LLCC68_STANDBY_CFG_RC);
  if (status != LLCC68_STATUS_OK) {
    handleInitError("Standby Fail");
  }
  //DEBUG_PRINTN("Standby OK");

  // Configure LoRa modulation
  status = llcc68_set_pkt_type(&llcc68_context, LLCC68_PKT_TYPE_LORA);
  if (status != LLCC68_STATUS_OK) {
    handleInitError("Pkt Type Fail");
  }
  //DEBUG_PRINTN("LoRa OK");

  // Set frequency to 867 MHz
  status = llcc68_set_rf_freq(&llcc68_context, 867000000);
  if (status != LLCC68_STATUS_OK) {
    handleInitError("Freq Fail");
  }
  //DEBUG_PRINTN("867 MHz OK");

  // Set modulation parameters: SF9, BW 125 kHz, CR 4/5
  llcc68_mod_params_lora_t mod_params = {
    .sf = LLCC68_LORA_SF9,
    .bw = LLCC68_LORA_BW_125,
    .cr = LLCC68_LORA_CR_4_5,
    .ldro = 1
  };
  status = llcc68_set_lora_mod_params(&llcc68_context, &mod_params);
  if (status != LLCC68_STATUS_OK) {
    handleInitError("Mod Params Fail");
  }
  //DEBUG_PRINTN("Mod OK");

  // Set packet parameters: 12-symbol preamble, explicit header, 32-byte payload, CRC on
  llcc68_pkt_params_lora_t pkt_params = {
    .preamble_len_in_symb = 12,
    .header_type = LLCC68_LORA_PKT_EXPLICIT,
    .pld_len_in_bytes = 32,
    .crc_is_on = 1,
    .invert_iq_is_on = 0
  };
  status = llcc68_set_lora_pkt_params(&llcc68_context, &pkt_params);
  if (status != LLCC68_STATUS_OK) {
    handleInitError("Pkt Params Fail");
  }
  //DEBUG_PRINTN("Pkt OK");

  // Configure high-power PA settings
  llcc68_pa_cfg_params_t pa_params = {
    .pa_duty_cycle = 0x04,
    .hp_max = 0x07,  // +17 dBm - 0x05
    .device_sel = 0x00,
    .pa_lut = 0x01
  };
  status = llcc68_set_pa_cfg(&llcc68_context, &pa_params);
  if (status != LLCC68_STATUS_OK) {
    handleInitError("PA Fail");
  }
  //DEBUG_PRINTN("PA OK");

  // Set TX parameters: 17 dBm
  status = llcc68_set_tx_params(&llcc68_context, 22, LLCC68_RAMP_200_US);
  if (status != LLCC68_STATUS_OK) {
    handleInitError("TX Params Fail");
  }
  //DEBUG_PRINTN("TX OK");

  // Set buffer base address
  status = llcc68_set_buffer_base_address(&llcc68_context, 0, 0);
  if (status != LLCC68_STATUS_OK) {
    handleInitError("Buf Fail");
  }
  //DEBUG_PRINTN("Buf OK");

  // Configure DIO1 for TX done and RX done interrupts
  //llcc68_irq_mask_t irq_mask = LLCC68_IRQ_TX_DONE | LLCC68_IRQ_RX_DONE;

  llcc68_irq_mask_t irq_mask = LLCC68_IRQ_TX_DONE | LLCC68_IRQ_RX_DONE | LLCC68_IRQ_TIMEOUT | LLCC68_IRQ_HEADER_ERROR | LLCC68_IRQ_CRC_ERROR;
  status = llcc68_set_dio_irq_params(&llcc68_context, irq_mask, irq_mask, 0, 0);


  if (status != LLCC68_STATUS_OK) {
    handleInitError("IRQ Fail");
  }
  //DEBUG_PRINTN("IRQ OK");

  // Attach interrupt
  attachInterrupt(digitalPinToInterrupt(LLCC68_DIO1), dio1_isr, RISING);

  //DEBUG_PRINTN("Int OK");

  // Initialization complete
  //DEBUG_PRINTN("LLCC68 Init Complete!");
  radio_state = STATE_IDLE;
  state_start_time = millis();
  stop_transmission = false;  // Ensure flag is reset
}



// LoRa State Machine - Call from loop()
void llcc68Func() {
  llcc68_status_t status;
  static unsigned long last_radio_activity = millis();  // watchdog timer

  switch (radio_state) {
    // -------------------------------------------------------
    case STATE_IDLE:
      // Always default back to RX
      radio_state = STATE_RX_SETUP;
      break;

    // -------------------------------------------------------
    case STATE_TX_SETUP:
      memcpy(bufferRadio, payload, 32);
      status = llcc68_write_buffer(&llcc68_context, 0, bufferRadio, 32);
      if (status != LLCC68_STATUS_OK) {
        DEBUG_PRINTN("Buf Wr Fail");
        radio_state = STATE_RX_SETUP;  // fallback to RX
        break;
      }

      dio1_triggered = false;
      status = llcc68_set_tx(&llcc68_context, 0);  // fire TX
      if (status != LLCC68_STATUS_OK) {
        DEBUG_PRINTN("TX Fail");
        radio_state = STATE_RX_SETUP;  // fallback
        break;
      }

      radio_state = STATE_TX_WAIT;
      state_start_time = millis();
      break;

    // -------------------------------------------------------
    case STATE_TX_WAIT:
      if (dio1_triggered) {
        DEBUG_PRINTN("TX Done");
        llcc68_clear_irq_status(&llcc68_context, LLCC68_IRQ_ALL);
        dio1_triggered = false;
        radio_state = STATE_RX_SETUP;  // go straight back to listening
      } else if (millis() - state_start_time >= TX_TIMEOUT_MS) {
        DEBUG_PRINTN("TX Timeout");
        llcc68_clear_irq_status(&llcc68_context, LLCC68_IRQ_ALL);
        radio_state = STATE_RX_SETUP;
      }
      break;

    // -------------------------------------------------------
    case STATE_RX_SETUP:
      dio1_triggered = false;
      // 0 = continuous RX until explicitly stopped
      status = llcc68_set_rx(&llcc68_context, 0);
      if (status != LLCC68_STATUS_OK) {
        DEBUG_PRINTN("RX Start Fail");
        radio_state = STATE_IDLE;  // retry via idle
        break;
      }
      radio_state = STATE_RX_WAIT;
      DEBUG_PRINTN("RX Continuous Start");
      break;

    // -------------------------------------------------------
    case STATE_RX_WAIT:
      if (dio1_triggered) {
        llcc68_irq_mask_t irq_status;
        status = llcc68_get_irq_status(&llcc68_context, &irq_status);
        if (status == LLCC68_STATUS_OK) {
          if (irq_status & LLCC68_IRQ_RX_DONE) {
            llcc68_rx_buffer_status_t rx_status;
            status = llcc68_get_rx_buffer_status(&llcc68_context, &rx_status);
            if (status == LLCC68_STATUS_OK) {
              uint8_t rx_length = rx_status.pld_len_in_bytes;
              uint8_t rx_buffer[32];
              status = llcc68_read_buffer(&llcc68_context, rx_status.buffer_start_pointer, rx_buffer, rx_length);
              if (status == LLCC68_STATUS_OK) {
                DEBUG_PRINT(F("RX raw: ["));
                for (uint8_t i = 0; i < rx_length; ++i) {
                  DEBUG_PRINT((char)rx_buffer[i]);
                }
                DEBUG_PRINTN("]");
                decryptNFunc(rx_buffer, rx_length);
              }
            }
          } else if (irq_status & (LLCC68_IRQ_HEADER_ERROR | LLCC68_IRQ_CRC_ERROR | LLCC68_IRQ_TIMEOUT)) {
            DEBUG_PRINTN("RX Error IRQ");
          }
        } else {
          DEBUG_PRINTN("IRQ read fail");
        }
        llcc68_clear_irq_status(&llcc68_context, LLCC68_IRQ_ALL);
        dio1_triggered = false;

        //  Stay in continuous RX (re-arm listener)
        radio_state = STATE_RX_SETUP;
      }
      break;
  }

  // Radio watchdog: if no activity for > 2 min, reset radio
  if (millis() - last_radio_activity > 120000UL) {
    DEBUG_PRINTN("Radio watchdog reset");
    llcc68_hal_reset(&llcc68_context);
    llcc68Init();  // reinitialize
    last_radio_activity = millis();
  }
}
