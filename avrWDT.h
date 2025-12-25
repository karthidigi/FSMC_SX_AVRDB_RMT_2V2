#include <avr/wdt.h>
#include <avr/io.h>

void wdtInit() {
  // Check if MCU was reset by WDT
  if (RSTCTRL.RSTFR & RSTCTRL_WDRF_bm) {
    Serial3.println("*** WDT Reset Detected! ***");
    // Clear all reset flags by writing 1 to each bit
    //    RSTCTRL.RSTFR = 0xFF;
  }
  wdt_reset();
}
