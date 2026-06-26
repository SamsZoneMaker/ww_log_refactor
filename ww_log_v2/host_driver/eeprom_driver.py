"""
eeprom_driver.py — BR24G2MFJ-5A I2C EEPROM protocol layer
==========================================================
Implements page-aligned read/write, ACK-polling for write completion,
and verify for the ROHM BR24G2MFJ-5A 256KB (2Mbit) I2C EEPROM.

Refer to: doc/br24g2mfj_spec.pdf
Layers on top of: i2c_driver.py

Device addressing (7-bit):
    Bits [6:3] = 1010  (fixed EEPROM class prefix)
    Bit  [2]   = A2    (hardware pin — chip-select for multi-chip)
    Bit  [1]   = P1    (memory address bit 17)
    Bit  [0]   = P0    (memory address bit 16)

18-bit memory address split:
    {P1, P0, WA[15:8], WA[7:0]}
    P1 and P0 go into the device address byte.
    WA[15:8] and WA[7:0] are the two address bytes in the data phase.

Write constraints:
    - Must not cross a 256-byte page boundary (WA[7:0] wraps).
    - Maximum data bytes per write = 254 (controller DataCnt limited to
      256 total data-phase bytes; 2 bytes used for WA_high / WA_low).
    - After each write, poll ACK until chip signals write complete
      (or timeout after WRITE_POLL_TIMEOUT_S seconds).
"""

import time
from . import i2c_driver


# ---------------------------------------------------------------------------
# EEPROM geometry
# ---------------------------------------------------------------------------
EEPROM_CAPACITY     = 256 * 1024       # 256 KiB total
EEPROM_PAGE_SIZE    = 256              # bytes per page (WA[7:0] boundary)
EEPROM_ADDR_BITS    = 18               # total address bits

# Maximum data bytes per single I2C write transaction.
# The I2C controller DataCnt field is 8-bit (0=256 bytes max).
# We consume 2 bytes for WA_high + WA_low, leaving 254 for payload.
EEPROM_MAX_WRITE_CHUNK = 254

# Maximum bytes per single I2C read transaction (DataCnt=0 → 256 bytes).
EEPROM_MAX_READ_CHUNK  = 256

# Write-cycle timeout: BR24G2MFJ max tWR = 3.5 ms
WRITE_POLL_TIMEOUT_S   = 0.010   # 10 ms — generous safety margin
WRITE_POLL_INTERVAL_S  = 0.0002  # 0.2 ms between ACK polls

# Default A2 pin value (0 = A2 pin tied to GND)
DEFAULT_A2 = 1


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def _make_dev_addr(a2: int, offset: int) -> int:
    """
    Build the 7-bit I2C device address for the given memory offset.

    7-bit layout: 1 0 1 0  A2  P1  P0
      A2  = hardware pin value (0 or 1)
      P1  = offset bit 17
      P0  = offset bit 16
    """
    p1 = (offset >> 17) & 1
    p0 = (offset >> 16) & 1
    return 0x50 | ((a2 & 1) << 2) | (p1 << 1) | p0


def _addr_bytes(offset: int):
    """Return (WA_high, WA_low) for the lower 16 bits of offset."""
    wa_high = (offset >> 8) & 0xFF
    wa_low  =  offset       & 0xFF
    return wa_high, wa_low


def _check_bounds(offset: int, length: int):
    """Raise ValueError if the access would exceed the device capacity."""
    if offset < 0 or offset >= EEPROM_CAPACITY:
        raise ValueError(
            f"offset 0x{offset:06X} out of range "
            f"[0, 0x{EEPROM_CAPACITY - 1:06X}]")
    if length <= 0:
        raise ValueError(f"length must be > 0, got {length}")
    if offset + length > EEPROM_CAPACITY:
        raise ValueError(
            f"Access 0x{offset:06X}+{length} exceeds device capacity "
            f"(0x{EEPROM_CAPACITY:06X})")


# ---------------------------------------------------------------------------
# Read
# ---------------------------------------------------------------------------

def eeprom_read(handle, base, offset: int, length: int,
                a2: int = DEFAULT_A2) -> bytes:
    """
    Read *length* bytes from EEPROM starting at *offset*.

    Uses the EEPROM random-read sequence:
        START → dev_addr(W) → WA_high → WA_low
        → rSTART → dev_addr(R) → data[0..N-1] → STOP

    Chunks at EEPROM_MAX_READ_CHUNK (256) bytes per I2C transaction.
    Re-sends the address for each chunk.

    Args:
        handle: JTAG handle.
        base:   I2C controller base address.
        offset: 18-bit start offset (0 to 0x3FFFF).
        length: Number of bytes to read.
        a2:     Value of the A2 hardware pin (default 0).

    Returns:
        bytes of length *length*.

    Raises:
        ValueError:   on out-of-range offset or length.
        RuntimeError: on I2C error or timeout.
    """
    _check_bounds(offset, length)

    data       = bytearray()
    remaining  = length
    cur_offset = offset

    while remaining > 0:
        # Each chunk must stay within the same P1/P0 bank (offset bits [17:16]).
        # We re-derive the device address per chunk to handle bank crossings.
        chunk = min(EEPROM_MAX_READ_CHUNK, remaining)

        # Ensure we don't cross a P1/P0 bank boundary (64 KiB per bank)
        bank_end = ((cur_offset >> 16) + 1) << 16
        if cur_offset + chunk > bank_end:
            chunk = bank_end - cur_offset

        dev_addr        = _make_dev_addr(a2, cur_offset)
        wa_high, wa_low = _addr_bytes(cur_offset)
        wr_data         = bytes([wa_high, wa_low])

        chunk_data = i2c_driver.i2c_write_read(
            handle, base, dev_addr, wr_data, chunk)

        data.extend(chunk_data)
        cur_offset += chunk
        remaining  -= chunk

    return bytes(data)


# ---------------------------------------------------------------------------
# Write (page-aligned, with ACK polling)
# ---------------------------------------------------------------------------

def _ack_poll(handle, base, dev_addr_7bit,
              timeout_s=WRITE_POLL_TIMEOUT_S,
              interval_s=WRITE_POLL_INTERVAL_S):
    """
    Poll ACK from the EEPROM until write cycle completes.

    Sends START + dev_addr(W) + STOP and checks Status.ACK.
    Returns True when ACK received, False on timeout.
    """
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if i2c_driver.i2c_probe(handle, base, dev_addr_7bit):
            return True
        time.sleep(interval_s)
    return False


def eeprom_write(handle, base, offset: int, data: bytes,
                 a2: int = DEFAULT_A2):
    """
    Write *data* to EEPROM starting at *offset*.

    Automatically splits writes at:
      - 256-byte EEPROM page boundaries (WA[7:0] wrap),
      - EEPROM_MAX_WRITE_CHUNK (254 bytes, I2C controller limit).

    After each page write, polls ACK until the EEPROM signals write
    complete (tWR ≤ 3.5 ms).

    I2C sequence per chunk:
        START → dev_addr(W) → WA_high → WA_low → data[0..N-1] → STOP

    Args:
        handle: JTAG handle.
        base:   I2C controller base address.
        offset: 18-bit start offset (0 to 0x3FFFF).
        data:   Bytes to write.
        a2:     Value of the A2 hardware pin (default 0).

    Raises:
        ValueError:   on out-of-range offset or length.
        RuntimeError: on I2C error, NACK, or write-cycle timeout.
    """
    total = len(data)
    _check_bounds(offset, total)

    written    = 0
    cur_offset = offset

    while written < total:
        remaining = total - written

        # Page boundary: WA[7:0] must not wrap within a write
        page_end   = ((cur_offset // EEPROM_PAGE_SIZE) + 1) * EEPROM_PAGE_SIZE
        page_space = page_end - cur_offset

        chunk = min(EEPROM_MAX_WRITE_CHUNK, page_space, remaining)
        chunk_data = data[written: written + chunk]

        dev_addr        = _make_dev_addr(a2, cur_offset)
        wa_high, wa_low = _addr_bytes(cur_offset)

        # Data phase: [WA_high, WA_low, payload...]
        write_payload = bytes([wa_high, wa_low]) + chunk_data

        i2c_driver.i2c_write(handle, base, dev_addr, write_payload)

        # ACK-poll until EEPROM signals write complete
        if not _ack_poll(handle, base, dev_addr):
            raise RuntimeError(
                f"EEPROM write-cycle timeout at 0x{cur_offset:06X} "
                f"(ACK poll exceeded {WRITE_POLL_TIMEOUT_S * 1000:.1f} ms)")

        written    += chunk
        cur_offset += chunk


# ---------------------------------------------------------------------------
# Verify
# ---------------------------------------------------------------------------

def eeprom_verify(handle, base, offset: int, expected: bytes,
                  a2: int = DEFAULT_A2) -> bool:
    """
    Read back EEPROM contents and compare with *expected*.

    Args:
        handle:   JTAG handle.
        base:     I2C controller base address.
        offset:   18-bit start offset.
        expected: Reference data to compare against.
        a2:       Value of the A2 hardware pin (default 0).

    Returns:
        True if contents match, False otherwise.
        Prints the first mismatch offset on failure.

    Raises:
        ValueError:   on out-of-range offset.
        RuntimeError: on I2C error.
    """
    length = len(expected)
    _check_bounds(offset, length)

    readback = eeprom_read(handle, base, offset, length, a2=a2)

    if readback == expected:
        return True

    first_mismatch = None
    mismatch_count = 0
    for i, (got, exp) in enumerate(zip(readback, expected)):
        if got != exp:
            if first_mismatch is None:
                first_mismatch = (i, exp, got)
            mismatch_count += 1

    if first_mismatch is not None:
        i, exp, got = first_mismatch
        print(f"  Verify MISMATCH at 0x{offset + i:06X}: "
              f"expected 0x{exp:02X}, got 0x{got:02X}")
    print(f"  Total mismatches: {mismatch_count}/{length}")
    return False


# ---------------------------------------------------------------------------
# Chip identification
# ---------------------------------------------------------------------------

def eeprom_check(handle, base):
    """
    Probe the EEPROM I2C address range 0x50–0x57.

    The 1010_xxx prefix is the fixed I2C class address for serial EEPROMs;
    bits [2:0] encode A2 / P1 / P0. A BR24G2MFJ-5A populated and powered
    will respond at 4 consecutive addresses (one per P1/P0 combination);
    smaller 24Cxx parts respond at a single address.

    Returns:
        list of addresses that ACKed, or None if no EEPROM was found.
    """
    found = []
    for dev_addr in range(0x50, 0x58):
        if i2c_driver.i2c_probe(handle, base, dev_addr):
            found.append(dev_addr)
    if not found:
        return None
    return found


# ---------------------------------------------------------------------------
# EepromDev — convenience class
# ---------------------------------------------------------------------------

class EepromDev:
    """
    Convenience wrapper binding a JTAG handle and I2C base address.

    Example:
        dev = EepromDev(handle, i2c_driver.CPE_I2C_BASE, a2=0)
        data = dev.read(0x000000, 256)
        dev.write(0x000000, data)
    """

    def __init__(self, handle, i2c_base: int, a2: int = DEFAULT_A2):
        self.handle   = handle
        self.i2c_base = i2c_base
        self.a2       = a2

    def read(self, offset: int, length: int) -> bytes:
        """Read *length* bytes from *offset*."""
        return eeprom_read(self.handle, self.i2c_base,
                           offset, length, a2=self.a2)

    def write(self, offset: int, data: bytes):
        """Write *data* to EEPROM starting at *offset* (page-split + ACK poll)."""
        eeprom_write(self.handle, self.i2c_base,
                     offset, data, a2=self.a2)

    def verify(self, offset: int, expected: bytes) -> bool:
        """Compare EEPROM contents against *expected*. Returns True on match."""
        return eeprom_verify(self.handle, self.i2c_base,
                             offset, expected, a2=self.a2)
