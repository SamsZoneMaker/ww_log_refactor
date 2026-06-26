#!/usr/bin/env python3
"""
log_tool.py -- ww_log v1 on-device log decoder via JTAG
=======================================================
Reads ENCODED ww_log data straight off the device and decodes it to readable
lines (the same output STR mode would have produced). No firmware cooperation
is required -- it just reads memory / flash / eeprom over the existing JTAG
channel, then decodes with ww_log_map.json.

Sources (where the encoded logs physically live):
    ram      4KB power-loss-retained DLM "maintain" region ('WLOG' header),
             read by memory address over JTAG SBA.
    flash    LOG partition in SPI NOR flash ('LOGH' blocks).
    eeprom   LOG partition in I2C EEPROM   ('LOGH' blocks).

The decoder auto-scans the read blob for the WLOG/LOGH magic, so an approximate
offset (or a whole-region read) still works.

Usage:
    python log_tool.py ram    --map ww_log_map.json --addr 0x00100000
    python log_tool.py flash  --map ww_log_map.json --offset 0x3F000
    python log_tool.py eeprom --map ww_log_map.json --offset 0x1000 --dev-addr 0x57
    python log_tool.py ram    --map ww_log_map.json --addr 0x00100000 -o dump.bin --raw
"""

import argparse
import os
import sys
import traceback


def _bootstrap_dora_root():
    dora_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    if not os.path.exists(os.path.join(dora_root, "api", "config.py")) and \
       not os.path.exists(os.path.join(dora_root, "api", "config.pyc")):
        raise RuntimeError(f"Cannot locate dora root at {dora_root}")
    if dora_root not in sys.path:
        sys.path.insert(0, dora_root)

_bootstrap_dora_root()

from api.dora import DORA


# ---------------------------------------------------------------------------
# Utility
# ---------------------------------------------------------------------------

def parse_int(s):
    return int(s, 0)


def parse_dev_addr(s):
    val = int(s, 0)
    if not (0x50 <= val <= 0x57):
        raise argparse.ArgumentTypeError(
            f"--dev-addr must be in 0x50-0x57, got 0x{val:02X}")
    return val


# ---------------------------------------------------------------------------
# Subcommands
# ---------------------------------------------------------------------------

def cmd_ram(args, dora):
    dora.f_log_decode_ram(
        args.map,
        parse_int(args.addr),
        v_length=parse_int(args.length),
        v_raw=args.raw,
        v_output=args.output,
        v_boardId=args.board)


def cmd_flash(args, dora):
    dora.f_log_decode_flash(
        args.map,
        v_offset=parse_int(args.offset),
        v_length=parse_int(args.length),
        v_raw=args.raw,
        v_output=args.output,
        v_boardId=args.board)


def cmd_eeprom(args, dora):
    dora.f_log_decode_eeprom(
        args.map,
        v_offset=parse_int(args.offset),
        v_length=parse_int(args.length),
        v_raw=args.raw,
        v_output=args.output,
        v_boardId=args.board,
        v_devAddr=args.dev_addr)


# ---------------------------------------------------------------------------
# Argument parser
# ---------------------------------------------------------------------------

def _add_common(p):
    p.add_argument('--map', required=True, help='Path to ww_log_map.json')
    p.add_argument('--length', default='0x1000',
                   help='Bytes to read (default: 0x1000 = one 4KB region)')
    p.add_argument('--raw', action='store_true',
                   help='Append the raw frame after each decoded line')
    p.add_argument('-o', '--output', default=None,
                   help='Save to a file; extension decides what is written: '
                        '.dump/.bin -> raw log bytes (no decode), '
                        '.txt/other -> decoded lines (default .txt)')


def build_parser():
    parser = argparse.ArgumentParser(
        prog='log_tool.pyc',
        description='ww_log v1 on-device log decoder via JTAG',
        epilog='Examples:\n'
               '  %(prog)s ram    --map ww_log_map.json --addr 0x00100000\n'
               '  %(prog)s flash  --map ww_log_map.json --offset 0x3F000\n'
               '  %(prog)s eeprom --map ww_log_map.json --offset 0x1000 --dev-addr 0x57\n'
               '  %(prog)s ram    --map ww_log_map.json --addr 0x00100000 -o dump.bin --raw\n',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument('-b', '--board', type=int, default=0,
                        help='Board ID (default: 0)')

    sub = parser.add_subparsers(dest='command', required=True,
                                help='Log source')

    # --- ram ---
    p_ram = sub.add_parser('ram',
                           help='Decode the DLM maintain region (JTAG mem read)')
    p_ram.add_argument('--addr', required=True,
                       help='RAM region memory address (value of __dlm_log_start)')
    _add_common(p_ram)

    # --- flash ---
    p_flash = sub.add_parser('flash',
                             help='Decode the LOG partition from SPI NOR flash')
    p_flash.add_argument('--offset', default='0x0',
                         help='Flash offset of the LOG partition (default: 0x0)')
    _add_common(p_flash)

    # --- eeprom ---
    p_eeprom = sub.add_parser('eeprom',
                              help='Decode the LOG partition from I2C EEPROM')
    p_eeprom.add_argument('--offset', default='0x0',
                          help='EEPROM offset of the LOG partition (default: 0x0)')
    p_eeprom.add_argument('--dev-addr', type=parse_dev_addr, default=None,
                          metavar='HEX',
                          help='EEPROM I2C 7-bit address (0x50-0x57); '
                               'auto-scan if omitted')
    _add_common(p_eeprom)

    return parser


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = build_parser()
    args = parser.parse_args()

    dispatch = {
        'ram':    cmd_ram,
        'flash':  cmd_flash,
        'eeprom': cmd_eeprom,
    }

    handler = dispatch.get(args.command)
    if handler is None:
        parser.print_help()
        return 1

    try:
        dora = DORA()
        print("\n" + "=" * 25 + " Log Tool start " + "=" * 25)
        handler(args, dora)
        return 0
    except FileNotFoundError as e:
        print(f"ERROR: {e}")
    except KeyboardInterrupt:
        print("\nAborted by user.")
    except Exception as e:
        print(f"ERROR: {e}")
        traceback.print_exc()

    return 1


if __name__ == '__main__':
    sys.exit(main())
