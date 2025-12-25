#define iotSerial Serial1
#define debugSerial Serial3

void hwSerialInit() {
   #ifdef SERIAL_DEBUG
  debugSerial.begin(SERIAL_BAUD);
  iotSerial.begin(SERIAL_BAUD);
  debugSerial.setTimeout(SERIAL_TIMEOUT);
  iotSerial.setTimeout(SERIAL_TIMEOUT);
  #endif
}