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
                   - 'XLOG' ext header  -> walks the append stream after it
                   - otherwise          -> treated as a raw entry stream
                 The magic is searched for, so dumping the whole chip (with
                 leading 0xFF padding) still works.

The input format is auto-detected; override with --format {auto,hex,bin}.

Encoding, 32-bit header (little-endian on the wire in binary):

   31                20 19              6 5   4 3        0
  +--------------------+------------------+-----+---------+
  |   file_id (12)     |    line (14)     |lv(2)| pcnt(4) |
  +--------------------+------------------+-----+---------+
          |
          +-- file_id = [ module_id : 5 ][ offset : 7 ]

level IS encoded (2 bits) and read straight from the header; the map is only used
for the file name + format string. %s parameters cannot be restored (only the
pointer was stored); they are shown as a placeholder <%s@0xXXXXXXXX>.

An external-storage archive deliberately survives firmware updates, so one
stream can hold entries built from several different maps. Each boot stamps a
boot record naming the map that decodes what follows; pass the archive written
by `make map-archive` with --map-dir and every stretch is decoded with the map
that actually produced it. Lines decoded with any other map are prefixed '?'.

Usage:
  python3 log_decoder.py --map ww_log_map.json capture.txt       # hex text
  python3 log_decoder.py --map ww_log_map.json dump.bin          # binary
  make_run | python3 log_decoder.py --map ww_log_map.json -      # stdin
  python3 log_decoder.py --map ww_log_map.json --format bin part.bin
  python3 log_decoder.py --map ww_log_map.json --map-dir maps/ dump.bin
"""

import argparse
import hashlib
import json
import os
import re
import struct
import sys

LEVEL_NAMES = ("ERR", "WRN", "INF", "DBG")

# Magic numbers (n_ww_log_storage.h), little-endian byte patterns in a dump.
RAM_MAGIC    = 0x574C4F47  # 'WLOG'  - LOG_RAM_HEADER_T at start of the RAM region
PART_MAGIC   = 0x474F4C58  # 'XLOG'  - LOG_EXT_PART_HDR_T at the ext partition base
RAM_MAGIC_LE   = struct.pack('<I', RAM_MAGIC)
PART_MAGIC_LE  = struct.pack('<I', PART_MAGIC)
ERASED = 0xFFFFFFFF

# Control records (n_ww_log_def.h): entries the log module writes into its own
# stream. file_id 0xFFF is never assigned to a source file, so these cannot
# collide with a real call site; each carries a normal pcnt, so the entry walker
# steps over them without knowing what they mean.
CTRL_FILE_ID   = 0xFFF
CTRL_LINE_FLUSH = 0x3FFF   # [hdr][tick]                      - one per flush batch
CTRL_LINE_BOOT  = 0x3FFE   # [hdr][map_id][version][git_id]   - one per boot

# RAM maintain region geometry (n_ww_log_storage.h). v2 header is 32 bytes.
RAM_HEADER_SIZE = 32
RAM_TOTAL_SIZE  = 4096
RAM_DATA_SIZE   = RAM_TOTAL_SIZE - RAM_HEADER_SIZE   # 4064

# External-storage geometry (n_ww_log_storage.h): an 8B 'XLOG' partition header
# followed by an append-only stream of whole entries, terminated by 0xFFFFFFFF.
EXT_PART_HDR_SIZE = 8

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
# Partition table
# ----------------------------------------------------------------------------
# The firmware does NOT hardcode where the LOG partition lives -- log_ext_mem_init()
# reads the partition table and takes part_offset/part_size from the LOG entry
# (n_ww_log_storage.c). The host used to hardcode both, so a device whose LOG
# partition moved or was resized got read at the wrong offset, or truncated.
# Parsing the same table here keeps one source of truth.
#
# !! VERIFY AGAINST THE REAL FIRMWARE BEFORE TRUSTING ON HARDWARE !!
# The layout below mirrors sim/init_ex.h, which is the simulator's stand-in and
# was written from the field names n_ww_log_storage.c uses -- the magic and the
# LOG type id in particular are placeholders. Correct these four constants and
# everything downstream follows; nothing else encodes the layout.
PT_MAGIC          = 0x50415254      # 'PART'
PT_ENTRY_TYPE_LOG = 8
PT_HDR_FMT        = '<IIIIHH'       # magic, version, product, ptableSize, pentryNum, rsv
PT_ENTRY_FMT      = '<BBBBII'       # part_type, part_id, slot_id, rsv, offset, size
PT_HDR_SIZE       = struct.calcsize(PT_HDR_FMT)      # 20
PT_ENTRY_SIZE     = struct.calcsize(PT_ENTRY_FMT)    # 12
PT_MAX_ENTRIES    = 16
PT_SCAN_LIMIT     = 64 * 1024       # how far into a blob to look for the table


def parse_partition_table(blob, off=None):
    """Parse the partition table found in `blob` -> list of entry dicts.

    With `off` omitted the magic is searched for, so a dump that starts at an
    arbitrary offset (or a whole-chip image) still works. Returns [] when no
    plausible table is present -- callers then fall back to explicit offsets.
    """
    if off is None:
        off = blob.find(struct.pack('<I', PT_MAGIC), 0, PT_SCAN_LIMIT)
        if off < 0:
            return []
    if off + PT_HDR_SIZE > len(blob):
        return []

    magic, _ver, _prod, _size, n, _rsv = struct.unpack_from(PT_HDR_FMT, blob, off)
    if magic != PT_MAGIC or not (0 < n <= PT_MAX_ENTRIES):
        return []

    out = []
    for i in range(n):
        eoff = off + PT_HDR_SIZE + i * PT_ENTRY_SIZE
        if eoff + PT_ENTRY_SIZE > len(blob):
            break
        ptype, pid, slot, _r, poff, psize = struct.unpack_from(PT_ENTRY_FMT, blob, eoff)
        out.append({'type': ptype, 'id': pid, 'slot': slot,
                    'offset': poff, 'size': psize})
    return out


def find_log_partition(blob, off=None):
    """(offset, size) of the LOG partition from a partition table, or None."""
    for e in parse_partition_table(blob, off):
        if e['type'] == PT_ENTRY_TYPE_LOG and e['size'] > 0:
            return e['offset'], e['size']
    return None


# ----------------------------------------------------------------------------
# header decode + format substitution
# ----------------------------------------------------------------------------

def decode_header(h):
    """Split a 32-bit header into (file_id, line, level, param_count)."""
    return (h >> 20) & 0xFFF, (h >> 6) & 0x3FFF, (h >> 4) & 0x3, h & 0xF


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


def short_names(files):
    """file_id -> display name. Prefer the map's precomputed 'short' (the same
    name STR mode prints); fall back to disambiguating here so older maps, which
    have no 'short' field, still do not print two different files as 'main.c'."""
    have = {fid: info['short'] for fid, info in files.items() if info.get('short')}
    todo = {fid: info['path'] for fid, info in files.items() if fid not in have}
    if not todo:
        return have

    parts = {fid: p.replace('\\', '/').split('/') for fid, p in todo.items()}
    depth = max((len(v) for v in parts.values()), default=1)
    for n in range(1, depth + 1):
        groups = {}
        for fid in list(todo):
            groups.setdefault('/'.join(parts[fid][-n:]), []).append(fid)
        for cand, group in groups.items():
            if len(group) == 1:
                have[group[0]] = cand
                del todo[group[0]]
        if not todo:
            break
    for fid, p in todo.items():
        have[fid] = p
    return have


# C escapes that can appear inside a format string. The map stores fmt exactly
# as written in the source, so "rc:0x%x\r\n" arrives as literal backslash-r-
# backslash-n; render them as the characters they stand for and drop the
# trailing line break (the decoder emits one line per entry itself).
_ESCAPES = (('\\\\', '\x00'), ('\\r', '\r'), ('\\n', '\n'), ('\\t', '\t'),
            ('\\"', '"'), ("\\'", "'"), ('\\0', '\0'))


def unescape(fmt):
    for src, dst in _ESCAPES:
        fmt = fmt.replace(src, dst)
    return fmt.replace('\x00', '\\').rstrip('\r\n')


def compute_map_id(the_map):
    """32-bit identity of a map. MUST match gen_log_map.py's compute_map_id().

    Covers only what affects decoding (encode tag, file_id->path, and every
    file_id/line/level/fmt) so that unrelated churn -- build_time, module enable
    flags, JSON formatting -- does not invent a new identity. Because it is a
    pure function of the map file, maps generated before meta.map_id existed can
    still be indexed; the stored field is only a cross-check.
    """
    h = hashlib.sha256()
    h.update(the_map.get('meta', {}).get('encoding', '').encode('utf-8'))
    for fid in sorted(the_map.get('files', {}), key=int):
        h.update(('\0F%d|%s' % (int(fid), the_map['files'][fid]['path']))
                 .encode('utf-8'))
    for e in sorted(the_map.get('entries', []),
                    key=lambda e: (e['file_id'], e['line'])):
        h.update(('\0E%d|%d|%s|%s'
                  % (e['file_id'], e['line'], e['level'], e['fmt']))
                 .encode('utf-8'))
    mid = int.from_bytes(h.digest()[:4], 'big')
    return 1 if mid in (0x00000000, 0xFFFFFFFF) else mid


def build_index(the_map):
    entries = {(e['file_id'], e['line']): e for e in the_map.get('entries', [])}
    files = {int(k): v for k, v in the_map.get('files', {}).items()}
    modules = {int(k): v for k, v in the_map.get('modules', {}).items()}
    return entries, files, modules, short_names(files)


def load_map_set(paths, map_dir=None):
    """Load every available map, keyed by map_id -> (index, path).

    Returns (maps, default_id). The first --map given is the default: it decodes
    stretches of stream whose map_id is unknown, and any stream with no boot
    record at all (pre-boot-record archives).
    """
    maps, default_id = {}, None
    candidates = list(paths)
    if map_dir:
        candidates += sorted(os.path.join(map_dir, f) for f in os.listdir(map_dir)
                             if f.endswith('.json'))
    for p in candidates:
        try:
            with open(p, 'r', encoding='utf-8') as f:
                the_map = json.load(f)
        except FileNotFoundError:
            sys.exit("Error: map file '%s' not found" % p)
        except json.JSONDecodeError as e:
            sys.exit("Error: invalid JSON in '%s': %s" % (p, e))
        mid = compute_map_id(the_map)

        stored = the_map.get('meta', {}).get('map_id')
        if stored and int(str(stored), 16) != mid:
            print("# WARNING: %s says map_id=%s but its content hashes to "
                  "0x%08X (edited by hand?)" % (p, stored, mid))

        maps.setdefault(mid, (build_index(the_map), p))
        if default_id is None:
            default_id = mid
    return maps, default_id


def format_frame(header, params, idx):
    """Turn one (header, params) frame into a readable line via the map index."""
    entries, _files, _modules, names = idx

    file_id, line, level, pcnt = decode_header(header)

    if file_id == CTRL_FILE_ID and line == CTRL_LINE_FLUSH:
        tick = params[0] if params else 0
        return ("----- flush @ tick=%u (0x%08X) -----" % (tick, tick))

    params = params[:pcnt]
    lvl = LEVEL_NAMES[level]   # level comes straight from the encoded header

    fname = names.get(file_id, 'file_id_%d' % file_id)

    entry = entries.get((file_id, line))
    if entry is None:
        return ("[%s] %s:%d - <no map entry> [raw 0x%08X, %d params: %s]"
                % (lvl, fname, line, header, pcnt,
                   ' '.join('0x%08X' % p for p in params)))

    return "[%s] %s:%d - %s" % (lvl, fname, line,
                                render_fmt(unescape(entry.get('fmt', '')), params))


def render_frames(frames, maps, default_id, raw=False):
    """Decode a whole stream, switching maps at every boot record.

    An archive deliberately survives firmware updates, so one stream can hold
    entries built from several different maps. Decoding all of it with today's
    map is the dangerous case: an old (file_id, line) usually still resolves to
    SOME entry in the new map -- a different log statement that happens to sit
    on that line now -- so the output looks reasonable and is wrong. The boot
    record names the map that produced everything after it, so this walks
    segment by segment and marks anything it cannot vouch for.

    Lines decoded with a map that is not the one that produced them are
    prefixed '?'. Entries ahead of the first boot record (archives written
    before boot records existed) get the same treatment.
    """
    lines = []
    cur_idx = maps[default_id][0] if default_id in maps else None
    trusted = False            # no boot record seen yet -> map is a guess

    for header, params in frames:
        file_id, line, _level, pcnt = decode_header(header)

        if file_id == CTRL_FILE_ID and line == CTRL_LINE_BOOT:
            map_id = params[0] if len(params) > 0 else 0
            version = params[1] if len(params) > 1 else 0
            git_id = params[2] if len(params) > 2 else 0
            if map_id in maps:
                cur_idx, trusted = maps[map_id][0], True
                lines.append("===== boot: map 0x%08X  version 0x%08X  git 0x%08X "
                             "(%s) =====" % (map_id, version, git_id,
                                             os.path.basename(maps[map_id][1])))
            else:
                # Do not keep using the PREVIOUS boot's trusted map. If the
                # requested map is missing, fall back explicitly to the CLI's
                # default map and mark every line as guessed.
                cur_idx = maps[default_id][0] if default_id in maps else None
                trusted = False
                lines.append("===== boot: map 0x%08X  version 0x%08X  git 0x%08X "
                             "=====" % (map_id, version, git_id))
                lines.append("# WARNING: no map with id 0x%08X was loaded; the "
                             "lines below are decoded with the default map and "
                             "may be WRONG (pass it via --map/--map-dir)"
                             % map_id)
            continue

        if cur_idx is None:
            lines.append("# ERROR: no map loaded")
            continue

        text = format_frame(header, params, cur_idx)
        if raw:
            text += ("    | 0x%08X " % header) + \
                    ' '.join('0x%08X' % p for p in params[:pcnt])
        lines.append(text if trusted else '?' + text)
    return lines


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
        pcnt = header & 0xF
        need = 4 + pcnt * 4
        if i + need > end:
            break   # truncated tail
        params = list(struct.unpack_from('<%dI' % pcnt, buf, i + 4)) if pcnt else []
        frames.append((header, params))
        i += need
    return frames


def parse_ram_region(buf, off, notes):
    """Parse a 'WLOG' RAM maintain region (v2 LOG_RAM_HEADER_T, 32 bytes),
    honouring the ring read/write pointers and the OVERFLOW flag."""
    (_magic, write_index, read_index, pending_len, flush_count,
     overflow_count, flags, log_count, _r1, _r2, _crc) = \
        struct.unpack_from('<IHHHHHHIIII', buf, off)
    overflow = flags & 0x01            # LOG_FLAG_OVERFLOW
    notes.append("# RAM region: write=%d read=%d pending=%d flushes=%d "
                 "overflow_cnt=%d flags=0x%X log_count=%d"
                 % (write_index, read_index, pending_len, flush_count,
                    overflow_count, flags, log_count))
    if flags & 0x08:                   # LOG_FLAG_CORRUPTED
        notes.append("# WARNING: LOG_FLAG_CORRUPTED set (data chain failed validation)")

    data_off = off + RAM_HEADER_SIZE
    data = buf[data_off:data_off + RAM_DATA_SIZE]
    if len(data) < RAM_DATA_SIZE:                 # dump shorter than a full region
        data = data + b'\xff' * (RAM_DATA_SIZE - len(data))

    # Decode everything physically present in the ring, oldest-first. read_index
    # is only the flush cursor (how far the ext backend has drained) -- the ring
    # never erases flushed bytes, so bounding by read_index would hide logs that
    # are still in RAM after a flush (read==write => "0 entries" even when full).
    # Mirror the firmware's log_ram_dump_hex and key off write_index instead:
    #   overflow -> ring wrapped & full: [write_index..end] + [0..write_index)
    #   else     -> linear fill:         [0..write_index)
    if overflow:
        ordered = data[write_index:] + data[:write_index]
    else:
        ordered = data[:write_index]
    if read_index != write_index or flush_count:
        notes.append("# note: showing all %d bytes physically in the ring "
                     "(incl. entries already flushed to ext)" % len(ordered))
    return parse_entry_stream(ordered, 0, len(ordered))


def parse_ext_partition(buf, off, notes):
    """Parse the external-storage append log: an 8B 'XLOG' partition header at
    `off`, then a contiguous stream of whole entries up to the first 0xFFFFFFFF
    (erased tail). No per-block header/CRC/footer -- the stream is self-describing
    (each entry's pcnt gives its length) and already in chronological order."""
    magic, version, _res = struct.unpack_from('<IHH', buf, off)
    notes.append("# ext partition header 'XLOG' at 0x%X (version=%d)"
                 % (off, version))
    if magic != PART_MAGIC:
        notes.append("# WARNING: partition magic mismatch (0x%08X) -> raw parse" % magic)
    return parse_entry_stream(buf, off + EXT_PART_HDR_SIZE, len(buf))


def parse_binary(data, notes):
    """Auto-frame a binary dump by locating the first known container magic.

    A whole-chip image is also handled: if it carries a partition table, the LOG
    partition's real offset/size are taken from it (the same source the firmware
    uses) and the search is confined to that window, instead of trusting a
    hardcoded geometry that a repartition would silently invalidate.
    """
    part = find_log_partition(data)
    if part is not None:
        off, size = part
        if off + 4 <= len(data):
            notes.append("# partition table: LOG at 0x%X, %u bytes" % (off, size))
            data = data[off:off + size]
        else:
            notes.append("# partition table names LOG at 0x%X, past the end of "
                         "this dump (%u bytes) -> ignoring it" % (off, len(data)))

    iw = data.find(RAM_MAGIC_LE)
    ip = data.find(PART_MAGIC_LE)
    cands = [(i, k) for i, k in ((iw, 'ram'), (ip, 'flog')) if i != -1]
    if not cands:
        notes.append("# no WLOG/XLOG magic found -> parsing as raw entry stream")
        return parse_entry_stream(data, 0, len(data))
    start, kind = min(cands)
    if kind == 'ram':
        notes.append("# found RAM 'WLOG' header at offset 0x%X" % start)
        return parse_ram_region(data, start, notes)
    notes.append("# found ext 'XLOG' append log at offset 0x%X" % start)
    return parse_ext_partition(data, start, notes)


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
    ap.add_argument('--map', action='append', default=[], metavar='PATH',
                    help='path to a ww_log_map.json. Repeatable; the first one '
                         'is the default used for stream segments whose map_id '
                         'is unknown.')
    ap.add_argument('--map-dir', metavar='DIR',
                    help='also load every *.json in DIR as a map (the archive '
                         'written by `make map-archive`). Maps are indexed by '
                         'map_id, so a log stream spanning several firmware '
                         'versions decodes each segment with its own map.')
    ap.add_argument('input', nargs='?',
                    help="hex/text or binary dump, or '-' for stdin")
    ap.add_argument('--hex', dest='hex_str', metavar='"0x.. 0x.."',
                    help='decode hex frame(s) given directly on the command line')
    ap.add_argument('--format', choices=('auto', 'hex', 'bin'), default='auto',
                    help='input format (default: auto-detect)')
    ap.add_argument('--raw', action='store_true',
                    help='append the raw frame after each decoded line')
    ap.add_argument('-o', '--output', metavar='PATH',
                    help='save to a file. Extension decides what is written:\n'
                         "  .dump/.bin -> the raw log bytes (no decode)\n"
                         '  .txt/other -> the decoded human-readable lines\n'
                         '(no extension defaults to .txt / decoded)')
    args = ap.parse_args()

    if not args.hex_str and not args.input:
        ap.error("provide an input file/'-' , or --hex \"0x..\"")
    if not args.map and not args.map_dir:
        ap.error("provide at least one --map or a --map-dir")

    maps, default_id = load_map_set(args.map, args.map_dir)
    if default_id is None:
        sys.exit("Error: no maps loaded")

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

    decoded_lines = render_frames(frames, maps, default_id, args.raw)
    for line in decoded_lines:
        print(line)

    if args.output:
        save_output(args.output, data, decoded_lines)

    print("# format=%s, decoded %d log entries" % (fmt, len(frames)),
          file=sys.stderr)


def save_output(path, raw_bytes, decoded_lines):
    """Save either the raw log bytes or the decoded text, picked by extension:
    .dump/.bin -> raw bytes; .txt or anything else -> decoded text."""
    import os
    ext = os.path.splitext(path)[1].lower()
    if ext in ('.dump', '.bin'):
        with open(path, 'wb') as f:
            f.write(raw_bytes)
        print("# saved raw log bytes (%d) -> %s" % (len(raw_bytes), path),
              file=sys.stderr)
    else:
        with open(path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(decoded_lines) + ('\n' if decoded_lines else ''))
        print("# saved %d decoded lines -> %s" % (len(decoded_lines), path),
              file=sys.stderr)


if __name__ == '__main__':
    main()
