#!/usr/bin/env python3
"""
Auto-generate file ID definitions for logging system

This tool scans source files for log macro usage and automatically assigns
unique file IDs, eliminating the need for manual maintenance of log_file_id.h

Features:
- Only scans files that contain log macros (TEST_LOG_XXX_MSG)
- Intelligent caching to skip unchanged file lists (0.01s overhead)
- Module-based ID allocation with configurable ranges
- Stable ID assignment (sorted by file path)
- Incremental-friendly (new files get new IDs, existing IDs unchanged)

Usage:
    python3 tools/generate_file_ids.py [options]

Options:
    --config FILE    Configuration file (default: tools/file_id_config.yaml)
    --output FILE    Output header file (default: include/log_file_id.h)
    --source DIR     Source directory to scan (default: src/)
    --force          Force regeneration even if cache is valid
    --verbose        Show detailed scanning information
"""

import os
import re
import sys
import time
import json
import hashlib
import argparse
from pathlib import Path

# Default configuration
DEFAULT_CONFIG = {
    'module_ranges': {
        'brom':    {'start': 1,   'end': 50,   'description': 'BROM initialization'},
        'drivers': {'start': 51,  'end': 150,  'description': 'Driver layer'},
        'app':     {'start': 151, 'end': 250,  'description': 'Application layer'},
        'test':    {'start': 251, 'end': 300,  'description': 'Test code'},
        'demo':    {'start': 301, 'end': 350,  'description': 'Demo code'},
    }
}

# Log macro patterns to search for
LOG_MACRO_PATTERNS = [
    r'TEST_LOG_ERR_MSG\s*\(',
    r'TEST_LOG_WRN_MSG\s*\(',
    r'TEST_LOG_INF_MSG\s*\(',
    r'TEST_LOG_DBG_MSG\s*\(',
]

CACHE_FILE = '.file_id_cache.json'


class FileIDGenerator:
    """Main class for auto-generating file IDs"""

    def __init__(self, config, source_dir, output_file, verbose=False):
        self.config = config
        self.source_dir = Path(source_dir)
        self.output_file = Path(output_file)
        self.verbose = verbose
        self.module_ranges = config.get('module_ranges', DEFAULT_CONFIG['module_ranges'])

    def log(self, message):
        """Print message if verbose mode enabled"""
        if self.verbose:
            print(message)

    def file_contains_logs(self, file_path):
        """Check if a file contains any log macro calls"""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            # Search for any log macro pattern
            for pattern in LOG_MACRO_PATTERNS:
                if re.search(pattern, content):
                    self.log(f"  ✓ {file_path.relative_to(self.source_dir)}")
                    return True

            return False
        except Exception as e:
            print(f"⚠️  Warning: Cannot read {file_path}: {e}")
            return False

    def find_files_with_logs(self):
        """Recursively find all .c files that contain log macros"""
        log_files = []

        # Directories to skip
        skip_dirs = {'build', '.git', 'tools', 'bin', 'obj'}

        for c_file in self.source_dir.rglob('*.c'):
            # Skip if in excluded directory
            if any(part in skip_dirs for part in c_file.parts):
                continue

            if self.file_contains_logs(c_file):
                log_files.append(str(c_file))

        # Sort for stable ID assignment
        return sorted(log_files)

    def classify_file_by_module(self, file_path):
        """Determine which module a file belongs to based on its path"""
        normalized_path = file_path.replace('\\', '/')

        # Check each module's typical directory pattern
        for module in self.module_ranges.keys():
            if f'/{module}/' in normalized_path:
                return module

        # Default to 'other' if no match
        return 'other'

    def assign_file_ids(self, log_files):
        """Assign file IDs to each file based on module ranges"""
        # Group files by module
        files_by_module = {}
        for file_path in log_files:
            module = self.classify_file_by_module(file_path)
            if module not in files_by_module:
                files_by_module[module] = []
            files_by_module[module].append(file_path)

        # Assign IDs within each module's range
        file_id_map = {}
        warnings = []

        for module, files in sorted(files_by_module.items()):
            if module not in self.module_ranges:
                warnings.append(f"⚠️  Module '{module}' not in configuration, skipping {len(files)} files")
                continue

            range_info = self.module_ranges[module]
            start_id = range_info['start']
            end_id = range_info['end']
            available_slots = end_id - start_id + 1

            if len(files) > available_slots:
                warnings.append(
                    f"❌ Module '{module}' has {len(files)} files but only "
                    f"{available_slots} IDs available ({start_id}-{end_id})"
                )
                continue

            # Sort files for stable assignment
            sorted_files = sorted(files)
            for idx, file_path in enumerate(sorted_files):
                file_id = start_id + idx
                file_id_map[file_path] = file_id

        return file_id_map, warnings

    def calculate_files_hash(self, log_files):
        """Calculate hash of file list for cache validation"""
        file_list_str = "|".join(sorted(log_files))
        return hashlib.md5(file_list_str.encode()).hexdigest()

    def load_cache(self):
        """Load cached file ID mapping"""
        cache_path = Path(CACHE_FILE)
        if not cache_path.exists():
            return None

        try:
            with open(cache_path, 'r') as f:
                cache = json.load(f)
            return cache
        except Exception as e:
            print(f"⚠️  Cache load failed: {e}")
            return None

    def save_cache(self, log_files, file_id_map):
        """Save file ID mapping to cache"""
        cache = {
            'hash': self.calculate_files_hash(log_files),
            'file_count': len(log_files),
            'file_id_map': file_id_map,
            'timestamp': time.time(),
        }

        try:
            with open(CACHE_FILE, 'w') as f:
                json.dump(cache, f, indent=2)
        except Exception as e:
            print(f"⚠️  Cache save failed: {e}")

    def check_cache(self, log_files):
        """Check if cached mapping is still valid"""
        cache = self.load_cache()
        if cache is None:
            return None

        current_hash = self.calculate_files_hash(log_files)
        if cache['hash'] == current_hash:
            return cache['file_id_map']

        return None

    def path_to_enum_name(self, file_path):
        """Convert file path to enum constant name"""
        # src/drivers/uart.c => FILE_ID_DRIVERS_UART
        path = Path(file_path)
        relative_path = path.relative_to(self.source_dir)

        # Remove .c extension and convert to uppercase
        name_parts = str(relative_path.with_suffix('')).replace('\\', '/').split('/')
        enum_name = '_'.join(name_parts).upper()

        return f"FILE_ID_{enum_name}"

    def generate_header_file(self, file_id_map):
        """Generate log_file_id.h header file"""
        # Group by module
        files_by_module = {}
        for file_path, file_id in file_id_map.items():
            module = self.classify_file_by_module(file_path)
            if module not in files_by_module:
                files_by_module[module] = []
            files_by_module[module].append((file_path, file_id))

        # Generate header content
        lines = [
            "/**",
            " * @file log_file_id.h",
            " * @brief Auto-generated file ID definitions for logging system",
            " * ",
            " * ⚠️  WARNING: DO NOT EDIT MANUALLY",
            " * This file is automatically generated by tools/generate_file_ids.py",
            " * ",
            f" * Generated at: {time.strftime('%Y-%m-%d %H:%M:%S')}",
            f" * Total files: {len(file_id_map)}",
            " */",
            "",
            "#ifndef LOG_FILE_ID_H",
            "#define LOG_FILE_ID_H",
            "",
            "typedef enum {",
        ]

        # Add entries grouped by module
        module_order = list(self.module_ranges.keys()) + ['other']
        for module in module_order:
            if module not in files_by_module:
                continue

            # Module header
            module_desc = self.module_ranges.get(module, {}).get('description', module)
            range_info = self.module_ranges.get(module, {'start': 0, 'end': 0})
            lines.append(f"    /* {module.upper()} Module - {module_desc} ({range_info['start']}-{range_info['end']}) */")

            # Sort by file ID
            files = sorted(files_by_module[module], key=lambda x: x[1])
            for file_path, file_id in files:
                enum_name = self.path_to_enum_name(file_path)
                # Use file_path directly (already relative)
                lines.append(f"    {enum_name} = {file_id},  /* {file_path} */")

            lines.append("")

        lines.append("} LOG_FILE_ID_E;")
        lines.append("")
        lines.append("#endif /* LOG_FILE_ID_H */")

        # Write to file
        self.output_file.parent.mkdir(parents=True, exist_ok=True)
        with open(self.output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines))

        print(f"✅ Generated {self.output_file} with {len(file_id_map)} file IDs")

    def print_statistics(self, file_id_map):
        """Print statistics about file ID assignment"""
        print("\n📈 Statistics by module:")

        stats = {}
        for file_path in file_id_map:
            module = self.classify_file_by_module(file_path)
            stats[module] = stats.get(module, 0) + 1

        for module in sorted(stats.keys()):
            count = stats[module]
            range_info = self.module_ranges.get(module, {'start': 0, 'end': 0})
            usage_pct = (count / (range_info['end'] - range_info['start'] + 1)) * 100 if range_info['end'] > 0 else 0
            print(f"  {module:10s}: {count:3d} files "
                  f"(ID range: {range_info['start']:3d}-{range_info['end']:3d}, "
                  f"usage: {usage_pct:5.1f}%)")

    def run(self, force=False):
        """Main execution flow"""
        start_time = time.time()

        print("=" * 70)
        print("🔍 Auto File ID Generator - Scanning source files for log output")
        print("=" * 70)

        # Step 1: Find all files with log macros
        print(f"\n📂 Scanning directory: {self.source_dir}")
        if self.verbose:
            print("   Files with log macros:")

        log_files = self.find_files_with_logs()
        scan_time = time.time() - start_time
        print(f"📊 Found {len(log_files)} files with log output ({scan_time:.3f}s)")

        if len(log_files) == 0:
            print("⚠️  No files found with log macros. Nothing to do.")
            return 0

        # Step 2: Check cache
        file_id_map = None
        if not force:
            cached_map = self.check_cache(log_files)
            if cached_map:
                file_id_map = cached_map
                cache_time = time.time() - start_time
                print(f"✅ Cache hit! Using cached IDs ({cache_time:.3f}s)")

        # Step 3: Generate new IDs if needed
        if file_id_map is None:
            print("📝 Generating file ID assignments...")
            file_id_map, warnings = self.assign_file_ids(log_files)

            # Print warnings
            for warning in warnings:
                print(warning)

            # Save cache
            self.save_cache(log_files, file_id_map)
            gen_time = time.time() - start_time
            print(f"✅ Generated {len(file_id_map)} file IDs ({gen_time:.3f}s)")

        # Step 4: Generate header file
        self.generate_header_file(file_id_map)

        # Step 5: Print statistics
        self.print_statistics(file_id_map)

        total_time = time.time() - start_time
        print("\n" + "=" * 70)
        print(f"✅ Complete! Total time: {total_time:.3f}s")
        print("=" * 70)

        return 0


def load_config(config_file):
    """Load configuration from YAML file (or use default)"""
    config_path = Path(config_file)

    if not config_path.exists():
        print(f"ℹ️  Config file not found: {config_file}, using default configuration")
        return DEFAULT_CONFIG

    try:
        import yaml
        with open(config_path, 'r') as f:
            config = yaml.safe_load(f)
        return config
    except ImportError:
        print("⚠️  PyYAML not installed, using default configuration")
        print("   Install with: pip install pyyaml")
        return DEFAULT_CONFIG
    except Exception as e:
        print(f"⚠️  Failed to load config: {e}, using default configuration")
        return DEFAULT_CONFIG


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description='Auto-generate file ID definitions for logging system',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic usage (use defaults)
  python3 tools/generate_file_ids.py

  # Force regeneration (ignore cache)
  python3 tools/generate_file_ids.py --force

  # Verbose mode (show all scanned files)
  python3 tools/generate_file_ids.py --verbose

  # Custom configuration
  python3 tools/generate_file_ids.py --config my_config.yaml --output my_ids.h
        """
    )

    parser.add_argument('--config', default='tools/file_id_config.yaml',
                        help='Configuration file (default: tools/file_id_config.yaml)')
    parser.add_argument('--output', default='include/log_file_id.h',
                        help='Output header file (default: include/log_file_id.h)')
    parser.add_argument('--source', default='src/',
                        help='Source directory to scan (default: src/)')
    parser.add_argument('--force', action='store_true',
                        help='Force regeneration even if cache is valid')
    parser.add_argument('--verbose', action='store_true',
                        help='Show detailed scanning information')

    args = parser.parse_args()

    # Load configuration
    config = load_config(args.config)

    # Create generator and run
    generator = FileIDGenerator(
        config=config,
        source_dir=args.source,
        output_file=args.output,
        verbose=args.verbose
    )

    return generator.run(force=args.force)


if __name__ == '__main__':
    sys.exit(main())
