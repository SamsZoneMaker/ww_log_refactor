#!/usr/bin/env python3
"""
flash_tool.py -- SPI Flash programming tool via JTAG
=====================================================
Access chain: PC -> USB -> FTDI FT2232H -> JTAG -> RISC-V Debug Module (DMI)
              -> System Bus Access (SBA) -> ATCSPI200 SPI controller -> SPI NOR Flash

Usage:
    python flash_tool.py info
    python flash_tool.py burn firmware.bin
    python flash_tool.py read 0x0 256

Commands:
    info          Read and display Flash JEDEC ID
    status        Read and decode Flash status register
    diag          SPI bus diagnostic
    read          Read flash contents (hexdump or save to file)
    write         Write hex bytes to a specific flash offset
    erase         Erase flash region (sector erase)
    burn          Sector erase + program (whole flash from 0x0, or --offset)
    verify        Compare file with flash contents
    blank_check   Check if a flash region is all 0xFF
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

def cmd_info(args, dora):
    dora.f_flash_info(v_boardId=args.board)


def cmd_status(args, dora):
    dora.f_flash_status(v_boardId=args.board)


def cmd_diag(args, dora):
    dora.f_flash_diag(v_boardId=args.board)


def cmd_read(args, dora):
    offset = parse_int(args.offset)
    length = parse_int(args.length)
    if length <= 0:
        raise ValueError("length must be > 0")

    dora.f_flash_read(offset, length,
                      v_output=args.output,
                      v_boardId=args.board)


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

    dora.f_flash_write(offset, data,
                       v_verify=args.verify,
                       v_boardId=args.board)


def cmd_erase(args, dora):
    offset = parse_int(args.offset)
    length = parse_int(args.length)
    if length <= 0:
        raise ValueError("length must be > 0")

    dora.f_flash_erase(offset, length, v_boardId=args.board)


def cmd_burn(args, dora):
    offset = parse_int(args.offset)
    dora.f_flash_burn(
        args.input,
        v_verify=args.verify,
        v_offset=offset,
        v_boardId=args.board)


def cmd_verify(args, dora):
    offset = parse_int(args.offset)
    dora.f_flash_verify(
        args.input, offset,
        v_verify=args.verify,
        v_boardId=args.board)


def cmd_blank_check(args, dora):
    offset = parse_int(args.offset)
    length = parse_int(args.length)
    if length <= 0:
        raise ValueError("length must be > 0")

    dora.f_flash_blank_check(offset, length, v_boardId=args.board)


# ---------------------------------------------------------------------------
# Argument parser
# ---------------------------------------------------------------------------

def build_parser():
    parser = argparse.ArgumentParser(
        prog='flash_tool.pyc',
        description='SPI Flash programming tool via JTAG',
        epilog='Examples:\n'
               '  %(prog)s info\n'
               '  %(prog)s status\n'
               '  %(prog)s burn firmware.bin --verify\n'
               '  %(prog)s burn fw.bin --offset 0x10000\n'
               '  %(prog)s read 0x0 256\n'
               '  %(prog)s write 0x100 DE AD BE EF\n'
               '  %(prog)s write 0x100 0xDEADBEEF\n'
               '  %(prog)s blank_check 0x0 0x1000\n',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument('-b', '--board', type=int, default=0,
                        help='Board ID (default: 0)')

    sub = parser.add_subparsers(dest='command', required=True,
                                help='Available commands')

    # --- info ---
    sub.add_parser('info', help='Read and display Flash JEDEC ID')

    # --- status ---
    sub.add_parser('status', help='Read and decode Flash status register')

    # --- diag ---
    sub.add_parser('diag', help='SPI bus diagnostic (RDID, WREN/WEL, read test)')

    # --- read ---
    p_read = sub.add_parser('read', help='Read flash contents')
    p_read.add_argument('offset', help='Start offset (e.g. 0x0)')
    p_read.add_argument('length', help='Number of bytes to read')
    p_read.add_argument('-o', '--output', default=None,
                        help='Output binary file (omit for hexdump)')

    # --- write ---
    p_write = sub.add_parser(
        'write',
        help='Write hex bytes directly to a flash offset',
        description='Write one or more hex bytes to a specific flash offset.\n'
                    'Accepts space-separated bytes (e.g. DE AD BE EF) or\n'
                    'a single hex word with 0x prefix (e.g. 0xDEADBEEF).',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p_write.add_argument('offset', help='Flash offset (e.g. 0x100)')
    p_write.add_argument('data', nargs='+',
                         help='Hex data bytes (e.g. DE AD BE EF or 0xDEADBEEF)')
    p_write.add_argument('--verify', action='store_true',
                         help='Read back and verify after writing')

    # --- erase ---
    p_erase = sub.add_parser('erase',
                             help='Erase flash region (sector erase)')
    p_erase.add_argument('offset', help='Start offset')
    p_erase.add_argument('length', help='Number of bytes to erase')

    # --- burn ---
    p_burn = sub.add_parser(
        'burn',
        help='Sector erase + program (whole flash from 0x0, or --offset)',
    )
    p_burn.add_argument('input', help='Input binary file')
    p_burn.add_argument('--offset', default='0x0',
                        help='Flash start offset (default: 0x0)')
    p_burn.add_argument('-v', '--verify',
                        nargs='?', const='fast', default=None,
                        choices=['fast', 'full', 'inline'],
                        metavar='{fast,full,inline}',
                        help='Verify strategy: fast=memory-mapped (default), '
                             'full=SPI read, inline=per-chunk with retry')

    # --- verify ---
    p_verify = sub.add_parser('verify',
                              help='Compare binary file with flash contents '
                                   '(memory-mapped by default)')
    p_verify.add_argument('input', help='Reference binary file')
    p_verify.add_argument('offset', nargs='?', default='0x0',
                          help='Flash start offset (default: 0x0)')
    p_verify.add_argument('-v', '--verify',
                          nargs='?', const='fast', default='fast',
                          choices=['fast', 'full'],
                          metavar='{fast,full}',
                          help='Verify mode: fast=memory-mapped (default), full=SPI read')

    # --- blank_check ---
    p_blank = sub.add_parser('blank_check',
                             help='Check if flash region is all 0xFF')
    p_blank.add_argument('offset', help='Start offset')
    p_blank.add_argument('length', help='Number of bytes to check')

    return parser


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = build_parser()
    args = parser.parse_args()

    dispatch = {
        'info':         cmd_info,
        'status':       cmd_status,
        'diag':         cmd_diag,
        'read':         cmd_read,
        'write':        cmd_write,
        'erase':        cmd_erase,
        'burn':         cmd_burn,
        'verify':       cmd_verify,
        'blank_check':  cmd_blank_check,
    }

    handler = dispatch.get(args.command)
    if handler is None:
        parser.print_help()
        return 1

    try:
        dora = DORA()
        print("\n" + "=" * 25 + " Flash Tool start " + "=" * 25)
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
