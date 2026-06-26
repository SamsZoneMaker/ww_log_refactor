#!/usr/bin/env python3
"""
log_operation.py -- ww_log v1 on-device log reader + decoder (dora api layer)
=============================================================================
Reads ENCODED ww_log data straight off the device (no firmware help needed) and
restores it to human-readable lines, identical to what STR mode would print.

Access chains (all via the same JTAG channel the flash/eeprom tools already use):

    RAM     PC -> JTAG -> RISC-V Debug Module -> System Bus -> memory read
            (the 4KB power-loss-retained DLM "maintain" region, 'WLOG' header)

    Flash   PC -> JTAG -> ATCSPI200 -> SPI NOR  (LOG partition, 'LOGH' blocks)

    EEPROM  PC -> JTAG -> ATCIIC100 -> I2C EEPROM (LOG partition, 'LOGH' blocks)

This module is SELF-CONTAINED: the encode format (CLAUDE.md S2) and the
RAM/storage container layout (ww_log_config.h) are reproduced here, so the dora
deployment has no dependency on the ww_log_v1/tools/ folder. Keep the constants
below in sync with ww_log_config.h if the firmware geometry ever changes.

Encoding, 32-bit header (little-endian on the wire):

   31                20 19              6 5         0
  +--------------------+------------------+-----------+
  |   file_id (12)     |    line (14)     | param_cnt |
  +--------------------+------------------+-----------+
          |
          +-- file_id = [ module_id : 5 ][ offset : 7 ]

level is NOT encoded; it is restored from the map by (file_id, line).
%s params cannot be restored (only the pointer was stored) -> shown as
<%s@0xXXXXXXXX>.
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
BLOCK_MAGIC    = 0x4C4F4748          # 'LOGH' - LOG_BLOCK_HEADER_T before each block
FOOTER_MAGIC   = 0x474F4C46          # 'FLOG' - LOG_EXT_FOOTER_T ring control
RAM_MAGIC_LE   = struct.pack('<I', RAM_MAGIC)
BLOCK_MAGIC_LE = struct.pack('<I', BLOCK_MAGIC)
ERASED         = 0xFFFFFFFF

# RAM maintain region geometry (n_ww_log_storage.h). v2 header is 32 bytes.
RAM_HEADER_SIZE   = 32
RAM_TOTAL_SIZE    = 4096
RAM_DATA_SIZE     = RAM_TOTAL_SIZE - RAM_HEADER_SIZE   # 4064

# External-storage block ring geometry (n_ww_log_storage.h).
EXT_BLOCK_SIZE    = 512
BLOCK_HEADER_SIZE = 28
EXT_FOOTER_SIZE   = 32

# Default read window when the caller does not pass an explicit length: one
# whole maintain region / LOG partition. Reading a superset is fine -- the
# magic auto-scan locates the real container inside it.
DEFAULT_READ_LEN = 4096

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
    file_id, line, pcnt = decode_header(header)
    params = params[:pcnt]

    finfo = files.get(file_id)
    fname = _basename(finfo['path']) if finfo else 'file_id_%d' % file_id

    entry = entries.get((file_id, line))
    if entry is None:
        return ("[???] %s:%d - <no map entry> [raw 0x%08X, %d params: %s]"
                % (fname, line, header, pcnt,
                   ' '.join('0x%08X' % p for p in params)))

    return "[%s] %s:%d - %s" % (entry.get('level', '???'), fname, line,
                                render_fmt(entry.get('fmt', ''), params))


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
        pcnt = header & 0x3F
        need = 4 + pcnt * 4
        if i + need > end:
            break   # truncated tail
        params = list(struct.unpack_from('<%dI' % pcnt, buf, i + 4)) if pcnt else []
        frames.append((header, params))
        i += need
    return frames


def _sum_u32(payload):
    """Match the firmware log_calc_checksum: sum of whole little-endian U32s."""
    s = 0
    for i in range(0, len(payload) - 3, 4):
        s = (s + struct.unpack_from('<I', payload, i)[0]) & 0xFFFFFFFF
    return s


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

    if overflow:                                  # ring wrapped: read..end + 0..write
        ordered = data[read_index:] + data[:write_index]
    else:
        ordered = data[read_index:write_index]
    return parse_entry_stream(ordered, 0, len(ordered))


def parse_block_ring(buf, notes):
    """Parse the external-storage block ring: fixed 512B slots, each a
    LOG_BLOCK_HEADER_T (28B 'LOGH' + payload). Slots are walked by stride,
    validated by magic + payload CRC, then ordered by sequence number so a
    wrapped ring (oldest slot overwritten) decodes in chronological order."""
    n = len(buf)
    slots = (n - EXT_FOOTER_SIZE) // EXT_BLOCK_SIZE if n > EXT_FOOTER_SIZE else n // EXT_BLOCK_SIZE
    blocks = []
    for i in range(max(slots, 0)):
        off = i * EXT_BLOCK_SIZE
        if off + BLOCK_HEADER_SIZE > n:
            break
        magic, seq, _ts, data_size, ecount, crc, _r1, _r2 = \
            struct.unpack_from('<IIIHHIII', buf, off)
        if magic != BLOCK_MAGIC:
            continue                              # erased / never-written slot
        payload = buf[off + BLOCK_HEADER_SIZE: off + BLOCK_HEADER_SIZE + data_size]
        ok = (_sum_u32(payload) == crc)
        notes.append("# slot %d: seq=%d size=%d entries=%d crc=%s"
                     % (i, seq, data_size, ecount, "ok" if ok else "BAD"))
        if ok:
            blocks.append((seq, payload))
    blocks.sort(key=lambda b: b[0])               # chronological by sequence
    frames = []
    for _seq, payload in blocks:
        frames += parse_entry_stream(payload, 0, len(payload))
    return frames


def parse_binary(data, notes):
    """Auto-frame a binary dump by locating the first known container magic."""
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
    notes.append("# found storage 'LOGH' block ring (slot stride %dB)" % EXT_BLOCK_SIZE)
    return parse_block_ring(data, notes)


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
        """Read `length` bytes of target memory at `addr` over JTAG SBA."""
        _, byte_list = self.dora.f_csr_byte_rd(addr, length, v_boardId=self.boardId)
        return bytes(byte_list)

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

    def _decode_blob(self, data, raw, output):
        """Auto-frame `data`, print decoded lines, optionally save, return frames.

        `output` extension decides what is saved:
          .dump/.bin -> the raw log bytes read off the device (no decode)
          .txt/other -> the decoded human-readable lines
        (no extension defaults to .txt / decoded)
        """
        notes = []
        frames = parse_binary(data, notes)
        for note in notes:
            print(note)

        decoded_lines = []
        for header, params in frames:
            line = format_frame(header, params, self.index)
            if raw:
                line += ("    | 0x%08X " % header) + \
                        ' '.join('0x%08X' % p for p in params[:header & 0x3F])
            decoded_lines.append(line)
            print(line)
        print("# decoded %d log entries" % len(frames))

        if output:
            _save_output(output, data, decoded_lines)
        return frames

    # --- public ------------------------------------------------------------

    def f_decode_ram(self, addr, length=DEFAULT_READ_LEN, raw=False, output=None):
        """Read + decode the RAM maintain region at memory address `addr`."""
        print(f"Reading {length} bytes from RAM 0x{addr:08X} ...")
        data = self._read_ram(addr, length)
        return self._decode_blob(data, raw, output)

    def f_decode_flash(self, offset=0x0, length=DEFAULT_READ_LEN,
                       raw=False, output=None):
        """Read + decode the LOG partition from flash at `offset`."""
        data = self._read_flash(offset, length)
        return self._decode_blob(data, raw, output)

    def f_decode_eeprom(self, offset=0x0, length=DEFAULT_READ_LEN,
                        devAddr=None, raw=False, output=None):
        """Read + decode the LOG partition from EEPROM at `offset`."""
        data = self._read_eeprom(offset, length, devAddr)
        return self._decode_blob(data, raw, output)
