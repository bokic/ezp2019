#!/usr/bin/env python3
import struct
import argparse
import sys

def generate_header(input_bin, output_header):
    """Parses ezp2019.dat and generates a C header file with aligned columns."""
    record_size = 68
    struct_format = "<48sIIHHHHI"
    chips_data = []

    try:
        with open(input_bin, "rb") as f:
            while True:
                data = f.read(record_size)
                if len(data) < record_size:
                    break
                
                # binary unpack
                (raw_name, chip_id, size, pagesize, address, timing, reserved, flags) = struct.unpack(struct_format, data)
                
                # decode name and normalize full-width characters
                name_str = raw_name.decode("gb18030").split("\0")[0]
                name_str = name_str.replace('（', '(').replace('）', ')')
                if not name_str: break
                
                parts = name_str.split(",")
                if len(parts) != 3: continue
                
                chips_data.append({
                    "type": parts[0].replace('"', '\\"'),
                    "manuf": parts[1].replace('"', '\\"'),
                    "name": parts[2].replace('"', '\\"'),
                    "id": chip_id,
                    "size": size,
                    "pagesize": pagesize,
                    "address": address,
                    "timing": timing,
                    "reserved": reserved,
                    "flags": flags
                })

    except Exception as e:
        print(f"Error reading {input_bin}: {e}", file=sys.stderr)
        sys.exit(1)

    # Prepare rows for column alignment
    rows = []
    for c in chips_data:
        rows.append([
            f'"{c["type"]}",',
            f'"{c["manuf"]}",',
            f'"{c["name"]}",',
            f'0x{c["id"]:08X},',
            f'{c["size"]},',
            f'{c["pagesize"]},',
            f'{c["address"]},',
            f'{c["timing"]},',
            f'{c["reserved"]},',
            f'0x{c["flags"]:08X}'
        ])


    # Calculate max width for each column
    col_widths = []
    if rows:
        for i in range(len(rows[0])):
            col_widths.append(max(len(row[i]) for row in rows))

    with open(output_header, "w") as f:
        f.write("#pragma once\n\n")
        f.write("#include <stdint.h>\n")
        f.write("#include <stddef.h>\n\n")
        
        f.write("typedef struct {\n")
        f.write("    const char *chip_type;\n")
        f.write("    const char *manufacturer;\n")
        f.write("    const char *chip_name;\n")
        f.write("    uint32_t chip_id;\n")
        f.write("    uint32_t size;\n")
        f.write("    uint16_t pagesize;\n")
        f.write("    uint16_t address;\n")
        f.write("    uint16_t timing;\n")
        f.write("    uint16_t reserved;\n")
        f.write("    uint32_t flags;\n")
        f.write("} Chip;\n\n")
        
        f.write("static const Chip ezp2019_chips[] = {\n")
        for row in rows:
            f.write("    { ")
            for i, val in enumerate(row):
                # Final field doesn't need a space after it in the struct but we handle it via alignment
                padding = " " * (col_widths[i] - len(val))
                f.write(f"{val}{padding} ")
            f.write("},\n")
        
        f.write("};\n\n")
        f.write(f"static const size_t ezp2019_num_chips = sizeof(ezp2019_chips) / sizeof(ezp2019_chips[0]);\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="EZP2019 Code Generator: generates C header from database.")
    parser.add_argument("input", help="Input binary database (ezp2019.dat)")
    parser.add_argument("-o", "--output", help="Output C header file", default="ezp2019_chips.h")
    args = parser.parse_args()
    
    generate_header(args.input, args.output)
    print(f"Header generated: {args.output}")
