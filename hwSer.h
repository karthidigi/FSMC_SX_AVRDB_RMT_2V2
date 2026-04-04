#pragma once

#define debugSerial Serial3

// iotSerial stub — Phase 2: UART IoT path removed; Serial1 is no longer
// initialized.  Remaining iotSerial.println() calls in menu.h / lcdDisp.h /
// protSettings.h compile harmlessly and send to the idle Serial1 (discarded).
// Remove this define in Phase 3 once all call-sites are cleaned up.
#define iotSerial Serial1

void hwSerialInit() {
  #ifdef SERIAL_DEBUG
  debugSerial.begin(SERIAL_BAUD);
  debugSerial.setTimeout(SERIAL_TIMEOUT);
  // iotSerial (Serial1) intentionally NOT initialized — UART IoT path removed.
  #endif
}

// ── readHWSerial() ───────────────────────────────────────────────────────────
// Echo any bytes arriving on the debug UART to the debug serial monitor.
// Kept here after serReader.h removal so the debug echo path stays functional.
void readHWSerial() {
  if (debugSerial.available() > 0) {
    Serial3.println(debugSerial.readString());
  }
}