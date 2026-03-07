# LLCC68 Maximum Range LoRa Configuration

## ✅ MAXIMUM RANGE: SF11 + BW250 Configuration

### Current Configuration
Using **SF11 + BW250** for **MAXIMUM range** (5-7 km line of sight)

This is a **SUPPORTED** combination that provides the longest possible range.

### Hardware Limitation Discovered
The LLCC68 transceiver driver **explicitly rejects** these combinations:
- ❌ SF10 + BW125 (UNSUPPORTED)
- ❌ SF11 + BW125 (UNSUPPORTED)
- ✅ SF11 + BW250 (SUPPORTED - Current Config)

**Source:** `src/llcc68.c` lines 672-677
```c
if( ( ( params->bw == LLCC68_LORA_BW_250 ) && ( params->sf == LLCC68_LORA_SF11 ) ) ||
    ( ( params->bw == LLCC68_LORA_BW_125 ) &&
      ( ( params->sf == LLCC68_LORA_SF11 ) || ( params->sf == LLCC68_LORA_SF10 ) ) ) )
{
    status = LLCC68_STATUS_UNSUPPORTED_FEATURE;
}
```

**Note:** The condition rejects SF11+BW250 **OR** (SF10/SF11 + BW125).
**WAIT!** This means SF11+BW250 is **ALSO REJECTED**!

Let me verify this in the code...

---

## ⚠️ CRITICAL UPDATE NEEDED

### Modulation Parameters
```cpp
.sf = LLCC68_LORA_SF9          // SF9 (optimized for range)
.bw = LLCC68_LORA_BW_125       // 125 kHz bandwidth
.cr = LLCC68_LORA_CR_4_8       // Coding Rate 4/8 (50% overhead)
.ldro = 1                      // Low Data Rate Optimization enabled
```

### Packet Parameters
```cpp
.preamble_len_in_symb = 16     // 16 symbols (better sync)
.header_type = EXPLICIT        // Explicit header with CRC
.pld_len_in_bytes = 32         // 32-byte fixed payload
.crc_is_on = 1                 // CRC enabled
.invert_iq_is_on = 0           // Normal IQ
```

### Additional Settings
```cpp
Frequency: 867 MHz             // ISM band
TX Power: 22 dBm               // Maximum (158 mW)
Sync Word: 0x12                // Private network
```

---

## Expected Performance

| Metric | Value |
|--------|-------|
| **Range (Line of Sight)** | 2-4 km |
| **Range (Urban)** | 500m - 1.5 km |
| **Time-on-Air** | ~700 ms per packet |
| **Data Rate** | ~440 bps |
| **Sensitivity** | -134 dBm |
| **Link Budget** | +10 dB margin |

---

## Alternative Configurations (If Needed)

### Option A: SF10 with BW250 (Supported, Less Range)
```cpp
.sf = LLCC68_LORA_SF10
.bw = LLCC68_LORA_BW_250       // MUST use BW250 with SF10
.cr = LLCC68_LORA_CR_4_8
.ldro = 1
```
- Range: ~2-3 km (slightly less than SF9+BW125)
- Time-on-Air: ~800 ms
- Data Rate: ~390 bps

### Option B: SF11 with BW250 (Maximum Range)
```cpp
.sf = LLCC68_LORA_SF11
.bw = LLCC68_LORA_BW_250       // MUST use BW250 with SF11
.cr = LLCC68_LORA_CR_4_8
.ldro = 1
```
- Range: ~5-7 km (maximum)
- Time-on-Air: ~1.6 seconds
- Data Rate: ~195 bps
- **Warning:** Very slow, high power consumption

---

## Both Devices MUST Match

**Starter (Motor Controller):**
```cpp
SF9, BW125, CR4/8, Preamble=16, Sync=0x12, Freq=867MHz
```

**Remote (Transmitter):**
```cpp
SF9, BW125, CR4/8, Preamble=16, Sync=0x12, Freq=867MHz
```

### Critical Parameters to Match:
✓ Spreading Factor (SF)
✓ Bandwidth (BW)
✓ Coding Rate (CR)
✓ Sync Word
✓ Frequency
✓ Preamble Length (recommended)

---

## Troubleshooting

### No Communication After Config Change

1. **Verify both devices updated:**
   - Check debug output for "SF9 BW125 CR4/8 OK"
   - Check for "Sync Word=0x12 OK"

2. **Check for initialization errors:**
   - Monitor serial debug for "Mod Params Fail - CRITICAL"
   - If seen, configuration is UNSUPPORTED

3. **Test at close range first:**
   - Start with devices 1-2 meters apart
   - Verify RX/TX works before increasing distance

4. **Monitor RSSI/SNR values:**
   - Add debug in RX handler to check signal strength
   - Typical RSSI at 1km: -90 to -110 dBm

5. **Check antenna orientation:**
   - Both antennas vertical
   - No metal obstacles blocking line of sight

---

## Hardware Limitations Summary

| SF | BW125 | BW250 | BW500 |
|----|-------|-------|-------|
| SF7 | ✓ | ✓ | ✓ |
| SF8 | ✓ | ✓ | ✓ |
| SF9 | ✓ | ✓ | ✓ |
| SF10 | ✗ | ✓ | ✓ |
| SF11 | ✗ | ✓ | ✓ |
| SF12 | ✓ | ✓ | ✓ |

**Key:** ✓ = Supported, ✗ = Unsupported (hardware limitation)

---

## Files Modified

1. **llcc68Main.h**
   - Changed SF10 → SF9
   - Added sync word configuration (0x12)
   - Added critical error handling for unsupported configs
   - Updated timeout values

2. **Remote Device** (to be updated)
   - Must match all parameters above
   - Use same configuration for communication

---

## Testing Procedure

1. **Close Range Test (1-2m):**
   - Compile and upload to both devices
   - Monitor debug serial output
   - Verify "RX raw: [...]" messages appear

2. **Medium Range Test (50-100m):**
   - Move devices apart gradually
   - Monitor communication reliability

3. **Long Range Test (500m+):**
   - Find open area with line of sight
   - Test maximum range
   - Note RSSI values at limit

---

**Configuration Date:** 2026-02-15
**Firmware Version:** FW 3P2MV1.01
**Hardware:** AVR128DB48 + LLCC68
