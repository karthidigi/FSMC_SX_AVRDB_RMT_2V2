// // ========== I2C Addresses ==========
// #define DS3231_ADDR  0x68
// // ========== DS3231 Functions ==========
// uint8_t bcd2dec(uint8_t val) {
//   return ((val >> 4) * 10) + (val & 0x0F);
// }

// void rtc_get_time(uint8_t& hr, uint8_t& min, uint8_t& sec) {
//   if (!i2c1_start(DS3231_ADDR, false)) return;
//   i2c1_write(0x00); // start at register 0
//   i2c1_stop();

//   if (!i2c1_start(DS3231_ADDR, true)) return;
//   sec = bcd2dec(i2c1_read(true));
//   min = bcd2dec(i2c1_read(true));
//   hr  = bcd2dec(i2c1_read(false)); // NACK after last
// }



//////////////////////////////////////////////////////////////////////

// ========== I2C Addresses ==========
#define DS3231_ADDR 0x68

char time_buffer_string[40];

////////////////////////////////////////////////////////////////////////////////

int rtcPassVars = 0;
byte tSec = 99, tMin = 99, tHrs = 99, tWday = 99, tDate = 99, tMonth = 99, tYear = 99, hrs24 = 99;
float dsTemp = 0;
char sDoW[4] = "ERR";
char tAmPm[3] = "AM";

////////////////////////////////////////////////////////////////////////////////

// ==========================================================================
// All code below requires the physical DS3231 to be present.
// Set #define ENABLE_RTC in zSettings.h to include it; comment it out to
// exclude all I2C traffic and compile only lightweight no-op stubs instead.
// ==========================================================================
#ifdef ENABLE_RTC

byte decToBcd(byte val) {
  // Convert normal decimal numbers to binary coded decimal
  return ((val / 10 * 16) + (val % 10));
}

uint8_t bcd2dec(uint8_t val) {
  // Convert binary coded decimal to normal decimal numbers
  return ((val >> 4) * 10) + (val & 0x0F);
}

byte readControlByte(bool which) {
  // Read selected control byte
  // first byte (0) is 0x0e, second (1) is 0x0f
  if (!i2c1_start(DS3231_ADDR, false)) return 0;
  i2c1_write(which ? 0x0f : 0x0e);
  i2c1_stop();

  if (!i2c1_start(DS3231_ADDR, true)) return 0;
  byte val = i2c1_read(false);  // NACK after last byte
  return val;
}

void writeControlByte(byte control, bool which) {
  // Write the selected control byte.
  // which=false -> 0x0e, true->0x0f.
  if (!i2c1_start(DS3231_ADDR, false)) return;
  i2c1_write(which ? 0x0f : 0x0e);
  i2c1_write(control);
  i2c1_stop();
}

void ds3231Init() {
  i2c1Init();
  if (!i2c1_start(DS3231_ADDR, false)) return;
  i2c1_write(0x0E);
  i2c1_write(0b00011100);
  i2c1_stop();
}

//////////////////////////////////////////////////////////////////////////////

void ds3231_getTemperature() {
  if (!i2c1_start(DS3231_ADDR, false)) return;
  i2c1_write(0x11);  // Temperature MSB register
  i2c1_stop();

  if (!i2c1_start(DS3231_ADDR, true)) return;
  int8_t msb = i2c1_read(true);        // Read temperature MSB (signed)
  uint8_t lsb = i2c1_read(false);      // Read temperature LSB
  //i2c1_stop();

  // Convert to float temperature
  dsTemp = msb + ((lsb >> 6) * 0.25);
}


////////////////////////////////////////////////////////////////////////////////

void rtc_get_time(uint8_t& hr, uint8_t& min, uint8_t& sec) {
  if (!i2c1_start(DS3231_ADDR, false)) {
    rtcPassVars = 0;
    return;
  }
  i2c1_write(0x00);  // start at register 0
  i2c1_stop();

  if (!i2c1_start(DS3231_ADDR, true)) {
    rtcPassVars = 0;
    return;
  }
  sec = bcd2dec(i2c1_read(true));
  min = bcd2dec(i2c1_read(true));
  hr = bcd2dec(i2c1_read(false));  // NACK after last
  rtcPassVars = 1;
}

void rtcFetchTimeFunc() {
  if (!i2c1_start(DS3231_ADDR, false)) {
    rtcPassVars = 0;
    return;
  }
  i2c1_write(0x00);
  i2c1_stop();

  if (!i2c1_start(DS3231_ADDR, true)) {
    rtcPassVars = 0;
    return;
  }
  tSec = bcd2dec(i2c1_read(true));
  tMin = bcd2dec(i2c1_read(true));
  tHrs = bcd2dec(i2c1_read(true));
  hrs24 = tHrs;
  tWday = bcd2dec(i2c1_read(true));
  tDate = bcd2dec(i2c1_read(true));
  tMonth = bcd2dec(i2c1_read(true));
  tYear = bcd2dec(i2c1_read(false));  // NACK after last
  if (tHrs >= 12) {
    strcpy(tAmPm, "PM");
    if (tHrs >= 13) {
      tHrs = tHrs - 12;
    }
  } else {
    if (tHrs == 0) {
      tHrs = 12;
    }
    strcpy(tAmPm, "AM");
  }

  switch (tWday) {
    case 1:
      strcpy(sDoW, "SUN");
      break;
    case 2:
      strcpy(sDoW, "MON");
      break;
    case 3:
      strcpy(sDoW, "TUE");
      break;
    case 4:
      strcpy(sDoW, "WED");
      break;
    case 5:
      strcpy(sDoW, "THU");
      break;
    case 6:
      strcpy(sDoW, "FRI");
      break;
    case 7:
      strcpy(sDoW, "SAT");
      break;
  }



  rtcPassVars = 1;
}

///////////////////////////////////////////////////////////////////////////////

String getRtcTimeStamp() {
  rtcFetchTimeFunc();
  sprintf(time_buffer_string, "20%02d-%02d-%02d_%02d:%02d:%02d",
          tYear, tMonth, tDate,
          tHrs, tMin, tSec);
  return (time_buffer_string);
}

String getTimeFormat() {
  rtcFetchTimeFunc();
  sprintf(time_buffer_string, "%02d:%02d:%02d %s", tHrs, tMin, tSec, tAmPm);
  return (time_buffer_string);
}

String getDateFormat() {
  rtcFetchTimeFunc();
  sprintf(time_buffer_string, "%02d/%02d/%02d %s", tDate, tMonth, tYear, sDoW);
  return (time_buffer_string);
}

/////////////////////////////////////////////////////////////////////////////

void ds3231_setSecond(byte Second) {
  // Sets the seconds
  // This function also resets the Oscillator Stop Flag, which is set
  // whenever power is interrupted.
  if (!i2c1_start(DS3231_ADDR, false)) return;
  i2c1_write(0x00);
  i2c1_write(decToBcd(Second));
  i2c1_stop();
  // Clear OSF flag
  byte temp_buffer = readControlByte(1);
  writeControlByte((temp_buffer & 0b01111111), 1);
}

void ds3231_setMinute(byte Minute) {
  // Sets the minutes
  if (!i2c1_start(DS3231_ADDR, false)) return;
  i2c1_write(0x01);
  i2c1_write(decToBcd(Minute));
  i2c1_stop();
}

void ds3231_setHour(byte Hour) {
  // Sets the hour, without changing 12/24h mode.
  // The hour must be in 24h format.
  bool h12;
  byte temp_hour;

  // Start by figuring out what the 12/24 mode is
  if (!i2c1_start(DS3231_ADDR, false)) return;
  i2c1_write(0x02);
  i2c1_stop();
  if (!i2c1_start(DS3231_ADDR, true)) return;
  h12 = (i2c1_read(false) & 0b01000000);
  // if h12 is true, it's 12h mode; false is 24h.

  if (h12) {
    // 12 hour
    bool am_pm = (Hour > 11);
    temp_hour = Hour;
    if (temp_hour > 11) {
      temp_hour = temp_hour - 12;
    }
    if (temp_hour == 0) {
      temp_hour = 12;
    }
    temp_hour = decToBcd(temp_hour) | (am_pm << 5) | 0b01000000;
  } else {
    // 24 hour
    temp_hour = decToBcd(Hour) & 0b10111111;
  }

  if (!i2c1_start(DS3231_ADDR, false)) return;
  i2c1_write(0x02);
  i2c1_write(temp_hour);
  i2c1_stop();
}

void ds3231_setDoW(byte DoW) {
  // Sets the Day of Week
  if (!i2c1_start(DS3231_ADDR, false)) return;
  i2c1_write(0x03);
  i2c1_write(decToBcd(DoW));
  i2c1_stop();
}

void ds3231_setDate(byte Date) {
  // Sets the Date
  if (!i2c1_start(DS3231_ADDR, false)) return;
  i2c1_write(0x04);
  i2c1_write(decToBcd(Date));
  i2c1_stop();
}

void ds3231_setMonth(byte Month) {
  // Sets the month
  if (!i2c1_start(DS3231_ADDR, false)) return;
  i2c1_write(0x05);
  i2c1_write(decToBcd(Month));
  i2c1_stop();
}

void ds3231_setYear(byte Year) {
  // Sets the year
  if (!i2c1_start(DS3231_ADDR, false)) return;
  i2c1_write(0x06);
  i2c1_write(decToBcd(Year));
  i2c1_stop();
}

void ds3231_SetDateTime(int YY, int mm, int DD, int HH, int MM, int SS, int DOW) {
  ds3231_setSecond(SS);
  ds3231_setMinute(MM);
  ds3231_setHour(HH);
  ds3231_setDoW(DOW);
  ds3231_setDate(DD);
  ds3231_setMonth(mm);
  ds3231_setYear(YY);
}

#else  // ENABLE_RTC not defined — provide lightweight no-op stubs
// All globals (rtcPassVars, tSec, tMin, tHrs …) are still declared above so
// scheduler, menu, and other modules that reference them compile without errors.
// rtcPassVars stays 0 (RTC absent), time globals stay at their default 99 values.

inline void rtcFetchTimeFunc()                                        {}
inline void ds3231_getTemperature()                                   {}
inline void ds3231Init()                                              {}
inline void rtc_get_time(uint8_t&, uint8_t&, uint8_t&)               {}
inline void ds3231_SetDateTime(int,int,int,int,int,int,int)           {}
inline String getRtcTimeStamp()  { return String("--"); }
inline String getTimeFormat()    { return String("--:--:-- --"); }
inline String getDateFormat()    { return String("--/--/-- ---"); }

#endif  // ENABLE_RTC