"""
i2c_driver.py — ATCIIC100 I2C Master register-level driver via JTAG
====================================================================
Wraps JTAG register access into I2C bus transactions using the
AndeShape ATCIIC100 I2C Master controller.

Refer to: doc/AndeShape_ATCIIC100_DS091_V1.5.pdf

Usage pattern mirrors reference_code/spi_driver.py.
"""

import time

from . import jtag


# ---------------------------------------------------------------------------
# I2C controller base address
# ---------------------------------------------------------------------------
CPE_I2C_BASE = 0xF0500000


# ---------------------------------------------------------------------------
# ATCIIC100 register offsets (relative to base)
# ---------------------------------------------------------------------------
IIC_REG_IDREV   = 0x00  # ID and Revision
IIC_REG_CFG     = 0x10  # Configuration (FIFO size, etc.)
IIC_REG_INTEN   = 0x14  # Interrupt Enable
IIC_REG_STATUS  = 0x18  # Status (R/W1C for Cmpl, AddrHit)
IIC_REG_ADDR    = 0x1C  # Slave address (7-bit in bits [6:0])
IIC_REG_DATA    = 0x20  # Data FIFO access (byte in bits [7:0])
IIC_REG_CTRL    = 0x24  # Transaction control
IIC_REG_CMD     = 0x28  # Command register
IIC_REG_SETUP   = 0x2C  # Timing and mode setup
IIC_REG_TPM     = 0x30  # Timing Prescaler Multiplier


# ---------------------------------------------------------------------------
# Ctrl register (0x24) bit fields
# ---------------------------------------------------------------------------
IIC_CTRL_PHASE_START_MASK    = 0x00001000  # bit 12: generate START condition
IIC_CTRL_PHASE_START_OFFSET  = 12
IIC_CTRL_PHASE_ADDR_MASK     = 0x00000800  # bit 11: send address byte
IIC_CTRL_PHASE_ADDR_OFFSET   = 11
IIC_CTRL_PHASE_DATA_MASK     = 0x00000400  # bit 10: enable data phase
IIC_CTRL_PHASE_DATA_OFFSET   = 10
IIC_CTRL_PHASE_STOP_MASK     = 0x00000200  # bit 9: generate STOP condition
IIC_CTRL_PHASE_STOP_OFFSET   = 9
IIC_CTRL_DIR_MASK            = 0x00000100  # bit 8: 0=TX, 1=RX
IIC_CTRL_DIR_OFFSET          = 8
IIC_CTRL_DATACNT_MASK        = 0x000000FF  # bits [7:0]: byte count (0=256)
IIC_CTRL_DATACNT_OFFSET      = 0

IIC_CTRL_DIR_TX = 0
IIC_CTRL_DIR_RX = 1


# ---------------------------------------------------------------------------
# Status register (0x18) bit fields
# ---------------------------------------------------------------------------
IIC_STATUS_LINESCL_MASK      = 0x00008000  # bit 15: SCL line state
IIC_STATUS_LINESDA_MASK      = 0x00004000  # bit 14: SDA line state
IIC_STATUS_BUSBUSY_MASK      = 0x00000800  # bit 11: bus busy
IIC_STATUS_ACK_MASK          = 0x00000400  # bit 10: 1=ACK received from slave
IIC_STATUS_CMPL_MASK         = 0x00000200  # bit 9:  transaction complete (R/W1C)
IIC_STATUS_ADDRHIT_MASK      = 0x00000008  # bit 3:  address phase done (R/W1C)
IIC_STATUS_FIFOEMPTY_MASK    = 0x00000001  # bit 0:  1=FIFO empty

IIC_STATUS_ACK_OFFSET        = 10
IIC_STATUS_CMPL_OFFSET       = 9
IIC_STATUS_ADDRHIT_OFFSET    = 3
IIC_STATUS_FIFOEMPTY_OFFSET  = 0


# ---------------------------------------------------------------------------
# Cmd register (0x28) values
# ---------------------------------------------------------------------------
IIC_CMD_NOP       = 0x00  # No operation
IIC_CMD_ISSUE     = 0x01  # Issue I2C transaction
IIC_CMD_CLR_FIFO  = 0x04  # Clear FIFO
IIC_CMD_RESET     = 0x05  # Reset controller


# ---------------------------------------------------------------------------
# Setup register (0x2C) bit fields
# ---------------------------------------------------------------------------
IIC_SETUP_TSUDAT_MASK      = 0x1F000000  # bits [28:24]: SDA setup time
IIC_SETUP_TSP_MASK         = 0x00E00000  # bits [23:21]: spike pulse filter
IIC_SETUP_THDDAT_MASK      = 0x001F0000  # bits [20:16]: SDA hold time
IIC_SETUP_TSCLRATIO_MASK   = 0x00002000  # bit  [13]:    SCL high/low ratio
IIC_SETUP_TSCLHI_MASK      = 0x00001FF0  # bits [12:4]:  SCL high count
IIC_SETUP_DMAEN_MASK       = 0x00000008  # bit  [3]:     DMA enable
IIC_SETUP_MASTER_MASK      = 0x00000004  # bit  [2]:     1=master mode
IIC_SETUP_ADDRESSING_MASK  = 0x00000002  # bit  [1]:     0=7-bit, 1=10-bit
IIC_SETUP_IICEN_MASK       = 0x00000001  # bit  [0]:     1=enable controller

IIC_SETUP_TSUDAT_OFFSET    = 24
IIC_SETUP_TSP_OFFSET       = 21
IIC_SETUP_THDDAT_OFFSET    = 16
IIC_SETUP_TSCLRATIO_OFFSET = 13
IIC_SETUP_TSCLHI_OFFSET    = 4


# ---------------------------------------------------------------------------
# Cfg register (0x10) bit fields
# ---------------------------------------------------------------------------
IIC_CFG_FIFOSIZE_MASK   = 0x00000003  # bits [1:0]
IIC_CFG_FIFOSIZE_OFFSET = 0

# Decode FIFO size from Cfg bits [1:0]
_IIC_FIFO_SIZE_TABLE = {0: 2, 1: 4, 2: 8, 3: 16}


# ---------------------------------------------------------------------------
# Timing defaults
# T_SCLHi = 23 gives ~1 MHz SCL at 48 MHz APB clock (adjust for your SoC).
# Set TPM = 0 (no prescaler multiplication).
# T_SCLRatio = 1 selects non-equal high/low split (fast-mode behaviour).
# ---------------------------------------------------------------------------
IIC_TPM_DEFAULT        = 0
IIC_TSCLHI_DEFAULT     = 23
IIC_TSCLRATIO_DEFAULT  = 1
IIC_TSUDAT_DEFAULT     = 2
IIC_TSP_DEFAULT        = 2
IIC_THDDAT_DEFAULT     = 2

# Poll iteration limits
_CMPL_TIMEOUT  = 500   # iterations waiting for transaction Cmpl
_FIFO_TIMEOUT  = 200   # iterations waiting for FIFO ready

# Retry count for full controller-reset recovery
MAX_RETRIES = 3


# ---------------------------------------------------------------------------
# Low-level register accessors
# ---------------------------------------------------------------------------

def _reg_addr(base, offset):
    return base + offset


def iic_get_idrev(handle, base):
    """Read the controller ID/Revision register."""
    return jtag.jtag_read_reg(handle, _reg_addr(base, IIC_REG_IDREV))


def iic_get_cfg(handle, base):
    return jtag.jtag_read_reg(handle, _reg_addr(base, IIC_REG_CFG))


def iic_get_status(handle, base):
    return jtag.jtag_read_reg(handle, _reg_addr(base, IIC_REG_STATUS))


def iic_set_status(handle, base, value):
    """Write back to Status to clear R/W1C bits (e.g. Cmpl, AddrHit)."""
    jtag.jtag_write_reg(handle, _reg_addr(base, IIC_REG_STATUS), value)


def iic_get_addr(handle, base):
    return jtag.jtag_read_reg(handle, _reg_addr(base, IIC_REG_ADDR))


def iic_set_addr(handle, base, value):
    jtag.jtag_write_reg(handle, _reg_addr(base, IIC_REG_ADDR), value)


def iic_get_data(handle, base):
    return jtag.jtag_read_reg(handle, _reg_addr(base, IIC_REG_DATA))


def iic_set_data(handle, base, value):
    jtag.jtag_write_reg(handle, _reg_addr(base, IIC_REG_DATA), value)


def iic_get_ctrl(handle, base):
    return jtag.jtag_read_reg(handle, _reg_addr(base, IIC_REG_CTRL))


def iic_set_ctrl(handle, base, value):
    jtag.jtag_write_reg(handle, _reg_addr(base, IIC_REG_CTRL), value)


def iic_set_cmd(handle, base, value):
    jtag.jtag_write_reg(handle, _reg_addr(base, IIC_REG_CMD), value)


def iic_get_setup(handle, base):
    return jtag.jtag_read_reg(handle, _reg_addr(base, IIC_REG_SETUP))


def iic_set_setup(handle, base, value):
    jtag.jtag_write_reg(handle, _reg_addr(base, IIC_REG_SETUP), value)


def iic_set_tpm(handle, base, value):
    jtag.jtag_write_reg(handle, _reg_addr(base, IIC_REG_TPM), value)


def iic_set_inten(handle, base, value):
    jtag.jtag_write_reg(handle, _reg_addr(base, IIC_REG_INTEN), value)


# ---------------------------------------------------------------------------
# Helper: read FIFO depth from Cfg register
# ---------------------------------------------------------------------------

# FIFO depth is fixed in hardware; cache per (handle, base) to avoid a JTAG
# read on every transaction. Use id(handle) since SWIG/C handles may not hash.
_fifo_depth_cache = {}


def iic_get_fifo_depth(handle, base):
    """Return the TX/RX FIFO depth in bytes (cached per controller)."""
    key = (id(handle), base)
    depth = _fifo_depth_cache.get(key)
    if depth is not None:
        return depth
    cfg = iic_get_cfg(handle, base)
    code = (cfg >> IIC_CFG_FIFOSIZE_OFFSET) & IIC_CFG_FIFOSIZE_MASK
    depth = _IIC_FIFO_SIZE_TABLE.get(code, 16)
    _fifo_depth_cache[key] = depth
    return depth


# ---------------------------------------------------------------------------
# Helper: dump controller registers (for debugging)
# ---------------------------------------------------------------------------

def dump_regs(handle, base, label=""):
    """Print all ATCIIC100 registers for debugging."""
    _REG_NAMES = {
        IIC_REG_IDREV:  "IDREV  ",
        IIC_REG_CFG:    "CFG    ",
        IIC_REG_INTEN:  "INTEN  ",
        IIC_REG_STATUS: "STATUS ",
        IIC_REG_ADDR:   "ADDR   ",
        IIC_REG_DATA:   "DATA   ",
        IIC_REG_CTRL:   "CTRL   ",
        IIC_REG_CMD:    "CMD    ",
        IIC_REG_SETUP:  "SETUP  ",
        IIC_REG_TPM:    "TPM    ",
    }
    tag = f" [{label}]" if label else ""
    print(f"[I2C REGS @ 0x{base:08X}]{tag}")
    for off, name in sorted(_REG_NAMES.items()):
        val = jtag.jtag_read_reg(handle, _reg_addr(base, off))
        print(f"  +0x{off:02X}  {name}  0x{val:08X}", end="")
        if off == IIC_REG_STATUS:
            ack      = (val >> IIC_STATUS_ACK_OFFSET)      & 1
            cmpl     = (val >> IIC_STATUS_CMPL_OFFSET)     & 1
            busbusy  = (val >> 11)                          & 1
            addrhit  = (val >> IIC_STATUS_ADDRHIT_OFFSET)  & 1
            fifoempty= (val >> IIC_STATUS_FIFOEMPTY_OFFSET) & 1
            print(f"  BusBusy={busbusy} Cmpl={cmpl} ACK={ack} "
                  f"AddrHit={addrhit} FIFOEmpty={fifoempty}", end="")
        elif off == IIC_REG_CTRL:
            ph_start = (val >> IIC_CTRL_PHASE_START_OFFSET) & 1
            ph_addr  = (val >> IIC_CTRL_PHASE_ADDR_OFFSET)  & 1
            ph_data  = (val >> IIC_CTRL_PHASE_DATA_OFFSET)  & 1
            ph_stop  = (val >> IIC_CTRL_PHASE_STOP_OFFSET)  & 1
            direction= (val >> IIC_CTRL_DIR_OFFSET)          & 1
            datacnt  = val & IIC_CTRL_DATACNT_MASK
            print(f"  Start={ph_start} Addr={ph_addr} Data={ph_data} "
                  f"Stop={ph_stop} Dir={'RX' if direction else 'TX'} "
                  f"DataCnt={datacnt if datacnt else 256}", end="")
        print()


# ---------------------------------------------------------------------------
# Controller init and reset
# ---------------------------------------------------------------------------

def i2c_reset_controller(handle, base):
    """Issue a software reset to the ATCIIC100 controller."""
    print("  >> Resetting I2C controller ...")
    iic_set_cmd(handle, base, IIC_CMD_RESET)
    time.sleep(0.001)
    # Clear FIFO after reset
    iic_set_cmd(handle, base, IIC_CMD_CLR_FIFO)
    print("  >> I2C controller reset done")


def i2c_init(handle, base,
             tsclhi=IIC_TSCLHI_DEFAULT,
             tsclratio=IIC_TSCLRATIO_DEFAULT,
             tsudat=IIC_TSUDAT_DEFAULT,
             tsp=IIC_TSP_DEFAULT,
             thddat=IIC_THDDAT_DEFAULT,
             tpm=IIC_TPM_DEFAULT):
    """
    Initialize the ATCIIC100 I2C Master controller.

    Configures master mode, 7-bit addressing, polled (no interrupt/DMA)
    operation, and SCL timing. Call once before any I2C transactions.

    Timing defaults target ~1 MHz at 48 MHz APB clock; adjust tsclhi
    for other APB frequencies:
        f_SCL ≈ f_APB / (2 * (tsclhi + 1) * (tpm + 1))

    Args:
        handle:     JTAG handle from open_connection().
        base:       I2C controller base address (e.g. CPE_I2C_BASE).
        tsclhi:     SCL high-period count (bits [12:4] of Setup).
        tsclratio:  SCL high/low ratio select (bit 13 of Setup).
        tsudat:     SDA setup time (bits [28:24] of Setup).
        tsp:        Spike-filter width (bits [23:21] of Setup).
        thddat:     SDA hold time (bits [20:16] of Setup).
        tpm:        Timing prescaler multiplier (TPM register).
    """
    # 1. Reset and disable controller
    iic_set_cmd(handle, base, IIC_CMD_RESET)
    time.sleep(0.001)

    # 2. Disable all interrupts — use polling throughout
    iic_set_inten(handle, base, 0x00000000)

    # 3. Configure Setup: Master, 7-bit, timing, enable
    setup = (
        ((tsudat   & 0x1F) << IIC_SETUP_TSUDAT_OFFSET)   |
        ((tsp      & 0x07) << IIC_SETUP_TSP_OFFSET)       |
        ((thddat   & 0x1F) << IIC_SETUP_THDDAT_OFFSET)    |
        ((tsclratio & 0x1) << IIC_SETUP_TSCLRATIO_OFFSET) |
        ((tsclhi   & 0x1FF) << IIC_SETUP_TSCLHI_OFFSET)   |
        IIC_SETUP_MASTER_MASK   |   # master mode
        # IIC_SETUP_ADDRESSING_MASK = 0 → 7-bit addressing
        IIC_SETUP_IICEN_MASK        # enable controller
    )
    iic_set_setup(handle, base, setup)

    # 4. Set TPM (prescaler multiplier)
    iic_set_tpm(handle, base, tpm & 0x1F)

    # 5. Clear FIFO
    iic_set_cmd(handle, base, IIC_CMD_CLR_FIFO)

    # 6. Warm FIFO-depth cache so the hot path skips the CFG read
    _fifo_depth_cache.pop((id(handle), base), None)
    iic_get_fifo_depth(handle, base)

    print(f"[i2c_init] base=0x{base:08X}  "
          f"setup=0x{setup:08X}  tpm={tpm}  tsclhi={tsclhi}")


# ---------------------------------------------------------------------------
# Internal: build Ctrl register value
# ---------------------------------------------------------------------------

def _build_ctrl(phase_start, phase_stop, direction, data_count):
    """
    Build the Ctrl register value.

    data_count: number of data-phase bytes; pass 0 to disable data phase.
                Maximum is 256 (encoded as datacnt=0).
    """
    has_data = data_count > 0
    datacnt  = data_count & 0xFF   # 0 encodes 256
    return (
        ((1 if phase_start else 0) << IIC_CTRL_PHASE_START_OFFSET) |
        (1 << IIC_CTRL_PHASE_ADDR_OFFSET)                          |
        ((1 if has_data   else 0) << IIC_CTRL_PHASE_DATA_OFFSET)   |
        ((1 if phase_stop else 0) << IIC_CTRL_PHASE_STOP_OFFSET)   |
        ((direction & 0x1)       << IIC_CTRL_DIR_OFFSET)           |
        (datacnt & IIC_CTRL_DATACNT_MASK)
    )


# ---------------------------------------------------------------------------
# Internal: wait for transaction Cmpl
# ---------------------------------------------------------------------------

def _wait_cmpl(handle, base, timeout=_CMPL_TIMEOUT):
    """
    Poll Status.Cmpl until set, then clear it (R/W1C).
    Returns the Status register value at completion.
    Raises RuntimeError on timeout.
    """
    for _ in range(timeout):
        st = iic_get_status(handle, base)
        if st & IIC_STATUS_CMPL_MASK:
            # Clear Cmpl and AddrHit (both R/W1C)
            iic_set_status(handle, base,
                           IIC_STATUS_CMPL_MASK | IIC_STATUS_ADDRHIT_MASK)
            return st
    # Timeout — dump state for debug
    st = iic_get_status(handle, base)
    ctrl = iic_get_ctrl(handle, base)
    print(f"  I2C TIMEOUT DUMP:")
    print(f"    Status(0x18) = 0x{st:08X}  "
          f"BusBusy={(st>>11)&1} Cmpl={(st>>9)&1} "
          f"ACK={(st>>10)&1} AddrHit={(st>>3)&1} FIFOEmpty={st&1}")
    print(f"    Ctrl  (0x24) = 0x{ctrl:08X}")
    raise RuntimeError(f"I2C timeout waiting for Cmpl (Status=0x{st:08X})")


# ---------------------------------------------------------------------------
# Internal: execute a single write transaction (no retry)
# ---------------------------------------------------------------------------

def _exec_write_once(handle, base, dev_addr_7bit, data,
                     phase_start, phase_stop, fifo_depth):
    """
    Execute one I2C write transaction.
    Streams *data* bytes through the TX FIFO.
    Raises RuntimeError on FIFO underrun timeout or Cmpl timeout.
    """
    total = len(data)

    # Clear status flags and FIFO
    iic_set_status(handle, base,
                   IIC_STATUS_CMPL_MASK | IIC_STATUS_ADDRHIT_MASK)
    iic_set_cmd(handle, base, IIC_CMD_CLR_FIFO)

    # Set slave address (7-bit, controller appends R/W bit)
    iic_set_addr(handle, base, dev_addr_7bit & 0x7F)

    # Configure Ctrl
    ctrl = _build_ctrl(phase_start, phase_stop,
                       IIC_CTRL_DIR_TX, total)
    iic_set_ctrl(handle, base, ctrl)

    # Fill FIFO with initial chunk then issue command
    offset = 0
    if total > 0:
        chunk = min(fifo_depth, total)
        for i in range(chunk):
            iic_set_data(handle, base, data[offset + i] & 0xFF)
        offset += chunk

    iic_set_cmd(handle, base, IIC_CMD_ISSUE)

    # Stream remaining bytes: wait for FIFO empty, then refill
    while offset < total:
        for _ in range(_FIFO_TIMEOUT):
            st = iic_get_status(handle, base)
            if st & IIC_STATUS_FIFOEMPTY_MASK:
                break
        else:
            raise RuntimeError(
                f"TX FIFO timeout at offset {offset}/{total}, "
                f"dev_addr=0x{dev_addr_7bit:02X}")

        chunk = min(fifo_depth, total - offset)
        for i in range(chunk):
            iic_set_data(handle, base, data[offset + i] & 0xFF)
        offset += chunk

    return _wait_cmpl(handle, base)


# ---------------------------------------------------------------------------
# Internal: execute a single read transaction (no retry)
# ---------------------------------------------------------------------------

def _exec_read_once(handle, base, dev_addr_7bit, length,
                    phase_start, phase_stop, fifo_depth):
    """
    Execute one I2C read transaction.
    Drains the RX FIFO in batches of up to *fifo_depth* bytes per status
    poll. Safe because the controller stretches SCL when the FIFO is
    full, so after FIFOEMPTY=0 the FIFO holds at least one byte and
    will refill while we drain (never underrunning into garbage as long
    as we don't exceed fifo_depth between status checks).
    Raises RuntimeError on FIFO timeout or Cmpl timeout.
    """
    if length == 0:
        return b''

    # Clear status flags and FIFO
    iic_set_status(handle, base,
                   IIC_STATUS_CMPL_MASK | IIC_STATUS_ADDRHIT_MASK)
    iic_set_cmd(handle, base, IIC_CMD_CLR_FIFO)

    # Set slave address
    iic_set_addr(handle, base, dev_addr_7bit & 0x7F)

    # Configure Ctrl for RX
    ctrl = _build_ctrl(phase_start, phase_stop,
                       IIC_CTRL_DIR_RX, length)
    iic_set_ctrl(handle, base, ctrl)

    # Issue command — controller starts clocking in data
    iic_set_cmd(handle, base, IIC_CMD_ISSUE)

    # Drain RX FIFO in batches: 1 status poll per batch, up to fifo_depth
    # data reads. Cuts JTAG round-trips ~fifo_depth× vs per-byte polling.
    received = bytearray()
    while len(received) < length:
        for _ in range(_FIFO_TIMEOUT):
            st = iic_get_status(handle, base)
            if (st & IIC_STATUS_FIFOEMPTY_MASK) == 0:   # data in FIFO
                break
        else:
            raise RuntimeError(
                f"RX FIFO timeout at byte {len(received)}/{length}, "
                f"dev_addr=0x{dev_addr_7bit:02X}")

        batch = min(fifo_depth, length - len(received))
        for _ in range(batch):
            received.append(iic_get_data(handle, base) & 0xFF)

    _wait_cmpl(handle, base)
    return bytes(received)


# ---------------------------------------------------------------------------
# Internal: write with SPIRST-level retry
# ---------------------------------------------------------------------------

def _exec_write(handle, base, dev_addr_7bit, data,
                phase_start=True, phase_stop=True):
    """
    Execute a write transaction with controller-reset recovery on failure.
    Returns the final Status register value.
    """
    fifo_depth = iic_get_fifo_depth(handle, base)

    for attempt in range(1, MAX_RETRIES + 1):
        try:
            return _exec_write_once(handle, base, dev_addr_7bit, data,
                                    phase_start, phase_stop, fifo_depth)
        except RuntimeError as e:
            print(f"  ERROR: {e} (attempt {attempt}/{MAX_RETRIES})")
            if attempt < MAX_RETRIES:
                i2c_reset_controller(handle, base)
                i2c_init(handle, base)
                print(f"  >> Retrying ...")
            else:
                print(f"  FATAL: All {MAX_RETRIES} attempts failed")
                raise


def _exec_read(handle, base, dev_addr_7bit, length,
               phase_start=True, phase_stop=True):
    """
    Execute a read transaction with controller-reset recovery on failure.
    Returns received bytes.
    """
    fifo_depth = iic_get_fifo_depth(handle, base)

    for attempt in range(1, MAX_RETRIES + 1):
        try:
            return _exec_read_once(handle, base, dev_addr_7bit, length,
                                   phase_start, phase_stop, fifo_depth)
        except RuntimeError as e:
            print(f"  ERROR: {e} (attempt {attempt}/{MAX_RETRIES})")
            if attempt < MAX_RETRIES:
                i2c_reset_controller(handle, base)
                i2c_init(handle, base)
                print(f"  >> Retrying ...")
            else:
                print(f"  FATAL: All {MAX_RETRIES} attempts failed")
                raise


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def i2c_write(handle, base, dev_addr_7bit, data: bytes):
    """
    Send an I2C write transaction: START + dev_addr(W) + data + STOP.

    Args:
        handle:         JTAG handle.
        base:           I2C controller base address.
        dev_addr_7bit:  7-bit slave address (without R/W bit).
        data:           Bytes to transmit (may be empty for address-only probe).

    Raises:
        RuntimeError: on NACK, timeout, or controller error.
    """
    st = _exec_write(handle, base, dev_addr_7bit, data)
    if not (st & IIC_STATUS_ACK_MASK):
        raise RuntimeError(
            f"I2C NACK: device 0x{dev_addr_7bit:02X} did not acknowledge "
            f"(Status=0x{st:08X})")


def i2c_read(handle, base, dev_addr_7bit, length: int) -> bytes:
    """
    Send an I2C read transaction: START + dev_addr(R) + recv length bytes + STOP.

    Args:
        handle:         JTAG handle.
        base:           I2C controller base address.
        dev_addr_7bit:  7-bit slave address (without R/W bit).
        length:         Number of bytes to receive (1–256).

    Returns:
        Received bytes (length bytes).

    Raises:
        RuntimeError: on NACK, timeout, or controller error.
    """
    return _exec_read(handle, base, dev_addr_7bit, length)


def i2c_write_read(handle, base, dev_addr_7bit,
                   wr_data: bytes, rd_len: int) -> bytes:
    """
    Combined write-then-read with a repeated START (no STOP between phases).

    Sequence:  START → dev_addr(W) → wr_data → rSTART → dev_addr(R)
               → rd_len bytes → STOP

    Used for random-address reads from I2C EEPROMs.

    Args:
        handle:         JTAG handle.
        base:           I2C controller base address.
        dev_addr_7bit:  7-bit slave address.
        wr_data:        Bytes to write (e.g. memory address bytes).
        rd_len:         Number of bytes to read back.

    Returns:
        Received bytes (rd_len bytes).

    Raises:
        RuntimeError: on NACK, timeout, or controller error.
    """
    # Write phase — suppress STOP so bus remains owned
    st = _exec_write(handle, base, dev_addr_7bit, wr_data,
                     phase_start=True, phase_stop=False)
    if not (st & IIC_STATUS_ACK_MASK):
        raise RuntimeError(
            f"I2C NACK during write phase of write_read, "
            f"dev_addr=0x{dev_addr_7bit:02X} (Status=0x{st:08X})")

    # Read phase — repeated START, then normal STOP
    return _exec_read(handle, base, dev_addr_7bit, rd_len,
                      phase_start=True, phase_stop=True)


def i2c_probe(handle, base, dev_addr_7bit) -> bool:
    """
    Probe a device by sending its address and checking for ACK.

    Sends: START + dev_addr(W) + STOP (no data bytes).

    Args:
        handle:         JTAG handle.
        base:           I2C controller base address.
        dev_addr_7bit:  7-bit slave address to probe.

    Returns:
        True if the device acknowledged (ACK), False if NACK.
    """
    # Address-only transaction: Phase_data = 0
    ctrl = _build_ctrl(phase_start=True, phase_stop=True,
                       direction=IIC_CTRL_DIR_TX, data_count=0)

    iic_set_status(handle, base,
                   IIC_STATUS_CMPL_MASK | IIC_STATUS_ADDRHIT_MASK)
    iic_set_cmd(handle, base, IIC_CMD_CLR_FIFO)
    iic_set_addr(handle, base, dev_addr_7bit & 0x7F)
    iic_set_ctrl(handle, base, ctrl)
    iic_set_cmd(handle, base, IIC_CMD_ISSUE)

    try:
        st = _wait_cmpl(handle, base)
        return bool(st & IIC_STATUS_ACK_MASK)
    except RuntimeError:
        return False
