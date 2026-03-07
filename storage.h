#include <EEPROM.h>
#include <string.h>

// CONFIG_START and CONFIG_VERSION are defined in zSettings.h (included first)

struct StoreStruct {
  char version[5];

  char dname[34];
  uint8_t app1Run : 1;
  uint8_t app2Run : 1;

  uint16_t ovrVol;
  uint16_t undVol;

  uint8_t modeM1;
  float dryRunM1;
  float ovLRunM1;

  char autoM1[6];
  char cyclicM1[10];
  char countM1[6];

  char sch[10][19];
  char lasRemM1[6];

  char remote[21];    // 20-char remote chip-serial + null; "" = not yet paired
  char gateway[21];   // 20-char GW ID + null; "" = not yet paired with gateway
  uint8_t rf_locked;  // 0 = using default RF params, 1 = OPER channel (post-pairing)

  uint16_t senseTime;   // Sense time in seconds (10–120)
  uint8_t  onRelay;     // ON relay latch in seconds (2–5)
  uint8_t  offRelay;    // OFF relay latch in seconds (2–5)

  uint8_t configflag : 1;
};

StoreStruct storage;
static StoreStruct storageShadow;
static bool storageShadowValid = false;

static void markStorageShadow() {
  memcpy(&storageShadow, &storage, sizeof(storage));
  storageShadowValid = true;
}

static bool isStorageDirty() {
  if (!storageShadowValid) {
    return true;
  }
  return memcmp(&storageShadow, &storage, sizeof(storage)) != 0;
}

StoreStruct storage_default = {
  CONFIG_VERSION,
  "SMART PUMP-2025",
  0,
  0,
  450,  // ovrVol default: mid-range of valid 350-500 V (was 500)
  280,  // undVol default: minimum of valid 280-350 V range per spec (was 200, below minimum)

  0,
  0.1,
  10.0,
  "z0000",
  "z00000000",
  "z0000",
  { "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000" },
  "z0000",
  "",    // remote: empty — set via LoRa pairing
  "",    // gateway: empty — set via LoRa pairing
  0,     // rf_locked: 0 = not yet on operational channel
  10,    // senseTime default: 10 s
  2,     // onRelay default: 2 s
  3,     // offRelay default: 3 s
  0
};



void printStorage() {
  Serial3.println(F("===== STORAGE DUMP ====="));

  Serial3.print(F("Version: "));
  Serial3.println(storage.version);
  Serial3.print(F("Device Name: "));
  Serial3.println(storage.dname);

  Serial3.print(F("Over Voltage: "));
  Serial3.println(storage.ovrVol);
  Serial3.print(F("Under Voltage: "));
  Serial3.println(storage.undVol);

  Serial3.print(F("Mode M1: "));
  Serial3.println(storage.modeM1);
  Serial3.print(F("Dry Run M1: "));
  Serial3.println(storage.dryRunM1, 2);
  Serial3.print(F("Overload Run M1: "));
  Serial3.println(storage.ovLRunM1, 2);

  Serial3.print(F("Auto M1: "));
  Serial3.println(storage.autoM1);
  Serial3.print(F("Cyclic M1: "));
  Serial3.println(storage.cyclicM1);
  Serial3.print(F("count M1: "));
  Serial3.println(storage.countM1);

  for (int i = 0; i < 10; i++) {
    Serial3.print(F("Schedule ["));
    Serial3.print(i);
    Serial3.print(F("]: "));
    Serial3.println(storage.sch[i]);
  }
   Serial3.print(F("lasRemM1 M1: "));
  Serial3.println(storage.lasRemM1);

  Serial3.print(F("last appliance state A1: "));
  Serial3.println(storage.app1Run);

  Serial3.print(F("last appliance state A2: "));
  Serial3.println(storage.app2Run);

  Serial3.print(F("Remote ID: "));
  Serial3.println(storage.remote);
  Serial3.print(F("Gateway ID: "));
  Serial3.println(storage.gateway);
  Serial3.print(F("RF locked: "));
  Serial3.println(storage.rf_locked);

  Serial3.println(F("========================"));
}

void con_default() {
  Serial3.println("Writing default values to EEPROM...");
  // Set storage to default values
  storage = storage_default;
  // Write default values to EEPROM
  for (unsigned int t = 0; t < sizeof(storage_default); t++) {
    uint8_t value = *((char*)&storage_default + t);
    EEPROM.update(CONFIG_START + t, value);
    // Verify write
    if (EEPROM.read(CONFIG_START + t) != value) {
      Serial3.print("Default write verification failed at offset ");
      Serial3.println(t);
    }
  }
  markStorageShadow();
}



void loadcon() {
  // Check if the stored version matches CONFIG_VERSION
  if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] && EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] && EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2]) {
    Serial3.println("Valid version found, loading EEPROM...");

    // lcd_set_cursor(1, 0);
    // lcd_print("EEPROM RETAIN ");

    // Read the structure from EEPROM
    for (unsigned int t = 0; t < sizeof(storage); t++) {
      *((char*)&storage + t) = EEPROM.read(CONFIG_START + t);
    }
  } else {
    Serial3.println("Invalid version or uninitialized EEPROM, loading defaults...");
    // lcd_set_cursor(1, 0);
    // lcd_print("EEPROM ERASED ");
    con_default();
  }


  //strncpy(storage.remote, REMOTE_ID, sizeof(storage.remote) - 1);
  //storage.remote[sizeof(storage.remote) - 1] = '\0';

  storage.remote[sizeof(storage.remote)  - 1] = '\0';  // ensure null termination
  storage.gateway[sizeof(storage.gateway) - 1] = '\0'; // ensure null termination
  if (storage.modeM1 >= '0' && storage.modeM1 <= '5') {
    storage.modeM1 = storage.modeM1 - '0';
  } else if (storage.modeM1 == 6) {
    storage.modeM1 = 5;
  } else if (storage.modeM1 > 5) {
    storage.modeM1 = 0;
  }

  printStorage();
  markStorageShadow();
}

void savecon() {
  if (!isStorageDirty()) {
    return;
  }
  Serial3.println("Saving to EEPROM...");
  // Write the structure to EEPROM
  for (unsigned int t = 0; t < sizeof(storage); t++) {
    uint8_t value = *((char*)&storage + t);
    EEPROM.update(CONFIG_START + t, value);
    // Verify write
    if (EEPROM.read(CONFIG_START + t) != value) {
      Serial3.print("Write verification failed at offset ");
      Serial3.println(t);
    }
  }
  markStorageShadow();
  Serial3.println("Save complete.");
}
