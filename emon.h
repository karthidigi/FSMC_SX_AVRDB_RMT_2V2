// three_phase_half_rectified_avr128db48_dump.ino
#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
float internalBatteryVolt = 0;

int volRY = 0;
int volYB = 0;
int volBR = 0;

float curR = 0;
float curY = 0;
float curBM1 = 0;
float curBM2 = 0;

int elst = 0;

// Define voltage and current pins for three phases plus fourth current
const uint8_t V_PINS[] = { PIN_PD1, PIN_PD2, PIN_PD3 };           // VRY, VYB, VBR (AIN1, AIN2, AIN3)
const uint8_t I_PINS[] = { PIN_PE1, PIN_PE2, PIN_PE3, PIN_PE0 };  // Current inputs (AIN9, AIN10, AIN11, AIN8)


class EnergyMonitor {
public:
  void voltage(uint8_t pin, double vCal);
  void current(uint8_t pin, double iCal);
  double Vrms;
  double Irms;

private:
  uint8_t vPin, iPin;
  double vCal, iCal;
};


void EnergyMonitor::voltage(uint8_t pin, double vCal) {
  vPin = pin;
  this->vCal = vCal;
}

void EnergyMonitor::current(uint8_t pin, double iCal) {
  iPin = pin;
  this->iCal = iCal;
}



// Channel MUXPOS values
const uint8_t channel_mux[] = {
  ADC_MUXPOS_AIN1_gc,   // V0: PD1
  ADC_MUXPOS_AIN9_gc,   // I0: PE1
  ADC_MUXPOS_AIN2_gc,   // V1: PD2
  ADC_MUXPOS_AIN10_gc,  // I1: PE2
  ADC_MUXPOS_AIN3_gc,   // V2: PD3
  ADC_MUXPOS_AIN11_gc,  // I2: PE3
  ADC_MUXPOS_AIN8_gc,   // I3: PE0
  ADC_MUXPOS_AIN0_gc    // Battery: PD0
};

const int num_channels = 8;

// Indices for V channels in channel_mux
const int v_channel_indices[] = { 0, 2, 4 };  // V0, V1, V2

// Create four EnergyMonitor instances
EnergyMonitor emon[4];

// Calibration constants for each phase (increased by factor of 100)
const float VOLT_CAL[] = { 11.1, 11.1, 11.1 };       // VRY, VYB, VBR (original x 100)
const float CURR_CAL[] = { 0.4, 0.4, 0.4, 0.4 };

// Measurement variables
volatile long long sumSq[8];             // sum of squares for each channel
volatile int lastSample[3];              // last sample for each V phase
volatile unsigned int cycle_count[3];    // cycle count for each V phase
volatile unsigned int sample_count = 0;  // total samples (shared)
volatile int current_channel = 0;
volatile bool measurement_done = false;
bool measuring = false;
unsigned long measurement_start;
unsigned int meas_crossings;
unsigned int meas_timeout;
unsigned long lastUpdate = 0;
// Sampling rate configuration
#define PER_CHANNEL_SAMPLING_RATE 2000                           // Hz per channel
#define TRIGGER_RATE (PER_CHANNEL_SAMPLING_RATE * num_channels)  // Total trigger rate
#define TCB_PERIOD ((F_CPU / TRIGGER_RATE) - 1)                  // CCMP value for TCB0


void start_measurement(unsigned int crossings, unsigned int timeout) {
  measuring = true;
  meas_crossings = crossings;
  meas_timeout = timeout;
  measurement_start = millis();
  measurement_done = false;

  // Reset accumulators
  for (int i = 0; i < num_channels; i++) {
    sumSq[i] = 0;
  }
  for (int i = 0; i < 3; i++) {
    cycle_count[i] = 0;
    lastSample[i] = 0;  // Initial last sample (assume starts at 0 for half-rectified)
  }
  sample_count = 0;
  current_channel = 0;

  // Set initial MUX
  ADC0.MUXPOS = channel_mux[0];

  // Reset and enable TCB0
  TCB0.CNT = 0;
  TCB0.CTRLA |= TCB_ENABLE_bm;

  // Enable ADC interrupt
  ADC0.INTCTRL |= ADC_RESRDY_bm;
}



void stop_measurement() {
  // Disable TCB0
  TCB0.CTRLA &= ~TCB_ENABLE_bm;

  // Disable ADC interrupt
  ADC0.INTCTRL &= ~ADC_RESRDY_bm;

  measuring = false;
  measurement_done = true;
}


void compute_rms() {
  if (sample_count == 0) {
    for (int i = 0; i < 3; i++) emon[i].Vrms = 0;
    for (int i = 0; i < 4; i++) emon[i].Irms = 0;
    emon[3].Vrms = 0;
    internalBatteryVolt = 0;
    return;
  }

  double sq_n = (double)sample_count / num_channels;  // Approximate samples per channel (since round-robin)

  // Compute Vrms for phases 0-2
  for (int i = 0; i < 3; i++) {
    int ch = v_channel_indices[i];
    double rmsV = sqrt((double)sumSq[ch] / sq_n);
    emon[i].Vrms = (VOLT_CAL[i] / 50.0) * rmsV;
  }
  emon[3].Vrms = emon[2].Vrms;  // Shared with phase 2

  // Compute Irms for 0-3
  int i_ch[] = { 1, 3, 5, 6 };  // Channel indices for I0-I3
  for (int j = 0; j < 4; j++) {
    int ch = i_ch[j];
    double rmsI = sqrt((double)sumSq[ch] / sq_n);
    emon[j].Irms = (CURR_CAL[j] / 50.0) * rmsI;
  }
  // Compute battery voltage (average, not RMS)

  double avg = (double)sumSq[7] / sq_n;

  double pin_volt = avg * (2.5 / 4096.0);

  internalBatteryVolt = pin_volt * 2.0;  // Assuming voltage divider factor of 2
}

ISR(ADC0_RESRDY_vect) {
  int sample = ADC0.RES;

  if (current_channel == 7) {

    // For battery, accumulate sum (DC)

    sumSq[current_channel] += sample;

  } else {

    // For others, accumulate sum of squares (AC RMS)

    sumSq[current_channel] += (long long)sample * sample;
  }
  //  sumSq[current_channel] += (long long)sample * sample;

  // Check if voltage channel
  int v_idx = -1;
  if (current_channel == 0) v_idx = 0;
  else if (current_channel == 2) v_idx = 1;
  else if (current_channel == 4) v_idx = 2;

  if (v_idx >= 0) {
    bool checkCross = (sample > 0 && lastSample[v_idx] <= 0);
    lastSample[v_idx] = sample;
    if (checkCross) {
      cycle_count[v_idx]++;
    }
  }

  sample_count++;  // Total samples across all channels

  // Advance to next channel
  current_channel = (current_channel + 1) % num_channels;
  ADC0.MUXPOS = channel_mux[current_channel];

  // Clear interrupt flag
  ADC0.INTFLAGS = ADC_RESRDY_bm;
}

void emonInit() {

  // Configure ADC for 12-bit resolution with 2.5V reference
  //VREF.ADC0REF = VREF_REFSEL_2V500_gc; //commented due to vdd refernce
  VREF.ADC0REF = VREF_REFSEL_VDD_gc;
  ADC0.CTRLA = ADC_ENABLE_bm | ADC_RESSEL_12BIT_gc;
  ADC0.CTRLB = 0;                             // No accumulation
  ADC0.CTRLC = ADC_PRESC_DIV4_gc | (1 << 4);  // Prescaler DIV4, SAMPCAP enabled (bit 4)

  // Setup event system for timer-triggered ADC
  EVSYS.CHANNEL0 = EVSYS_CHANNEL0_TCB0_CAPT_gc;
  EVSYS.USERADC0START = EVSYS_USER_CHANNEL0_gc;
  ADC0.EVCTRL = ADC_STARTEI_bm;  // Enable event start conversion

  // Setup TCB0 for periodic events
  TCB0.CCMP = TCB_PERIOD;
  TCB0.CTRLB = TCB_CNTMODE_INT_gc;     // Periodic mode
  TCB0.EVCTRL = TCB_CAPTEI_bm;         // Enable capture event output
  TCB0.INTCTRL = 0;                    // No timer interrupt
  TCB0.CTRLA = TCB_CLKSEL_CLKDIV1_gc;  // CLK_PER, not enabled yet


  for (int i = 0; i < 4; i++) {
    int voltageIndex = (i < 3) ? i : 2;  // Associate fourth current with B phase
    emon[i].voltage(V_PINS[voltageIndex], VOLT_CAL[voltageIndex]);
    emon[i].current(I_PINS[i], CURR_CAL[i]);
  }
}

void emonFunc() {
  unsigned long currentMillis = millis();
  // Measurement and display update
  if (currentMillis - lastUpdate >= 1000) {
    if (!measuring) {
      start_measurement(20, 2000);
    } else {
      // Check if all phases have completed crossings or timeout
      bool all_done = (cycle_count[0] >= meas_crossings) && (cycle_count[1] >= meas_crossings) && (cycle_count[2] >= meas_crossings);
      if (all_done || (currentMillis - measurement_start > meas_timeout)) {
        stop_measurement();
        compute_rms();

        volRY = emon[0].Vrms;
        volYB = emon[1].Vrms;
        volBR = emon[2].Vrms;

        curR = emon[0].Irms;
        curY = emon[1].Irms;
        curBM1 = emon[2].Irms;
        curBM2 = emon[3].Irms;
       

        // curR = random(2, 9);
        // curY = random(2, 9);
        // curBM1 = random(2, 9);
        // curBM2 = random(2, 9);

        lastUpdate = currentMillis;
      }
    }
  }
}
