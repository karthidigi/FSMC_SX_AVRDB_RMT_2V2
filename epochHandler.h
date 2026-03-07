// +05:30 IST offset = 19800 seconds
constexpr int32_t IST_OFFSET_SECS = 19800;

// Function to convert epoch time to date/time components (with IST adjustment)
void epochToDateTime(uint32_t epoch, uint8_t& year, uint8_t& month, uint8_t& day, 
                     uint8_t& hour, uint8_t& minute, uint8_t& second, uint8_t& dow) {
  

  const uint32_t SECS_PER_DAY = 86400UL;
  const uint16_t DAYS_PER_YEAR = 365;
  const uint16_t DAYS_PER_4YEARS = 1461;
  const uint8_t DAYS_IN_MONTH[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  second = epoch % 60;
  epoch /= 60;
  minute = epoch % 60;
  epoch /= 60;
  hour = epoch % 24;
  epoch /= 24;

  dow = (epoch + 4) % 7 + 1;

  uint16_t years = 1970;
  uint32_t days = epoch;
  while (days >= DAYS_PER_4YEARS) {
    days -= DAYS_PER_4YEARS;
    years += 4;
  }
  while (true) {
    bool isLeap = (years % 4 == 0 && (years % 100 != 0 || years % 400 == 0));
    uint16_t daysInYear = isLeap ? 366 : 365;
    if (days < daysInYear) break;
    days -= daysInYear;
    years++;
  }
  year = years - 2000;

  month = 1;
  bool isLeap = (years % 4 == 0 && (years % 100 != 0 || years % 400 == 0));
  while (days >= DAYS_IN_MONTH[month - 1] + ((month == 2 && isLeap) ? 1 : 0)) {
    days -= DAYS_IN_MONTH[month - 1] + ((month == 2 && isLeap) ? 1 : 0);
    month++;
  }
  day = days + 1;
}

void ds3231_setEpochIST(uint32_t epochUTC) {
  // convert UTC → IST
  uint32_t epochIST = epochUTC + IST_OFFSET_SECS;
  uint8_t year, month, day, hour, minute, second, dow;
  epochToDateTime(epochIST, year, month, day, hour, minute, second, dow);
  // DS3231 year register is 2-digit (00–99)
  ds3231_SetDateTime(year % 100, month, day, hour, minute, second, dow);
}


void ds3231_setEpochUTC(uint32_t epochUTC) {
  uint8_t year, month, day, hour, minute, second, dow;
  epochToDateTime(epochUTC, year, month, day, hour, minute, second, dow);
  // DS3231 year register is 2-digit (00–99)
  ds3231_SetDateTime(year % 100, month, day, hour, minute, second, dow);
}

// overload for String type input
void ds3231_setEpochUTC(String epochStr) {
  uint32_t epochUTC = (uint32_t)epochStr.toInt();
  ds3231_setEpochUTC(epochUTC);
}
