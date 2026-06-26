import struct
import time

from . import jtag
from . import spi_driver as spi


# ---------------------------------------------------------------------------
# Flash command opcodes (MX25S1633F)
# ---------------------------------------------------------------------------
SPIROM_OP_RDID      = 0x9F
SPIROM_OP_READ      = 0x03
SPIROM_OP_FAST_READ = 0x0B
SPIROM_OP_PP        = 0x02
SPIROM_OP_SE        = 0x20
SPIROM_OP_BE        = 0x52
SPIROM_OP_CE        = 0x60
SPIROM_OP_WREN      = 0x06
SPIROM_OP_WRDI      = 0x04
SPIROM_OP_RDSR      = 0x05
SPIROM_OP_WRSR      = 0x01
SPIROM_OP_SBLK      = 0x36
SPIROM_OP_SBULK     = 0x39
SPIROM_OP_ULBPR     = 0x98     # SST26WF: Global Block Protection Unlock

# Flash geometry
SECTOR_SIZE = 0x1000      # 4 KiB (SPIROM_OP_SE)
BLOCK_SIZE  = 0x8000      # 32 KiB (SPIROM_OP_BE)
PAGE_SIZE   = 0x100       # 256 B

# Status register bits
SR_WIP = 0x01
SR_WEL = 0x02

# TX FIFO capacity (Config Reg 0x7C = 0x1044: TxFIFOSize=0x4 -> 32 words = 128 bytes)
TX_FIFO_BYTES = 128

# Layer 2 retry count
MAX_RETRIES = 3

# Layer 1 FIFO fence parameters
FENCE_MAX_READS   = 50
FENCE_MAX_REFILLS = 2


# ---------------------------------------------------------------------------
# Layer 2 helper: full SPI controller reset
# ---------------------------------------------------------------------------

def spi_reset_controller(handle, spi_base):
    """SPIRST -> wait for auto-clear -> spi_init"""
    print("  >> Resetting SPI controller (SPIRST) ...")
    ctrl = spi.spi_get_ctrl(handle, spi_base)
    ctrl |= spi.SPI_CTRL_SPIRST_MASK
    spi.spi_set_ctrl(handle, spi_base, ctrl)

    for _ in range(1000):
        ctrl = spi.spi_get_ctrl(handle, spi_base)
        if (ctrl & spi.SPI_CTRL_SPIRST_MASK) == 0:
            break
    else:
        print("  >> WARNING: SPIRST did not auto-clear")

    spi.spi_init(handle, spi_base)
    print("  >> SPI controller reset and re-initialized")


# ---------------------------------------------------------------------------
# Layer 1 helper: FIFO fence — confirm TX FIFO data landed
# ---------------------------------------------------------------------------

def _fifo_fence(handle, spi_base, write_data, cmd, offset):
    """
    Verify TX FIFO word count after fill.
    On first failure: clr_fifo + refill and retry once (Layer 1 lightweight recovery).
    Returns True on success, False if both attempts fail.
    """
    expected_words = (len(write_data) + 3) // 4
    offset_str = f"0x{offset:08X}" if offset is not None else "N/A"

    for refill in range(FENCE_MAX_REFILLS):
        if refill > 0:
            print(f"  FIFO fence miss, retrying fill "
                  f"(attempt {refill + 1}/{FENCE_MAX_REFILLS}) ...")
            spi.spi_clr_fifo(handle, spi_base)
            spi.spi_tx_data(handle, spi_base, write_data)

        tx_num = 0
        for _ in range(FENCE_MAX_READS):
            status = jtag.jtag_read_reg(handle, spi_base + 0x34)
            tx_num = ((status >> 16) & 0x3F) | (((status >> 28) & 0x3) << 6)
            if tx_num == expected_words:
                return True

        print(f"  FIFO fence: tx_num={tx_num}, expected={expected_words}, "
              f"cmd=0x{cmd:02X}, addr={offset_str}")

    return False


# ---------------------------------------------------------------------------
# Layer 1: single SPI command execution
# ---------------------------------------------------------------------------

def _exec_cmd_once(handle, spi_base, cmd, offset, write_data, read_count,
                   has_write, has_read, trans_mode, wcnt, rcnt):
    """
    Execute one SPI command.
    Raises RuntimeError on FIFO fence failure or transfer timeout;
    upper layer _exec_cmd handles SPIRST-level recovery.
    """

    spi.spi_clr_fifo(handle, spi_base)
    if has_write:
        spi.spi_tx_data(handle, spi_base, write_data)

        if not _fifo_fence(handle, spi_base, write_data, cmd, offset):
            spi.spi_clr_fifo(handle, spi_base)
            raise RuntimeError(
                f"TX FIFO fence failed after {FENCE_MAX_REFILLS} fills, "
                f"cmd=0x{cmd:02X}, "
                f"addr={'0x{:08X}'.format(offset) if offset is not None else 'N/A'}")

    if offset is not None:
        spi.spi_set_addr(handle, spi_base, offset)

    trans_ctrl = spi.spi_prepare_transctrl(
        cmden=1,
        addren=1 if offset is not None else 0,
        trans_mode=trans_mode,
        wcnt=wcnt, dummy_cnt=0, rcnt=rcnt
    )
    spi.spi_set_transctrl(handle, spi_base, trans_ctrl)

    spi.spi_set_cmd(handle, spi_base, cmd)
    ret = spi.spi_wait_spi(handle, spi_base)
    if ret != 0:
        raise RuntimeError(
            f"SPI transfer timeout! cmd=0x{cmd:02X}, "
            f"addr={'0x{:08X}'.format(offset) if offset is not None else 'N/A'}, "
            f"wcnt={wcnt}, rcnt={rcnt}")

    if has_read:
        return spi.spi_rx_data(handle, spi_base, read_count)
    return None


# ---------------------------------------------------------------------------
# Layer 2: SPI command execution with auto-retry
# ---------------------------------------------------------------------------

def _exec_cmd(handle, spi_base, cmd, offset=None, write_data=None, read_count=0):
    """
    Execute one SPI command; on failure do SPIRST + spi_init reset and retry
    up to MAX_RETRIES times.
    """
    has_write = bool(write_data and len(write_data) > 0)
    has_read = read_count > 0

    if has_write and has_read:
        trans_mode = 0x3
    elif has_write:
        trans_mode = 0x1
    elif has_read:
        trans_mode = 0x2
    else:
        trans_mode = 0x7

    wcnt = len(write_data) - 1 if has_write else 0
    rcnt = read_count - 1 if has_read else 0

    for attempt in range(1, MAX_RETRIES + 1):
        try:
            return _exec_cmd_once(handle, spi_base, cmd, offset,
                                  write_data, read_count,
                                  has_write, has_read, trans_mode, wcnt, rcnt)
        except RuntimeError as e:
            print(f"ERROR: {e} (attempt {attempt}/{MAX_RETRIES})")
            if attempt < MAX_RETRIES:
                spi_reset_controller(handle, spi_base)
                print(f"  >> Retrying ...")
            else:
                print(f"FATAL: All {MAX_RETRIES} attempts failed")
                raise


# ---------------------------------------------------------------------------
# Flash status
# ---------------------------------------------------------------------------

def flash_read_status(handle, spi_base):
    data = _exec_cmd(handle, spi_base, SPIROM_OP_RDSR, read_count=1)
    return data[0] if data else 0xFF


def dump_flash_status(handle, spi_base, label=""):
    """Read and decode the flash status register for debugging."""
    sr = flash_read_status(handle, spi_base)
    tag = f" [{label}]" if label else ""
    print(f"[Flash SR{tag}]  raw=0x{sr:02X} (0b{sr:08b})")
    print(f"  [7] SRWD={( sr>>7)&1}  [6] QE  ={( sr>>6)&1}"
          f"  [5] BP3 ={( sr>>5)&1}  [4] BP2 ={( sr>>4)&1}")
    print(f"  [3] BP1 ={( sr>>3)&1}  [2] BP0 ={( sr>>2)&1}"
          f"  [1] WEL ={( sr>>1)&1}  [0] WIP ={( sr>>0)&1}")
    if sr == 0x00:
        print("  -> Idle / unprogrammed state  (WIP=0 WEL=0 BP=0)")
    elif sr == 0xFF:
        print("  -> 0xFF: Flash not responding or MISO floating")
    if (sr >> 0) & 1:
        print("  -> WIP=1: write/erase in progress")
    if (sr >> 2) & 0xF:
        print(f"  -> BP[3:0]={(sr>>2)&0xF:#x}: block-protect bits set")


def flash_wait_ready(handle, spi_base, timeout=1000):
    for i in range(timeout):
        sr = flash_read_status(handle, spi_base)
        if (sr & SR_WIP) == 0:
            return True
        time.sleep(0.001)
    print(f"WARNING: flash_wait_ready timeout after {timeout} iterations")
    return False


def flash_write_enable(handle, spi_base):
    _exec_cmd(handle, spi_base, SPIROM_OP_WREN)
    sr = flash_read_status(handle, spi_base)
    if (sr & SR_WEL) == 0:
        print(f"WARNING: WREN sent but WEL not set, SR=0x{sr:02X}")


# ---------------------------------------------------------------------------
# Flash ID check
# ---------------------------------------------------------------------------

def flash_check(handle, spi_base):
    data = _exec_cmd(handle, spi_base, SPIROM_OP_RDID, read_count=3)
    if not data or len(data) < 3:
        print("ERROR: RDID returned no data")
        return {'valid': False, 'mfr': 0, 'type': 0, 'density': 0, 'id': 0}

    mfr, mtype, mdens = data[0], data[1], data[2]
    jid = mfr | (mtype << 8) | (mdens << 16)
    is_valid = mfr not in (0x00, 0xFF)
    print(f"Flash JEDEC ID: Manufacturer=0x{mfr:02X}, "
            f"Type=0x{mtype:02X}, Density=0x{mdens:02X}")
    print(f"(Combined: 0x{jid:06X})")
    if not is_valid:
        print("ERROR: Invalid manufacturer ID")
    return {'valid': is_valid, 'mfr': mfr, 'type': mtype, 'density': mdens, 'id': jid}


# ---------------------------------------------------------------------------
# Erase (Sector 4KB + Block 32KB mixed)
# ---------------------------------------------------------------------------

def flash_erase(handle, spi_base, offset, size):
    start = offset - (offset % SECTOR_SIZE)
    end = offset + size
    end = ((end + SECTOR_SIZE - 1) // SECTOR_SIZE) * SECTOR_SIZE

    block_start = ((start + BLOCK_SIZE - 1) // BLOCK_SIZE) * BLOCK_SIZE
    block_end   = (end // BLOCK_SIZE) * BLOCK_SIZE
    if block_start >= block_end:
        # 区段太小或未跨 32KB block 边界，全部走 SE
        block_start = block_end = start

    total_sectors = (end - start) // SECTOR_SIZE
    total_blocks  = (block_end - block_start) // BLOCK_SIZE
    head_sectors  = (block_start - start) // SECTOR_SIZE
    tail_sectors  = (end - block_end) // SECTOR_SIZE
    print(f"Erasing from 0x{start:08X} to 0x{end:08X} "
          f"({total_sectors} sectors total -> "
          f"{head_sectors} SE + {total_blocks} BE + {tail_sectors} SE) ... ")

    def _erase_one(addr, opcode, timeout_ms):
        flash_write_enable(handle, spi_base)
        _exec_cmd(handle, spi_base, opcode, offset=addr)
        if not flash_wait_ready(handle, spi_base, timeout=timeout_ms):
            print(f"ERROR: Erase timeout at 0x{addr:08X} (opcode=0x{opcode:02X})")
            return False
        return True

    for s in range(start, block_start, SECTOR_SIZE):
        if not _erase_one(s, SPIROM_OP_SE, 5000):
            return 1
    for b in range(block_start, block_end, BLOCK_SIZE):
        # MX25S1633F tBE typ 1.5s / max 4s; 留宽裕到 30s
        if not _erase_one(b, SPIROM_OP_BE, 30000):
            return 1
    for s in range(block_end, end, SECTOR_SIZE):
        if not _erase_one(s, SPIROM_OP_SE, 5000):
            return 1

    print("Erase complete.")
    return 0


# ---------------------------------------------------------------------------
# Page Program
# ---------------------------------------------------------------------------

def flash_program(handle, spi_base, offset, data_bytes):
    total = len(data_bytes)
    written = 0

    print(f"Programming {total} bytes at 0x{offset:08X} ... ")

    while written < total:
        cur_offset = offset + written
        remaining = total - written

        page_end = ((cur_offset // PAGE_SIZE) + 1) * PAGE_SIZE
        max_in_page = page_end - cur_offset
        chunk = min(TX_FIFO_BYTES, max_in_page, remaining)
        chunk_data = data_bytes[written:written + chunk]

        flash_write_enable(handle, spi_base)
        _exec_cmd(handle, spi_base, SPIROM_OP_PP,
                  offset=cur_offset, write_data=chunk_data)

        if not flash_wait_ready(handle, spi_base, timeout=2000):
            print(f"ERROR: Program timeout at 0x{cur_offset:08X}")
            return 1

        written += chunk

    print("Program complete.")
    return 0


# ---------------------------------------------------------------------------
# Read Data
# ---------------------------------------------------------------------------

def flash_read(handle, spi_base, offset, length):
    data = bytearray()
    remaining = length
    cur_offset = offset

    while remaining > 0:
        chunk = min(TX_FIFO_BYTES, remaining)
        resp = _exec_cmd(handle, spi_base, SPIROM_OP_READ,
                         offset=cur_offset, read_count=chunk)
        data.extend(resp[:chunk])
        cur_offset += chunk
        remaining -= chunk

    return bytes(data)


# ---------------------------------------------------------------------------
# Public wrapper for external callers
# ---------------------------------------------------------------------------

def f_exec_cmd(handle, spi_base, cmd, offset=None, write_data=None, read_count=0):
    return _exec_cmd(handle, spi_base, cmd, offset, write_data, read_count)


# ---------------------------------------------------------------------------
# Per-flash unlock quirks
#
# Some parts ship with a default protection state that blocks all erase/program
# until explicitly unlocked. This is independent of the standard SR BP bits and
# cannot be observed/cleared with the generic driver path. Each quirk function
# below is invoked once per Flash() construction when its JEDEC ID is matched.
# ---------------------------------------------------------------------------

def _unlock_sst26wf(handle, spi_base):
    """SST26WF040B (and family): an independent Block Protection Register (BPR)
    protects the whole device on power-up. The standard SR shows 0x00 (BP bits
    do not reflect BPR), so any WREN sets WEL=1 but the subsequent erase/PP is
    silently blocked and WEL stays latched. ULBPR (0x98) globally clears BPR.

    BPR is volatile -> we must reapply on every power-up; running ULBPR when
    already unlocked is harmless, so we do it unconditionally.
    """
    sr_before = flash_read_status(handle, spi_base)
    _exec_cmd(handle, spi_base, SPIROM_OP_WREN)
    _exec_cmd(handle, spi_base, SPIROM_OP_ULBPR)
    flash_wait_ready(handle, spi_base, timeout=100)
    sr_after = flash_read_status(handle, spi_base)
    print(f"[quirk] SST26WF: ULBPR issued. "
          f"SR 0x{sr_before:02X} -> 0x{sr_after:02X}")


def _unlock_atmel_swp(handle, spi_base):
    """AT25DF021A (Adesto/Atmel): SR bits[3:2] = SWP default to 0b11 at power-up
    (entire device software-protected). WRSR 0x00 clears SWP (and SPRL, which
    is fine on a dev tool — SPRL is normally 0 anyway).

    SR is non-volatile here, so check first and skip the write when already 0
    to avoid unnecessary NV wear.
    """
    sr = flash_read_status(handle, spi_base)
    SWP_MASK = 0x0C
    if (sr & SWP_MASK) == 0:
        print(f"[quirk] Adesto SWP: already unlocked (SR=0x{sr:02X}), skipping")
        return
    _exec_cmd(handle, spi_base, SPIROM_OP_WREN)
    _exec_cmd(handle, spi_base, SPIROM_OP_WRSR, write_data=bytes([0x00]))
    flash_wait_ready(handle, spi_base, timeout=100)
    sr_after = flash_read_status(handle, spi_base)
    print(f"[quirk] Adesto SWP: WRSR 0x00 issued. "
          f"SR 0x{sr:02X} -> 0x{sr_after:02X}")


def apply_quirks(handle, spi_base, mfr, mtype, density):
    key = (mfr, mtype, density)
    if key == (0xBF, 0x26, 0x54):       # SST26WF040B
        print(f"[quirk] JEDEC 0x{mfr:02X}{mtype:02X}{density:02X} matched, "
              f"applying unlock quirk")
        _unlock_sst26wf(handle, spi_base)
    elif key == (0x1F, 0x43, 0x01):     # AT25DF021A
        print(f"[quirk] JEDEC 0x{mfr:02X}{mtype:02X}{density:02X} matched, "
              f"applying unlock quirk")
        _unlock_atmel_swp(handle, spi_base)


# ---------------------------------------------------------------------------
# FlashDev
# ---------------------------------------------------------------------------

class FlashDev:
    def __init__(self, handle, spi_base):
        self.handle = handle
        self.spi_base = spi_base

    def check(self):
        return flash_check(self.handle, self.spi_base)

    def erase(self, offset, size):
        return flash_erase(self.handle, self.spi_base, offset, size)

    def program(self, offset, data_bytes):
        return flash_program(self.handle, self.spi_base, offset, data_bytes)

    def read(self, offset, length):
        return flash_read(self.handle, self.spi_base, offset, length)

    def read_status(self):
        return flash_read_status(self.handle, self.spi_base)

    def write_enable(self):
        flash_write_enable(self.handle, self.spi_base)
