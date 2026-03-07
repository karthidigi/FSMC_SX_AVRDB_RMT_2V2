// ================= relayTimer.h =================
// Hardware timer-based relay control for M1 ON/OFF
// Uses TCB1 for ON pulse timing and TCB3 for OFF release timing
// AVR128DB48 has TCB0-TCB3 available
// NOTE: TCB0 = ADC trigger (emon.h), TCB2 = millis() (board config)

#ifndef RELAY_TIMER_H
#define RELAY_TIMER_H

// -------- Timer Configuration --------
// F_CPU = 24MHz (AVR128DB48 default)
// Prescaler DIV2 = 12MHz timer clock
// For 1ms tick: 12000 - 1 = 11999

#define TIMER_PRESCALER     TCB_CLKSEL_DIV2_gc
#define TIMER_1MS_TICKS     11999UL    // (F_CPU / 2000) - 1

// -------- Timing Constants --------
// Pulse durations are driven by LCD menu settings (ON/OFF RELAY LAT):
//   storage.onRelay  (2–5 s)  → TCB1 ISR stops M1ON  after onRelay  seconds
//   storage.offRelay (2–5 s)  → TCB3 ISR re-energises M1OFF after offRelay seconds
// Evaluated at ISR time so LCD changes take effect on the next timer cycle.
#define ON_PULSE_MS()   ((uint16_t)storage.onRelay  * 1000U)
#define OFF_PULSE_MS()  ((uint16_t)storage.offRelay * 1000U)

// -------- State Variables --------
volatile bool m1OnTimerActive = false;
volatile bool m1OffTimerActive = false;
volatile uint16_t m1OnTickCounter = 0;
volatile uint16_t m1OffTickCounter = 0;

// -------- External Variables (defined in other headers) --------
// These are declared here to avoid circular dependencies
extern bool m1OffActive;  // Defined in motor.h
extern int elst;          // Defined in emon.h

// -------- Function Prototypes --------
void relayTimerInit();
void startOnTimer();
void stopOnTimer();
void startOffTimer();
void stopOffTimer();
void resetOffTimer();

// ===============================================
// TCB1 - M1 ON Pulse Timer (storage.onRelay seconds)
// ===============================================
void relayTimerInit() {
  // Configure TCB1 for ON pulse timing
  TCB1.CCMP = TIMER_1MS_TICKS;              // 1ms period
  TCB1.CTRLB = TCB_CNTMODE_INT_gc;          // Periodic interrupt mode
  TCB1.INTCTRL = TCB_CAPT_bm;               // Enable interrupt
  TCB1.CTRLA = TIMER_PRESCALER;             // Set prescaler, not enabled yet

  // Configure TCB3 for OFF release timing
  TCB3.CCMP = TIMER_1MS_TICKS;              // 1ms period
  TCB3.CTRLB = TCB_CNTMODE_INT_gc;          // Periodic interrupt mode
  TCB3.INTCTRL = TCB_CAPT_bm;               // Enable interrupt
  TCB3.CTRLA = TIMER_PRESCALER;             // Set prescaler, not enabled yet
}

// -------- ON Timer Control --------
void startOnTimer() {
  m1OnTickCounter = 0;
  m1OnTimerActive = true;
  TCB1.CNT = 0;                             // Reset counter
  TCB1.INTFLAGS = TCB_CAPT_bm;              // Clear interrupt flag
  TCB1.CTRLA |= TCB_ENABLE_bm;              // Enable timer
}

void stopOnTimer() {
  TCB1.CTRLA &= ~TCB_ENABLE_bm;             // Disable timer
  m1OnTimerActive = false;
  m1OnTickCounter = 0;
}

// -------- OFF Timer Control --------
void startOffTimer() {
  m1OffTickCounter = 0;
  m1OffTimerActive = true;
  TCB3.CNT = 0;                             // Reset counter
  TCB3.INTFLAGS = TCB_CAPT_bm;              // Clear interrupt flag
  TCB3.CTRLA |= TCB_ENABLE_bm;              // Enable timer
}

void stopOffTimer() {
  TCB3.CTRLA &= ~TCB_ENABLE_bm;             // Disable timer
  m1OffTimerActive = false;
  m1OffTickCounter = 0;
}

void resetOffTimer() {
  // Reset counter to keep relay active while fault persists
  m1OffTickCounter = 0;
  TCB3.CNT = 0;
  TCB3.INTFLAGS = TCB_CAPT_bm;              // Clear interrupt flag
}

// ===============================================
// TCB1 ISR - ON Pulse Timer (1ms tick)
// ===============================================
ISR(TCB1_INT_vect) {
  TCB1.INTFLAGS = TCB_CAPT_bm;              // Clear interrupt flag

  if (m1OnTimerActive) {
    m1OnTickCounter++;

    // Check if ON pulse duration reached (storage.onRelay seconds from LCD menu)
    if (m1OnTickCounter >= ON_PULSE_MS()) {
      digitalWrite(PIN_M1ON, LOW);
      stopOnTimer();
    }
  }
}

// ===============================================
// TCB3 ISR - OFF Release Timer (1ms tick)
// Counts to storage.offRelay seconds then:
//   fault clear  → re-energise M1OFF (HIGH), release m1OffActive
//   fault active → reset counter and let it run again
// ===============================================
ISR(TCB3_INT_vect) {
  TCB3.INTFLAGS = TCB_CAPT_bm;              // Clear interrupt flag

  if (m1OffTimerActive) {
    m1OffTickCounter++;

    // Check if OFF release duration reached (storage.offRelay seconds from LCD menu)
    if (m1OffTickCounter >= OFF_PULSE_MS()) {
      if (elst == 0) {
        // No fault: re-energise relay → NO closes → normal run restored
        digitalWrite(PIN_M1OFF, HIGH);
        m1OffActive = false;
        stopOffTimer();
      } else {
        // Fault still active: restart counter for another cycle
        m1OffTickCounter = 0;
      }
    }
  }
}

#endif // RELAY_TIMER_H
