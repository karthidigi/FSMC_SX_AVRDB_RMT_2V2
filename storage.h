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

  uint16_t senseTime;   // Sense time in seconds (10–120)
  uint8_t  onRelay;     // ON relay latch in seconds (2–5)
  uint8_t  offRelay;    // OFF relay latch in seconds (2–5)

  float    openWireThA;  // open-wire phase current threshold (A); default 1.0
  float    leakagePct;   // leakage / imbalance trip threshold (%); default 30.0

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
  CONFIG_VERSION,        // version[5]
  "SMART PUMP-2026",     // dname[34]
  0,                     // app1Run
  0,                     // app2Run
  490,                   // ovrVol  — over-voltage trip (V); valid 350–500 V
  280,                   // undVol  — under-voltage trip (V); valid 280–350 V

  DEFAULT_MODE_M1,       // modeM1  — see zSettings.h → DEFAULT_MODE_M1
  1.0f,                  // dryRunM1  — dry-run trip current (A); motor stops if Iavg < this
  10.0f,                 // ovLRunM1  — overload trip current (A); motor stops if Imax > this

  "z0000",               // autoM1
  "z00000000",           // cyclicM1
  "z0000",               // countM1
  { "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000",
    "z00000000000000000" },  // sch[10]
  "z0000",               // lasRemM1
  "",                    // remote  — set via LoRa pairing

  10,                    // senseTime — fault sense window (s); valid 1–120 s
  2,                     // onRelay   — ON relay latch time (s)
  2,                     // offRelay  — OFF relay latch time (s)

  1.0f,                  // openWireThA — open-wire phase threshold (A)
  30.0f,                 // leakagePct  — leakage imbalance threshold (%)
  0                      // configflag
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

  Serial3.print(F("Sense Time (s): "));
  Serial3.println(storage.senseTime);
  Serial3.print(F("ON Relay Latch (s): "));
  Serial3.println(storage.onRelay);
  Serial3.print(F("OFF Relay Latch (s): "));
  Serial3.println(storage.offRelay);

  Serial3.print(F("Over Voltage Trip (V): "));
  Serial3.println(storage.ovrVol);
  Serial3.print(F("Under Voltage Trip (V): "));
  Serial3.println(storage.undVol);
  Serial3.print(F("Dry Run Trip (A): "));
  Serial3.println(storage.dryRunM1, 2);
  Serial3.print(F("Overload Trip (A): "));
  Serial3.println(storage.ovLRunM1, 2);
  Serial3.print(F("Open Wire Threshold (A): "));
  Serial3.println(storage.openWireThA, 2);
  Serial3.print(F("Leakage Imbalance (%): "));
  Serial3.println(storage.leakagePct, 1);

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
  // Check all 4 bytes of CONFIG_VERSION — previously only 3 were checked,
  // so versions differing only in the last character (e.g. "8093" vs "8094")
  // were treated as identical and stale EEPROM was never reinitialised.
  if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
      EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
      EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2] &&
      EEPROM.read(CONFIG_START + 3) == CONFIG_VERSION[3]) {
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

  storage.remote[sizeof(storage.remote) - 1] = '\0';  // ensure null termination
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
