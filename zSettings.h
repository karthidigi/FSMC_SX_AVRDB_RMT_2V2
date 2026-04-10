////////////////////////////////////////////////////////////////////////////////////////////////
#define FIRMWARE_VERSION        "FSMC_3PHSX"
#define HARDWARE_VERSION        "0.0.2"
////////////////////////////////////////////////////////////////////////////////////////////////
// Serial Monitor for Debug Purpose
#define SERIAL_DEBUG                // for serial monitor, comment it before uploading to a product
#define SERIAL_DEBUG_P              // for serial monitor with priority, comment it before uploading to a product
#define SERIAL_BAUD     115200
#define SERIAL_TIMEOUT  100

#define STA_MIN_AMPHS 1.5

#define PHASE_VOLAGE_MINIMUM 200
#define CURRENT_TH_DELAY_MILLIS 5000
#define VOLTAGE_TH_DELAY_MILLIS 3000


//////////////////////////
// HEY THERE DONT FORGET TO COMMENT IN PRODUCTION 
// OR ELSE => YOU WILL BE NOODLE
// #define SIMULTION
//////////////////////////
// ---- DS3231 Real-Time Clock ----
// Define ENABLE_RTC if the DS3231 module is physically installed.
// Comment it out to exclude all RTC I2C code from compilation and
// remove the Date/Time item from the status screen.
// #define ENABLE_RTC

// ── Default operating mode for fresh EEPROM ──────────────────────────────────
// 0 = Normal  1 = Auto  2 = Cyclic  3 = Countdown  4 = Scheduler  5 = Last State Remember
// Last State Remember (5): motor restores its last commanded ON/OFF on every power-up.
#define DEFAULT_MODE_M1  5

// ── Remote-only operation lock ────────────────────────────────────────────────
// Define REMOTE_ONLY_MODE to lock the device to Last State Remember (mode 5).
// While defined:
//   • LCD menu mode-change items (Normal/Auto/Cyclic/Countdown/Scheduler) are
//     blocked — selecting them shows "MODE LOCKED" and returns to the home screen.
//   • Last State delay setting (menu case 1610) remains accessible.
//   • The MODE-button 1-second hold (toggleNormalAutoMode) is disabled.
//   • IoT a1ops shared-attribute can still override mode from the app if needed.
// Comment out this line to restore full menu mode-switching.
// #define REMOTE_ONLY_MODE
 
#define BPCOUNTER 120    // 120 × 5 ms = 600 ms hold to confirm OK / CANCEL (was 1000 = 5 s)


#define ENCRYPTION_ENABLED 1

#define CONFIG_START 30
#define CONFIG_VERSION "9001"   // bumped: full 4-byte check; forces clean EEPROM reset with correct defaults

// REMOTE_ID removed — remote serial is now stored via LoRa pairing (storage.remote).
// Commented-out IDs kept for reference only.
//#define FAKE_REMOTE_ID "305550514849b9fe3f28"
//#define REMOTE_ID "30555051484979133914" // Electricain Palanisamy
//#define REMOTE_ID "30555051484919363A24" //LLH3
//#define REMOTE_ID "30555051484979BE1F4E" // LLH1
//#define REMOTE_ID "305550514849395D1F61" // LLH2 - Working Code
//#define REMOTE_ID "30555051484959DE3F2D" // LLH4
//#define REMOTE_ID "305550514849B923390F" //SX1262-AVRDB-V2-2 (was hardcoded; use pairing instead)

// ── LoRa Pairing channel (SF7, distinct sync word — invisible to operational nodes) ──
#define PAIR_SF           7
#define PAIR_BW           SX1268_BW_125000
#define PAIR_CR           SX1268_CR_4_5
#define PAIR_PREAMBLE     8
#define PAIR_SYNC_MSB     0x12
#define PAIR_SYNC_LSB     0x34
#define PAIR_TX_POWER     14
// LDRO: SF7+BW125 → symbol time 1.0ms < 16ms → OFF (literal 0)

// ── Operational channel (ALL 3 DEVICES must use this after pairing) ──
#define OPER_SF           11
#define OPER_BW           SX1268_BW_125000
#define OPER_CR           SX1268_CR_4_5
#define OPER_PREAMBLE     12
#define OPER_SYNC_MSB     0x34
#define OPER_SYNC_LSB     0x44
#define OPER_TX_POWER     22
// LDRO: SF11+BW125 → symbol time 16.4ms >= 16ms → ON (literal 1)

// ── Pairing packet types ──
// 0x0A-0x0F are binary control bytes; AES-encrypted hex always starts 0x30-0x46 → unambiguous
#define PKT_REM_PAIR_REQ  0x0D   // Remote  → Starter  [type][rem_serial:20]
#define PKT_REM_PAIR_ACK  0x0E   // Starter → Remote   [type][starter_id:20][sf][bwCode][cr][pre][pwr][syncMsb][syncLsb]
#define PKT_REM_PAIR_DONE 0x0F   // Remote  → Starter  [type][serial_echo:4]

// ── Pairing timing ──
#define PAIR_BEACON_INTERVAL_MS  2000UL   // beaconing devices re-TX every 2 s
#define PAIR_SCAN_WINDOW_MS     60000UL   // scanner gives up after 60 s
#define PAIR_ACK_TIMEOUT_MS      5000UL   // wait-for-DONE per attempt
#define PAIR_ACK_MAX_RETRIES         3
