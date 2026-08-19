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
matters: the log core tests these both ways (`#ifdef CONFIG_N_LOG_BACKEND_RAM`
in one file, `#if (CONFIG_N_LOG_BACKEND_RAM == 1)` in another), and a 0 would
satisfy the second while defeating the first.

Two sanity checks that menuconfig would otherwise give us for free, since a
.conf here is hand-edited:
  * exactly one N_LOG_MODE_* selected -- zero of them silently falls through to
    DISABLED in n_ww_log_macro.h, i.e. firmware that ships mute;
  * EXT_MEM implies RAM -- the external backend drains the RAM ring.

Usage:
    conf_to_autoconf.py <in.conf> [--write <out.h>]
"""

import os
import re
import sys

MODE_SYMBOLS = ("CONFIG_N_LOG_MODE_STRING",
                "CONFIG_N_LOG_MODE_ENCODE",
                "CONFIG_N_LOG_MODE_DISABLE")

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
    names = {s for s, _ in symbols}

    modes = [s for s in MODE_SYMBOLS if s in names]
    if len(modes) != 1:
        sys.exit("Error: exactly one of %s must be set, found %s"
                 % ('/'.join(MODE_SYMBOLS), modes or 'none'))

    if ("CONFIG_N_LOG_BACKEND_EXT_MEM" in names
            and "CONFIG_N_LOG_BACKEND_RAM" not in names):
        sys.exit("Error: CONFIG_N_LOG_BACKEND_EXT_MEM requires "
                 "CONFIG_N_LOG_BACKEND_RAM (the external backend has no "
                 "storage of its own; it drains the RAM ring)")


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
