// ---------- FILE: debug.h ----------
#pragma once
#include "zSettings.h"

#ifdef SERIAL_DEBUG
  #define DEBUG_BEGIN() debugSerial.begin(SERIAL_BAUD); debugSerial.setTimeout(SERIAL_TIMEOUT)
  #define DEBUG_PRINT(x) debugSerial.print(x)
  #define DEBUG_PRINTN(x) debugSerial.println(x)
  #define DEBUG_PRINTLN(x) debugSerial.println(x)
#else
  #define DEBUG_BEGIN()
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTN(x)
  #define DEBUG_PRINTLN(x)
#endif