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
  --write <path>  write a derived artifact to <path> instead of stdout, and
                  only if its content actually changed. Every output of this
                  script is write-if-changed, which is what keeps a source edit
                  (new line numbers, same file IDs) from touching file_ids.mk /
                  auto_file_ids.h and forcing a full rebuild.

The --makefile / --header modes DERIVE from an existing ww_log_map.json
(generate it first).  Encoding tag: file12_line14_lvl2_pcnt4.
"""

import json
import os
import re
import sys
from datetime import datetime, timezone

ENCODING_TAG = "file12_line14_lvl2_pcnt4"
LEVELS = ("ERR", "WRN", "INF", "DBG")
LOG_CALL_RE = re.compile(r'\bN?_?LOG_(ERR|WRN|INF|DBG)\s*\(')

# Helper macros (n_ww_log_macro.h) that expand to an N_LOG_ERR with a FIXED
# format string. __LINE__ inside a macro body expands at the INVOCATION site, so
# these emit a real entry attributed to the .c line that used them -- without an
# entry in the map every one of them decodes as <no map entry>.
#   name -> (level, fmt as written in the header, param count)
# The _WO_PRINT variants log nothing and are deliberately absent: the trailing
# \s*\( below keeps e.g. N_RETURN_IF_TRUE_WO_PRINT( from matching the shorter
# N_RETURN_IF_TRUE (the '_' after the name is a word char, so no '(' follows).
HELPER_MACROS = {
    'N_RETURN_CODE_IF_TRUE': ('ERR', r'-- line:%d rc:0x%x\r\n', 2),
    'N_RETURN_IF_TRUE':      ('ERR', r'-- line:%d rc:0x%x\r\n', 2),
    'N_BREAK_IF_TRUE':       ('ERR', r'rc:0x%x\r\n', 1),
    'N_CONTINUE_IF_TRUE':    ('ERR', r'rc:0x%x\r\n', 1),
    'N_PRINT_IF_TRUE':       ('ERR', r'rc:0x%x\r\n', 1),
}
HELPER_CALL_RE = re.compile(
    r'\b(' + '|'.join(sorted(HELPER_MACROS, key=len, reverse=True)) + r')\s*\(')


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


def basename(path):
    return norm(path).rsplit('/', 1)[-1]


def disambiguate_names(paths):
    """path -> shortest unique trailing path segment(s), for display.

    A file whose basename is unique keeps just the basename (so existing STR /
    decoder output is unchanged); only genuine collisions grow a directory:
        src/demo/demo_init.c        -> demo_init.c
        target/a/main.c             -> a/main.c
        target/b/main.c             -> b/main.c
    """
    paths = list(paths)
    parts = {p: norm(p).split('/') for p in paths}
    depth = max((len(v) for v in parts.values()), default=1)
    out, todo = {}, set(paths)

    for n in range(1, depth + 1):
        groups = {}
        for p in todo:
            groups.setdefault('/'.join(parts[p][-n:]), []).append(p)
        for cand, group in groups.items():
            if len(group) == 1:
                out[group[0]] = cand
        todo -= set(out)
        if not todo:
            break
    for p in todo:                       # identical full paths: cannot happen
        out[p] = norm(p)
    return out


def write_if_changed(path, text):
    """Write `text` to `path` only when it differs from what is already there.

    Keeping the mtime stable when the content is unchanged is what stops a
    source edit (which moves line numbers but no file IDs) from invalidating
    every object file -- see the Makefile's incremental-build notes.
    Returns True if the file was actually rewritten.
    """
    try:
        with open(path, 'r', encoding='utf-8') as f:
            if f.read() == text:
                return False
    except (OSError, UnicodeDecodeError):
        pass
    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)
    return True


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


def scan_files(modules, root='.', suffix='.c'):
    """Return {path: module_name} for every *suffix* file under any module dir.

    Paths in the returned dict are relative to *root* so they stay stable
    regardless of where the script is invoked from.
    """
    found = {}
    root = os.path.abspath(root)
    for _name, info in modules.items():
        for d in info.get('dirs', []):
            d_abs = os.path.normpath(os.path.join(root, d))
            if not os.path.isdir(d_abs):
                continue
            for walk_root, _dirs, files in os.walk(d_abs):
                for fn in files:
                    if fn.endswith(suffix):
                        abs_p = os.path.join(walk_root, fn)
                        p = norm(os.path.relpath(abs_p, root))
                        # assign by longest prefix (handles overlapping dirs)
                        found[p] = module_of(p, modules)
    return found


def scan_c_files(modules, root='.'):
    return scan_files(modules, root, '.c')


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

    # Helper macros expand to an N_LOG_ERR with a fixed fmt at the invocation
    # line, so synthesise their entries from the table rather than the source.
    for m in HELPER_CALL_RE.finditer(text):
        level, fmt, _pcnt = HELPER_MACROS[m.group(1)]
        line = text.count('\n', 0, m.start()) + 1
        entries.append({"file_id": file_id, "line": line,
                        "level": level, "fmt": fmt, "via": m.group(1)})

    # Two log-producing constructs on one line collapse to the same (file_id,
    # line) key and cannot be told apart at decode time.
    seen = {}
    for e in entries:
        if e["line"] in seen:
            print("Warning: %s:%d two log calls share one line -> decode is "
                  "ambiguous (%r vs %r)"
                  % (path, e["line"], seen[e["line"]]["fmt"], e["fmt"]),
                  file=sys.stderr)
        seen[e["line"]] = e

    entries.sort(key=lambda e: (e["file_id"], e["line"]))
    return entries


def warn_header_logs(paths, root):
    """Warn about log calls in .h files -- they cannot be mapped.

    A log inside a header (typically a `static inline`) is expanded once per
    including .c, so it emits the INCLUDING file's CURRENT_FILE_ID paired with
    the HEADER's line number. One source line therefore produces a different
    file_id per includer, and that line number can also collide with a real
    entry of the including .c. Neither is representable in the (file_id, line)
    map, so the honest answer is to flag it rather than emit wrong entries.
    """
    for path in sorted(paths):
        try:
            with open(os.path.join(root, path), 'r', encoding='utf-8',
                      errors='replace') as f:
                text = f.read()
        except OSError:
            continue
        hits = sorted(set(
            [text.count('\n', 0, m.start()) + 1 for m in LOG_CALL_RE.finditer(text)] +
            [text.count('\n', 0, m.start()) + 1 for m in HELPER_CALL_RE.finditer(text)]))
        for line in hits:
            print("Warning: %s:%d log call in a header cannot be decoded "
                  "(expanded per includer -> ambiguous file_id/line)"
                  % (path, line), file=sys.stderr)


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

    # Logs inside headers cannot be represented in this map -- flag them.
    warn_header_logs(scan_files(modules, root, '.h'), root)

    # Display name: basename when unique, otherwise the shortest unique path
    # tail. Consumed by the decoder (readable output) and by the Makefile
    # (STR mode's __NOTDIR_FILE__), so both agree on one name per file.
    shorts = disambiguate_names([info["path"] for info in files.values()])
    for info in files.values():
        info["short"] = shorts[info["path"]]

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
        # STR mode prints this instead of $(notdir $<): identical to the
        # basename unless two sources share one, in which case it carries just
        # enough directory to stay unambiguous.
        lines.append("SHORT_NAME_%s = %s" % (v, info.get('short', basename(path))))
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

    # --write <path> sends a derived artifact to a file instead of stdout, and
    # leaves the file (and its mtime) alone when the content is unchanged.
    write_to = flags[flags.index("--write") + 1] if "--write" in flags else None

    def emit(text):
        if write_to is None:
            print(text)
        elif write_if_changed(write_to, text + '\n'):
            print("Updated %s" % write_to, file=sys.stderr)

    if "--makefile" in flags or "--header" in flags:
        the_map = load_json(map_path)
        if the_map is None:
            sys.exit("Error: %s missing; run generate first" % map_path)
        emit(emit_makefile(config, the_map) if "--makefile" in flags
             else emit_header(config, the_map))
        return

    # default: scan + (re)generate map
    old_map = load_json(map_path)
    new_map = generate(config, old_map, root)

    # Keep build_time from the old map when nothing else changed, so a rebuild
    # that found no source change does not churn the file (and its git diff).
    if old_map is not None:
        probe = dict(new_map)
        probe["meta"] = dict(new_map["meta"],
                             build_time=old_map.get("meta", {}).get("build_time", ""))
        if probe == old_map:
            new_map = probe

    text = json.dumps(new_map, indent=2, ensure_ascii=False) + '\n'
    if write_if_changed(map_path, text):
        print("Generated %s: %d files, %d entries"
              % (map_path, len(new_map['files']), len(new_map['entries'])),
              file=sys.stderr)


if __name__ == '__main__':
    main()
