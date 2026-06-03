#!/usr/bin/env python3
"""
log_decoder.py - ww_log v1 encode-mode decoder (map-driven).

Reads the unified map (ww_log_map.json) and restores encoded log data back into
human-readable lines, identical to what STR mode would have printed.

It accepts BOTH of the forms log data shows up in:

  1. HEX TEXT  - the frames the UART backend prints, or any .txt/.hex capture:
                 lines like "0x08000441 0x00000000". Live serial logs can be
                 piped in directly; non-frame lines are ignored.

  2. BINARY    - a raw dump pulled off the device (JTAG / read-back), e.g. the
                 external-storage LOG partition or the 4KB RAM maintain region:
                   - 'WLOG' RAM header  -> reads the ring read/write pointers
                   - 'LOGH' block headers -> walks each flushed block
                   - otherwise           -> treated as a raw entry stream
                 The magic is searched for, so dumping the whole chip (with
                 leading 0xFF padding) still works.

The input format is auto-detected; override with --format {auto,hex,bin}.

Encoding (CLAUDE.md S2), 32-bit header (little-endian on the wire in binary):

   31                20 19              6 5         0
  +--------------------+------------------+-----------+
  |   file_id (12)     |    line (14)     | param_cnt |
  +--------------------+------------------+-----------+
          |
          +-- file_id = [ module_id : 5 ][ offset : 7 ]

level is NOT encoded; it is restored here from the map by (file_id, line).
%s parameters cannot be restored (only the pointer was stored); they are shown
as a placeholder <%s@0xXXXXXXXX>.

Usage:
  python3 log_decoder.py --map ww_log_map.json capture.txt       # hex text
  python3 log_decoder.py --map ww_log_map.json dump.bin          # binary
  make_run | python3 log_decoder.py --map ww_log_map.json -      # stdin
  python3 log_decoder.py --map ww_log_map.json --format bin part.bin
"""

import argparse
import json
import re
import struct
import sys

LEVEL_NAMES = ("ERR", "WRN", "INF", "DBG")

# Magic numbers (see ww_log_config.h), little-endian byte patterns in a dump.
RAM_MAGIC   = 0x574C4F47  # 'WLOG'  - LOG_RAM_HEADER at start of the RAM region
BLOCK_MAGIC = 0x4C4F4748  # 'LOGH'  - LOG_BLOCK_HEADER before each storage block
RAM_MAGIC_LE   = struct.pack('<I', RAM_MAGIC)
BLOCK_MAGIC_LE = struct.pack('<I', BLOCK_MAGIC)
ERASED = 0xFFFFFFFF

# Geometry of the RAM maintain region (ww_log_config.h).
RAM_HEADER_SIZE = 64
RAM_TOTAL_SIZE  = 4096
RAM_DATA_SIZE   = RAM_TOTAL_SIZE - RAM_HEADER_SIZE
BLOCK_HEADER_SIZE = 32

# A printf conversion specifier: %[flags][width][.precision][length]conv
SPEC_RE = re.compile(
    r'%'
    r'([-+ #0]*)'                 # flags
    r'(\*|\d+)?'                  # width
    r'(?:\.(\*|\d+))?'           # .precision
    r'(hh|h|ll|l|L|z|j|t)?'      # length modifier
    r'([diouxXeEfFgGcspn%])'     # conversion
)


# ----------------------------------------------------------------------------
# header decode + format substitution
# ----------------------------------------------------------------------------

def decode_header(h):
    """Split a 32-bit header into (file_id, line, param_count)."""
    return (h >> 20) & 0xFFF, (h >> 6) & 0x3FFF, h & 0x3F


def to_signed32(v):
    return v - 0x100000000 if v & 0x80000000 else v


def render_fmt(fmt, params):
    """Substitute U32 params into a C printf fmt -> readable string."""
    out, pos, pi, n = [], 0, 0, len(params)

    for m in SPEC_RE.finditer(fmt):
        out.append(fmt[pos:m.start()])
        pos = m.end()
        flags, width, prec, _length, conv = m.groups()

        if conv == '%':
            out.append('%')
            continue

        if width == '*':
            width = str(to_signed32(params[pi])) if pi < n else ''
            pi += 1
        if prec == '*':
            prec = str(to_signed32(params[pi])) if pi < n else ''
            pi += 1

        if pi >= n:
            out.append(m.group(0))   # no value left: keep the literal spec
            continue
        raw = params[pi] & 0xFFFFFFFF
        pi += 1

        if conv in 'di':
            val, py = to_signed32(raw), 'd'
        elif conv == 'u':
            val, py = raw, 'd'
        elif conv in 'xX':
            val, py = raw, conv
        elif conv == 'o':
            val, py = raw, 'o'
        elif conv == 'c':
            ch = raw & 0xFF
            out.append(chr(ch) if 32 <= ch < 127 else '\\x%02x' % ch)
            continue
        elif conv == 'p':
            out.append('0x%08X' % raw)
            continue
        elif conv == 's':
            out.append('<%%s@0x%08X>' % raw)
            continue
        elif conv in 'eEfFgG':
            out.append('<%%%s@0x%08X>' % (conv, raw))
            continue
        elif conv == 'n':
            continue
        else:
            out.append(m.group(0))
            continue

        spec = '%' + flags + (width or '') + (('.' + prec) if prec else '') + py
        try:
            out.append(spec % val)
        except (ValueError, TypeError):
            out.append(str(val))

    out.append(fmt[pos:])
    return ''.join(out)


# ----------------------------------------------------------------------------
# map handling
# ----------------------------------------------------------------------------

def basename(path):
    return path.replace('\\', '/').rsplit('/', 1)[-1]


def build_index(the_map):
    entries = {(e['file_id'], e['line']): e for e in the_map.get('entries', [])}
    files = {int(k): v for k, v in the_map.get('files', {}).items()}
    modules = {int(k): v for k, v in the_map.get('modules', {}).items()}
    return entries, files, modules


def format_frame(header, params, idx):
    """Turn one (header, params) frame into a readable line via the map index."""
    entries, files, modules = idx
    file_id, line, pcnt = decode_header(header)
    params = params[:pcnt]

    finfo = files.get(file_id)
    fname = basename(finfo['path']) if finfo else 'file_id_%d' % file_id

    entry = entries.get((file_id, line))
    if entry is None:
        return ("[???] %s:%d - <no map entry> [raw 0x%08X, %d params: %s]"
                % (fname, line, header, pcnt,
                   ' '.join('0x%08X' % p for p in params)))

    return "[%s] %s:%d - %s" % (entry.get('level', '???'), fname, line,
                                render_fmt(entry.get('fmt', ''), params))


# ----------------------------------------------------------------------------
# parsers -> list of (header:int, params:list[int])
# ----------------------------------------------------------------------------

def parse_hex_text(data):
    """Parse hex-text frames; one entry per line, first hex value is the header."""
    text = data.decode('utf-8', errors='replace')
    frames = []
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line.lower().startswith('0x'):
            continue   # ignore banners / dumps / blank lines
        vals = [int(h, 16) for h in re.findall(r'0[xX]([0-9A-Fa-f]+)', line)]
        if vals:
            frames.append((vals[0], vals[1:]))
    return frames


def parse_entry_stream(buf, start, end):
    """Walk back-to-back binary entries: 4B LE header + pcnt*4B LE params."""
    frames, i = [], start
    while i + 4 <= end:
        header = struct.unpack_from('<I', buf, i)[0]
        if header == ERASED:
            break   # hit erased flash / padding
        pcnt = header & 0x3F
        need = 4 + pcnt * 4
        if i + need > end:
            break   # truncated tail
        params = list(struct.unpack_from('<%dI' % pcnt, buf, i + 4)) if pcnt else []
        frames.append((header, params))
        i += need
    return frames


def parse_ram_region(buf, off, notes):
    """Parse a 'WLOG' RAM maintain region, honouring the ring read/write idx."""
    (_magic, _version, write_index, read_index, total_written, flush_count,
     _last, overflow) = struct.unpack_from('<IIHHIIIB', buf, off)
    notes.append("# RAM region: write=%d read=%d total=%d flushes=%d overflow=%d"
                 % (write_index, read_index, total_written, flush_count, overflow))

    data_off = off + RAM_HEADER_SIZE
    data = buf[data_off:data_off + RAM_DATA_SIZE]
    if len(data) < RAM_DATA_SIZE:                 # dump shorter than a full region
        data = data + b'\xff' * (RAM_DATA_SIZE - len(data))

    if overflow:                                  # ring wrapped: read..end + 0..write
        ordered = data[read_index:] + data[:write_index]
    else:
        ordered = data[read_index:write_index]
    return parse_entry_stream(ordered, 0, len(ordered))


def parse_logh_blocks(buf, off, notes):
    """Walk one or more 'LOGH' storage blocks: 32B header + data_size bytes."""
    frames = []
    n = len(buf)
    while off + BLOCK_HEADER_SIZE <= n:
        magic, seq, _ts, data_size, entry_count, _ovf = \
            struct.unpack_from('<IIIHHB', buf, off)
        if magic != BLOCK_MAGIC:
            break
        notes.append("# block seq=%d size=%d entries=%d" % (seq, data_size, entry_count))
        data_off = off + BLOCK_HEADER_SIZE
        frames += parse_entry_stream(buf, data_off, min(data_off + data_size, n))
        off = data_off + data_size
    return frames


def parse_binary(data, notes):
    """Auto-frame a binary dump by locating the first known magic."""
    iw = data.find(RAM_MAGIC_LE)
    il = data.find(BLOCK_MAGIC_LE)
    cands = [(i, k) for i, k in ((iw, 'ram'), (il, 'logh')) if i != -1]
    if not cands:
        notes.append("# no WLOG/LOGH magic found -> parsing as raw entry stream")
        return parse_entry_stream(data, 0, len(data))
    start, kind = min(cands)
    if kind == 'ram':
        notes.append("# found RAM 'WLOG' header at offset 0x%X" % start)
        return parse_ram_region(data, start, notes)
    notes.append("# found storage 'LOGH' block at offset 0x%X" % start)
    return parse_logh_blocks(data, start, notes)


# ----------------------------------------------------------------------------
# format detection + main
# ----------------------------------------------------------------------------

def looks_like_hex_text(data):
    sample = data[:8192]
    if not sample:
        return True
    printable = sum(1 for b in sample
                    if b in (9, 10, 13) or 32 <= b < 127)
    if printable / len(sample) < 0.95:
        return False
    return (b'0x' in sample) or (b'0X' in sample)


def read_input_bytes(path):
    if path == '-':
        return sys.stdin.buffer.read()
    with open(path, 'rb') as f:
        return f.read()


def main():
    ap = argparse.ArgumentParser(description="ww_log v1 encode-mode decoder")
    ap.add_argument('--map', required=True, help='path to ww_log_map.json')
    ap.add_argument('input', nargs='?',
                    help="hex/text or binary dump, or '-' for stdin")
    ap.add_argument('--hex', dest='hex_str', metavar='"0x.. 0x.."',
                    help='decode hex frame(s) given directly on the command line')
    ap.add_argument('--format', choices=('auto', 'hex', 'bin'), default='auto',
                    help='input format (default: auto-detect)')
    ap.add_argument('--raw', action='store_true',
                    help='append the raw frame after each decoded line')
    args = ap.parse_args()

    if not args.hex_str and not args.input:
        ap.error("provide an input file/'-' , or --hex \"0x..\"")

    try:
        with open(args.map, 'r', encoding='utf-8') as f:
            the_map = json.load(f)
    except FileNotFoundError:
        sys.exit("Error: map file '%s' not found" % args.map)
    except json.JSONDecodeError as e:
        sys.exit("Error: invalid JSON in '%s': %s" % (args.map, e))

    idx = build_index(the_map)

    if args.hex_str:
        data = args.hex_str.encode('utf-8')
        fmt = 'hex'                      # inline hex is always text frames
    else:
        data = read_input_bytes(args.input)
        fmt = args.format
        if fmt == 'auto':
            fmt = 'hex' if looks_like_hex_text(data) else 'bin'

    notes = []
    if fmt == 'hex':
        frames = parse_hex_text(data)
    else:
        frames = parse_binary(data, notes)

    for note in notes:
        print(note)
    for header, params in frames:
        line = format_frame(header, params, idx)
        if args.raw:
            line += ("    | 0x%08X " % header) + \
                    ' '.join('0x%08X' % p for p in params[:header & 0x3F])
        print(line)

    print("# format=%s, decoded %d log entries" % (fmt, len(frames)),
          file=sys.stderr)


if __name__ == '__main__':
    main()
