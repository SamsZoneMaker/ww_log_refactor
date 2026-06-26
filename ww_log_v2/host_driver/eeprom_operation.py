#!/usr/bin/python3
# -*- coding: utf-8 -*-

import time

from . import i2c_driver as i2c
from . import eeprom_driver as eeprom
from . import jtag


# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
PROGRESS_BAR_WIDTH = 30
PROGRESS_CHUNK_SIZE = 1024

def _hexdump(data, base_addr=0, width=16, size_fmt=str):
    for offset in range(0, len(data), width):
        chunk = data[offset:offset + width]
        hex_str = ' '.join(f'{b:02x}' for b in chunk)
        ascii_str = ''.join(
            chr(b) if 0x20 <= b < 0x7f else '.' for b in chunk
        )
        print(f"  {base_addr + offset:06x}: {hex_str:<{width*3}}  {ascii_str}")
        if offset >= 0x400:
            remaining_bytes = len(data) - offset - width
            if remaining_bytes > 0:
                print(f"  ... ({size_fmt(remaining_bytes)} more, "
                      f"use -o to save to file)")
            break


# ---------------------------------------------------------------------------
# Eeprom
# ---------------------------------------------------------------------------

class Eeprom:
    def __init__(self, v_jtagHandle, v_dora, dev_addr=None):
        self.handle = v_jtagHandle
        self.dora = v_dora

        # Halt CPU for SBA access
        jtag.jtag_halt_cpu(self.handle)

        # I2C controller init (ATCIIC100)
        self.i2c_base = i2c.CPE_I2C_BASE
        i2c.i2c_init(self.handle, self.i2c_base)

        # Device address is resolved lazily by __check_dev_addr() on first
        # use by read/write/burn/verify. scan/info/diag don't need it.
        self.dev_addr = dev_addr
        self.a2 = None
        self.dev = None

    def __del__(self):
        pass

    def _scan_bus(self):
        """
        Probe 0x50-0x57. Returns the list of ACK addresses (may be empty).
        Used by f_scan and _resolve_dev_addr so the bus is scanned only once
        per Eeprom instance method that needs it.
        """
        return eeprom.eeprom_check(self.handle, self.i2c_base) or []

    def _resolve_dev_addr(self):
        """
        Validate and resolve the EEPROM device address. Must be called before
        any operation that requires actual device access (read/write/burn/verify).

        Raises RuntimeError / ValueError if the address can't be resolved
        (no EEPROM on bus, user gave an out-of-range or non-base address).

        Rules:
          1) If user provided dev_addr, it must be within 0x50-0x57.
          2) Scan the bus to get the actual ACK list.
          3) If no ACK at all -> raise.
          4) The lowest ACKing address is the true base (P1/P0 of the lower
             addresses are borrowed for memory addressing).
          5) If user provided dev_addr:
               - must be in the ACK list, otherwise raise with the actual ACKs.
               - must equal the lowest ACK (the true base), otherwise raise
                 and tell the user the correct base address.
          6) If user did not provide dev_addr, auto-pick the lowest ACK.
        """
        # Range pre-check
        if self.dev_addr is not None and not (0x50 <= self.dev_addr <= 0x57):
            raise ValueError(
                f"EEPROM address 0x{self.dev_addr:02X} out of EEPROM class "
                f"range 0x50-0x57")

        eeprom_dev = self._scan_bus()
        if not eeprom_dev:
            raise RuntimeError(
                "No EEPROM detected on I2C bus (scanned 0x50-0x57 all NACK). "
                "Check power, pull-ups, or confirm EEPROM is present.")

        base_addr = eeprom_dev[0]
        ack_addr = ', '.join(f'0x{a:02X}' for a in eeprom_dev)

        if self.dev_addr is None:
            # Auto mode
            if len(eeprom_dev) > 1:
                print(f"[auto-scan] EEPROM ACK addresses: {ack_addr}")
                print(f"[auto-scan] Using base 0x{base_addr:02X} "
                      f"(driver will compute P1/P0 per access).")
            else:
                print(f"[auto-scan] EEPROM detected at 0x{base_addr:02X}")
            self.dev_addr = base_addr
        else:
            # User explicitly specified
            if self.dev_addr not in eeprom_dev:
                raise RuntimeError(
                    f"User-specified address 0x{self.dev_addr:02X} did not "
                    f"ACK on the bus. Actual ACK addresses: {ack_addr}. "
                    f"Correct base should be 0x{base_addr:02X}.")
            if self.dev_addr != base_addr:
                raise RuntimeError(
                    f"User-specified address 0x{self.dev_addr:02X} is not the "
                    f"EEPROM base. This EEPROM occupies {ack_addr}; the "
                    f"correct base is 0x{base_addr:02X}. "
                    f"Re-run with --addr 0x{base_addr:02X}.")

        # A2 is bit 2 of the device address; P1/P0 (bits 1:0) are
        # recomputed per access from offset by the driver.
        self.a2 = (self.dev_addr >> 2) & 1
        self.dev = eeprom.EepromDev(self.handle, self.i2c_base, a2=self.a2)

#####################################################
# internal
#####################################################
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
        print(f"  First mismatch at offset 0x{first_mismatch:06X}: "
              f"expected 0x{expected_data[first_mismatch]:02X}, got 0x{got:02X}")
        return False

    def _read_with_progress(self, offset, length, prefix="Read"):
        data = bytearray()
        remaining = length
        cur_offset = offset

        while remaining > 0:
            chunk = min(PROGRESS_CHUNK_SIZE, remaining)
            resp = self.dev.read(cur_offset, chunk)
            data.extend(resp)
            cur_offset += chunk
            remaining -= chunk
            self._print_progress(len(data), length, prefix)

        return bytes(data)

    def _write_with_progress(self, offset, data_bytes, prefix="Program"):
        total = len(data_bytes)
        written = 0

        while written < total:
            chunk = min(PROGRESS_CHUNK_SIZE, total - written)
            self.dev.write(offset + written, data_bytes[written: written + chunk])
            written += chunk
            self._print_progress(written, total, prefix)

#####################################################
# public
#####################################################
    def f_scan(self):
        print("\nScanning I2C EEPROM range 0x50-0x57 ...")
        eeprom_dev = self._scan_bus()
        if not eeprom_dev:
            print("  No EEPROM detected on I2C bus.")
            print("  Run `diag` for low-level controller diagnostics.")
            return {'valid': False, 'base': None, 'addrs': [],
                    'banks': 0, 'capacity': 0}

        for dev_addr in range(0x50, 0x58):
            ack = dev_addr in eeprom_dev
            a2 = (dev_addr >> 2) & 1
            p1 = (dev_addr >> 1) & 1
            p0 =  dev_addr       & 1
            print(f"  0x{dev_addr:02X}  (A2={a2} P1={p1} P0={p0})  "
                  f"{'ACK' if ack else '--'}")

        addrs_str = ', '.join(f'0x{a:02X}' for a in eeprom_dev)
        # Each ACKing address = one 64 KiB bank borrowed via P1/P0.
        # Assumes a single EEPROM populated on the bus.
        bank_count  = len(eeprom_dev)
        capacity_b  = bank_count * 64 * 1024
        capacity_kb = capacity_b // 1024

        print("\nSummary:")
        print(f"Detected {len(eeprom_dev)} ACKing address(es): {addrs_str}")
        print(f"  Base I2C address   : 0x{eeprom_dev[0]:02X}")
        print(f"  Bank count         : {bank_count} / 4")
        print(f"  Detected capacity  : {capacity_kb} KiB "
              f"({bank_count} bank x 64 KiB)")

        print(f"  Driver geometry    : {eeprom.EEPROM_CAPACITY} bytes "
              f"total, {eeprom.EEPROM_PAGE_SIZE} B/page")
        if capacity_b != eeprom.EEPROM_CAPACITY:
            print(f"  WARNING: detected capacity ({capacity_kb} KiB) "
                  f"differs from driver geometry "
                  f"({eeprom.EEPROM_CAPACITY // 1024} KiB).")

        print(f'!!! NOTE: Please use 0x{eeprom_dev[0]:02X} as the base address for all operations !!!')
        return {'valid': True, 'base': eeprom_dev[0], 'addrs': eeprom_dev,
                'banks': bank_count, 'capacity': capacity_b}


    def f_diag(self):
        h = self.handle
        b = self.i2c_base

        # Test 1: IDREV + controller register dump
        print("\n[Test 1] Controller register dump")
        i2c.dump_regs(h, b, label="initial")

        # Test 2: Address probe across all 4 banks for A2=0 and A2=1
        print("\n[Test 2] Address probe  (1010_A2_P1_P0)")
        any_ack = False
        for a2 in (0, 1):
            for bank in range(4):
                p1 = (bank >> 1) & 1
                p0 = bank & 1
                dev_addr = 0x50 | ((a2 & 1) << 2) | (p1 << 1) | p0
                ack = i2c.i2c_probe(h, b, dev_addr)
                print(f"  A2={a2} bank={bank} (P1={p1},P0={p0})  "
                      f"addr=0x{dev_addr:02X}  {'ACK' if ack else 'NACK'}")
                if ack:
                    any_ack = True

        # Test 3: quick register dump after probes
        print("\n[Test 3] Controller registers after probing")
        i2c.dump_regs(h, b, label="after probe")

        if any_ack:
            print("\nSUMMARY: EEPROM is responding on at least one address.")
        else:
            print("\nSUMMARY: No ACK on any address.")
        return any_ack

    def f_read(self, v_offset, v_length, v_output=None, v_print=True):
        self._resolve_dev_addr()
        print(f"\nReading {self.dora.f_size_format(v_length)} from 0x{v_offset:06X} ...")
        t0 = time.time()
        data = self._read_with_progress(v_offset, v_length, prefix="Read")
        elapsed = time.time() - t0
        speed = v_length / elapsed if elapsed > 0 else 0
        print(f"Read complete in {elapsed:.1f}s ({self.dora.f_size_format(int(speed))}/s)")

        if v_output:
            with open(v_output, 'wb') as f:
                f.write(data)
            print(f"Saved to {v_output}")
        elif v_print:
            _hexdump(data, v_offset, size_fmt=self.dora.f_size_format)
        return data

    def f_write(self, v_offset, v_data, v_verify=False):
        self._resolve_dev_addr()
        total = len(v_data)
        print(f"\nWriting {self.dora.f_size_format(total)} to 0x{v_offset:06X} ...")
        t0 = time.time()
        self._write_with_progress(v_offset, v_data, prefix="Program")
        elapsed = time.time() - t0
        speed = total / elapsed if elapsed > 0 else 0
        print(f"Write complete in {elapsed:.1f}s ({self.dora.f_size_format(int(speed))}/s)")

        if v_verify:
            print(f"Verifying {self.dora.f_size_format(total)} at 0x{v_offset:06X} ...")
            readback = self._read_with_progress(v_offset, total, prefix="Verify")
            if not self._compare_verify(readback, v_data):
                raise RuntimeError(f"Write verify mismatch at 0x{v_offset:06X}")

    def f_burn(self, v_binFile, v_offset=0x0, v_verify=False):
        with open(v_binFile, 'rb') as f:
            v_data = f.read()

        self._resolve_dev_addr()
        jtag.jtag_set_check(False)

        try:
            total_size = len(v_data)
            print(f'Burning: {self.dora.f_size_format(total_size)} to eeprom at 0x{v_offset:06X} with verify={"ON" if v_verify else "OFF"} ...')

            steps = 2 if v_verify else 1
            t_total = time.time()

            # Step 1: Program
            print(f"\n[1/{steps}] Programming {self.dora.f_size_format(total_size)} ...")
            t0 = time.time()
            self._write_with_progress(v_offset, v_data, prefix="Program")
            print(f"      Program done in {time.time() - t0:.1f}s")

            # Step 2: Verify (optional)
            if v_verify:
                print(f"\n[2/{steps}] Verifying ...")
                t0 = time.time()
                readback = self._read_with_progress(v_offset, total_size, prefix="Verify")
                if not self._compare_verify(readback, v_data):
                    raise RuntimeError("Burn verify failed")
                print(f"      Verify done in {time.time() - t0:.1f}s")

            total_elapsed = time.time() - t_total
            print(f"\nAll burn process complete in {total_elapsed:.1f}s")
        finally:
            jtag.jtag_set_check(True)

    def f_verify(self, v_binFile, v_offset):
        with open(v_binFile, 'rb') as f:
            v_expectedData = f.read()

        self._resolve_dev_addr()
        jtag.jtag_set_check(False)
        try:
            length = len(v_expectedData)
            print(f"\nVerifying {self.dora.f_size_format(length)} at 0x{v_offset:06X} ...")
            t0 = time.time()
            readback = self._read_with_progress(v_offset, length, prefix="Verify")
            is_match = self._compare_verify(readback, v_expectedData)
            elapsed = time.time() - t0
            speed = length / elapsed if elapsed > 0 else 0
            print(f"Verify done in {elapsed:.1f}s ({self.dora.f_size_format(int(speed))}/s)")
            if not is_match:
                raise RuntimeError("Verify failed")
        finally:
            jtag.jtag_set_check(True)
