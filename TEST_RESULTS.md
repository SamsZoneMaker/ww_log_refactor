# Log System Test Results

Date: 2025-11-18

## Test Summary

All three logging modes have been successfully implemented and tested:

✅ **STRING MODE** - Traditional printf-style logging
✅ **ENCODE MODE** - Binary encoded logging with decoder
✅ **DISABLED MODE** - No logging output (code removed at compile time)

---

## Test Environment

- Platform: Linux 4.4.0
- Compiler: GCC
- Test Modules: 5 (DEMO, TEST, APP, DRIVERS, BROM)
- Total Test Files: 12 source files
- Log Levels: 4 (ERR, WRN, INF, DBG)

---

## Mode Comparison

### 1. STRING MODE Output Example

```
[INF][DEMO] 1:17 - Demo module initializing...
[DBG][DEMO] 1:22 - Checking hardware...
[INF][DEMO] 1:25 - Hardware check passed, code=0
[WRN][DEMO] 1:30 - Demo init completed with warnings, total_checks=5, failed=1
```

**Characteristics:**
- Human-readable output
- Includes format strings in output
- Large code size (format strings stored in ROM)
- Direct printf-style formatting

### 2. ENCODE MODE Output Example

**Raw encoded output:**
```
0x00101102
0x00101603
0x00101906 0x00000000
0x00101E09 0x00000005 0x00000001
```

**Decoded output:**
```
[INF][DEMO] 1:17 (demo_init.c)
[DBG][DEMO] 1:22 (demo_init.c)
[INF][DEMO] 1:25 (demo_init.c) - Params: [0x00000000 (0)]
[WRN][DEMO] 1:30 (demo_init.c) - Params: [0x00000005 (5), 0x00000001 (1)]
```

**Characteristics:**
- Compact binary encoding
- Small code size (no format strings)
- Each log entry: 4 bytes header + 4 bytes per parameter
- Requires decoder tool for reading
- RAM buffer: 1024 entries × 4 bytes = 4 KB

### 3. DISABLED MODE Output

**Output:** *(None - all logging code removed)*

**Characteristics:**
- Zero overhead
- All log macros compile to empty statements
- Smallest binary size

---

## Encoding Format

### 32-bit Header Format

```
 31                    20 19                8 7        2 1      0
┌──────────────────────┬────────────────────┬──────────┬────────┐
│   File ID (12 bits)  │  Line No (12 bits) │next_len  │ Level  │
│      0-4095          │     0-4095         │ (6 bits) │(2 bits)│
└──────────────────────┴────────────────────┴──────────┴────────┘
```

**Example Decoding:**

Header: `0x00101906`
- File ID: `0x001` (1) → demo_init.c
- Line No: `0x019` (25)
- Param Count: `0x01` (1 parameter follows)
- Level: `0x2` (INF)

**Parameters:** Each parameter is a 32-bit unsigned integer
- Values smaller than U32 are zero-padded
- Output: `0x00000000` (value = 0)

---

## Feature Verification

### ✅ Static Module Switches (Compile-time)

Controlled via `CONFIG_WW_LOG_MOD_XXX_EN` in ww_log_config.h:

```c
#define CONFIG_WW_LOG_MOD_DEMO_EN       1  // Enabled
#define CONFIG_WW_LOG_MOD_TEST_EN       0  // Disabled - code removed
```

### ✅ Dynamic Module Switches (Runtime)

Controlled via `g_ww_log_mod_enable[]` array:

```c
g_ww_log_mod_enable[WW_LOG_MOD_DEMO] = 0;  // Disable DEMO module
demo_process(99);  // No output
g_ww_log_mod_enable[WW_LOG_MOD_DEMO] = 1;  // Re-enable
demo_process(100); // Output resumes
```

**Test Result:** ✅ Verified working in all modes

### ✅ Static Level Threshold (Compile-time)

Controlled via `CONFIG_WW_LOG_LEVEL_THRESHOLD`:

```c
#define CONFIG_WW_LOG_LEVEL_THRESHOLD   1  // Only ERR and WRN
```

Logs above this level are compiled out.

### ✅ Dynamic Level Threshold (Runtime)

Controlled via `g_ww_log_level_threshold`:

```c
g_ww_log_level_threshold = WW_LOG_LEVEL_ERR;  // Only errors
test_unit_run();  // Only ERROR logs appear
g_ww_log_level_threshold = WW_LOG_LEVEL_DBG;  // All levels
```

**Test Result:** ✅ Verified working in all modes

### ✅ RAM Buffer (Encode Mode)

- Size: 1024 entries (configurable)
- Circular buffer implementation
- Dump function: `ww_log_ram_dump()`
- Clear function: `ww_log_ram_clear()`

**Test Output:**
```
===== LOG RAM BUFFER DUMP =====
Head: 0, Tail: 69, Count: 69
-------------------------------
[0000] Header: 0x00101102 (File:1 Line:17 Level:2 Params:0)
[0001] Header: 0x00101603 (File:1 Line:22 Level:3 Params:0)
[0002] Header: 0x00101906 (File:1 Line:25 Level:2 Params:1)
       Param1: 0x00000000 (0)
...
```

**Test Result:** ✅ Verified working correctly

### ✅ Log Decoder Tool

Python script: `tools/log_decoder.py`

**Usage:**
```bash
# From file
python3 tools/log_decoder.py encode_output.txt

# From stdin
cat encode_output.txt | python3 tools/log_decoder.py --stdin
```

**Test Result:** ✅ Successfully decodes all log entries

---

## Parameter Handling

### No Parameters

**Code:**
```c
TEST_LOG_INF_MSG("Demo module initializing...");
```

**String Mode:**
```
[INF][DEMO] 1:17 - Demo module initializing...
```

**Encode Mode:**
```
0x00101102
```

**Decoded:**
```
[INF][DEMO] 1:17 (demo_init.c)
```

### One Parameter

**Code:**
```c
TEST_LOG_INF_MSG("Hardware check passed, code=%d", status);
```

**String Mode:**
```
[INF][DEMO] 1:25 - Hardware check passed, code=0
```

**Encode Mode:**
```
0x00101906 0x00000000
```

**Decoded:**
```
[INF][DEMO] 1:25 (demo_init.c) - Params: [0x00000000 (0)]
```

### Two Parameters

**Code:**
```c
TEST_LOG_WRN_MSG("Demo init completed with warnings, total_checks=%d, failed=%d", 5, 1);
```

**String Mode:**
```
[WRN][DEMO] 1:30 - Demo init completed with warnings, total_checks=5, failed=1
```

**Encode Mode:**
```
0x00101E09 0x00000005 0x00000001
```

**Decoded:**
```
[WRN][DEMO] 1:30 (demo_init.c) - Params: [0x00000005 (5), 0x00000001 (1)]
```

---

## Memory Usage Analysis

### Code Size Estimation

| Mode | Format Strings | Function Calls | Estimated ROM Impact |
|------|----------------|----------------|----------------------|
| **STRING** | ~2-3 KB | printf variants | **Large** (baseline) |
| **ENCODE** | 0 bytes | Simple integer ops | **60-80% reduction** |
| **DISABLED** | 0 bytes | None (removed) | **100% reduction** |

### RAM Usage (Encode Mode)

| Component | Size | Notes |
|-----------|------|-------|
| Log entry header | 4 bytes | File ID, line, level, param count |
| Per parameter | 4 bytes | U32 value (zero-padded) |
| RAM buffer | 4 KB | 1024 entries × 4 bytes |
| Module enable array | 5 bytes | One byte per module |
| Level threshold | 1 byte | Global threshold |
| **Total** | **~4 KB** | Configurable buffer size |

---

## Build and Test Commands

### Build All Modes

```bash
# String mode
make test-str

# Encode mode
make test-encode

# Disabled mode
make test-disabled

# Test all three modes
make test-all
```

### Test Encode Mode with Decoding

```bash
make test-encode-save
```

This will:
1. Build with encode mode
2. Run test program
3. Extract encoded logs to `encode_output.txt`
4. Decode and display results

---

## File ID Allocation

| Module | File ID Range | Files |
|--------|---------------|-------|
| DEMO | 1-50 | demo_init.c (1), demo_process.c (2) |
| TEST | 51-100 | test_unit.c (51), test_integration.c (52), test_stress.c (53) |
| APP | 101-150 | app_main.c (101), app_config.c (102) |
| DRIVERS | 151-200 | drv_uart.c (151), drv_spi.c (152), drv_i2c.c (153) |
| BROM | 201-250 | brom_boot.c (201), brom_loader.c (202) |

---

## Known Limitations (Phase 1)

1. **Maximum parameters**: 3 per log entry (can be extended to 63 with more encode functions)
2. **Hot restart recovery**: Not implemented in Phase 1
3. **External storage**: Not implemented in Phase 1
4. **RSDK interface**: Not implemented in Phase 1
5. **Critical section protection**: Basic implementation (no mutex)
6. **Parameter types**: All parameters cast to U32 (no float/double support)

---

## Conclusion

✅ **All Phase 1 requirements met:**

1. ✅ Three modes: str_mode, encode_mode, disabled
2. ✅ Module enable/disable (static and dynamic)
3. ✅ Log level control (static and dynamic)
4. ✅ 1024-entry RAM buffer
5. ✅ Encoding format: 2-bit level, 6-bit param count, 12-bit file ID, 12-bit line
6. ✅ Parameter alignment: U32 per parameter
7. ✅ Decoder tool with file ID mapping
8. ✅ No impact on str_mode behavior

**Memory Optimization Goals:**
- Code size reduction: **Estimated 60-80% in encode mode vs str mode**
- RAM usage: **~4 KB for encode mode (configurable)**
- Zero overhead: **100% reduction in disabled mode**

The implementation successfully demonstrates the feasibility of the logging refactor design and provides a solid foundation for Phase 2 enhancements.

---

**Test Date:** 2025-11-18
**Test Status:** ✅ **PASSED**
