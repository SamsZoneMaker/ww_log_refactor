#!/usr/bin/env python3
"""
log_operation.py -- ww_log v1 on-device log reader + decoder (dora api layer)
=============================================================================
Reads ENCODED ww_log data straight off the device (no firmware help needed) and
restores it to human-readable lines, identical to what STR mode would print.

Access chains (all via the same JTAG channel the flash/eeprom tools already use):

    RAM     PC -> JTAG -> RISC-V Debug Module -> System Bus -> memory read
            (the 4KB power-loss-retained DLM "maintain" region, 'WLOG' header)

    Flash   PC -> JTAG -> ATCSPI200 -> SPI NOR  (LOG partition, 'XLOG' append log)

    EEPROM  PC -> JTAG -> ATCIIC100 -> I2C EEPROM (LOG partition, 'XLOG' append log)

This module is SELF-CONTAINED: the encode format and the RAM/storage container
layout (n_ww_log_storage.h) are reproduced here, so the dora deployment has no
dependency on the ww_log_v2/scripts/ folder. Keep the constants below in sync
with the firmware headers if the geometry ever changes.

Encoding, 32-bit header (little-endian on the wire):

   31                20 19              6 5   4 3        0
  +--------------------+------------------+-----+---------+
  |   file_id (12)     |    line (14)     |lv(2)| pcnt(4) |
  +--------------------+------------------+-----+---------+
          |
          +-- file_id = [ module_id : 5 ][ offset : 7 ]

level IS encoded (2 bits) and read straight from the header; the map is only used
for the file name + format string. %s params cannot be restored (only the pointer
was stored) -> shown as <%s@0xXXXXXXXX>.
"""

import json
import os
import re
import struct

# Memory-mapped flash window base (see flash_operation.FLASH_MMAP_BASE): the SPI
# NOR is mapped here, so the LOG partition can be read fast over JTAG/SBA word
# reads instead of slow per-page SPI READ commands.
FLASH_MMAP_BASE = 0x80000000


# ----------------------------------------------------------------------------
# Constants -- MUST match ww_log_config.h
# ----------------------------------------------------------------------------

LEVEL_NAMES = ("ERR", "WRN", "INF", "DBG")

RAM_MAGIC      = 0x574C4F47          # 'WLOG' - LOG_RAM_HEADER_T at region start
PART_MAGIC     = 0x474F4C58          # 'XLOG' - LOG_EXT_PART_HDR_T at partition base
# Flush-batch marker header (n_ww_log_storage.h LOG_EXT_FLUSH_MARKER_HDR):
# N_WW_LOG_ENCODE(file_id=0xFFF, line=0x3FFF, level=0, pcnt=1). Prepended to each
# ext flush batch; its single U32 param is the FreeRTOS tick at flush time.
FLUSH_MARKER   = 0xFFFFFFC1
RAM_MAGIC_LE   = struct.pack('<I', RAM_MAGIC)
PART_MAGIC_LE  = struct.pack('<I', PART_MAGIC)
ERASED         = 0xFFFFFFFF

# RAM maintain region geometry (n_ww_log_storage.h). v2 header is 32 bytes.
RAM_HEADER_SIZE   = 32
RAM_TOTAL_SIZE    = 4096
RAM_DATA_SIZE     = RAM_TOTAL_SIZE - RAM_HEADER_SIZE   # 4064

# External-storage geometry (n_ww_log_storage.h): 8B 'XLOG' partition header then
# an append-only stream of whole entries, terminated by the first 0xFFFFFFFF.
EXT_PART_HDR_SIZE = 8

# Default read window when the caller does not pass an explicit length: the
# whole LOG region of each source. Reading a superset is fine -- the magic
# auto-scan locates the real container inside it.
RAM_LOG_SIZE     = 4096            # DLM maintain region
FLASH_LOG_SIZE   = 4096            # flash LOG partition (4KB)
EEPROM_LOG_SIZE  = 21 * 1024       # eeprom LOG partition (21KB)
DEFAULT_READ_LEN = RAM_LOG_SIZE    # back-compat default

# "Unwritten" fill byte per medium: RAM powers up / clears to 0x00, NOR flash
# and EEPROM erase to 0xFF. A region that is entirely its fill byte holds no
# logs, so decode is skipped (a --hex dump still shows the raw bytes).
RAM_FILL_BYTE    = 0x00
EXT_FILL_BYTE    = 0xFF

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
# header decode + format substitution (ported from tools/log_decoder.py)
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

def _basename(path):
    return path.replace('\\', '/').rsplit('/', 1)[-1]


def build_index(the_map):
    entries = {(e['file_id'], e['line']): e for e in the_map.get('entries', [])}
    files = {int(k): v for k, v in the_map.get('files', {}).items()}
    modules = {int(k): v for k, v in the_map.get('modules', {}).items()}
    return entries, files, modules


def format_frame(header, params, idx):
    """Turn one (header, params) frame into a readable line via the map index."""
    entries, files, _modules = idx

    if header == FLUSH_MARKER:                 # ext flush-batch boundary
        tick = params[0] if params else 0
        return ("----- flush @ tick=%u (0x%08X) -----" % (tick, tick))

    file_id, line, level, pcnt = decode_header(header)
    params = params[:pcnt]
    lvl = LEVEL_NAMES[level]   # level comes straight from the encoded header

    finfo = files.get(file_id)
    fname = _basename(finfo['path']) if finfo else 'file_id_%d' % file_id

    entry = entries.get((file_id, line))
    if entry is None:
        return ("[%s] %s:%d - <no map entry> [raw 0x%08X, %d params: %s]"
                % (lvl, fname, line, header, pcnt,
                   ' '.join('0x%08X' % p for p in params)))

    return "[%s] %s:%d - %s" % (lvl, fname, line,
                                render_fmt(entry.get('fmt', ''), params))


# ----------------------------------------------------------------------------
# raw hex dump (no decode) + empty-region detection
# ----------------------------------------------------------------------------

def is_all_fill(data, fill):
    """True if `data` is non-empty and every byte equals `fill` (0x00 for an
    unwritten RAM region, 0xFF for erased flash/eeprom) -> nothing to decode."""
    return len(data) > 0 and data.count(fill) == len(data)


def hex_dump_lines(data, base=0):
    """Render `data` as offset-prefixed rows of 4 little-endian U32 words each
    (a wider take on the firmware's log_ram_dump_hex, which prints 1 word/row).
    A short tail is zero-padded to a full word so the columns stay aligned."""
    lines = []
    for off in range(0, len(data), 16):
        chunk = data[off:off + 16]
        words = []
        for w in range(0, len(chunk), 4):
            wb = chunk[w:w + 4]
            wb = wb + b'\x00' * (4 - len(wb))       # pad a ragged tail word
            words.append('%08X' % struct.unpack('<I', wb)[0])
        lines.append('0x%04X | %s' % (base + off, ' '.join(words)))
    return lines


# ----------------------------------------------------------------------------
# binary parsers -> list of (header:int, params:list[int])
# ----------------------------------------------------------------------------

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
    notes.append("# ext partition header 'XLOG' at 0x%X (version=%d)" % (off, version))
    if magic != PART_MAGIC:
        notes.append("# WARNING: partition magic mismatch (0x%08X) -> raw parse" % magic)
    return parse_entry_stream(buf, off + EXT_PART_HDR_SIZE, len(buf))


def parse_binary(data, notes):
    """Auto-frame a binary dump by locating the first known container magic."""
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


def _save_output(path, raw_bytes, decoded_lines):
    """Save either the raw log bytes or the decoded text, picked by extension:
    .dump/.bin -> raw bytes; .txt or anything else -> decoded text."""
    ext = os.path.splitext(path)[1].lower()
    if ext in ('.dump', '.bin'):
        with open(path, 'wb') as f:
            f.write(raw_bytes)
        print(f"Saved raw log bytes ({len(raw_bytes)}) -> {path}")
    else:
        with open(path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(decoded_lines) + ('\n' if decoded_lines else ''))
        print(f"Saved {len(decoded_lines)} decoded lines -> {path}")


# ----------------------------------------------------------------------------
# Log: device read + decode, mirroring api/flash_operation.Flash
# ----------------------------------------------------------------------------

class Log:
    """
    On-device ww_log reader/decoder.

    Built by dora.__board_log_get(); reads via the existing dora primitives and
    decodes with the map (ww_log_map.json) loaded here.

    Args:
        dora      - the DORA instance (provides f_csr_byte_rd / f_flash_read /
                    f_eeprom_read).
        map_path  - path to ww_log_map.json.
        boardId   - board id forwarded to every device read.
    """

    def __init__(self, dora, map_path, boardId=0):
        self.dora = dora
        self.boardId = boardId
        with open(map_path, 'r', encoding='utf-8') as f:
            self.map = json.load(f)
        self.index = build_index(self.map)

    # --- device reads -> bytes --------------------------------------------

    def _read_ram(self, addr, length):
        """Read `length` bytes of the DLM maintain region at `addr` over JTAG,
        using word (csr_word) reads. `addr` must be 4-byte aligned."""
        words = (length + 3) // 4
        _addrs, vals = self.dora.f_csr_word_rd(addr, words, v_boardId=self.boardId)
        blob = b''.join(struct.pack('<I', v & 0xFFFFFFFF) for v in vals)
        return blob[:length]

    def _read_flash(self, offset, length):
        """Fast flash read via the memory-mapped window (JTAG/SBA word reads),
        rather than SPI page reads. Mirrors flash_operation._verify_mmap."""
        words = (length + 3) // 4
        _addrs, vals = self.dora.f_csr_word_rd(FLASH_MMAP_BASE + offset, words,
                                               v_boardId=self.boardId)
        blob = b''.join(struct.pack('<I', v & 0xFFFFFFFF) for v in vals)
        return blob[:length]

    def _read_eeprom(self, offset, length, devAddr):
        return self.dora.f_eeprom_read(offset, length, v_print=False,
                                       v_boardId=self.boardId, v_devAddr=devAddr)

    # --- decode + emit -----------------------------------------------------

    def _decode_blob(self, data, raw, output, fill, hex_only=False):
        """Turn a raw device read into output, three ways:

          hex_only=True  -> dump the raw bytes as 4 U32/line, never decode.
          all `fill`     -> region is unwritten (0x00 RAM / 0xFF ext): report
                            it and skip decode (use --hex to see raw bytes).
          otherwise      -> auto-frame + decode to readable lines.

        `output` (if set) receives whatever was produced; its extension decides
        the format: .dump/.bin -> raw device bytes, .txt/.log/other -> the text.
        Returns the decoded frames (empty for the hex/empty paths).
        """
        # 1) raw hex dump: bytes as-is, no interpretation.
        if hex_only:
            lines = hex_dump_lines(data)
            print("# raw hex dump: %d bytes, 4x U32 (LE) per line" % len(data))
            for line in lines:
                print(line)
            if output:
                _save_output(output, data, lines)
            return []

        # 2) empty-region guard: an all-fill blob holds no logs.
        if is_all_fill(data, fill):
            print("# region is entirely 0x%02X (unwritten) -> nothing to decode "
                  "(use --hex to dump the raw bytes)" % fill)
            if output:
                _save_output(output, data, [])
            return []

        # 3) normal decode path.
        notes = []
        frames = parse_binary(data, notes)
        for note in notes:
            print(note)

        decoded_lines = []
        for header, params in frames:
            line = format_frame(header, params, self.index)
            if raw:
                line += ("    | 0x%08X " % header) + \
                        ' '.join('0x%08X' % p for p in params[:header & 0xF])
            decoded_lines.append(line)
            print(line)
        print("# decoded %d log entries" % len(frames))

        if output:
            _save_output(output, data, decoded_lines)
        return frames

    # --- public ------------------------------------------------------------

    def f_decode_ram(self, addr, length=RAM_LOG_SIZE, raw=False,
                     output=None, hex_only=False):
        """Read + decode (or hex-dump) the RAM maintain region at `addr`."""
        print(f"Reading {length} bytes from RAM 0x{addr:08X} ...")
        data = self._read_ram(addr, length)
        return self._decode_blob(data, raw, output, RAM_FILL_BYTE, hex_only)

    def f_decode_flash(self, offset=0x0, length=FLASH_LOG_SIZE,
                       raw=False, output=None, hex_only=False):
        """Read + decode (or hex-dump) the LOG partition from flash at `offset`."""
        print(f"Reading {length} bytes from flash LOG @ 0x{offset:X} ...")
        data = self._read_flash(offset, length)
        return self._decode_blob(data, raw, output, EXT_FILL_BYTE, hex_only)

    def f_decode_eeprom(self, offset=0x0, length=EEPROM_LOG_SIZE,
                        devAddr=None, raw=False, output=None, hex_only=False):
        """Read + decode (or hex-dump) the LOG partition from EEPROM at `offset`."""
        print(f"Reading {length} bytes from eeprom LOG @ 0x{offset:X} ...")
        data = self._read_eeprom(offset, length, devAddr)
        return self._decode_blob(data, raw, output, EXT_FILL_BYTE, hex_only)
