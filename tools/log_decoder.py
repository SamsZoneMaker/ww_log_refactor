#!/usr/bin/env python3
"""
Log Decoder for Encode Mode
Decodes binary log entries to human-readable format

Encoding format (32-bit):
  Bits 31-20: LOG_ID (12 bits, 0-4095)
  Bits 19-8:  LINE (12 bits, 0-4095)
  Bits 7-4:   LEVEL (4 bits, 0-15)
  Bits 3-0:   MSG_ID (4 bits, 0-15)

Usage:
  python3 log_decoder.py <encoded_log_file>
  python3 log_decoder.py -  (read from stdin)
"""

import sys
import re

# Log level names
LEVEL_NAMES = {
    0: "ERR",
    1: "WRN",
    2: "INF",
    3: "DBG",
}

# Module/File ID to name mapping
FILE_ID_MAP = {
    0: "main.c",
    # DEMO module (32-63)
    32: "demo (default)",
    33: "demo_init.c",
    34: "demo_process.c",
    # TEST module (64-95)
    64: "test (default)",
    65: "test_unit.c",
    66: "test_integration.c",
    67: "test_stress.c",
    # APP module (96-127)
    96: "app (default)",
    97: "app_main.c",
    98: "app_config.c",
    # DRIVERS module (128-159)
    128: "drivers (default)",
    129: "drv_uart.c",
    130: "drv_spi.c",
    131: "drv_i2c.c",
    # BROM module (160-191)
    160: "brom (default)",
    161: "brom_boot.c",
    162: "brom_loader.c",
}

MODULE_NAMES = {
    range(32, 64): "DEMO",
    range(64, 96): "TEST",
    range(96, 128): "APP",
    range(128, 160): "DRV",
    range(160, 192): "BROM",
}

def get_module_name(log_id):
    for id_range, name in MODULE_NAMES.items():
        if log_id in id_range:
            return name
    return "UNKNOWN"

def decode_log_entry(encoded_value):
    if isinstance(encoded_value, str):
        encoded_value = int(encoded_value, 16)
    
    log_id = (encoded_value >> 20) & 0xFFF
    line = (encoded_value >> 8) & 0xFFF
    level = (encoded_value >> 4) & 0xF
    msg_id = encoded_value & 0xF
    
    return {
        'raw': encoded_value,
        'log_id': log_id,
        'line': line,
        'level': level,
        'msg_id': msg_id,
        'level_name': LEVEL_NAMES.get(level, f"L{level}"),
        'file_name': FILE_ID_MAP.get(log_id, f"ID_{log_id}"),
        'module_name': get_module_name(log_id),
    }

def format_decoded_log(decoded):
    return (f"[{decoded['level_name']}][{decoded['module_name']}] "
            f"{decoded['file_name']}:{decoded['line']} "
            f"(MsgID:{decoded['msg_id']}) [Raw: 0x{decoded['raw']:08X}]")

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 log_decoder.py <file|->")
        sys.exit(1)
    
    if sys.argv[1] == '-':
        input_file = sys.stdin
        filename = "stdin"
    else:
        try:
            input_file = open(sys.argv[1], 'r')
            filename = sys.argv[1]
        except FileNotFoundError:
            print(f"Error: File '{sys.argv[1]}' not found")
            sys.exit(1)
    
    print(f"Decoding logs from {filename}...")
    print("=" * 80)
    
    count = 0
    try:
        for line in input_file:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            
            matches = re.findall(r'0x([0-9A-Fa-f]{1,8})', line)
            for hex_val in matches:
                try:
                    decoded = decode_log_entry(f"0x{hex_val}")
                    print(f"{count:4d}: {format_decoded_log(decoded)}")
                    count += 1
                except Exception as e:
                    print(f"ERROR: {e}")
    finally:
        if input_file != sys.stdin:
            input_file.close()
    
    print("=" * 80)
    print(f"Decoded {count} log entries")

if __name__ == "__main__":
    main()
