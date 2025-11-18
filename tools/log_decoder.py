#!/usr/bin/env python3
"""
Log Decoder Tool for Encode Mode
Date: 2025-11-18

This tool decodes binary-encoded log entries back to human-readable format.

Usage:
    python3 log_decoder.py <log_file>
    python3 log_decoder.py --stdin  # Read from stdin
    cat encode_log.txt | python3 log_decoder.py --stdin

Format:
    Input: 0xHEADER [0xPARAM1] [0xPARAM2] ...
    Output: [LEVEL][MODULE] file_id:line - Message with parameters
"""

import sys
import re

# File ID to name mapping (from log_file_id.h)
FILE_ID_MAP = {
    # DEMO Module (1-50)
    1: "demo_init.c",
    2: "demo_process.c",

    # TEST Module (51-100)
    51: "test_unit.c",
    52: "test_integration.c",
    53: "test_stress.c",

    # APP Module (101-150)
    101: "app_main.c",
    102: "app_config.c",

    # DRIVERS Module (151-200)
    151: "drv_uart.c",
    152: "drv_spi.c",
    153: "drv_i2c.c",

    # BROM Module (201-250)
    201: "brom_boot.c",
    202: "brom_loader.c",
}

# Module ID to name mapping
MODULE_MAP = {
    0: "DEMO",
    1: "TEST",
    2: "APP",
    3: "DRV",
    4: "BROM",
}

# Log level mapping (2-bit encoding)
LEVEL_MAP = {
    0: "ERR",
    1: "WRN",
    2: "INF",
    3: "DBG",
}


def decode_header(header):
    """
    Decode 32-bit log header.

    Format (32 bits):
     31                    20 19                8 7        2 1      0
    ┌──────────────────────┬────────────────────┬──────────┬────────┐
    │   File ID (12 bits)  │  Line No (12 bits) │next_len  │ Level  │
    │      0-4095          │     0-4095         │ (6 bits) │(2 bits)│
    └──────────────────────┴────────────────────┴──────────┴────────┘

    Args:
        header: 32-bit integer (or string like "0x12345678")

    Returns:
        dict with keys: file_id, line, level, param_count
    """
    if isinstance(header, str):
        header = int(header, 16)

    file_id = (header >> 20) & 0xFFF      # Bits 31-20
    line_no = (header >> 8) & 0xFFF       # Bits 19-8
    param_count = (header >> 2) & 0x3F    # Bits 7-2
    level = header & 0x03                 # Bits 1-0

    return {
        'file_id': file_id,
        'line': line_no,
        'level': level,
        'param_count': param_count,
    }


def get_file_name(file_id):
    """Get filename from file ID."""
    return FILE_ID_MAP.get(file_id, f"unknown_{file_id}")


def get_level_str(level):
    """Get log level string."""
    return LEVEL_MAP.get(level, "???")


def get_module_from_file_id(file_id):
    """Determine module from file ID range."""
    if 1 <= file_id <= 50:
        return "DEMO"
    elif 51 <= file_id <= 100:
        return "TEST"
    elif 101 <= file_id <= 150:
        return "APP"
    elif 151 <= file_id <= 200:
        return "DRV"
    elif 201 <= file_id <= 250:
        return "BROM"
    else:
        return "UNKNOWN"


def format_log_entry(info, params):
    """
    Format decoded log entry.

    Args:
        info: dict from decode_header()
        params: list of parameter values (U32)

    Returns:
        Formatted string
    """
    level_str = get_level_str(info['level'])
    module_str = get_module_from_file_id(info['file_id'])
    file_name = get_file_name(info['file_id'])
    line = info['line']

    # Base format: [LEVEL][MODULE] file_id:line (filename) - params
    output = f"[{level_str}][{module_str}] {info['file_id']}:{line} ({file_name})"

    # Append parameters if any
    if params:
        params_str = ", ".join([f"0x{p:08X} ({p})" for p in params])
        output += f" - Params: [{params_str}]"

    return output


def parse_log_line(line):
    """
    Parse one line of encoded log output.

    Expected format: 0xHEADER [0xPARAM1] [0xPARAM2] ...

    Args:
        line: String containing hex values

    Returns:
        Formatted decoded string, or None if parsing fails
    """
    # Remove whitespace and split by spaces
    line = line.strip()
    if not line:
        return None

    # Find all hex values (0x...)
    hex_pattern = r'0x[0-9A-Fa-f]+'
    matches = re.findall(hex_pattern, line)

    if not matches:
        return None

    # First value is header
    header = int(matches[0], 16)
    info = decode_header(header)

    # Remaining values are parameters
    params = [int(m, 16) for m in matches[1:]]

    # Verify parameter count matches
    if len(params) != info['param_count']:
        # Warning: parameter count mismatch
        return f"{format_log_entry(info, params)} [WARNING: Expected {info['param_count']} params, got {len(params)}]"

    return format_log_entry(info, params)


def decode_file(file_path):
    """Decode log file."""
    try:
        with open(file_path, 'r') as f:
            for line_num, line in enumerate(f, 1):
                decoded = parse_log_line(line)
                if decoded:
                    print(decoded)
    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found.", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


def decode_stdin():
    """Decode log from stdin."""
    for line in sys.stdin:
        decoded = parse_log_line(line)
        if decoded:
            print(decoded)


def main():
    """Main function."""
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python3 log_decoder.py <log_file>")
        print("  python3 log_decoder.py --stdin")
        print("  cat encode_log.txt | python3 log_decoder.py --stdin")
        sys.exit(1)

    if sys.argv[1] == '--stdin':
        decode_stdin()
    else:
        decode_file(sys.argv[1])


if __name__ == '__main__':
    main()
