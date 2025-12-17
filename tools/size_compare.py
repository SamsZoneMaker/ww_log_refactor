#!/usr/bin/env python3
"""
Binary Size Comparison Tool
Analyzes and compares binary sizes across different logging modes
"""

import sys
import subprocess
import os

def get_size_info(binary_path):
    """Get size information from binary using 'size' command"""
    try:
        result = subprocess.run(['size', binary_path],
                              capture_output=True, text=True, check=True)
        lines = result.stdout.strip().split('\n')
        if len(lines) >= 2:
            # Parse size output: text    data     bss     dec     hex filename
            parts = lines[1].split()
            return {
                'text': int(parts[0]),
                'data': int(parts[1]),
                'bss': int(parts[2]),
                'total': int(parts[3]),
                'filename': os.path.basename(binary_path)
            }
    except Exception as e:
        print(f"Error analyzing {binary_path}: {e}", file=sys.stderr)
        return None

def check_strings(binary_path):
    """Check for format strings in binary"""
    try:
        result = subprocess.run(['strings', binary_path],
                              capture_output=True, text=True, check=True)
        strings_output = result.stdout

        # Look for common log format patterns
        log_patterns = [
            "initializing", "Starting", "completed", "failed",
            "Status", "Configuration", "Hardware", "Test"
        ]

        found_strings = []
        for pattern in log_patterns:
            if pattern in strings_output:
                # Count occurrences
                count = strings_output.count(pattern)
                found_strings.append((pattern, count))

        return found_strings
    except Exception as e:
        return []

def print_comparison(str_info, enc_info, dis_info):
    """Print detailed comparison"""

    print("\n┌─────────────────┬──────────────┬──────────────┬──────────────┐")
    print("│ Section         │ STR Mode     │ ENC Mode     │ DIS Mode     │")
    print("├─────────────────┼──────────────┼──────────────┼──────────────┤")

    # Text section
    print(f"│ .text (code)    │ {str_info['text']:>10,} B │ {enc_info['text']:>10,} B │ {dis_info['text']:>10,} B │")

    # Data section
    print(f"│ .data (init)    │ {str_info['data']:>10,} B │ {enc_info['data']:>10,} B │ {dis_info['data']:>10,} B │")

    # BSS section
    print(f"│ .bss (uninit)   │ {str_info['bss']:>10,} B │ {enc_info['bss']:>10,} B │ {dis_info['bss']:>10,} B │")

    print("├─────────────────┼──────────────┼──────────────┼──────────────┤")

    # Total
    print(f"│ TOTAL           │ {str_info['total']:>10,} B │ {enc_info['total']:>10,} B │ {dis_info['total']:>10,} B │")

    print("└─────────────────┴──────────────┴──────────────┴──────────────┘")

    # Calculate reductions
    print("\n📊 Size Reduction Analysis:")
    print()

    str_total = str_info['text']
    enc_total = enc_info['text']
    dis_total = dis_info['text']

    enc_reduction = ((str_total - enc_total) / str_total) * 100
    dis_reduction = ((str_total - dis_total) / str_total) * 100

    print(f"  ENCODE mode vs STRING mode:")
    print(f"    • Code size: {str_total - enc_total:,} bytes smaller")
    print(f"    • Reduction: {enc_reduction:.1f}%")
    print()

    print(f"  DISABLED mode vs STRING mode:")
    print(f"    • Code size: {str_total - dis_total:,} bytes smaller")
    print(f"    • Reduction: {dis_reduction:.1f}%")
    print()

    # File sizes
    print("📁 Total Binary Sizes (including all sections):")
    for info in [str_info, enc_info, dis_info]:
        filename = info['filename']
        total_kb = info['total'] / 1024
        print(f"  • {filename:25s}: {total_kb:>7.2f} KB ({info['total']:,} bytes)")

def main():
    if len(sys.argv) != 4:
        print("Usage: python3 size_compare.py <str_binary> <enc_binary> <dis_binary>")
        sys.exit(1)

    str_bin, enc_bin, dis_bin = sys.argv[1], sys.argv[2], sys.argv[3]

    # Get size info
    str_info = get_size_info(str_bin)
    enc_info = get_size_info(enc_bin)
    dis_info = get_size_info(dis_bin)

    if not all([str_info, enc_info, dis_info]):
        print("Error: Failed to analyze one or more binaries", file=sys.stderr)
        sys.exit(1)

    # Print comparison
    print_comparison(str_info, enc_info, dis_info)

    # Check for format strings in encode mode
    print("\n🔍 Format String Check (ENCODE mode):")
    enc_strings = check_strings(enc_bin)

    if enc_strings:
        print(f"  ⚠️  Found {len(enc_strings)} potential format strings:")
        for pattern, count in enc_strings[:5]:  # Show first 5
            print(f"     - \"{pattern}\" ({count} occurrence(s))")
        print()
        print("  💡 These strings should be removed in encode mode for maximum size reduction.")
    else:
        print("  ✅ No obvious format strings detected in binary.")

    print()

if __name__ == '__main__':
    main()
