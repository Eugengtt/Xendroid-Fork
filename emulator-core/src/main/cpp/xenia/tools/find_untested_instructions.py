#!/usr/bin/env python3
"""
Find PPC instructions that lack test coverage by comparing:
1. Instructions covered by existing tests
2. Instructions actually used in game binaries
"""

import os
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path


def build_mnemonic_to_opcode_map():
    """
    Build a mapping from assembly mnemonics to internal opcode names
    by parsing ppc-instructions.xml
    """
    mnemonic_map = {}
    xml_file = Path("tools/ppc-instructions.xml")

    if not xml_file.exists():
        return mnemonic_map

    try:
        with open(xml_file, 'r') as f:
            current_opcode = None
            for line in f:
                # Match lines like: <insn mnem="subficx" ...>
                insn_match = re.search(r'<insn\s+mnem="(\w+)"', line)
                if insn_match:
                    current_opcode = insn_match.group(1)
                    continue

                # Match disasm lines like: <disasm>subfic [RD], [RA], [SIMM]</disasm>
                if current_opcode:
                    disasm_match = re.search(r'<disasm>(\w+)', line)
                    if disasm_match:
                        asm_mnemonic = disasm_match.group(1).lower()
                        mnemonic_map[asm_mnemonic] = current_opcode
                        current_opcode = None  # Reset after finding disasm
    except Exception as e:
        print(f"Warning: Error parsing XML: {e}", file=sys.stderr)

    return mnemonic_map


def extract_tested_instructions():
    """
    Extract all PPC instructions actually tested by parsing test files.
    Returns a dict mapping instruction -> test file count.
    """
    tested_instructions = defaultdict(int)
    test_dir = Path("src/xenia/cpu/ppc/testing")

    if not test_dir.exists():
        print(f"Error: Test directory not found: {test_dir}", file=sys.stderr)
        return tested_instructions

    # Build mnemonic to opcode mapping
    mnemonic_map = build_mnemonic_to_opcode_map()

    # Pattern to match PPC instructions in assembly
    # Matches: "  add r1, r2, r3" or "  add. r1, r2, r3"
    instr_pattern = re.compile(r'^\s+(\w+)(\.?)\s+', re.IGNORECASE)

    # Find all test files: instr_*.s
    for test_file in test_dir.glob("instr_*.s"):
        instructions_in_file = set()

        try:
            with open(test_file, 'r') as f:
                for line in f:
                    # Skip comments and labels
                    if line.strip().startswith('#') or line.strip().startswith('test_'):
                        continue
                    if ':' in line and not ' ' in line.split(':')[0]:
                        continue  # Skip labels

                    # Match instruction
                    match = instr_pattern.match(line)
                    if match:
                        mnemonic = match.group(1).lower()
                        has_dot = match.group(2) == '.'

                        # Map mnemonic to opcode name
                        if mnemonic in mnemonic_map:
                            opcode = mnemonic_map[mnemonic]
                            instructions_in_file.add(opcode)
                        else:
                            # Fallback for unknown mnemonics
                            instructions_in_file.add(mnemonic)

                        if has_dot:
                            # Record bit variant
                            mnemonic_with_dot = mnemonic + '.'
                            if mnemonic_with_dot in mnemonic_map:
                                opcode = mnemonic_map[mnemonic_with_dot]
                                instructions_in_file.add(opcode)
                            else:
                                # Assume 'x' suffix for record bit
                                instructions_in_file.add(mnemonic + 'x')
        except Exception as e:
            print(f"Warning: Error parsing {test_file}: {e}", file=sys.stderr)
            continue

        # Count files that test each instruction
        for instr in instructions_in_file:
            tested_instructions[instr] += 1

    return tested_instructions


def extract_instructions_from_asm_dump(dump_file):
    """
    Extract PPC instructions from an objdump-style disassembly.

    Expected format:
    82000000:  7c 22 1a 14     add      r1,r2,r3
    82000004:  4e 80 00 20     blr
    """
    instructions = Counter()

    # Pattern to match disassembly lines
    # Matches: address: bytes    mnemonic   operands
    pattern = re.compile(r'^\s*[0-9a-f]+:\s+[0-9a-f\s]+\s+(\w+)', re.IGNORECASE)

    try:
        with open(dump_file, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                match = pattern.match(line)
                if match:
                    mnemonic = match.group(1).lower()
                    # Remove condition code suffixes and other variants
                    # e.g., "addi" stays "addi", "bc" stays "bc"
                    base_mnemonic = mnemonic.rstrip('.')
                    instructions[base_mnemonic] += 1
    except Exception as e:
        print(f"Error reading {dump_file}: {e}", file=sys.stderr)

    return instructions


def extract_instructions_from_hir_dumps(hir_dir):
    """
    Alternative: Extract PPC instructions from HIR dump comments.
    Some HIR dumps may include PPC source comments.
    """
    instructions = Counter()

    hir_path = Path(hir_dir)
    if not hir_path.exists():
        return instructions

    # This is a fallback - HIR dumps may not have PPC instruction comments
    # but we can try to find them if they exist
    for hir_file in hir_path.glob("*"):
        if hir_file.is_file():
            try:
                with open(hir_file, 'r', encoding='utf-8', errors='ignore') as f:
                    for line in f:
                        # Look for PPC instruction comments (if format includes them)
                        # This depends on HIR dump format
                        pass
            except:
                pass

    return instructions


def analyze_opcode_header():
    """
    Extract all defined PPC opcodes from ppc_opcode.h
    This gives us the complete list of instructions Xenia knows about.
    """
    opcodes = set()
    opcode_file = Path("src/xenia/cpu/ppc/ppc_opcode.h")

    if not opcode_file.exists():
        print(f"Warning: {opcode_file} not found", file=sys.stderr)
        return opcodes

    try:
        with open(opcode_file, 'r') as f:
            in_enum = False
            for line in f:
                if 'enum class PPCOpcode' in line:
                    in_enum = True
                    continue

                if in_enum:
                    if '};' in line:
                        break

                    # Match opcode names (e.g., "addx," or "addi,")
                    match = re.match(r'\s*(\w+)\s*[,=]', line)
                    if match:
                        opcode = match.group(1)
                        opcodes.add(opcode)
    except Exception as e:
        print(f"Error reading opcode file: {e}", file=sys.stderr)

    return opcodes


def main():
    print("=" * 80)
    print("PPC Instruction Test Coverage Analysis")
    print("=" * 80)
    print()

    # Step 1: Get all tested instructions
    print("Step 1: Scanning existing tests...")
    tested_instructions = extract_tested_instructions()
    print(f"  Found {len(tested_instructions)} unique instructions tested")
    print(f"  Total test files parsed: {sum(tested_instructions.values())} file references")

    # Show most tested instructions
    if tested_instructions:
        print(f"\n  Top 10 most tested instructions:")
        sorted_tested = sorted(tested_instructions.items(), key=lambda x: x[1], reverse=True)
        for instr, count in sorted_tested[:10]:
            print(f"    {instr.ljust(20)} (tested in {count} files)")
    print()

    # Step 2: Get all defined opcodes
    print("Step 2: Scanning PPC opcode definitions...")
    all_opcodes = analyze_opcode_header()
    print(f"  Found {len(all_opcodes)} defined opcodes in Xenia")
    print()

    # Step 3: Check for disassembly file
    print("Step 3: Analyzing game executable...")
    print("  (Optional) Provide path to objdump output:")
    print()

    # Check common locations
    dump_files = []
    for pattern in ['*.dump', '*.asm', '*.dis', 'disasm.txt']:
        dump_files.extend(Path('.').glob(pattern))

    game_instructions = Counter()
    if dump_files:
        print(f"  Found disassembly file: {dump_files[0]}")
        game_instructions = extract_instructions_from_asm_dump(dump_files[0])
        print(f"  Extracted {len(game_instructions)} unique instruction types")
        print(f"  Total instruction instances: {sum(game_instructions.values()):,}")
    else:
        print("  No disassembly file found")
        print("  To analyze game code, provide a disassembly file:")
        print("    - Use: powerpc-eabi-objdump -d game.xex > game.dump")
        print("    - Or extract from Xbox 360 executable")
    print()

    # Step 4: Generate reports
    print("=" * 80)
    print("RESULTS")
    print("=" * 80)
    print()

    # Report 1: Opcodes without tests
    tested_set = set(tested_instructions.keys())
    untested_opcodes = all_opcodes - tested_set
    if untested_opcodes:
        print(f"Opcodes without tests ({len(untested_opcodes)}):")
        print("-" * 80)
        for opcode in sorted(untested_opcodes):
            print(f"  {opcode}")
        print()

    # Report 2: Instructions used in game but not tested
    if game_instructions:
        untested_in_game = set()
        for instr in game_instructions.keys():
            # Check base instruction name
            base_instr = instr.rstrip('.')
            if base_instr not in tested_set:
                untested_in_game.add(base_instr)

        if untested_in_game:
            print(f"Instructions used in game but NOT tested ({len(untested_in_game)}):")
            print("-" * 80)
            for instr in sorted(untested_in_game):
                count = game_instructions.get(instr, 0)
                print(f"  {instr.ljust(20)} (used {count:,} times)")
            print()

        # Report 3: Most common untested instructions (prioritize these!)
        print("Top 20 untested instructions by frequency:")
        print("-" * 80)
        untested_with_counts = [(i, game_instructions[i]) for i in untested_in_game]
        untested_with_counts.sort(key=lambda x: x[1], reverse=True)
        for instr, count in untested_with_counts[:20]:
            print(f"  {instr.ljust(20)} {count:>10,} occurrences")
        print()

    # Report 4: Test coverage summary
    print("Test Coverage Summary:")
    print("-" * 80)
    if all_opcodes:
        # Calculate actual tested opcodes (only those that exist in the opcode enum)
        tested_set = set(tested_instructions.keys())
        tested_opcodes_that_exist = tested_set & all_opcodes
        num_tested = len(tested_opcodes_that_exist)
        num_total = len(all_opcodes)
        coverage = num_tested / num_total * 100
        print(f"  Tested opcodes:     {num_tested:>4} / {num_total:<4} ({coverage:.1f}%)")

    if game_instructions:
        game_unique = len(game_instructions)
        game_tested = len(set(game_instructions.keys()) & tested_set)
        game_coverage = game_tested / game_unique * 100 if game_unique > 0 else 0
        print(f"  Game instructions:  {game_tested:>4} / {game_unique:<4} tested ({game_coverage:.1f}%)")

        # Weighted coverage (by frequency)
        total_occurrences = sum(game_instructions.values())
        tested_occurrences = sum(count for instr, count in game_instructions.items()
                                if instr in tested_set)
        weighted_coverage = tested_occurrences / total_occurrences * 100 if total_occurrences > 0 else 0
        print(f"  Weighted coverage:  {weighted_coverage:.1f}% (by instruction frequency)")
    print()

    # Generate test file suggestions
    if game_instructions and untested_in_game:
        print("Suggested new test files to create:")
        print("-" * 80)
        untested_sorted = sorted(untested_in_game,
                                key=lambda x: game_instructions.get(x, 0),
                                reverse=True)
        for i, instr in enumerate(untested_sorted[:10], 1):
            count = game_instructions.get(instr, 0)
            print(f"  {i:2}. src/xenia/cpu/ppc/testing/instr_{instr}.s  (priority: {count:,} uses)")
        print()


if __name__ == '__main__':
    main()
