#!/usr/bin/python3
# -*- coding: utf-8 -*-

import struct
import time

from . import spi_driver as spi
from . import spinor_driver as spinor
from . import jtag


# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
FLASH_MMAP_BASE    = 0x80000000
PINMUX_SPI_REG     = 0xd0c00414
PROGRESS_BAR_WIDTH = 30
MMAP_READ_WORDS    = 256

_PREFIX = '[Flash]'


def hexdump(data, base_addr=0, width=16, size_fmt=str):
    print(f"{_PREFIX} Hexdump of {size_fmt(len(data))} bytes:")
    for offset in range(0, len(data), width):
        chunk = data[offset:offset + width]
        hex_str = ' '.join(f'{b:02x}' for b in chunk)
        ascii_str = ''.join(
            chr(b) if 0x20 <= b < 0x7f else '.' for b in chunk
        )
        print(f"  {base_addr + offset:08x}: {hex_str:<{width*3}}  {ascii_str}")
        if offset >= 0x400:
            remaining_bytes = len(data) - offset - width
            if remaining_bytes > 0:
                print(f"  ... ({size_fmt(remaining_bytes)} more, "
                      f"use -o to save to file)")
            break

# ---------------------------------------------------------------------------
# Flash
# ---------------------------------------------------------------------------

class Flash:
    def __init__(self, v_jtagHandle, v_dora):
        self.handle = v_jtagHandle
        self.dora = v_dora

        # Halt CPU for SBA access
        jtag.jtag_halt_cpu(self.handle)

        # PINMUX check and fix
        self._pinmux_init()

        # SPI controller init
        spi.spi_init(self.handle, spi.CPE_SPI_BASE)

        # Flash device init
        self.spi_base = spi.CPE_SPI_BASE
        self.dev = spinor.FlashDev(self.handle, self.spi_base)

        # Apply per-flash unlock quirks for parts that ship default-protected
        # (e.g. SST26WF040B BPR, AT25DF021A SWP). No-op on JEDEC IDs not in
        # the quirk table, so behaviour for the 5 already-validated parts is
        # unchanged.
        self._check_quirks()

    def __del__(self):
        pass


#####################################################
# internal log module (under control by dora.log)
#####################################################
    def _log(self, msg):
        if self.dora.log:
            print(f"{_PREFIX}-[log] {msg}")

    def _warn(self, msg):
        if self.dora.log:
            print(f"{_PREFIX}-[warn] {msg}")


#####################################################
# internal
#####################################################
    def _pinmux_init(self):
        mux_val = jtag.jtag_read_reg(self.handle, PINMUX_SPI_REG)
        self._log(f"PINMUX 0x{PINMUX_SPI_REG:08X} = 0x{mux_val:08X} (bit0={mux_val & 1})")
        if (mux_val & 1) == 0:
            self._log("bit0=0 -> SPI pin not in SPI mode, fixing ...")
            jtag.jtag_write_reg(self.handle, PINMUX_SPI_REG, mux_val | 1)
            mux_val2 = jtag.jtag_read_reg(self.handle, PINMUX_SPI_REG)
            self._log(f"PINMUX after fix = 0x{mux_val2:08X} (bit0={mux_val2 & 1})")

    def _check_flash_dev(self):
        info = self.dev.check()
        if not info.get('valid'):
            raise RuntimeError("Flash not detected")

    def _check_quirks(self):
        info = self.dev.check()
        if not info.get('valid'):
            raise RuntimeError("Flash not detected")
        mfr = info.get('mfr')
        mtype = info.get('type')
        mdens = info.get('density')
        spinor.apply_quirks(self.handle, self.spi_base, mfr, mtype, mdens)

    def _print_progress(self, current, total, prefix="Progress"):
        if total <= 0:
            return
        pct = min(current * 100 // total, 100)
        filled = PROGRESS_BAR_WIDTH * pct // 100
        bar = '█' * filled + '-' * (PROGRESS_BAR_WIDTH - filled)
        print(f"\r  {prefix}: [{bar}] {pct:3d}%  "
              f"{self.dora.f_size_format(current):>8s}/{self.dora.f_size_format(total)}",
              end="", flush=True)
        if current >= total:
            print()

    def _align_down(self, offset, alignment):
        return offset & ~(alignment - 1)

    def _align_up(self, offset, alignment):
        return (offset + alignment - 1) & ~(alignment - 1)

    def _compare_verify(self, read_back, expected_data):
        if read_back == expected_data:
            print("Verify PASSED.")
            return True

        length = len(expected_data)
        mismatch_count = 0
        first_mismatch = -1
        for i in range(length):
            if i >= len(read_back) or expected_data[i] != read_back[i]:
                if first_mismatch < 0:
                    first_mismatch = i
                mismatch_count += 1

        got = read_back[first_mismatch] if first_mismatch < len(read_back) else 0xFF
        print(f"Verify FAILED: {mismatch_count} byte(s) differ")
        print(f"  First mismatch at offset 0x{first_mismatch:08X}: "
              f"expected 0x{expected_data[first_mismatch]:02X}, got 0x{got:02X}")
        return False

    def _erase_with_progress(self, offset, size):
        h = self.handle
        b = self.spi_base

        start = self._align_down(offset, spinor.SECTOR_SIZE)
        end = self._align_up(offset + size, spinor.SECTOR_SIZE)
        total = end - start
        done = 0

        for sector_offset in range(start, end, spinor.SECTOR_SIZE):
            spinor.flash_write_enable(h, b)
            spinor.f_exec_cmd(h, b, spinor.SPIROM_OP_SE,
                              offset=sector_offset)
            if not spinor.flash_wait_ready(h, b, timeout=2000):
                raise RuntimeError(
                    f"Erase timeout at sector 0x{sector_offset:08X}")
            done += spinor.SECTOR_SIZE
            self._print_progress(done, total, "Erase")

    def _program_with_progress(self, offset, data_bytes, inline_verify=False):
        h = self.handle
        b = self.spi_base

        total = len(data_bytes)
        written = 0
        SECTOR_RETRY_MAX = 3

        while written < total:
            cur_offset = offset + written
            remaining = total - written

            page_end = ((cur_offset // spinor.PAGE_SIZE) + 1) * spinor.PAGE_SIZE
            max_in_page = page_end - cur_offset
            chunk = min(spinor.TX_FIFO_BYTES, max_in_page, remaining)
            chunk_data = data_bytes[written:written + chunk]

            try:
                spinor.flash_write_enable(h, b)
                spinor.f_exec_cmd(h, b, spinor.SPIROM_OP_PP,
                                  offset=cur_offset, write_data=chunk_data)
                if not spinor.flash_wait_ready(h, b, timeout=1000):
                    raise RuntimeError(
                        f"flash_wait_ready timeout at 0x{cur_offset:08X}")

                if inline_verify:
                    words = (len(chunk_data) + 3) // 4
                    vals = jtag.jtag_read_regs(h, FLASH_MMAP_BASE + cur_offset, words)
                    rb = b''.join(struct.pack('<I', v) for v in vals)
                    if rb[:len(chunk_data)] != chunk_data:
                        raise RuntimeError(
                            f"inline verify mismatch at 0x{cur_offset:08X}")
            except RuntimeError as e:
                print(f"\n  !! Write failed at 0x{cur_offset:08X}: {e}")
                print(f"  !! Starting sector-level retry ...")

                sector_start = self._align_down(cur_offset, spinor.SECTOR_SIZE)
                sector_end = sector_start + spinor.SECTOR_SIZE
                sector_data_offset = max(0, sector_start - offset)
                sector_data_end = min(sector_end - offset, total)
                sector_data = data_bytes[sector_data_offset:sector_data_end]

                success = False
                for retry in range(1, SECTOR_RETRY_MAX + 1):
                    print(f"  !! Sector retry {retry}/{SECTOR_RETRY_MAX}: "
                          f"re-erase 0x{sector_start:08X}, "
                          f"rewrite {len(sector_data)} bytes")
                    try:
                        spinor.spi_reset_controller(h, b)

                        spinor.flash_write_enable(h, b)
                        spinor.f_exec_cmd(h, b, spinor.SPIROM_OP_SE,
                                          offset=sector_start)
                        if not spinor.flash_wait_ready(h, b, timeout=5000):
                            print("  !! Erase timeout on retry, trying again ...")
                            continue

                        sec_written = 0
                        write_ok = True
                        while sec_written < len(sector_data):
                            wr_offset = sector_start + sec_written
                            wr_page_end = (
                                (wr_offset // spinor.PAGE_SIZE + 1)
                                * spinor.PAGE_SIZE
                            )
                            wr_max = wr_page_end - wr_offset
                            wr_chunk = min(spinor.TX_FIFO_BYTES, wr_max,
                                           len(sector_data) - sec_written)
                            wr_data = sector_data[sec_written:sec_written + wr_chunk]

                            spinor.flash_write_enable(h, b)
                            spinor.f_exec_cmd(h, b, spinor.SPIROM_OP_PP,
                                              offset=wr_offset,
                                              write_data=wr_data)
                            if not spinor.flash_wait_ready(h, b, timeout=1000):
                                print(f"  !! PP timeout during sector rewrite "
                                      f"at 0x{wr_offset:08X}")
                                write_ok = False
                                break
                            if inline_verify:
                                iv_words = (len(wr_data) + 3) // 4
                                iv_vals = jtag.jtag_read_regs(
                                    h, FLASH_MMAP_BASE + wr_offset, iv_words)
                                iv_rb = b''.join(
                                    struct.pack('<I', v) for v in iv_vals)
                                if iv_rb[:len(wr_data)] != wr_data:
                                    print(f"  !! Inline verify mismatch during "
                                          f"sector rewrite at 0x{wr_offset:08X}")
                                    write_ok = False
                                    break
                            sec_written += wr_chunk

                        if write_ok:
                            success = True
                            print(f"  !! Sector retry {retry} succeeded")
                            break
                    except RuntimeError as retry_err:
                        print(f"  !! Retry {retry} failed: {retry_err}")
                        continue

                if not success:
                    raise RuntimeError(
                        f"Sector 0x{sector_start:08X} failed after "
                        f"{SECTOR_RETRY_MAX} retries")

                written = sector_data_end
                continue

            written += chunk
            self._print_progress(written, total, "Program")

    def _verify_mmap(self, offset, expected_data):
        length = len(expected_data)
        print(f"Verifying {self.dora.f_size_format(length)} at 0x{offset:08X} (memory-mapped) ...")

        read_back = bytearray()
        remaining = length
        cur_offset = offset

        while remaining > 0:
            words = min(MMAP_READ_WORDS, (remaining + 3) // 4)
            vals = jtag.jtag_read_regs(self.handle, FLASH_MMAP_BASE + cur_offset, words)
            chunk_bytes = b''.join(struct.pack('<I', v) for v in vals)
            take = min(remaining, words * 4)
            read_back.extend(chunk_bytes[:take])
            cur_offset += take
            remaining -= take
            self._print_progress(len(read_back), length, "Verify")

        read_back = bytes(read_back)
        return self._compare_verify(read_back, expected_data)

    def _verify_spi(self, offset, expected_data):
        length = len(expected_data)
        print(f"Verifying {self.dora.f_size_format(length)} at 0x{offset:08X} (SPI read) ...")

        read_back = bytearray()
        remaining = length
        cur_offset = offset

        while remaining > 0:
            chunk = min(spinor.PAGE_SIZE, remaining)
            resp = self.dev.read(cur_offset, chunk)
            read_back.extend(resp)
            cur_offset += chunk
            remaining -= chunk
            self._print_progress(len(read_back), length, "Verify")

        read_back = bytes(read_back)
        return self._compare_verify(read_back, expected_data)

#####################################################
# public
#####################################################
    def f_info(self):
        info = self.dev.check()
        if not info['valid']:
            raise RuntimeError("Flash not detected — invalid manufacturer ID")
        return info

    def f_status(self):
        sr = self.dev.read_status()
        print(f"Flash Status Register: 0x{sr:02X} (0b{sr:08b})")
        print(f"  [7] SRWD = {(sr >> 7) & 1}   "
              f"(Status Register Write Disable)")
        print(f"  [6] QE   = {(sr >> 6) & 1}   "
              f"(Quad Enable)")
        print(f"  [5] BP3  = {(sr >> 5) & 1}   "
              f"(Block Protect bit 3)")
        print(f"  [4] BP2  = {(sr >> 4) & 1}   "
              f"(Block Protect bit 2)")
        print(f"  [3] BP1  = {(sr >> 3) & 1}   "
              f"(Block Protect bit 1)")
        print(f"  [2] BP0  = {(sr >> 2) & 1}   "
              f"(Block Protect bit 0)")
        print(f"  [1] WEL  = {(sr >> 1) & 1}   "
              f"(Write Enable Latch)")
        print(f"  [0] WIP  = {(sr >> 0) & 1}   "
              f"(Write In Progress)")

        bp = (sr >> 2) & 0x0F
        if bp != 0:
            print(f"  NOTE: Block protect bits BP[3:0]=0x{bp:X} -- "
                  f"some regions may be write-protected.")
            print(f"        CE (chip erase) requires all BP bits = 0.")
        return sr

    def f_diag(self):
        h = self.handle
        b = self.spi_base

        print("=" * 60)
        print("SPI Bus Diagnostic")
        print("=" * 60)

        # Test 1: RDID x3
        print("\n[Test 1] RDID x3")
        for i in range(3):
            data = spinor.f_exec_cmd(h, b, spinor.SPIROM_OP_RDID,
                                            read_count=3)
            if data and len(data) >= 3:
                print(f"  #{i+1}: MFR=0x{data[0]:02X}  TYPE=0x{data[1]:02X}  "
                      f"DENS=0x{data[2]:02X}  "
                      f"{'OK' if data[0] not in (0x00, 0xFF) else 'BAD'}")
            else:
                print(f"  #{i+1}: no response")

        # Test 2: WREN -> RDSR (WEL bit test)
        print("\n[Test 2] WREN -> RDSR (WEL bit check)")
        sr_before = spinor.flash_read_status(h, b)
        print(f"  SR before WREN: 0x{sr_before:02X}  WEL={(sr_before>>1)&1}")

        spinor.f_exec_cmd(h, b, spinor.SPIROM_OP_WREN)

        sr_after = spinor.flash_read_status(h, b)
        wel = (sr_after >> 1) & 1
        print(f"  SR after  WREN: 0x{sr_after:02X}  WEL={wel}")

        if wel == 1:
            print("  -> PASS: Flash responded to WREN (WEL=1), bus is alive")
        else:
            print("  -> FAIL: WEL still 0 after WREN")
            print("     Possible causes:")
            print("       a) MISO stuck low -- flash not driving the bus at all")
            print("       b) Flash in hardware-protected state (BP bits / WP# pin)")
            print("       c) CS not reaching flash -- check board schematic")

        # WRDI cleanup
        spinor.f_exec_cmd(h, b, spinor.SPIROM_OP_WRDI)
        sr_final = spinor.flash_read_status(h, b)
        print(f"  SR after  WRDI: 0x{sr_final:02X}  WEL={(sr_final>>1)&1}")

        # Test 3: Read 4 bytes from address 0x000000
        print("\n[Test 3] Read 4 bytes @ 0x000000")
        read_data = spinor.f_exec_cmd(h, b, spinor.SPIROM_OP_READ,
                                      offset=0x000000, read_count=4)
        if read_data:
            hex_str = ' '.join(f'{x:02X}' for x in read_data)
            print(f"  Data: {hex_str}")
            if all(x == 0x00 for x in read_data):
                print("  -> All 0x00 -- suspicious (erased flash should be 0xFF)")
            elif all(x == 0xFF for x in read_data):
                print("  -> All 0xFF -- consistent with erased flash")
            else:
                print("  -> Non-trivial data -- flash is responding")
        else:
            print("  -> No response")

        # Test 4: SPI controller register dump
        print()
        spi.dump_regs(h, b, label="after all tests")

        # Test 5: Memory-mapped read vs SPI read comparison
        print(f"\n[Test 5] Memory-mapped read @ 0x{FLASH_MMAP_BASE:08X}")
        print("  Comparing memory-mapped read with SPI controller read ...")

        mmap_words = []
        for i in range(4):
            val = jtag.jtag_read_reg(self.handle, FLASH_MMAP_BASE + i * 4)
            mmap_words.append(val)
        mmap_data = b''.join(w.to_bytes(4, 'little') for w in mmap_words)

        spi_data = spinor.f_exec_cmd(h, b, spinor.SPIROM_OP_READ,
                                     offset=0x000000, read_count=16)

        print(f"  Memory-mapped: {' '.join(f'{x:02X}' for x in mmap_data)}")
        print(f"  SPI READ cmd:  {' '.join(f'{x:02X}' for x in spi_data)}")

        if mmap_data == spi_data:
            print("  -> MATCH: Memory-mapped read works, can be used for fast verify")
        else:
            if all(x == 0x00 for x in mmap_data):
                print("  -> MISMATCH: memory-mapped reads all 0x00 "
                      "(mapping may not be enabled)")
            elif all(x == 0xFF for x in mmap_data):
                print("  -> MISMATCH: memory-mapped reads all 0xFF "
                      "(bus error / unmapped region)")
            else:
                print("  -> MISMATCH: data differs (endianness or config issue?)")

        # Summary
        print("\n" + "=" * 60)
        if wel == 1:
            print("SUMMARY: Flash is alive and responding to commands.")
            print("         If RDID still shows 0x000000, the issue is")
            print("         specific to the RDID command or read-back path.")
        else:
            print("SUMMARY: Flash is NOT responding (MISO appears stuck low).")
            print("         Problem is at the physical/bus level, not software.")
        if mmap_data == spi_data and spi_data and not all(x == 0 for x in spi_data):
            print("MMAP:    Memory-mapped read verified -- fast verify is feasible.")
        print("=" * 60)

        if wel != 1:
            raise RuntimeError("Flash not responding (MISO appears stuck low)")

    def f_read(self, v_offset, v_length, v_output=None, v_print=True):
        self._check_flash_dev()

        print(f"Reading {self.dora.f_size_format(v_length)} from 0x{v_offset:08X} ...")
        t0 = time.time()

        data = bytearray()
        remaining = v_length
        cur_offset = v_offset

        while remaining > 0:
            chunk = min(spinor.PAGE_SIZE, remaining)
            resp = self.dev.read(cur_offset, chunk)
            data.extend(resp)
            cur_offset += chunk
            remaining -= chunk
            self._print_progress(len(data), v_length, "Read")

        elapsed = time.time() - t0
        speed = v_length / elapsed if elapsed > 0 else 0
        print(f"Read complete in {elapsed:.1f}s ({self.dora.f_size_format(int(speed))}/s)")

        data = bytes(data)
        if v_output:
            with open(v_output, 'wb') as f:
                f.write(data)
            print(f"Saved to {v_output}")
        elif v_print:
            hexdump(data, v_offset, size_fmt=self.dora.f_size_format)
        return data

    def f_write(self, v_offset, v_data, v_verify=False):
        self._check_flash_dev()

        h = self.handle
        b = self.spi_base

        print(f"Writing {len(v_data)} byte(s) to 0x{v_offset:08X}: "
              f"{' '.join(f'{x:02X}' for x in v_data)}")

        spinor.flash_write_enable(h, b)
        spinor.f_exec_cmd(h, b, spinor.SPIROM_OP_PP,
                          offset=v_offset, write_data=v_data)
        if not spinor.flash_wait_ready(h, b, timeout=1000):
            raise RuntimeError(f"Program timeout at 0x{v_offset:08X}")

        print("Write OK.")

        if v_verify:
            readback = self.dev.read(v_offset, len(v_data))
            if readback == v_data:
                print(f"Verify OK: "
                      f"{' '.join(f'{x:02X}' for x in readback)}")
            else:
                print("Verify FAILED!")
                print(f"  Expected: {' '.join(f'{x:02X}' for x in v_data)}")
                print(f"  Got:      {' '.join(f'{x:02X}' for x in readback)}")
                raise RuntimeError(f"Write verify mismatch at 0x{v_offset:08X}")

    def f_erase(self, v_offset, v_length):
        erase_start = self._align_down(v_offset, spinor.SECTOR_SIZE)
        erase_end = self._align_up(v_offset + v_length, spinor.SECTOR_SIZE)
        actual_size = erase_end - erase_start

        if erase_start != v_offset or actual_size != v_length:
            print(f"NOTE: Erase aligned to sector boundaries: "
                  f"0x{erase_start:08X} - 0x{erase_end:08X} "
                  f"({self.dora.f_size_format(actual_size)})")

        self._check_flash_dev()

        t0 = time.time()
        self._erase_with_progress(v_offset, v_length)
        elapsed = time.time() - t0
        print(f"Erase took {elapsed:.1f}s")

    def f_burn(self, v_binFile, v_verify=None, v_offset=0x0):
        # v_verify: None | 'fast' (mmap post-burn) | 'full' (SPI post-burn) | 'inline' (per-chunk+retry)
        with open(v_binFile, 'rb') as f:
            v_data = f.read()

        jtag.jtag_set_check(False)
        try:
            total_size = len(v_data)
            print(f"=== Burn: {v_binFile} ({self.dora.f_size_format(total_size)}) "
                  f"-> flash @ 0x{v_offset:08X} ===")

            self._check_flash_dev()

            has_post_verify = v_verify in ('fast', 'full')
            inline_verify   = v_verify == 'inline'
            t_total = time.time()
            steps = 3 if has_post_verify else 2

            # Step 1: Erase
            erase_size = self._align_up(total_size, spinor.SECTOR_SIZE)
            print(f"\n[1/{steps}] Erasing {self.dora.f_size_format(erase_size)} ...")
            t0 = time.time()
            self._erase_with_progress(v_offset, total_size)
            print(f"      Erase done in {time.time() - t0:.1f}s")

            # Step 2: Program
            label = "Programming + inline-verify" if inline_verify else "Programming"
            print(f"\n[2/{steps}] {label} {self.dora.f_size_format(total_size)} ...")
            t0 = time.time()
            self._program_with_progress(v_offset, v_data, inline_verify=inline_verify)
            print(f"      {label} done in {time.time() - t0:.1f}s")

            # Step 3: Post-burn verify (fast or full only)
            if has_post_verify:
                if v_verify == 'full':
                    print(f"\n[3/{steps}] Verifying (SPI read, full) ...")
                    t0 = time.time()
                    ok = self._verify_spi(v_offset, v_data)
                else:
                    print(f"\n[3/{steps}] Verifying (memory-mapped, fast) ...")
                    t0 = time.time()
                    ok = self._verify_mmap(v_offset, v_data)
                if not ok:
                    raise RuntimeError("Burn verify failed")
                print(f"      Verify done in {time.time() - t0:.1f}s")

            total_elapsed = time.time() - t_total
            print(f"\n=== Burn complete in {total_elapsed:.1f}s ===")
        finally:
            jtag.jtag_set_check(True)

    def f_verify(self, v_binFile, v_offset, v_verify='fast'):
        # v_verify: 'fast' (mmap) | 'full' (SPI read)
        with open(v_binFile, 'rb') as f:
            v_expectedData = f.read()

        jtag.jtag_set_check(False)
        try:
            self._check_flash_dev()
            t0 = time.time()
            if v_verify == 'full':
                ok = self._verify_spi(v_offset, v_expectedData)
            else:
                ok = self._verify_mmap(v_offset, v_expectedData)
            elapsed = time.time() - t0
            speed = len(v_expectedData) / elapsed if elapsed > 0 else 0
            print(f"Verify done in {elapsed:.1f}s ({self.dora.f_size_format(int(speed))}/s)")
            if not ok:
                raise RuntimeError("Verify failed")
        finally:
            jtag.jtag_set_check(True)

    def f_blank_check(self, v_offset, v_length):
        self._check_flash_dev()

        print(f"Blank check: {self.dora.f_size_format(v_length)} at 0x{v_offset:08X} ...")
        t0 = time.time()

        remaining = v_length
        cur_offset = v_offset
        checked = 0
        first_non_ff = -1

        while remaining > 0:
            chunk = min(spinor.PAGE_SIZE, remaining)
            resp = self.dev.read(cur_offset, chunk)

            if first_non_ff < 0:
                for i, byte in enumerate(resp[:chunk]):
                    if byte != 0xFF:
                        first_non_ff = cur_offset + i
                        break

            cur_offset += chunk
            remaining -= chunk
            checked += chunk
            self._print_progress(checked, v_length, "Check")

        elapsed = time.time() - t0

        if first_non_ff < 0:
            print(f"Blank check PASSED -- region is all 0xFF "
                  f"({elapsed:.1f}s)")
        else:
            print(f"Blank check FAILED -- first non-0xFF byte at "
                  f"0x{first_non_ff:08X} ({elapsed:.1f}s)")
            raise RuntimeError(
                f"Blank check failed: first non-0xFF at 0x{first_non_ff:08X}")
