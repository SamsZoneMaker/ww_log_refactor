#!/usr/bin/env python3
"""
conf_to_autoconf.py - .conf -> autoconf.h, for the PC simulator only.

The firmware build already turns its Kconfig .conf files into autoconf.h; the
simulator has no Kconfig, so this stands in for that one step. It is NOT part
of the log module and does not travel to the firmware tree -- keeping it here,
rather than folding it into gen_log_map.py, is what lets that script stay purely
about file IDs and the map, so it can be merged into the firmware verbatim with
nothing dead in it.

Kconfig's conversion rules, reproduced:
    CONFIG_X=y              ->  #define CONFIG_X 1
    CONFIG_X=n              ->  omitted   (as is "# CONFIG_X is not set")
    CONFIG_X=<number>       ->  #define CONFIG_X <number>
    CONFIG_X="text"         ->  #define CONFIG_X "text"

Omitting a disabled symbol rather than defining it to 0 is the part that
matters: the log core uses `#ifdef CONFIG_N_LOG_BACKEND_*`, matching the
firmware Kconfig convention.

The validator mirrors the important Kconfig dependencies and numeric ranges
because this .conf is hand-edited rather than produced by menuconfig.

Usage:
    conf_to_autoconf.py <in.conf> [--write <out.h>]
"""

import os
import re
import sys

OBSOLETE_CHOICE_SYMBOLS = (
    "CONFIG_N_LOG_MODE_STRING",
    "CONFIG_N_LOG_MODE_ENCODE",
    "CONFIG_N_LOG_MODE_DISABLE",
    *("CONFIG_N_LOG_COMPILE_THRESHOLD_" + x
      for x in ("ERR", "WRN", "INF", "DBG")),
    *("CONFIG_N_LOG_RUNTIME_LEVEL_" + x
      for x in ("ERR", "WRN", "INF", "DBG")),
    *("CONFIG_N_LOG_EXT_LEVEL_THRESHOLD_" + x
      for x in ("ERR", "WRN", "INF", "DBG")),
    "CONFIG_N_LOG_EXT_FULL_FREEZE",
    "CONFIG_N_LOG_EXT_FULL_ERASE",
)

REMOVED_TUNING_SYMBOLS = (
    "CONFIG_N_LOG_RAM_FLUSH_THRESHOLD",
    "CONFIG_N_LOG_EXT_FLUSH_STAGE_SIZE",
    "CONFIG_N_LOG_WRITE_TIMEOUT_MS",
    "CONFIG_N_LOG_FLUSH_TASK_STACK_SIZE",
    "CONFIG_N_LOG_FLUSH_TASK_PRIORITY",
)

LINE_RE = re.compile(r'^\s*(CONFIG_[A-Za-z0-9_]+)\s*=\s*(.+?)\s*$')
NOT_SET_RE = re.compile(r'^\s*#\s*(CONFIG_[A-Za-z0-9_]+)\s+is not set\s*$')


def parse_conf(path):
    """Return an ordered list of (symbol, value) for the symbols that are SET.

    value is True for a bool, otherwise the literal text to emit.
    """
    out = []
    try:
        with open(path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except OSError as e:
        sys.exit("Error: cannot read %s: %s" % (path, e))

    for n, raw in enumerate(lines, 1):
        if NOT_SET_RE.match(raw) or not raw.strip() or raw.lstrip().startswith('#'):
            continue
        m = LINE_RE.match(raw)
        if not m:
            sys.exit("Error: %s:%d: cannot parse %r" % (path, n, raw.rstrip()))
        sym, val = m.group(1), m.group(2)
        if val == 'n':
            continue                       # explicitly disabled -> omit
        out.append((sym, True if val == 'y' else val))
    return out


def validate(symbols):
    values = {}
    for name, value in symbols:
        if name in values:
            sys.exit("Error: duplicate setting for %s" % name)
        values[name] = value
    names = set(values)

    def integer(name, low, high, multiple=None):
        if name not in values:
            sys.exit("Error: %s is required by the selected backends" % name)
        try:
            value = int(str(values[name]), 0)
        except ValueError:
            sys.exit("Error: %s must be an integer, got %r"
                     % (name, values[name]))
        if value < low or value > high:
            sys.exit("Error: %s must be in [%d, %d], got %d"
                     % (name, low, high, value))
        if multiple and value % multiple:
            sys.exit("Error: %s must be a multiple of %d, got %d"
                     % (name, multiple, value))
        return value

    def boolean(name):
        if name in values and values[name] is not True:
            sys.exit("Error: %s is boolean and must be y or n, got %r"
                     % (name, values[name]))

    def forbid(group, reason):
        selected = sorted(s for s in group if s in names)
        if selected:
            sys.exit("Error: %s cannot be set %s"
                     % (', '.join(selected), reason))

    forbid(OBSOLETE_CHOICE_SYMBOLS,
           "after the numeric-enum configuration migration")
    forbid(REMOVED_TUNING_SYMBOLS,
           "because this tuning is owned by the functional headers")

    for name in ("CONFIG_N_LOG",
                 "CONFIG_N_LOG_BACKEND_UART",
                 "CONFIG_N_LOG_BACKEND_RAM",
                 "CONFIG_N_LOG_BACKEND_EXT_MEM",
                 "CONFIG_N_LOG_EXT_FLUSH_MARKER"):
        boolean(name)

    if "CONFIG_N_LOG" not in names:
        stray = sorted(s for s in names if s.startswith("CONFIG_N_LOG_"))
        if stray:
            sys.exit("Error: CONFIG_N_LOG is disabled but log options are set: %s"
                     % ', '.join(stray))
        return

    mode = integer("CONFIG_N_LOG_MODE", 1, 3)
    active = mode != 3
    if active:
        integer("CONFIG_N_LOG_COMPILE_THRESHOLD", 0, 3)
        integer("CONFIG_N_LOG_RUNTIME_THRESHOLD", 0, 3)
    else:
        forbid(("CONFIG_N_LOG_COMPILE_THRESHOLD",
                "CONFIG_N_LOG_RUNTIME_THRESHOLD"),
               "in N_LOG_MODE_DISABLE")

    uart = "CONFIG_N_LOG_BACKEND_UART" in names
    ram = "CONFIG_N_LOG_BACKEND_RAM" in names
    ext = "CONFIG_N_LOG_BACKEND_EXT_MEM" in names
    if not active and (uart or ram or ext):
        sys.exit("Error: backends cannot be selected in N_LOG_MODE_DISABLE")
    if mode != 2 and (ram or ext):
        sys.exit("Error: RAM and EXT_MEM backends require CONFIG_N_LOG_MODE=2")
    if ext and not ram:
        sys.exit("Error: CONFIG_N_LOG_BACKEND_EXT_MEM requires "
                 "CONFIG_N_LOG_BACKEND_RAM (it drains the RAM ring)")

    if ext:
        integer("CONFIG_N_LOG_EXT_LEVEL_THRESHOLD", 0, 3)
        integer("CONFIG_N_LOG_EXT_FULL", 1, 2)
        integer("CONFIG_N_LOG_FLUSH_TIMEOUT_MS", 1, 86400000)
    else:
        forbid(("CONFIG_N_LOG_EXT_LEVEL_THRESHOLD",
                "CONFIG_N_LOG_EXT_FULL",
                "CONFIG_N_LOG_EXT_FLUSH_MARKER",
                "CONFIG_N_LOG_FLUSH_TIMEOUT_MS"),
               "without the EXT_MEM backend")


def render(symbols, src):
    lines = [
        "/**",
        " * @file log_autoconf.h",
        " * @brief Auto-generated from %s. DO NOT EDIT." % src,
        " *",
        " * Sim stand-in for the firmware's Kconfig-generated autoconf.h.",
        " */",
        "",
        "#ifndef LOG_AUTOCONF_H",
        "#define LOG_AUTOCONF_H",
        "",
    ]
    for sym, val in symbols:
        lines.append("#define %-37s %s" % (sym, "1" if val is True else val))
    lines += ["", "#endif /* LOG_AUTOCONF_H */"]
    return '\n'.join(lines)


def write_if_changed(path, text):
    """Leave the file (and its mtime) alone when the content is unchanged, so a
    no-op regeneration does not invalidate every object that includes it."""
    try:
        with open(path, 'r', encoding='utf-8') as f:
            if f.read() == text:
                return False
    except (OSError, UnicodeDecodeError):
        pass
    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)
    return True


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    conf_path = sys.argv[1]
    flags = sys.argv[2:]

    symbols = parse_conf(conf_path)
    validate(symbols)
    text = render(symbols, os.path.basename(conf_path)) + '\n'

    if "--write" in flags:
        dst = flags[flags.index("--write") + 1]
        if write_if_changed(dst, text):
            print("Updated %s" % dst, file=sys.stderr)
    else:
        print(text, end='')


if __name__ == '__main__':
    main()
