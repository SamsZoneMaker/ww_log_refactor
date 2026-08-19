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
             read by memory address over JTAG (default addr 0xA021BD00).
    flash    LOG partition in SPI NOR flash ('XLOG' append log).
    eeprom   LOG partition in I2C EEPROM      ('XLOG' append log).

For flash/eeprom the partition offset and size are read from the device's
partition table -- the same source log_ext_mem_init() uses -- so a repartition
or a resize needs no change here. --offset/--length override that.

The decoder auto-scans the read blob for the WLOG/XLOG magic, so an approximate
offset (or a whole-region read) still works. An unwritten region reads back all
0x00 (RAM) / all 0xFF (flash/eeprom); those are reported and not decoded, but
--hex still dumps their raw bytes.

Default behaviour: read the whole LOG region -> decode -> print. Extra switches:
    --hex    print the raw encoded words (4 U32/line), skip decode entirely.
    --save   also write the output to <YYYYmmdd_HHMMSS>.log in the cwd.

Usage:
    python log_tool.py ram    --map ww_log_map.json
    python log_tool.py ram    --map ww_log_map.json --addr 0xA021BD00 --save
    python log_tool.py flash  --map ww_log_map.json --hex
    python log_tool.py eeprom --map ww_log_map.json --dev-addr 0x57
    python log_tool.py ram    --map ww_log_map.json -o dump.bin --raw
"""

import argparse
import os
import sys
import time
import traceback

RAM_DEFAULT_ADDR = 0xA021BD00       # DLM maintain region (__dlm_log_start)
RAM_LEN_DEFAULT    = '0x1000'       # 4KB
FLASH_LEN_DEFAULT  = '0x1000'       # 4KB
EEPROM_LEN_DEFAULT = '0x5400'       # 21KB

# Fallback LOG partition bases, used only when the partition table cannot be
# read; normally the geometry comes from the table.
FLASH_LOG_OFFSET   = '0x1F000'
EEPROM_LOG_OFFSET  = '0x1AA00'


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

def _resolve_output(args):
    """Explicit -o wins; otherwise --save auto-names <timestamp>.log; else None."""
    if args.output:
        return args.output
    if args.save:
        return time.strftime('%Y%m%d_%H%M%S') + '.log'
    return None


def cmd_ram(args, dora):
    dora.f_log_decode_ram(
        args.map,
        parse_int(args.addr),
        v_length=parse_int(args.length),
        v_raw=args.raw,
        v_output=_resolve_output(args),
        v_hex=args.hexdump,
        v_boardId=args.board,
        v_mapDir=args.map_dir)


def cmd_flash(args, dora):
    dora.f_log_decode_flash(
        args.map,
        v_offset=parse_int(args.offset) if args.offset else None,
        v_length=parse_int(args.length) if args.length else None,
        v_raw=args.raw,
        v_output=_resolve_output(args),
        v_hex=args.hexdump,
        v_boardId=args.board,
        v_mapDir=args.map_dir)


def cmd_eeprom(args, dora):
    dora.f_log_decode_eeprom(
        args.map,
        v_offset=parse_int(args.offset) if args.offset else None,
        v_length=parse_int(args.length) if args.length else None,
        v_raw=args.raw,
        v_output=_resolve_output(args),
        v_hex=args.hexdump,
        v_boardId=args.board,
        v_mapDir=args.map_dir,
        v_devAddr=args.dev_addr)


# ---------------------------------------------------------------------------
# Argument parser
# ---------------------------------------------------------------------------

def _add_common(p, default_len):
    """default_len=None means "ask the device partition table"."""
    p.add_argument('--map', required=True,
                   help='Path to ww_log_map.json (the current build)')
    p.add_argument('--map-dir', default=None, metavar='DIR',
                   help='Directory of archived maps (make map-archive). A log '
                        'archive survives firmware updates, so one read can '
                        'span several builds; with the archive present each '
                        'stretch is decoded with the map that produced it, and '
                        'anything decoded with a different map is marked')
    p.add_argument('--length', default=default_len,
                   help='Bytes to read (default: %s)'
                        % (default_len or 'the LOG partition size from the '
                                          'partition table'))
    p.add_argument('--raw', action='store_true',
                   help='Append the raw frame after each decoded line')
    p.add_argument('--hex', dest='hexdump', action='store_true',
                   help='Dump raw encoded words (4 U32/line), skip decode '
                        '(works on empty 0x00/0xFF regions too)')
    p.add_argument('--save', action='store_true',
                   help='Also save the output to <YYYYmmdd_HHMMSS>.log')
    p.add_argument('-o', '--output', default=None,
                   help='Save to a file; extension decides what is written: '
                        '.dump/.bin -> raw log bytes (no decode), '
                        '.txt/.log/other -> printed text (overrides --save)')


def build_parser():
    parser = argparse.ArgumentParser(
        prog='log_tool.pyc',
        description='ww_log v1 on-device log decoder via JTAG',
        epilog='Examples:\n'
               '  %(prog)s ram    --map ww_log_map.json\n'
               '  %(prog)s ram    --map ww_log_map.json --save\n'
               '  %(prog)s flash  --map ww_log_map.json --hex\n'
               '  %(prog)s eeprom --map ww_log_map.json --dev-addr 0x57\n'
               '  %(prog)s ram    --map ww_log_map.json -o dump.bin --raw\n',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument('-b', '--board', type=int, default=0,
                        help='Board ID (default: 0)')

    sub = parser.add_subparsers(dest='command', required=True,
                                help='Log source')

    # --- ram ---
    p_ram = sub.add_parser('ram',
                           help='Decode the DLM maintain region (JTAG mem read)')
    p_ram.add_argument('--addr', default='0x%08X' % RAM_DEFAULT_ADDR,
                       help='RAM region memory address (value of '
                            '__dlm_log_start; default 0x%08X)' % RAM_DEFAULT_ADDR)
    _add_common(p_ram, RAM_LEN_DEFAULT)

    # --- flash ---
    p_flash = sub.add_parser('flash',
                             help='Decode the LOG partition from SPI NOR flash')
    p_flash.add_argument('--offset', default=None,
                         help='Flash offset of the LOG partition. Default: read '
                              'it from the device partition table (what the '
                              'firmware itself uses); pass this only to override')
    _add_common(p_flash, None)

    # --- eeprom ---
    p_eeprom = sub.add_parser('eeprom',
                              help='Decode the LOG partition from I2C EEPROM')
    p_eeprom.add_argument('--offset', default=None,
                          help='EEPROM offset of the LOG partition. Default: read '
                               'it from the device partition table (what the '
                               'firmware itself uses); pass this only to override')
    p_eeprom.add_argument('--dev-addr', type=parse_dev_addr, default=None,
                          metavar='HEX',
                          help='EEPROM I2C 7-bit address (0x50-0x57); '
                               'auto-scan if omitted')
    _add_common(p_eeprom, None)

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
