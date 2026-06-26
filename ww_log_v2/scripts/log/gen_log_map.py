#!/usr/bin/env python3
"""
gen_log_map.py - ww_log v1 unified map generator.

Reads log_config.json (modules -> dirs), scans every .c under those dirs,
extracts each LOG_xxx(...) call (line, level, fmt, param count) and produces
ONE map file, ww_log_map.json, that drives both the build and the decoder.

  file_id = module_id*128 + offset      (module_id 0-31, offset 0-127)

file_id LOCKING (CLAUDE.md §2): on regeneration the previous ww_log_map.json
is read first; files already assigned keep their offset, new files take the
lowest free offset, and offsets of deleted files are reserved (never recycled)
so historical logs from old firmware still decode correctly.

Modes:
  gen_log_map.py <config> [--out ww_log_map.json]   scan + (re)write the map
  gen_log_map.py <config> --makefile                derive build/file_ids.mk  (stdout)
  gen_log_map.py <config> --header                  derive auto_file_ids.h    (stdout)

Options:
  --root <path>   project root used to resolve dirs in config and to anchor
                  relative file paths in the map (defaults to CWD).

The --makefile / --header modes DERIVE from an existing ww_log_map.json
(generate it first).  Encoding tag: file12_line14_pcnt6.
"""

import json
import os
import re
import sys
from datetime import datetime, timezone

ENCODING_TAG = "file12_line14_pcnt6"
LEVELS = ("ERR", "WRN", "INF", "DBG")
LOG_CALL_RE = re.compile(r'\bN?_?LOG_(ERR|WRN|INF|DBG)\s*\(')


# ----------------------------------------------------------------------------
# config / IO helpers
# ----------------------------------------------------------------------------

def load_json(path):
    try:
        with open(path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except FileNotFoundError:
        return None
    except json.JSONDecodeError as e:
        sys.exit("Error: invalid JSON in '%s': %s" % (path, e))


def load_config(path):
    cfg = load_json(path)
    if cfg is None:
        sys.exit("Error: config file '%s' not found" % path)
    return cfg


def norm(p):
    return p.replace('\\', '/')


def safe_var(path):
    return path.replace('/', '_').replace('.', '_').replace('-', '_')


# ----------------------------------------------------------------------------
# scanning
# ----------------------------------------------------------------------------

def module_of(path, modules):
    """Longest dir-prefix wins. Returns module name or None."""
    best_name, best_len = None, -1
    for name, info in modules.items():
        for d in info.get('dirs', []):
            d = norm(d).rstrip('/')
            if path == d or path.startswith(d + '/'):
                if len(d) > best_len:
                    best_name, best_len = name, len(d)
    return best_name


def scan_c_files(modules, root='.'):
    """Return {path: module_name} for every .c under any module dir.

    Paths in the returned dict are relative to *root* so they stay stable
    regardless of where the script is invoked from.
    """
    found = {}
    root = os.path.abspath(root)
    for name, info in modules.items():
        for d in info.get('dirs', []):
            d_abs = os.path.normpath(os.path.join(root, d))
            if not os.path.isdir(d_abs):
                continue
            for walk_root, _dirs, files in os.walk(d_abs):
                for fn in files:
                    if fn.endswith('.c'):
                        abs_p = os.path.join(walk_root, fn)
                        p = norm(os.path.relpath(abs_p, root))
                        # assign by longest prefix (handles overlapping dirs)
                        found[p] = module_of(p, modules)
    return found


def split_top_level_args(s):
    """Split a C argument list on top-level commas (respects strings/parens)."""
    args, depth, i, cur = [], 0, 0, []
    in_str = in_chr = False
    while i < len(s):
        c = s[i]
        if in_str:
            cur.append(c)
            if c == '\\':
                cur.append(s[i + 1]); i += 2; continue
            if c == '"':
                in_str = False
        elif in_chr:
            cur.append(c)
            if c == '\\':
                cur.append(s[i + 1]); i += 2; continue
            if c == "'":
                in_chr = False
        else:
            if c == '"':
                in_str = True; cur.append(c)
            elif c == "'":
                in_chr = True; cur.append(c)
            elif c in '([{':
                depth += 1; cur.append(c)
            elif c in ')]}':
                depth -= 1; cur.append(c)
            elif c == ',' and depth == 0:
                args.append(''.join(cur).strip()); cur = []
            else:
                cur.append(c)
        i += 1
    tail = ''.join(cur).strip()
    if tail or args:
        args.append(tail)
    return args


def extract_fmt(arg0):
    """Concatenate adjacent string-literal contents from the first argument."""
    parts = re.findall(r'"((?:\\.|[^"\\])*)"', arg0)
    return ''.join(parts)


def count_placeholders(fmt):
    """Count printf conversion specifiers, ignoring %%."""
    return len(re.findall(r'%[^%]', fmt.replace('%%', '')))


def matching_paren(text, open_idx):
    """Index just after the ')' matching the '(' at open_idx."""
    depth, i = 0, open_idx
    in_str = in_chr = False
    while i < len(text):
        c = text[i]
        if in_str:
            if c == '\\':
                i += 2; continue
            if c == '"':
                in_str = False
        elif in_chr:
            if c == '\\':
                i += 2; continue
            if c == "'":
                in_chr = False
        elif c == '"':
            in_str = True
        elif c == "'":
            in_chr = True
        elif c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return -1


def extract_entries(path, file_id):
    """Parse all LOG_xxx() calls in a file -> list of entry dicts."""
    try:
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            text = f.read()
    except OSError as e:
        print("Warning: cannot read %s: %s" % (path, e), file=sys.stderr)
        return []

    entries = []
    for m in LOG_CALL_RE.finditer(text):
        level = m.group(1)
        open_paren = text.index('(', m.start())
        end = matching_paren(text, open_paren)
        if end < 0:
            print("Warning: %s: unbalanced LOG_%s(" % (path, level), file=sys.stderr)
            continue
        inner = text[open_paren + 1:end - 1]
        args = split_top_level_args(inner)
        line = text.count('\n', 0, m.start()) + 1
        fmt = extract_fmt(args[0]) if args else ''
        pcnt = max(0, len(args) - 1) if args and args[0] else 0

        # build-time validation (warn only)
        ph = count_placeholders(fmt)
        if ph != pcnt:
            print("Warning: %s:%d LOG_%s placeholders=%d but args=%d  fmt=%r"
                  % (path, line, level, ph, pcnt, fmt), file=sys.stderr)
        if '%s' in fmt:
            print("Warning: %s:%d LOG_%s uses %%s (param value cannot be "
                  "restored at decode)" % (path, line, level), file=sys.stderr)

        entries.append({"file_id": file_id, "line": line,
                        "level": level, "fmt": fmt})
    entries.sort(key=lambda e: (e["file_id"], e["line"]))
    return entries


# ----------------------------------------------------------------------------
# generate (scan + merge with old map for file_id locking)
# ----------------------------------------------------------------------------

def generate(config, old_map, root='.'):
    modules = config.get('modules', {})
    root = os.path.abspath(root)
    scanned = scan_c_files(modules, root)        # path -> module name (relative to root)

    # --- recover previous assignments (path -> offset) and reserved offsets ---
    prev = {}                                    # path -> (module, offset)
    reserved = {}                                # module name -> set(offset)
    for name in modules:
        reserved[name] = set()
    if old_map:
        for fid_str, info in old_map.get('files', {}).items():
            fid = int(fid_str)
            offset = fid & 0x7F
            mod = info.get('module')
            prev[info['path']] = (mod, offset)
            reserved.setdefault(mod, set()).add(offset)

    def next_offset(mod):
        used = reserved.setdefault(mod, set())
        for i in range(128):
            if i not in used:
                return i
        sys.exit("Error: module '%s' exhausted its 128 file slots" % mod)

    files = {}        # file_id(int) -> {path, module, present}
    unregistered = []

    # keep deleted files as reserved placeholders (offset not recycled)
    scanned_set = set(scanned)
    for path, (mod, offset) in prev.items():
        if path not in scanned_set and mod in modules:
            mid = modules[mod]['id']
            files[mid * 128 + offset] = {"path": path, "module": mod,
                                         "present": False}

    # assign present files (lock existing, append new in lowest free slot)
    for path in sorted(scanned):
        mod = scanned[path]
        if mod is None:
            unregistered.append(path)
            continue
        mid = modules[mod]['id']
        if path in prev and prev[path][0] == mod:
            offset = prev[path][1]
        else:
            offset = next_offset(mod)
        reserved[mod].add(offset)
        files[mid * 128 + offset] = {"path": path, "module": mod,
                                     "present": True}

    policy = config.get('unregistered', 'warn')
    for path in unregistered:
        if policy == 'error':
            sys.exit("Error: %s is not under any module dir" % path)
        print("Warning: %s not under any module dir -> logs disabled" % path,
              file=sys.stderr)

    # --- entries (scan present files only) ---
    entries = []
    for fid in sorted(files):
        if files[fid]["present"]:
            abs_path = os.path.join(root, files[fid]["path"])
            entries.extend(extract_entries(abs_path, fid))
    entries.sort(key=lambda e: (e["file_id"], e["line"]))

    out_modules = {}
    for name, info in sorted(modules.items(), key=lambda kv: kv[1]['id']):
        out_modules[str(info['id'])] = {"name": name,
                                        "enable": bool(info.get('enable', True))}

    out_files = {}
    for fid in sorted(files):
        out_files[str(fid)] = files[fid]

    version = (old_map or {}).get('meta', {}).get('version', "")
    return {
        "meta": {
            "version": version,
            "build_time": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "encoding": ENCODING_TAG,
        },
        "modules": out_modules,
        "files": out_files,
        "entries": entries,
    }


# ----------------------------------------------------------------------------
# derive build artifacts from an existing map
# ----------------------------------------------------------------------------

def emit_makefile(config, the_map):
    modules = config.get('modules', {})
    mod_enabled = {info['id']: info.get('enable', True)
                   for info in modules.values()}
    lines = ["# Auto-generated by tools/gen_log_map.py -- DO NOT EDIT", ""]
    n_en = n_dis = 0
    for fid_str, info in sorted(the_map.get('files', {}).items(), key=lambda kv: kv[1]['path']):
        if not info.get('present', True):
            continue
        fid = int(fid_str)
        mid = fid >> 7
        path = info['path']
        if not mod_enabled.get(mid, True):
            lines.append("# %s: module %s DISABLED" % (path, info['module']))
            n_dis += 1
            continue
        v = safe_var(path)
        lines.append("FILE_ID_%s = %d" % (v, fid))
        lines.append("MODULE_ID_%s = %d" % (v, mid))
        lines.append("MODULE_STATIC_EN_%s = 1" % v)
        n_en += 1
    lines.append("")
    lines.append("# Summary: %d files enabled, %d disabled" % (n_en, n_dis))
    return '\n'.join(lines)


def emit_header(config, the_map):
    modules = config.get('modules', {})
    lines = [
        "/**",
        " * @file auto_file_ids.h",
        " * @brief Auto-generated module/file IDs. DO NOT EDIT.",
        " * Generated by scripts/log/gen_log_map.py from log_config.json + ww_log_map.json",
        " */",
        "",
        "#ifndef AUTO_FILE_IDS_H",
        "#define AUTO_FILE_IDS_H",
        "",
        "/* ===== Module IDs ===== */",
    ]
    for name, info in sorted(modules.items(), key=lambda kv: kv[1]['id']):
        lines.append("#define WW_LOG_MODULE_%-10s %d" % (name.upper(), info['id']))
    lines.append("")
    lines.append("#define WW_LOG_MODULE_MAX  32")
    lines.append("")
    lines.append("/* ===== File IDs ===== */")
    for fid_str, info in sorted(the_map.get('files', {}).items(),
                                key=lambda kv: int(kv[0])):
        if not info.get('present', True):
            lines.append("/* %s: removed, id reserved */" % info['path'])
            continue
        macro = "FILE_ID_" + safe_var(info['path']).upper()
        lines.append("#define %-40s %s" % (macro, fid_str))
    lines.append("")
    lines.append("#endif /* AUTO_FILE_IDS_H */")
    return '\n'.join(lines)


# ----------------------------------------------------------------------------
# main
# ----------------------------------------------------------------------------

def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)

    config_path = sys.argv[1]
    flags = sys.argv[2:]
    config = load_config(config_path)

    root = os.getcwd()
    if "--root" in flags:
        root = flags[flags.index("--root") + 1]
    root = os.path.abspath(root)

    map_path = "ww_log_map.json"
    if "--out" in flags:
        map_path = flags[flags.index("--out") + 1]

    if "--makefile" in flags:
        the_map = load_json(map_path)
        if the_map is None:
            sys.exit("Error: %s missing; run generate first" % map_path)
        print(emit_makefile(config, the_map))
        return
    if "--header" in flags:
        the_map = load_json(map_path)
        if the_map is None:
            sys.exit("Error: %s missing; run generate first" % map_path)
        print(emit_header(config, the_map))
        return

    # default: scan + (re)generate map
    old_map = load_json(map_path)
    new_map = generate(config, old_map, root)
    with open(map_path, 'w', encoding='utf-8') as f:
        json.dump(new_map, f, indent=2, ensure_ascii=False)
        f.write('\n')
    print("Generated %s: %d files, %d entries"
          % (map_path, len(new_map['files']), len(new_map['entries'])),
          file=sys.stderr)


if __name__ == '__main__':
    main()
