import json
import argparse
import os
import sys

def parse_args():
    parser = argparse.ArgumentParser(
        description="Extract -D macros from compile_commands.json and write to macros.txt"
    )
    parser.add_argument(
        "-f", "--file",
        default="compile_commands.json",
        help="Path to compile_commands.json (default: compile_commands.json)"
    )
    parser.add_argument(
        "-e", "--exclude",
        help="Path to file with macros to exclude (one per line)"
    )
    parser.add_argument(
        "-a", "--add",
        help="Path to file with macros to add (one per line)"
    )
    parser.add_argument(
    "-o", "--output",
    default="../generated/macros.txt",
    help="Path to output file (default: ../generated/macros.txt)"
    )
    parser.add_argument(
        "-c", "--customsonly",
        help="Only adds/excludes custom macros to output file, not regenerate it."
    )
    return parser.parse_args()

def load_list(path):
    if not path or not os.path.exists(path):
        return set()
    with open(path, "r") as f:
        return set(line.strip() for line in f if line.strip())

def extract_macros(file_path):
    if not os.path.exists(file_path):
        print(f"Error: File not found: {file_path}")
        sys.exit(1)

    with open(file_path, "r") as f:
        data = json.load(f)

    macros = set()
    for entry in data:
        if "arguments" in entry:
            for i in range(len(entry["arguments"])-1, 0, -1):
                arg = entry["arguments"][i]
                if arg.startswith("-D"):
                    macros.add(arg[2:])
    return macros

def write_macros(macros, output_path="../generated/macros.txt"):
    with open(output_path, "w") as f:
        for macro in sorted(macros):
            f.write(macro + "\n")

def main():
    args = parse_args()
    exclude_macros = load_list(args.exclude)
    add_macros = load_list(args.add)

    if(args.customsonly):
        extracted_macros = load_list(args.output)
        final_macros = (extracted_macros - exclude_macros) | add_macros
        write_macros(final_macros, args.output)
        print(f" Wrote {len(final_macros)} macros to {args.output}")
        return 0

    extracted_macros = extract_macros(args.file)

    final_macros = (extracted_macros - exclude_macros) | add_macros
    write_macros(final_macros, args.output)

    print(f" Wrote {len(final_macros)} macros to {args.output}")

if __name__ == "__main__":
    main()
