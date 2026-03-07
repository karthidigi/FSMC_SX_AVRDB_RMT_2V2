// ---------- FILE: debug.h ----------
#pragma once
#include "zSettings.h"

#ifdef SERIAL_DEBUG
  #define DEBUG_BEGIN(...) do { debugSerial.begin(SERIAL_BAUD); debugSerial.setTimeout(SERIAL_TIMEOUT); } while (0)
  #define DEBUG_PRINT(...)  debugSerial.print(__VA_ARGS__)
  #define DEBUG_PRINTN(...) debugSerial.println(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) debugSerial.println(__VA_ARGS__)
#else
  #define DEBUG_BEGIN(...) do { } while (0)
  #define DEBUG_PRINT(...) do { } while (0)
  #define DEBUG_PRINTN(...) do { } while (0)
  #define DEBUG_PRINTLN(...) do { } while (0)
#endif