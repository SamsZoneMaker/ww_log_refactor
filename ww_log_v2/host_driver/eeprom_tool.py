#!/usr/bin/env python3
"""
eeprom_tool.py -- I2C EEPROM programming tool via JTAG
======================================================
Access chain: PC -> USB -> FTDI FT2232H -> JTAG -> RISC-V Debug Module (DMI)
              -> System Bus Access (SBA) -> ATCIIC100 I2C Master -> BR24G2MFJ EEPROM

Usage:
    python eeprom_tool.py info
    python eeprom_tool.py burn firmware.bin
    python eeprom_tool.py read 0x0 256

Commands:
    info          Probe EEPROM, report responding addrs and capacity
    scan          List ACK/NACK across 0x50-0x57
    diag          I2C bus diagnostic
    read          Read EEPROM contents (hexdump or save to file)
    write         Write hex bytes to a specific address
    burn          Program EEPROM from a binary file (+ optional verify)
    verify        Compare file with EEPROM contents
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


# ---------------------------------------------------------------------------
# Subcommands
# ---------------------------------------------------------------------------


def cmd_scan(args, dora):
    dora.f_eeprom_scan(v_boardId=args.board, v_devAddr=args.addr)


def cmd_diag(args, dora):
    if not dora.f_eeprom_diag(v_boardId=args.board, v_devAddr=args.addr):
        raise RuntimeError("eeprom diag failed")


def cmd_read(args, dora):
    offset = parse_int(args.offset)
    length = parse_int(args.length)
    if length <= 0:
        raise ValueError("length must be > 0")

    dora.f_eeprom_read(offset, length,
                       v_output=args.output,
                       v_boardId=args.board,
                       v_devAddr=args.addr)


def cmd_write(args, dora):
    offset = parse_int(args.offset)

    raw = args.data
    if len(raw) == 1 and (raw[0].startswith('0x') or raw[0].startswith('0X')):
        val = int(raw[0], 16)
        byte_len = max(1, (val.bit_length() + 7) // 8)
        data = val.to_bytes(byte_len, byteorder='big')
    else:
        try:
            data = bytes(int(b, 16) for b in raw)
        except ValueError:
            raise ValueError(f"Invalid hex byte in: {' '.join(raw)}")

    if not data:
        raise ValueError("No data to write")

    dora.f_eeprom_write(offset, data,
                        v_verify=args.verify,
                        v_boardId=args.board,
                        v_devAddr=args.addr)


def cmd_burn(args, dora):
    offset = parse_int(args.offset)
    dora.f_eeprom_burn(
        args.input,
        v_offset=offset,
        v_verify=args.verify,
        v_boardId=args.board,
        v_devAddr=args.addr)


def cmd_verify(args, dora):
    offset = parse_int(args.offset)
    dora.f_eeprom_verify(args.input, offset,
                         v_boardId=args.board,
                         v_devAddr=args.addr)


def cmd_clean(args, dora):
    offset = parse_int(args.offset)
    length = parse_int(args.length)
    if length <= 0:
        raise ValueError("length must be > 0")
    dora.f_eeprom_clean(offset, length,
                        v_fill=parse_int(args.fill),
                        v_verify=args.verify,
                        v_boardId=args.board,
                        v_devAddr=args.addr)


# ---------------------------------------------------------------------------
# Argument parser
# ---------------------------------------------------------------------------

def parse_dev_addr(s):
    val = int(s, 0)
    if not (0x50 <= val <= 0x57):
        raise argparse.ArgumentTypeError(
            f"--addr must be in 0x50-0x57, got 0x{val:02X}")
    return val


def build_parser():
    parser = argparse.ArgumentParser(
        prog='eeprom_tool.pyc',
        description='I2C EEPROM programming tool via JTAG (BR24G2MFJ-5A)',
        epilog='Examples:\n'
               '  %(prog)s scan\n'
               '  %(prog)s --addr 0x57 read 0x0 256\n'
               '  %(prog)s burn firmware.bin\n'
               '  %(prog)s burn firmware.bin --verify\n'
               '  %(prog)s burn firmware.bin --offset 0x1000\n'
               '  %(prog)s read 0x0 256\n'
               '  %(prog)s write 0x100 DE AD BE EF\n'
               '  %(prog)s write 0x100 0xDEADBEEF\n'
               '  %(prog)s verify firmware.bin\n'
               '  %(prog)s verify firmware.bin 0x1000\n'
               '  %(prog)s clean 0x1AA00 0x5400            # wipe the LOG partition\n'
               '\n'
               'When --addr is omitted, the tool scans 0x50-0x57 and uses\n'
               'the lowest ACKing address (with P1/P0 computed per access).\n',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument('-b', '--board', type=int, default=0,
                        help='Board ID (default: 0)')
    parser.add_argument('--addr', type=parse_dev_addr, default=None,
                        metavar='HEX',
                        help='EEPROM I2C 7-bit address (0x50-0x57). '
                             'If omitted, auto-scan the bus.')

    sub = parser.add_subparsers(dest='command', required=True,
                                help='Available commands')

    # --- scan ---
    sub.add_parser('scan', help='Probe 0x50-0x57 and list responding addresses')

    # --- diag ---
    sub.add_parser('diag', help='I2C bus diagnostic (probe + register dump)')

    # --- read ---
    p_read = sub.add_parser('read', help='Read EEPROM contents')
    p_read.add_argument('offset', help='Start offset (e.g. 0x0)')
    p_read.add_argument('length', help='Number of bytes to read')
    p_read.add_argument('-o', '--output', default=None,
                        help='Output binary file (omit for hexdump)')

    # --- write ---
    p_write = sub.add_parser(
        'write',
        help='Write hex bytes directly to an EEPROM offset',
        description='Write one or more hex bytes to a specific EEPROM offset.\n'
                    'Accepts space-separated bytes (e.g. DE AD BE EF) or\n'
                    'a single hex word with 0x prefix (e.g. 0xDEADBEEF).',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p_write.add_argument('offset', help='EEPROM offset (e.g. 0x100)')
    p_write.add_argument('data', nargs='+',
                         help='Hex data bytes (e.g. DE AD BE EF or 0xDEADBEEF)')
    p_write.add_argument('--verify', action='store_true',
                         help='Read back and verify after writing')

    # --- burn ---
    p_burn = sub.add_parser(
        'burn',
        help='Program EEPROM from a binary file (+ optional verify)',
    )
    p_burn.add_argument('input', help='Input binary file')
    p_burn.add_argument('--offset', default='0x0',
                        help='EEPROM start offset (default: 0x0)')
    p_burn.add_argument('--verify', action='store_true', default=False,
                        help='Read back and verify after programming '
                             '(default: off, since verify is slow)')

    # --- verify ---
    p_verify = sub.add_parser('verify',
                              help='Compare binary file with EEPROM contents')
    p_verify.add_argument('input', help='Reference binary file')
    p_verify.add_argument('offset', nargs='?', default='0x0',
                          help='EEPROM start offset (default: 0x0)')

    # --- clean ---
    p_clean = sub.add_parser(
        'clean',
        help='clean a region by overwriting it with a fill byte (default 0xFF)',
        description='EEPROM has no native clean (it is byte-writable), so this\n'
                    'overwrites [offset, offset+length) with --fill (0xFF by\n'
                    'default, matching flash cleand state so the log decoder sees\n'
                    'the region as empty).',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p_clean.add_argument('offset', help='Start offset (e.g. 0x1AA00)')
    p_clean.add_argument('length', help='Number of bytes to clean (e.g. 0x5400)')
    p_clean.add_argument('--fill', default='0xFF',
                         help='Fill byte (default: 0xFF)')
    p_clean.add_argument('--verify', action='store_true',
                         help='Read back and verify after cleaning')

    return parser


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = build_parser()
    args = parser.parse_args()

    dispatch = {
        'scan':    cmd_scan,
        'diag':    cmd_diag,
        'read':    cmd_read,
        'write':   cmd_write,
        'burn':    cmd_burn,
        'verify':  cmd_verify,
        'clean':   cmd_clean,
    }

    handler = dispatch.get(args.command)
    if handler is None:
        parser.print_help()
        return 1

    try:
        dora = DORA()
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
