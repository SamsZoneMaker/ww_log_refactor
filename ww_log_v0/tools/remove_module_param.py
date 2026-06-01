#!/usr/bin/env python3
"""
Remove module_id parameter from LOG_XXX calls
Converts: LOG_INF(WW_LOG_MODULE_XXX, "msg") -> LOG_INF("msg")
"""

import re
import sys
import os
from pathlib import Path

def process_file(filepath):
    """Process a single file to remove module parameters"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        original = content

        # Pattern to match LOG_XXX(WW_LOG_MODULE_XXX, ...)
        # Captures the LOG macro name and everything after the first comma
        pattern = r'(LOG_(?:ERR|WRN|INF|DBG))\(WW_LOG_MODULE_\w+,\s*'

        # Replace with just LOG_XXX(
        content = re.sub(pattern, r'\1(', content)

        if content != original:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return True
        return False

    except Exception as e:
        print(f"Error processing {filepath}: {e}", file=sys.stderr)
        return False

def main():
    """Main entry point"""
    if len(sys.argv) < 2:
        print("Usage: remove_module_param.py <directory>")
        sys.exit(1)

    root_dir = sys.argv[1]

    # Find all .c files
    c_files = list(Path(root_dir).rglob('*.c'))

    modified_count = 0
    for filepath in c_files:
        if process_file(filepath):
            print(f"Modified: {filepath}")
            modified_count += 1

    print(f"\nTotal files modified: {modified_count}")

if __name__ == '__main__':
    main()
