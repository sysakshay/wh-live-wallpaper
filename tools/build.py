#!/usr/bin/env python3
"""
==============================================================================
tools/build.py - Professional C++ Modular Build System for Windhawk Mods
==============================================================================
Assembles multiple C++ source files (.h, .hpp, .cpp) from `src/` into a single,
clean translation unit (`generated/LiveVideoWallpaper.cpp`) for Windhawk compilation.

Features:
- Source discovery & duplicate filename check across subdirectories
- Metadata block extraction & single-instance preservation:
    // ==WindhawkMod== ... // ==/WindhawkMod==
    // ==WindhawkModReadme== ... // ==/WindhawkModReadme==
    // ==WindhawkModSettings== ... // ==/WindhawkModSettings==
- Global system include (#include <...>) extraction & alphabetical deduplication
- Project local include (#include "...") resolution with cycle detection
- Single-emission header guards (preventing duplicate header contents & preserving #pragma once)
- Deterministic concatenation order:
    Metadata -> Global Includes -> Headers -> Helper CPPs -> Main.cpp
- Readable build logs with debug section separators
- CLI flags: --watch, --clean, --stats
==============================================================================
"""

import os
import sys
import time
import re
import argparse
from pathlib import Path

# Regex patterns for includes and metadata
SYS_INCLUDE_RE = re.compile(r'^\s*#include\s+<([^>]+)>\s*$', re.MULTILINE)
LOCAL_INCLUDE_RE = re.compile(r'^\s*#include\s+"([^"]+)"\s*$', re.MULTILINE)

METADATA_BLOCK_NAMES = [
    ("WindhawkMod", re.compile(r'(?:^//\s*clang-format\s+off\s*\r?\n)?^//\s*==WindhawkMod==\s*\r?\n.*?^//\s*==/WindhawkMod==\s*\r?\n(?:^//\s*clang-format\s+on\s*\r?\n)?', re.DOTALL | re.MULTILINE)),
    ("WindhawkModReadme", re.compile(r'(?:^//\s*clang-format\s+off\s*\r?\n)?^//\s*==WindhawkModReadme==\s*\r?\n.*?^//\s*==/WindhawkModReadme==\s*\r?\n(?:^//\s*clang-format\s+on\s*\r?\n)?', re.DOTALL | re.MULTILINE)),
    ("WindhawkModSettings", re.compile(r'(?:^//\s*clang-format\s+off\s*\r?\n)?^//\s*==WindhawkModSettings==\s*\r?\n.*?^//\s*==/WindhawkModSettings==\s*\r?\n(?:^//\s*clang-format\s+on\s*\r?\n)?', re.DOTALL | re.MULTILINE)),
]

class BuildError(Exception):
    pass

class ModBuilder:
    def __init__(self, src_dir="src", output_file="generated/LiveVideoWallpaper.cpp"):
        self.project_root = Path(__file__).resolve().parent.parent
        self.src_dir = self.project_root / src_dir
        self.output_file = self.project_root / output_file
        
        # State populated during build
        self.discovered_files = {}  # basename -> Path
        self.file_contents = {}     # basename -> raw string content
        self.file_lines_of_code = {}# basename -> line count
        self.metadata_blocks = {}   # block_name -> string block
        self.system_includes = set()# set of system include filenames e.g. "windows.h"
        self.emitted_headers = set()# basenames of headers already emitted
        self.stats = {
            "total_files": 0,
            "total_loc": 0,
            "output_size": 0,
            "build_time_ms": 0.0
        }

    def discover_files(self):
        if not self.src_dir.exists():
            raise BuildError(f"Source directory not found: {self.src_dir}")

        self.discovered_files.clear()
        self.file_contents.clear()
        self.file_lines_of_code.clear()

        for root, dirs, files in os.walk(self.src_dir):
            for file in sorted(files):
                if file.endswith((".h", ".hpp", ".hxx", ".cpp", ".cc", ".c")):
                    path = Path(root) / file
                    basename = path.name
                    if basename in self.discovered_files:
                        other_path = self.discovered_files[basename]
                        raise BuildError(
                            f"Duplicate file detected: '{basename}' found at both:\n"
                            f"  1) {other_path}\n"
                            f"  2) {path}\n"
                            f"All source file basenames must be unique across the project."
                        )
                    self.discovered_files[basename] = path
                    
                    try:
                        content = path.read_text(encoding="utf-8")
                    except Exception as e:
                        raise BuildError(f"Failed to read source file '{path}': {e}")
                    
                    self.file_contents[basename] = content
                    self.file_lines_of_code[basename] = len(content.splitlines())
                    print(f"Reading {basename}")

        self.stats["total_files"] = len(self.discovered_files)
        self.stats["total_loc"] = sum(self.file_lines_of_code.values())

        if not self.discovered_files:
            raise BuildError(f"No source files (.h, .cpp) found in {self.src_dir}")

    def extract_metadata_and_system_includes(self):
        self.metadata_blocks.clear()
        self.system_includes.clear()

        # Track occurrences across all files for error reporting
        block_occurrences = {name: [] for name, _ in METADATA_BLOCK_NAMES}

        # First pass: strip metadata blocks
        for basename, content in list(self.file_contents.items()):
            for block_name, regex in METADATA_BLOCK_NAMES:
                matches = list(regex.finditer(content))
                for m in matches:
                    block_text = m.group(0).strip() + "\n"
                    block_occurrences[block_name].append((basename, block_text))
                
                # Remove matched blocks from the file content so they aren't duplicated later
                content = regex.sub("", content)
            self.file_contents[basename] = content

        # Validate exactly 1 occurrence of each required metadata block across the project
        for block_name, occurrences in block_occurrences.items():
            if len(occurrences) == 0:
                raise BuildError(
                    f"Missing required metadata block: // =={block_name}== ... // ==/{block_name}==\n"
                    f"Must be present in one of the source files inside {self.src_dir}."
                )
            elif len(occurrences) > 1:
                file_list = ", ".join([f"'{f}'" for f, _ in occurrences])
                raise BuildError(
                    f"Duplicate metadata block // =={block_name}== detected {len(occurrences)} times across files: {file_list}.\n"
                    f"Exactly one instance of this block is permitted in the project."
                )
            else:
                self.metadata_blocks[block_name] = occurrences[0][1]

        # Second pass: extract system includes from remaining content
        for basename, content in list(self.file_contents.items()):
            new_lines = []
            for line in content.splitlines():
                match = re.match(r'^\s*#include\s+<([^>]+)>\s*$', line)
                if match:
                    sys_inc = match.group(1).strip()
                    self.system_includes.add(sys_inc)
                else:
                    new_lines.append(line)
            self.file_contents[basename] = "\n".join(new_lines) + "\n"

    def identify_main_file(self):
        """
        Identifies the primary translation unit (e.g. LiveVideoWallpaper.cpp or Main.cpp)
        that should be placed at the very end of the concatenation.
        """
        cpp_files = [f for f in self.discovered_files.keys() if f.endswith((".cpp", ".cc", ".c"))]
        if not cpp_files:
            raise BuildError("No .cpp files found in project sources.")

        # Check for explicit name or Windhawk mod entry point
        for cpp in cpp_files:
            if cpp.lower() in ("main.cpp", "livevideowallpaper.cpp", "mod.cpp"):
                return cpp

        # Otherwise check which cpp file contained the WindhawkMod block originally or Wh_ModInit
        for cpp in cpp_files:
            raw = self.discovered_files[cpp].read_text(encoding="utf-8")
            if "Wh_ModInit" in raw or "==WindhawkMod==" in raw or "WinMain" in raw or "main(" in raw:
                return cpp

        # Fallback to the largest cpp file
        return max(cpp_files, key=lambda f: len(self.file_contents[f]))

    def process_file_content(self, basename, stack):
        """
        Processes a file's content:
        - Resolves local #include "..." recursively
        - Checks for circular dependencies and missing files
        - Handles deduplication of already emitted headers
        """
        if basename in stack:
            chain = " -> ".join(stack + [basename])
            raise BuildError(f"Circular include detected: {chain}")

        content = self.file_contents[basename]
        output_lines = []
        stack.append(basename)

        for line_num, line in enumerate(content.splitlines(), start=1):
            match = re.match(r'^\s*#include\s+"([^"]+)"\s*$', line)
            if match:
                inc_name = match.group(1).strip()
                # Handle path if someone wrote e.g. #include "subdir/Foo.h"
                inc_basename = Path(inc_name).name
                
                if inc_basename not in self.discovered_files:
                    raise BuildError(
                        f"Missing include: '{inc_name}' referenced in '{basename}' (line {line_num}).\n"
                        f"File not found among project sources in {self.src_dir}."
                    )

                # Check if it's a header or helper cpp
                if inc_basename.endswith((".h", ".hpp", ".hxx")):
                    if inc_basename in self.emitted_headers:
                        output_lines.append(f"// [Deduplicated] #include \"{inc_name}\" (already included)")
                    else:
                        self.emitted_headers.add(inc_basename)
                        header_body = self.process_file_content(inc_basename, stack)
                        header_block = (
                            f"\n//////////////////////////////////////////////////////////\n"
                            f"// Begin {inc_basename}\n"
                            f"//////////////////////////////////////////////////////////\n"
                            f"{header_body}\n"
                            f"//////////////////////////////////////////////////////////\n"
                            f"// End {inc_basename}\n"
                            f"//////////////////////////////////////////////////////////\n"
                        )
                        output_lines.append(header_block)
                else:
                    # Including a .cpp file locally
                    output_lines.append(f"// [Deduplicated/Concatenated] #include \"{inc_name}\" (concatenated by build script)")
            elif re.match(r'^\s*#pragma\s+once\s*$', line):
                output_lines.append("// #pragma once (header guard handled by build engine)")
            else:
                output_lines.append(line)

        stack.pop()
        return "\n".join(output_lines)

    def assemble(self):
        self.emitted_headers.clear()
        output_chunks = []

        # 1. Metadata Blocks
        output_chunks.append("// ============================================================================")
        output_chunks.append("// AUTOMATICALLY GENERATED BY tools/build.py - DO NOT EDIT DIRECTLY")
        output_chunks.append("// ============================================================================\n")
        
        output_chunks.append(self.metadata_blocks.get("WindhawkMod", ""))
        output_chunks.append(self.metadata_blocks.get("WindhawkModReadme", ""))
        output_chunks.append(self.metadata_blocks.get("WindhawkModSettings", ""))

        # 2. Global / System Includes (Sorted deterministically)
        if self.system_includes:
            output_chunks.append("\n// ============================================================================")
            output_chunks.append("// Global / System Includes")
            output_chunks.append("// ============================================================================")
            for sys_inc in sorted(self.system_includes):
                output_chunks.append(f"#include <{sys_inc}>")
            output_chunks.append("")

        # 3. Headers Section
        # Emit all headers that haven't been inlined yet, sorted deterministically
        headers = sorted([f for f in self.discovered_files.keys() if f.endswith((".h", ".hpp", ".hxx"))])
        if headers:
            output_chunks.append("// ============================================================================")
            output_chunks.append("// Project Headers")
            output_chunks.append("// ============================================================================")
            for header in headers:
                if header not in self.emitted_headers:
                    self.emitted_headers.add(header)
                    header_body = self.process_file_content(header, [])
                    output_chunks.append(
                        f"\n//////////////////////////////////////////////////////////\n"
                        f"// Begin {header}\n"
                        f"//////////////////////////////////////////////////////////\n"
                        f"{header_body}\n"
                        f"//////////////////////////////////////////////////////////\n"
                        f"// End {header}\n"
                        f"//////////////////////////////////////////////////////////\n"
                    )

        # 4. Helper CPP Implementations
        main_file = self.identify_main_file()
        cpp_helpers = sorted([
            f for f in self.discovered_files.keys()
            if f.endswith((".cpp", ".cc", ".c")) and f != main_file
        ])

        if cpp_helpers:
            output_chunks.append("// ============================================================================")
            output_chunks.append("// CPP Implementations")
            output_chunks.append("// ============================================================================")
            for cpp in cpp_helpers:
                cpp_body = self.process_file_content(cpp, [])
                output_chunks.append(
                    f"\n//////////////////////////////////////////////////////////\n"
                    f"// Begin {cpp}\n"
                    f"//////////////////////////////////////////////////////////\n"
                    f"{cpp_body}\n"
                    f"//////////////////////////////////////////////////////////\n"
                    f"// End {cpp}\n"
                    f"//////////////////////////////////////////////////////////\n"
                )

        # 5. Main Translation Unit
        output_chunks.append("// ============================================================================")
        output_chunks.append(f"// Main Translation Unit ({main_file})")
        output_chunks.append("// ============================================================================")
        main_body = self.process_file_content(main_file, [])
        output_chunks.append(
            f"\n//////////////////////////////////////////////////////////\n"
            f"// Begin {main_file}\n"
            f"//////////////////////////////////////////////////////////\n"
            f"{main_body}\n"
            f"//////////////////////////////////////////////////////////\n"
            f"// End {main_file}\n"
            f"//////////////////////////////////////////////////////////\n"
        )

        # Ensure output directory exists and write final file
        self.output_file.parent.mkdir(parents=True, exist_ok=True)
        final_code = "\n".join(output_chunks)
        self.output_file.write_text(final_code, encoding="utf-8")
        
        self.stats["output_size"] = len(final_code.encode("utf-8"))
        print(f"Generating {self.output_file.name}")
        print("Done.")

    def run_build(self):
        start_time = time.perf_counter()
        self.discover_files()
        self.extract_metadata_and_system_includes()
        self.assemble()
        end_time = time.perf_counter()
        self.stats["build_time_ms"] = (end_time - start_time) * 1000.0

    def print_stats(self):
        size_kb = self.stats["output_size"] / 1024.0
        print("\n==========================================================")
        print("                   BUILD STATISTICS                       ")
        print("==========================================================")
        print(f"Total source files  : {self.stats['total_files']}")
        print(f"Lines of code       : {self.stats['total_loc']}")
        print(f"Generated file size : {size_kb:.2f} KB ({self.stats['output_size']} bytes)")
        print(f"Build time          : {self.stats['build_time_ms']:.2f} ms")
        print("==========================================================\n")

    def clean(self):
        if self.output_file.exists():
            try:
                os.remove(self.output_file)
                print(f"Cleaned generated file: {self.output_file}")
            except Exception as e:
                print(f"Failed to delete {self.output_file}: {e}", file=sys.stderr)
        else:
            print(f"Nothing to clean: {self.output_file} does not exist.")

def get_dir_mtime_snapshot(src_dir):
    snapshot = {}
    for root, dirs, files in os.walk(src_dir):
        for file in files:
            if file.endswith((".h", ".hpp", ".hxx", ".cpp", ".cc", ".c")):
                p = Path(root) / file
                try:
                    snapshot[p] = p.stat().st_mtime
                except OSError:
                    pass
    return snapshot

def run_watch_mode(builder):
    print("==========================================================")
    print("Watching for file changes in:", builder.src_dir)
    print("Press Ctrl+C to exit watch mode.")
    print("==========================================================")
    
    try:
        builder.run_build()
    except BuildError as e:
        print(f"[ERROR] {e}", file=sys.stderr)
    except Exception as e:
        print(f"[UNEXPECTED ERROR] {e}", file=sys.stderr)

    last_snapshot = get_dir_mtime_snapshot(builder.src_dir)

    try:
        while True:
            time.sleep(0.5)
            current_snapshot = get_dir_mtime_snapshot(builder.src_dir)
            
            # Check for additions, deletions, or modification time changes
            if current_snapshot != last_snapshot:
                changed = [str(p.name) for p, mtime in current_snapshot.items() if last_snapshot.get(p) != mtime]
                added = [str(p.name) for p in current_snapshot if p not in last_snapshot]
                removed = [str(p.name) for p in last_snapshot if p not in current_snapshot]
                
                reasons = []
                if changed: reasons.append(f"modified: {', '.join(changed)}")
                if added: reasons.append(f"added: {', '.join(added)}")
                if removed: reasons.append(f"removed: {', '.join(removed)}")
                
                print(f"\n[Watch] Change detected ({'; '.join(reasons)}). Rebuilding...")
                last_snapshot = current_snapshot
                
                try:
                    builder.run_build()
                except BuildError as e:
                    print(f"[ERROR] {e}", file=sys.stderr)
                except Exception as e:
                    print(f"[UNEXPECTED ERROR] {e}", file=sys.stderr)
    except KeyboardInterrupt:
        print("\n[Watch] Stopping watch mode.")

def main():
    parser = argparse.ArgumentParser(description="Modular C++ build script for Windhawk mods.")
    parser.add_argument("--watch", action="store_true", help="Automatically rebuild whenever any source file changes.")
    parser.add_argument("--clean", action="store_true", help="Delete generated files.")
    parser.add_argument("--stats", action="store_true", help="Print detailed build statistics after building.")
    parser.add_argument("--src", default="src", help="Source directory containing .h and .cpp files (default: src).")
    parser.add_argument("--output", default="generated/LiveVideoWallpaper.cpp", help="Output generated single source file path.")

    args = parser.parse_args()
    builder = ModBuilder(src_dir=args.src, output_file=args.output)

    if args.clean:
        builder.clean()
        if not args.watch and not args.stats:
            return

    if args.watch:
        run_watch_mode(builder)
    else:
        try:
            builder.run_build()
            if args.stats:
                builder.print_stats()
        except BuildError as e:
            print(f"[ERROR] {e}", file=sys.stderr)
            sys.exit(1)
        except Exception as e:
            print(f"[UNEXPECTED ERROR] {e}", file=sys.stderr)
            sys.exit(1)

if __name__ == "__main__":
    main()
