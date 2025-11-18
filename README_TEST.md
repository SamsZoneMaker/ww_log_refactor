# Log System Refactor - Test Project

This is a test implementation of the logging system refactor design for the ww_log project.

## Project Structure

```
ww_log_refactor/
├── include/
│   ├── ww_log_config.h     # Configuration options
│   ├── log_file_id.h       # File ID enumeration
│   └── ww_log.h            # Public API
├── src/
│   ├── core/               # Core logging implementation
│   │   ├── ww_log_common.c
│   │   ├── ww_log_str.c    # String mode
│   │   └── ww_log_encode.c # Encode mode
│   ├── demo/               # Demo module (2 files)
│   ├── test/               # Test module (3 files)
│   ├── app/                # App module (2 files)
│   ├── drivers/            # Drivers module (3 files)
│   └── brom/               # BROM module (2 files)
├── examples/
│   └── main.c              # Test program
├── tools/
│   └── log_decoder.py      # Decode tool for encode mode
├── Makefile
└── README_TEST.md          # This file
```

## Features Implemented

### 1. Three Logging Modes

- **DISABLED**: No logging output (all code removed at compile time)
- **STR_MODE**: Traditional printf-style logging with format strings
- **ENCODE_MODE**: Binary encoding with file ID + line number + parameters

### 2. Module Control

- **Static switches**: Compile-time enable/disable via `CONFIG_WW_LOG_MOD_XXX_EN`
- **Dynamic switches**: Runtime enable/disable via `g_ww_log_mod_enable[]` array

### 3. Log Level Control

- **Static threshold**: Compile-time filter via `CONFIG_WW_LOG_LEVEL_THRESHOLD`
- **Dynamic threshold**: Runtime control via `g_ww_log_level_threshold`

### 4. Log Encoding Format (32-bit)

```
 31                    20 19                8 7        2 1      0
┌──────────────────────┬────────────────────┬──────────┬────────┐
│   File ID (12 bits)  │  Line No (12 bits) │next_len  │ Level  │
│      0-4095          │     0-4095         │ (6 bits) │(2 bits)│
└──────────────────────┴────────────────────┴──────────┴────────┘
```

- File ID: 12 bits (0-4095)
- Line Number: 12 bits (0-4095)
- Parameter Count: 6 bits (0-63)
- Log Level: 2 bits (0=ERR, 1=WRN, 2=INF, 3=DBG)

### 5. RAM Buffer

- Circular buffer with 1024 entries (configurable)
- Stores encoded log entries for persistence
- Includes dump function for inspection

## Quick Start

### Build and Test

```bash
# Show help
make help

# Test all three modes
make test-all

# Test individual modes
make test-str          # String mode
make test-encode       # Encode mode
make test-disabled     # Disabled mode

# Test encode mode with decoding
make test-encode-save
```

### Manual Build

```bash
# Build with current config
make clean
make

# Run the test program
make run
```

### Decode Encoded Logs

```bash
# Run encode mode and save output
./bin/log_test 2>&1 | grep "^0x" > encode_output.txt

# Decode the logs
python3 tools/log_decoder.py encode_output.txt

# Or decode from stdin
cat encode_output.txt | python3 tools/log_decoder.py --stdin
```

## Configuration

Edit `include/ww_log_config.h` to change settings:

```c
/* Select one mode */
// #define CONFIG_WW_LOG_DISABLED
#define CONFIG_WW_LOG_STR_MODE
// #define CONFIG_WW_LOG_ENCODE_MODE

/* Module static switches */
#define CONFIG_WW_LOG_MOD_DEMO_EN       1
#define CONFIG_WW_LOG_MOD_TEST_EN       1
#define CONFIG_WW_LOG_MOD_APP_EN        1
#define CONFIG_WW_LOG_MOD_DRIVERS_EN    1
#define CONFIG_WW_LOG_MOD_BROM_EN       1

/* RAM buffer size */
#define CONFIG_WW_LOG_RAM_ENTRY_NUM     1024

/* Global log level threshold */
#define CONFIG_WW_LOG_LEVEL_THRESHOLD   3  /* DBG */
```

## Usage Examples

### In Application Code

```c
#include "ww_log.h"

/* Define for each source file */
#define CURRENT_FILE_ID   FILE_ID_DEMO_INIT
#define CURRENT_MODULE_ID WW_LOG_MOD_DEMO

void my_function(void)
{
    /* No parameters */
    TEST_LOG_INF_MSG("System starting...");

    /* One parameter */
    int status = 0;
    TEST_LOG_INF_MSG("Status code: %d", status);

    /* Two parameters */
    int x = 100, y = 200;
    TEST_LOG_INF_MSG("Coordinates: x=%d, y=%d", x, y);

    /* Error logging */
    if (status != 0) {
        TEST_LOG_ERR_MSG("Operation failed!");
    }
}
```

### Dynamic Module Control

```c
/* Disable DEMO module at runtime */
g_ww_log_mod_enable[WW_LOG_MOD_DEMO] = 0;

/* Re-enable DEMO module */
g_ww_log_mod_enable[WW_LOG_MOD_DEMO] = 1;
```

### Dynamic Level Control

```c
/* Only show errors */
g_ww_log_level_threshold = WW_LOG_LEVEL_ERR;

/* Show all levels */
g_ww_log_level_threshold = WW_LOG_LEVEL_DBG;
```

## Output Examples

### String Mode Output

```
[INF][DEMO] 1:10 - Demo module initializing...
[DBG][DEMO] 1:15 - Checking hardware...
[INF][DEMO] 1:18 - Hardware check passed, code=0
[WRN][DEMO] 1:23 - Demo init completed with warnings, total_checks=5, failed=1
```

### Encode Mode Output

```
0x00100283
0x000100C3
0x00100484 0x00000000
0x001005C1 0x00000005 0x00000001
```

### Decoded Encode Mode Output

```
[INF][DEMO] 1:10 (demo_init.c)
[DBG][DEMO] 1:15 (demo_init.c)
[INF][DEMO] 1:18 (demo_init.c) - Params: [0x00000000 (0)]
[WRN][DEMO] 1:23 (demo_init.c) - Params: [0x00000005 (5), 0x00000001 (1)]
```

## Test Modules

The project includes 5 test modules:

1. **DEMO** (File IDs 1-50): Basic initialization and processing
2. **TEST** (File IDs 51-100): Unit, integration, and stress tests
3. **APP** (File IDs 101-150): Application main and configuration
4. **DRIVERS** (File IDs 151-200): UART, SPI, I2C drivers
5. **BROM** (File IDs 201-250): Boot ROM and loader

Each module demonstrates:
- Logs without parameters
- Logs with 1 parameter
- Logs with 2 parameters
- All 4 log levels (ERR, WRN, INF, DBG)

## Memory Usage

### String Mode
- Large code size (format strings stored in ROM)
- Stack usage for printf formatting
- Human-readable output

### Encode Mode
- Minimal code size (only integer encoding)
- Low RAM usage (4 bytes per header + 4 bytes per parameter)
- RAM buffer: 1024 entries × 4 bytes = 4 KB
- Requires decoder tool for reading

## Next Steps

This is a Phase 1 implementation. Future enhancements:

1. External storage support (Flash/EEPROM)
2. Hot restart recovery
3. RSDK interface integration
4. Critical section protection (mutex/interrupt disable)
5. More sophisticated parameter encoding (variable length)
6. Log filtering by file/module/level
7. Real-time streaming over network

## License

This is a test implementation for design validation.
