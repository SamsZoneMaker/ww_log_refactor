import struct

from . import jtag

# ---------------------------------------------------------------------------
# Register base addresses (same as in platform.c)
# ---------------------------------------------------------------------------
REG_SMU_BASE = 0xd0400000
REG_SMU_BASE_16 = 0x00f01000
CPE_SPI_BASE = 0xF0100000

# ---------------------------------------------------------------------------
# SPI register offsets (relative to spi_base)
# ---------------------------------------------------------------------------
SPI_REG_VER = 0x0
SPI_REG_TRANSFMT = 0x10
SPI_REG_DIRECTIO = 0x14
SPI_REG_TRANSCTRL = 0x20
SPI_REG_CMD = 0x24
SPI_REG_ADDR = 0x28
SPI_REG_DATA = 0x2c
SPI_REG_CTRL = 0x30
SPI_REG_STATUS = 0x34
SPI_REG_INTREN = 0x38
SPI_REG_INTRST = 0x3c
SPI_REG_TIMING = 0x40

# ---------------------------------------------------------------------------
# Bit masks and offsets (identical to platform.c)
# ---------------------------------------------------------------------------
SPI_TRANSCTRL_CMDEN_MASK = 0x40000000
SPI_TRANSCTRL_ADDREN_MASK = 0x20000000
SPI_TRANSCTRL_TRANSMODE_MASK = 0x0f000000
SPI_TRANSCTRL_WCNT_MASK = 0x001ff000
SPI_TRANSCTRL_DUMMYCNT_MASK = 0x00000600
SPI_TRANSCTRL_RCNT_MASK = 0x000001ff

SPI_TRANSCTRL_CMDEN_OFFSET = 30
SPI_TRANSCTRL_ADDREN_OFFSET = 29
SPI_TRANSCTRL_TRANSMODE_OFFSET = 24
SPI_TRANSCTRL_WCNT_OFFSET = 12
SPI_TRANSCTRL_DUMMYCNT_OFFSET = 9
SPI_TRANSCTRL_RCNT_OFFSET = 0

# TRANSFMT (0x10) bit fields
SPI_TRANSFMT_ADDRLEN_MASK     = 0x00030000
SPI_TRANSFMT_DATALEN_MASK     = 0x00001F00
SPI_TRANSFMT_DATAMERGE_MASK   = 0x00000080
SPI_TRANSFMT_MOSIBIDIR_MASK   = 0x00000010
SPI_TRANSFMT_LSB_MASK         = 0x00000008
SPI_TRANSFMT_SLVMODE_MASK     = 0x00000004
SPI_TRANSFMT_CPOL_MASK        = 0x00000002
SPI_TRANSFMT_CPHA_MASK        = 0x00000001

SPI_TRANSFMT_ADDRLEN_OFFSET   = 16
SPI_TRANSFMT_DATALEN_OFFSET   = 8
SPI_TRANSFMT_DATAMERGE_OFFSET = 7
SPI_TRANSFMT_MOSIBIDIR_OFFSET = 4
SPI_TRANSFMT_LSB_OFFSET       = 3
SPI_TRANSFMT_SLVMODE_OFFSET   = 2
SPI_TRANSFMT_CPOL_OFFSET      = 1
SPI_TRANSFMT_CPHA_OFFSET      = 0

# TIMING (0x40) bit fields
SPI_TIMING_CS2SCLK_MASK       = 0x00003000
SPI_TIMING_CSHT_MASK          = 0x00000F00
SPI_TIMING_SCLK_DIV_MASK      = 0x000000FF

SPI_TIMING_CS2SCLK_OFFSET     = 12
SPI_TIMING_CSHT_OFFSET        = 8
SPI_TIMING_SCLK_DIV_OFFSET    = 0

SPI_CTRL_TXFRST_MASK = 0x00000004
SPI_CTRL_RXFRST_MASK = 0x00000002
SPI_CTRL_SPIRST_MASK = 0x00000001

SPI_STATUS_TXFFL_MASK = 0x00800000
SPI_STATUS_TXFEM_MASK = 0x00400000
SPI_STATUS_TXFVE_MASK = 0x001f0000
SPI_STATUS_RXFFL_MASK = 0x00008000
SPI_STATUS_RXFEM_MASK = 0x00004000
SPI_STATUS_RXFVE_MASK = 0x00001f00
SPI_STATUS_SPIBSY_MASK = 0x00000001

SPI_STATUS_TXFFL_OFFSET = 23
SPI_STATUS_TXFEM_OFFSET = 22
SPI_STATUS_TXFVE_OFFSET = 16
SPI_STATUS_RXFFL_OFFSET = 15
SPI_STATUS_RXFEM_OFFSET = 14
SPI_STATUS_RXFVE_OFFSET = 8
SPI_STATUS_SPIBSY_OFFSET = 0

# ---------------------------------------------------------------------------
# Helper to compute absolute register address
# ---------------------------------------------------------------------------
def _reg_addr(spi_base, offset):
    return spi_base + offset

# ---------------------------------------------------------------------------
# Low-level register access wrappers
# ---------------------------------------------------------------------------
def spi_get_idver(handle, spi_base):
    return jtag.jtag_read_reg(handle, _reg_addr(spi_base, SPI_REG_VER))

def spi_get_transfmt(handle, spi_base):
    return jtag.jtag_read_reg(handle, _reg_addr(spi_base, SPI_REG_TRANSFMT))

def spi_set_transfmt(handle, spi_base, value):
    jtag.jtag_write_reg(handle, _reg_addr(spi_base, SPI_REG_TRANSFMT), value)

def spi_get_directio(handle, spi_base):
    return jtag.jtag_read_reg(handle, _reg_addr(spi_base, SPI_REG_DIRECTIO))

def spi_set_directio(handle, spi_base, value):
    jtag.jtag_write_reg(handle, _reg_addr(spi_base, SPI_REG_DIRECTIO), value)

def spi_get_transctrl(handle, spi_base):
    return jtag.jtag_read_reg(handle, _reg_addr(spi_base, SPI_REG_TRANSCTRL))

def spi_set_transctrl(handle, spi_base, value):
    jtag.jtag_write_reg(handle, _reg_addr(spi_base, SPI_REG_TRANSCTRL), value)

def spi_get_cmd(handle, spi_base):
    return jtag.jtag_read_reg(handle, _reg_addr(spi_base, SPI_REG_CMD))

def spi_set_cmd(handle, spi_base, value):
    jtag.jtag_write_reg(handle, _reg_addr(spi_base, SPI_REG_CMD), value)

def spi_get_addr(handle, spi_base):
    return jtag.jtag_read_reg(handle, _reg_addr(spi_base, SPI_REG_ADDR))

def spi_set_addr(handle, spi_base, value):
    jtag.jtag_write_reg(handle, _reg_addr(spi_base, SPI_REG_ADDR), value)

def spi_get_data(handle, spi_base):
    return jtag.jtag_read_reg(handle, _reg_addr(spi_base, SPI_REG_DATA))

def spi_set_data(handle, spi_base, value):
    jtag.jtag_write_reg(handle, _reg_addr(spi_base, SPI_REG_DATA), value)

def spi_get_ctrl(handle, spi_base):
    return jtag.jtag_read_reg(handle, _reg_addr(spi_base, SPI_REG_CTRL))

def spi_set_ctrl(handle, spi_base, value):
    jtag.jtag_write_reg(handle, _reg_addr(spi_base, SPI_REG_CTRL), value)

def spi_get_status(handle, spi_base):
    return jtag.jtag_read_reg(handle, _reg_addr(spi_base, SPI_REG_STATUS))

def spi_get_intren(handle, spi_base):
    return jtag.jtag_read_reg(handle, _reg_addr(spi_base, SPI_REG_INTREN))

def spi_set_intren(handle, spi_base, value):
    jtag.jtag_write_reg(handle, _reg_addr(spi_base, SPI_REG_INTREN), value)

def spi_get_intrst(handle, spi_base):
    return jtag.jtag_read_reg(handle, _reg_addr(spi_base, SPI_REG_INTRST))

def spi_set_intrst(handle, spi_base, value):
    jtag.jtag_write_reg(handle, _reg_addr(spi_base, SPI_REG_INTRST), value)

def spi_get_timing(handle, spi_base):
    return jtag.jtag_read_reg(handle, _reg_addr(spi_base, SPI_REG_TIMING))

def spi_set_timing(handle, spi_base, value):
    jtag.jtag_write_reg(handle, _reg_addr(spi_base, SPI_REG_TIMING), value)

# ---------------------------------------------------------------------------
# Helper to build the DCTRL register (identical to spi_prepare_transctrl)
# ---------------------------------------------------------------------------
def spi_prepare_transfmt(addrlen, datalen, datamerge, mosibidir, lsb, slvmode, cpol, cpha):
    fmt = 0
    fmt |= (addrlen   << SPI_TRANSFMT_ADDRLEN_OFFSET)   & SPI_TRANSFMT_ADDRLEN_MASK
    fmt |= (datalen   << SPI_TRANSFMT_DATALEN_OFFSET)   & SPI_TRANSFMT_DATALEN_MASK
    fmt |= (datamerge << SPI_TRANSFMT_DATAMERGE_OFFSET) & SPI_TRANSFMT_DATAMERGE_MASK
    fmt |= (mosibidir << SPI_TRANSFMT_MOSIBIDIR_OFFSET) & SPI_TRANSFMT_MOSIBIDIR_MASK
    fmt |= (lsb       << SPI_TRANSFMT_LSB_OFFSET)       & SPI_TRANSFMT_LSB_MASK
    fmt |= (slvmode   << SPI_TRANSFMT_SLVMODE_OFFSET)   & SPI_TRANSFMT_SLVMODE_MASK
    fmt |= (cpol      << SPI_TRANSFMT_CPOL_OFFSET)      & SPI_TRANSFMT_CPOL_MASK
    fmt |= (cpha      << SPI_TRANSFMT_CPHA_OFFSET)      & SPI_TRANSFMT_CPHA_MASK
    return fmt

def spi_prepare_timing(cs2sclk, csht, sclk_div):
    timing = 0
    timing |= (cs2sclk  << SPI_TIMING_CS2SCLK_OFFSET)   & SPI_TIMING_CS2SCLK_MASK
    timing |= (csht     << SPI_TIMING_CSHT_OFFSET)      & SPI_TIMING_CSHT_MASK
    timing |= (sclk_div << SPI_TIMING_SCLK_DIV_OFFSET)  & SPI_TIMING_SCLK_DIV_MASK
    return timing

def spi_prepare_transctrl(cmden, addren, trans_mode, wcnt, dummy_cnt, rcnt):
    dctrl = 0
    dctrl |= (cmden      << SPI_TRANSCTRL_CMDEN_OFFSET)     & SPI_TRANSCTRL_CMDEN_MASK
    dctrl |= (addren     << SPI_TRANSCTRL_ADDREN_OFFSET)    & SPI_TRANSCTRL_ADDREN_MASK
    dctrl |= (trans_mode << SPI_TRANSCTRL_TRANSMODE_OFFSET) & SPI_TRANSCTRL_TRANSMODE_MASK
    dctrl |= (wcnt       << SPI_TRANSCTRL_WCNT_OFFSET)      & SPI_TRANSCTRL_WCNT_MASK
    dctrl |= (dummy_cnt  << SPI_TRANSCTRL_DUMMYCNT_OFFSET)  & SPI_TRANSCTRL_DUMMYCNT_MASK
    dctrl |= (rcnt       << SPI_TRANSCTRL_RCNT_OFFSET)      & SPI_TRANSCTRL_RCNT_MASK
    return dctrl

# ---------------------------------------------------------------------------
# FIFO handling
# ---------------------------------------------------------------------------
def spi_clr_fifo(handle, spi_base):
    """Reset TX and RX FIFOs (same as spi_clr_fifo in C)."""
    ctrl = spi_get_ctrl(handle, spi_base)
    ctrl |= (SPI_CTRL_TXFRST_MASK | SPI_CTRL_RXFRST_MASK)
    spi_set_ctrl(handle, spi_base, ctrl)

def spi_wait_spi(handle, spi_base, timeout=100):
    """Wait until SPI bus is idle (same as spi_wait_spi)."""
    for _ in range(timeout):
        st = spi_get_status(handle, spi_base)  # Status Register 0x34
        if (st & SPI_STATUS_SPIBSY_MASK) == 0:
            return 0

    # ===== Timeout: dump controller state for debug =====
    status    = spi_get_status(handle, spi_base)     # 0x34
    ctrl      = spi_get_ctrl(handle, spi_base)       # 0x30
    intrst    = jtag.jtag_read_reg(handle, spi_base + 0x3C)
    transctrl = jtag.jtag_read_reg(handle, spi_base + 0x20)

    spi_active = status & 0x1
    tx_empty   = (status >> 22) & 0x1
    tx_full    = (status >> 23) & 0x1
    tx_num     = ((status >> 16) & 0x3F) | (((status >> 28) & 0x3) << 6)
    rx_empty   = (status >> 14) & 0x1
    rx_num     = ((status >> 8) & 0x3F) | (((status >> 24) & 0x3) << 6)

    print(f"  TIMEOUT DUMP:")
    print(f"    Status(0x34)   = 0x{status:08X}")
    print(f"      SPIActive={spi_active}, "
          f"TxEmpty={tx_empty}, TxFull={tx_full}, TxNum={tx_num}, "
          f"RxEmpty={rx_empty}, RxNum={rx_num}")
    print(f"    Ctrl(0x30)     = 0x{ctrl:08X}")
    print(f"    IntrSt(0x3C)   = 0x{intrst:08X}")
    print(f"      EndInt={(intrst >> 4) & 1}, "
          f"TxFIFOInt={(intrst >> 3) & 1}, "
          f"RxFIFOORInt={intrst & 1}")
    print(f"    TransCtrl(0x20)= 0x{transctrl:08X}")

    return 1  # timeout

def spi_get_rx_empty(handle, spi_base):
    return spi_get_status(handle, spi_base) & SPI_STATUS_RXFEM_MASK

def spi_get_rx_entries(handle, spi_base):
    reg = spi_get_status(handle, spi_base)
    return (reg & SPI_STATUS_RXFVE_MASK) >> SPI_STATUS_RXFVE_OFFSET

# ---------------------------------------------------------------------------
# Data transfer helpers (simplified versions)
# ---------------------------------------------------------------------------
def spi_rx_data(handle, spi_base, length):
    """
    Read ``length`` bytes from the SPI data register.
    Returns a ``bytes`` object.
    """
    words = (length + 3) // 4
    data = bytearray()
    for _ in range(words):
        word = spi_get_data(handle, spi_base)
        data.extend(struct.pack("<I", word))
    return bytes(data[:length])

def spi_tx_data(handle, spi_base, data):
    """
    Write ``data`` (bytes) to the SPI data register.
    Data is padded to a multiple of 4 bytes.
    """
    # Pad to 4-byte alignment
    if len(data) % 4 != 0:
        data += b'\x00' * (4 - (len(data) % 4))
    words = len(data) // 4
    for i in range(words):
        word, = struct.unpack("<I", data[i*4:(i+1)*4])
        spi_set_data(handle, spi_base, word)

def dump_regs(handle, spi_base, label=""):
    """Dump all SPI controller registers (offset 0x00–0x40) for debugging."""
    _REG_NAMES = {
        0x00: "VER      ",
        0x10: "TRANSFMT ",
        0x14: "DIRECTIO ",
        0x20: "TRANSCTRL",
        0x24: "CMD      ",
        0x28: "ADDR     ",
        0x2C: "DATA     ",
        0x30: "CTRL     ",
        0x34: "STATUS   ",
        0x38: "INTREN   ",
        0x3C: "INTRST   ",
        0x40: "TIMING   ",
    }
    tag = f" [{label}]" if label else ""
    print(f"[SPI REGS @ 0x{spi_base:08X}]{tag}")
    for offset in range(0x00, 0x44, 0x04):
        val = jtag.jtag_read_reg(handle, spi_base + offset)
        name = _REG_NAMES.get(offset, f"RSV      ")
        print(f"  +0x{offset:02X}  {name}  0x{val:08X}", end="")
        if offset == 0x34:   # STATUS — decode key bits inline
            spibsy  = (val >> 0)  & 0x1
            rxfem   = (val >> 14) & 0x1
            rxfve   = (val >> 8)  & 0x3F
            txfem   = (val >> 22) & 0x1
            txffl   = (val >> 23) & 0x1
            txfve   = (val >> 16) & 0x3F
            print(f"  SPIBusy={spibsy} TxEmpty={txfem} TxFull={txffl} "
                  f"TxEntries={txfve} RxEmpty={rxfem} RxEntries={rxfve}", end="")
        elif offset == 0x30: # CTRL — decode reset bits
            spirst  = (val >> 0)  & 0x1
            rxfrst  = (val >> 1)  & 0x1
            txfrst  = (val >> 2)  & 0x1
            print(f"  SPIRST={spirst} RXFRST={rxfrst} TXFRST={txfrst}", end="")
        print()

def spi_init(handle, spi_base):
    """初始化 SPI 控制器 (从固件 trace 中提取的值) """

    # 0. 先复位控制器，确保干净状态
    ctrl = spi_get_ctrl(handle, spi_base)
    ctrl |= SPI_CTRL_SPIRST_MASK
    spi_set_ctrl(handle, spi_base, ctrl)
    # 等待 SPIRST 位由硬件自动清零（ATCSPI200 标准行为）
    for _ in range(1000):
        ctrl = spi_get_ctrl(handle, spi_base)
        if (ctrl & SPI_CTRL_SPIRST_MASK) == 0:
            break
    else:
        print("[WARNING] SPI reset timeout")

    # 1. Timing 寄存器 (0x40) - 固件写入 0x00000031
    spi_set_timing(handle, spi_base, spi_prepare_timing(
        cs2sclk  = 0,     # no extra CS-to-SCLK delay
        csht     = 0,     # no extra CS-high hold
        sclk_div = 0x31,  # SCLK = clk_src / (2*(0x31+1)) = clk_src/100
    ))

    # 2. TransFmt 寄存器 (0x10) - 固件最终值 0x00020780
    spi_set_transfmt(handle, spi_base, spi_prepare_transfmt(
        addrlen   = 2,  # 3-byte (24-bit) address
        datalen   = 7,  # 8-bit data unit (7+1=8)
        datamerge = 1,  # Data Merge mode enabled
        mosibidir = 0,  # uni-directional MOSI
        lsb       = 0,  # MSB first
        slvmode   = 0,  # master mode
        cpol      = 0,  # SCLK idle LOW
        cpha      = 0,  # sample on odd (1st) edge
    ))

    # 3. 清空 FIFO
    spi_clr_fifo(handle, spi_base)

    # 4. 关闭中断 (Python 用轮询，不需要中断)
    spi_set_intren(handle, spi_base, 0x00000000)

