# CLAUDE.md - AI Assistant Guide for ww_log_refactor

> **Last Updated:** 2025-11-18
> **Repository:** ww_log_refactor
> **Purpose:** Logging System Refactor Design Documentation

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Repository Structure](#repository-structure)
3. [Key Concepts](#key-concepts)
4. [Development Workflows](#development-workflows)
5. [Conventions and Standards](#conventions-and-standards)
6. [Common Tasks](#common-tasks)
7. [Important Notes for AI Assistants](#important-notes-for-ai-assistants)

---

## Project Overview

### Purpose

This repository contains **design documentation** for refactoring the logging system in "Project-D" (an embedded systems project). The primary goals are:

1. **Reduce code size by 60-80%** by replacing string-based logging with encoded logging
2. **Reduce RAM usage by 50%+** through compact log encoding and efficient buffering
3. **Maintain complete log traceability** with file ID and line number encoding
4. **Support flexible logging modes** (string mode vs encode mode) via compile-time configuration

### Current Status

This is a **documentation-only repository** at this stage. It contains:
- Design requirements (Chinese)
- Detailed design specifications (Chinese)
- Architecture diagrams (Mermaid)
- Data structure definitions
- API specifications

**No implementation code exists yet** - this is purely design phase documentation.

### Target Audience

- Embedded systems developers working on Project-D
- Firmware engineers implementing the logging system
- AI assistants helping with design reviews or implementation planning

---

## Repository Structure

```
ww_log_refactor/
├── README.md                           # Brief project description (English)
├── CLAUDE.md                          # This file - AI assistant guide
└── doc/                               # Design documentation (Chinese)
    ├── log模块更新设计要求.md          # Requirements specification
    └── 日志模块重构设计方案.md         # Detailed design document
```

### Key Files

| File | Purpose | Language | Lines |
|------|---------|----------|-------|
| `README.md` | Brief project overview | English | ~3 |
| `doc/log模块更新设计要求.md` | Design requirements and constraints | Chinese | ~112 |
| `doc/日志模块重构设计方案.md` | Comprehensive design specification with architecture, data structures, APIs, examples | Chinese | ~2812 |

---

## Key Concepts

### 1. Two Logging Modes

The system supports two mutually exclusive modes (selected at compile time):

#### **str_mode (String Mode)**
- Traditional printf-style logging with format strings
- Example: `TEST_LOG_INFO_MSG("Temperature: %d°C", temp);`
- **Pros:** Human-readable, familiar interface
- **Cons:** Large code size (format strings), high RAM usage (stack space for formatting)

#### **encode_mode (Encoded Mode)**
- Compact binary encoding: 32-bit integer contains file ID + line number + log level
- Example: Same `TEST_LOG_INFO_MSG()` interface, but internally encodes to 0xABC12304
- **Pros:** Minimal code size, low RAM usage, persistent storage friendly
- **Cons:** Requires decoder tool to read logs, slightly more complex setup

### 2. Unified Interface

**Critical:** Both modes use **identical macros** in application code:
- `TEST_LOG_ERR_MSG(fmt, ...)`
- `TEST_LOG_WRN_MSG(fmt, ...)`
- `TEST_LOG_INF_MSG(fmt, ...)`
- `TEST_LOG_DBG_MSG(fmt, ...)`

The implementation switches based on compile-time configuration.

### 3. File ID Management

Each source file gets a unique 12-bit ID (0-4095):
- Defined in `log_file_id.h` enum (e.g., `FILE_ID_SENSOR_TEMPERATURE = 151`)
- Each .c file defines `#define CURRENT_FILE_ID FILE_ID_SENSOR_TEMPERATURE`
- IDs are organized by functional module (1-50 for init, 51-150 for drivers, etc.)

### 4. Log Encoding Scheme (encode_mode)

32-bit encoding format:
```
 31                    20 19                8 7       4 3      0
┌──────────────────────┬────────────────────┬─────────┬────────┐
│   File ID (12 bits)  │  Line No (12 bits) │Reserved │ Level  │
│      0-4095          │     0-4095         │ (4 bits)│(4 bits)│
└──────────────────────┴────────────────────┴─────────┴────────┘
```

Example:
- File ID: 151 (sensor_temperature.c)
- Line: 234
- Level: 3 (INFO)
- Encoded: `0x0970EA03` = `(151 << 20) | (234 << 8) | 3`

### 5. Log Levels

```c
typedef enum {
    WW_LOG_LEVEL_OFF = 0,   // All logging disabled
    WW_LOG_LEVEL_ERR = 1,   // Errors: system failures, data corruption
    WW_LOG_LEVEL_WRN = 2,   // Warnings: potential issues, resource limits
    WW_LOG_LEVEL_INF = 3,   // Info: key state changes, major events
    WW_LOG_LEVEL_DBG = 4,   // Debug: detailed execution flow
} WW_LOG_LEVEL_E;
```

### 6. Output Targets

Logs can be routed to multiple destinations (configured at compile time):
- **UART:** Real-time output for debugging
- **RAM buffer:** Circular buffer for persistence across warm resets
- **External storage:** Optional flash/EEPROM for long-term storage

### 7. RAM Buffer Structure

```c
typedef struct {
    U16 logEntryHead;                        // Read pointer
    U16 logEntryTail;                        // Write pointer
    U32 logEntry[WW_LOG_RAM_ENTRY_NUM];     // Circular buffer (64-128 entries)
    U16 logMedia;                            // External storage log count
    // ...
} WW_LOG_RAM_T;
```

- Empty: `logEntryHead == logEntryTail`
- Full: `(logEntryTail + 1) % SIZE == logEntryHead`
- Write: Increment `logEntryTail`, handle wraparound
- Read: Increment `logEntryHead`, handle wraparound

---

## Development Workflows

### Current Phase: Design & Documentation

Since this is a documentation repository, typical workflows involve:

1. **Reviewing/Updating Requirements**
   - File: `doc/log模块更新设计要求.md`
   - Update design constraints, feature requirements, or use cases

2. **Refining Design Specifications**
   - File: `doc/日志模块重构设计方案.md`
   - Update architecture diagrams, data structures, or API definitions

3. **Adding Examples**
   - Add code snippets demonstrating usage patterns
   - Include both str_mode and encode_mode examples

### Future Phase: Implementation

When implementation begins, expected structure:

```
ww_log_refactor/
├── include/
│   ├── ww_log.h              # Public API
│   ├── ww_log_config.h       # Configuration options
│   └── log_file_id.h         # File ID enumeration
├── src/
│   ├── ww_log_str.c          # String mode implementation
│   ├── ww_log_encode.c       # Encode mode implementation
│   ├── ww_log_ram.c          # RAM buffer management
│   ├── ww_log_storage.c      # External storage (optional)
│   └── ww_log_api.c          # RSDK interface
├── tools/
│   └── log_decoder.py        # Decode binary logs
├── examples/
│   └── basic_usage.c
└── tests/
    └── test_log_buffer.c
```

---

## Conventions and Standards

### 1. Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| File IDs | `FILE_ID_<MODULE>_<NAME>` | `FILE_ID_SENSOR_TEMPERATURE` |
| Functions | `ww_log_<action>_<object>` | `ww_log_ram_write_entry()` |
| Macros (API) | `TEST_LOG_<LEVEL>_MSG` | `TEST_LOG_ERR_MSG()` |
| Config Macros | `CONFIG_WW_LOG_<FEATURE>` | `CONFIG_WW_LOG_ENCODE_MODE` |
| Types | `<NAME>_T` or `<NAME>_E` | `WW_LOG_RAM_T`, `WW_LOG_LEVEL_E` |

### 2. Module Organization

File IDs are organized by functional area:
- **1-50:** System initialization
- **51-150:** Driver layer
- **151-250:** Sensors
- **251-350:** Algorithms
- **351-450:** Communication
- **451-550:** Application layer

### 3. Configuration Management

All compile-time options are in `ww_log_config.h`:

```c
// Mode selection (mutually exclusive)
#define CONFIG_WW_LOG_ENCODE_MODE    // or CONFIG_WW_LOG_STR_MODE

// Output targets
#define CONFIG_WW_LOG_OUTPUT_UART
#define CONFIG_WW_LOG_OUTPUT_RAM
// #define CONFIG_WW_LOG_OUTPUT_FLASH

// Buffer size
#define CONFIG_WW_LOG_RAM_ENTRY_NUM  64

// Module-level static switches
#define CONFIG_WW_LOG_MOD_INIT_EN
#define CONFIG_WW_LOG_MOD_SENSOR_EN
// #define CONFIG_WW_LOG_MOD_DEBUG_EN  // Disabled
```

### 4. File ID Management Best Practices

1. **Never reuse IDs** - Mark as deprecated if file is removed
2. **Pre-allocate ranges** - Leave gaps for future additions
3. **Document in comments** - Include file path next to ID definition
4. **Version control** - Track ID assignments in design docs

Example:
```c
/* Sensor Module (151-250) */
FILE_ID_SENSOR_TEMPERATURE = 151,  // src/sensors/temperature.c
FILE_ID_SENSOR_PRESSURE = 152,     // src/sensors/pressure.c
// 153-160: Reserved for future sensors
```

### 5. Code Documentation

Use Doxygen-style comments for all public APIs:

```c
/**
 * @brief Write encoded log entry to buffer
 * @param level Log level (WW_LOG_LEVEL_E)
 * @param param1 Optional parameter (e.g., error code)
 * @param param2 Optional parameter (e.g., data value)
 * @return 0 on success, -1 if buffer full
 */
int log_encode_write(U8 level, U32 param1, U32 param2);
```

---

## Common Tasks

### Task 1: Adding a New Design Section

**When to do:** Expanding the design specification

**Steps:**
1. Open `doc/日志模块重构设计方案.md`
2. Follow the existing structure (numbered sections with clear hierarchy)
3. Include:
   - Design rationale
   - Code examples (C snippets)
   - Diagrams if applicable (use Mermaid)
   - Edge cases and error handling

**Example Addition:**
```markdown
## X. [New Feature Name]

### X.1 Design Rationale

[Explain why this feature is needed...]

### X.2 Implementation Approach

```c
// Code example
```

### X.3 Configuration

```c
#define CONFIG_NEW_FEATURE  1
```
```

### Task 2: Updating Requirements

**When to do:** Clarifying or adding new requirements

**Steps:**
1. Open `doc/log模块更新设计要求.md`
2. Add to appropriate section:
   - `## 总体目标` - High-level goals
   - `## 当前情况` - Current state analysis
   - `## 总体需求` - General requirements
   - `## Encode_mode` - Encode mode specifics
   - `## 使用场景` - Usage scenarios

### Task 3: Reviewing Design for Completeness

**Checklist:**
- [ ] All requirements have corresponding design sections
- [ ] Data structures are fully specified (with size constraints)
- [ ] APIs have clear parameter descriptions
- [ ] Error handling is documented
- [ ] Thread safety / critical sections are addressed
- [ ] Configuration options are documented
- [ ] Memory usage is analyzed (code size, RAM, etc.)
- [ ] Examples demonstrate common use cases

### Task 4: Preparing for Implementation

**Before coding begins:**
1. Finalize file ID allocation (complete `log_file_id.h` design)
2. Define all configuration macros
3. Document state machines (if any)
4. Create test plan document
5. Design decoder tool interface
6. Set up build configuration examples

---

## Important Notes for AI Assistants

### Understanding the Context

1. **Language:** Primary documentation is in **Chinese** (Simplified). When discussing with users, clarify their preferred language.

2. **Domain:** This is **embedded systems / firmware** development:
   - Memory constraints are critical (every byte counts)
   - Code size directly impacts ROM/Flash usage
   - RAM is scarce (likely <64KB total)
   - No dynamic memory allocation (malloc/free avoided)
   - Performance: microsecond-level latency concerns

3. **Target Platform:** Likely a microcontroller (ARM Cortex-M or similar):
   - Limited stack space
   - No OS or RTOS assumed (bare metal or lightweight RTOS)
   - UART is primary debug interface
   - May have watchdog timers (long operations are dangerous)

### When Helping with Design

1. **Memory Analysis:** Always consider:
   - How many bytes per log entry?
   - What's the ROM overhead (format strings, function calls)?
   - What's the stack usage?

2. **Trade-offs:** The core trade-off is:
   - **str_mode:** Easy to read, large code size, high RAM
   - **encode_mode:** Compact, needs decoder, minimal overhead

3. **Critical Sections:** Multi-task logging requires:
   - Atomic operations on circular buffer pointers
   - Minimal time in critical sections
   - Consider interrupt-driven logging

4. **Maintainability:** File ID management is crucial:
   - Avoid ID collisions
   - Make ID assignment systematic
   - Provide tools to validate ID uniqueness

### When Reviewing Code (Future)

1. **Check for Mode Leakage:**
   - Ensure str_mode code doesn't affect encode_mode binary size
   - Use `#ifdef` guards properly
   - Verify linker doesn't pull in unused printf code

2. **Validate Encoding:**
   - File ID fits in 12 bits (max 4095)
   - Line number fits in 12 bits (warn if file >4095 lines)
   - Level is 0-15

3. **Buffer Safety:**
   - Check for off-by-one errors in circular buffer
   - Verify wraparound logic
   - Test full/empty conditions

4. **Performance:**
   - Log functions should be inline or very fast
   - Minimize string operations
   - Avoid variadic functions if possible (they're slow on some platforms)

### Common Pitfalls

1. **Macro Hygiene:**
   ```c
   // BAD: Missing do-while, can break if-else
   #define LOG(x) foo(x); bar(x)

   // GOOD: Safe in all contexts
   #define LOG(x) do { foo(x); bar(x); } while(0)
   ```

2. **Parameter Extraction:**
   - C doesn't have true reflection
   - Variadic macros (`##__VA_ARGS__`) have limitations
   - May need multiple macro variants for different param counts

3. **Compile-Time vs Runtime:**
   - **Static switch:** `#ifdef CONFIG_MODULE_EN` (code is removed)
   - **Dynamic switch:** `if (g_log_enabled)` (code still in binary)

4. **Endianness:**
   - When storing to external flash, consider byte order
   - Document whether encoded U32 is stored as big/little endian

### Suggested Improvements (for Discussion)

If asked for recommendations, consider:

1. **Timestamping:** Use reserved 4 bits for compressed timestamp (e.g., ticks % 16)
2. **Task ID:** In RTOS environment, log which task generated the log
3. **Sequence Numbers:** Detect log loss in circular buffer
4. **Conditional Compilation:** Per-file log level thresholds
5. **Static Analysis:** Script to validate file IDs at build time
6. **Decoder Tool:** Python script to convert binary logs to readable format
7. **Log Viewer:** Real-time monitoring tool for UART output

### Questions to Ask Users

Before making significant suggestions:

1. "What's the target microcontroller family?" (affects available features)
2. "Is there an RTOS, or is this bare metal?"
3. "What's the total RAM budget?" (to size buffers appropriately)
4. "How are logs retrieved in production?" (UART only, or do devices upload to server?)
5. "Are there real-time constraints?" (logging in ISRs?)

### Working with Chinese Documentation

The documentation uses technical Chinese. Key terms:

| Chinese | English | Notes |
|---------|---------|-------|
| 日志 | Log | Generic logging term |
| 编码模式 | Encode mode | Binary encoding mode |
| 字符串模式 | String mode | Traditional printf mode |
| 临界区 | Critical section | Thread safety |
| 外存 | External storage | Flash/EEPROM |
| 热重启 | Warm restart | Reset without power loss |
| 文件编号 | File ID | Unique identifier per source file |
| 行数 | Line number | Source line number |
| 开关 | Switch | Enable/disable flag |
| 静态开关 | Static switch | Compile-time flag |
| 动态开关 | Dynamic switch | Runtime flag |

### Git Workflow Notes

**Current Branch:** `claude/claude-md-mi4db0xsm2nfl59u-015yMkSTg2L8cSdNHKKLN18M`

**Important:**
- All development should occur on this branch
- Branch name must start with `claude/` and end with session ID
- Use `git push -u origin <branch-name>` when pushing
- Create descriptive commit messages (this is a design repository, so commits should explain design decisions)

Example good commit message:
```
Add detailed encode_mode data structure specification

- Defined WW_LOG_RAM_T structure with circular buffer pointers
- Documented buffer full/empty conditions
- Added wraparound handling requirements
- Specified persistent storage across warm resets
```

---

## Quick Reference

### Essential Design Documents

1. **Requirements:** `doc/log模块更新设计要求.md` (~112 lines)
   - Read this first to understand project goals and constraints

2. **Design Spec:** `doc/日志模块重构设计方案.md` (~2812 lines)
   - Comprehensive architecture, data structures, and examples
   - Sections include: Background, Architecture, File IDs, Encoding, APIs, RAM buffer, External storage, Examples, Testing

### Key Design Elements

| Element | Description | Location in Design Doc |
|---------|-------------|------------------------|
| Architecture | Mermaid diagram showing module relationships | Section 2.1 |
| File ID Enum | `log_file_id.h` design | Section 3 |
| Encoding Format | 32-bit bitfield layout | Section 6.1 |
| RAM Buffer | `WW_LOG_RAM_T` structure | Section on RAM management |
| Config Options | All `CONFIG_WW_LOG_*` macros | Section 2.3 |
| API Macros | `TEST_LOG_XXX_MSG()` definitions | Section 5 |

### Document Navigation Tips

The design document (`日志模块重构设计方案.md`) is very long. Key sections:

- **Lines 1-55:** Background and objectives
- **Lines 56-138:** Overall architecture
- **Lines 139-240:** File ID management system
- **Lines 241-301:** Log level definitions
- **Lines 302-415:** Unified API design
- **Lines 416-600:** encode_mode detailed design
- **Lines 600+:** Implementation details, examples, testing strategies

---

## Version History

| Date | Version | Changes |
|------|---------|---------|
| 2025-11-18 | 1.0 | Initial CLAUDE.md creation - comprehensive guide covering current design documentation state |

---

## Contact & Support

For questions about this repository or the logging system design:
- Review the design documents in `doc/` directory
- Check commit history for design evolution
- Refer to this CLAUDE.md for AI assistant guidance

---

**End of CLAUDE.md**
