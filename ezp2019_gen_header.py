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

    rows = []
    for c in chips_data:
        op_flags = []
        if c["flags"] & 0x00000002:
            op_flags.append("EZP_OP_UNKNOWN")
        if c["flags"] & 0x00010000:
            op_flags.append("EZP_OP_SUPPORTS_ERASE")
        if c["flags"] & 0x00040000:
            op_flags.append("EZP_OP_SECTOR_ERASE")
        if c["flags"] & 0x01000000:
            op_flags.append("EZP_OP_UI_SYNC_1V8")
        
        if op_flags:
            remaining = c["flags"] & ~(0x00000002 | 0x00010000 | 0x00040000 | 0x01000000)
            if remaining:
                op_flags.append(f"0x{remaining:08X}")
            op_mask_str = " | ".join(op_flags)
        else:
            op_mask_str = f"0x{c['flags']:08X}"

        rows.append([
            f'"{c["type"]}",',
            f'"{c["manuf"]}",',
            f'"{c["name"]}",',
            f'0x{c["id"]:08X},',
            f'0x{c["size"]:08X},',
            f'0x{c["pagesize"]:04X},',
            f'0x{c["address"]:04X},',
            f'{c["timing"]},',
            f'{c["reserved"]},',
            f'{op_mask_str}'
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

        f.write("#define EZP_PROTO_NONE           0x00\n")
        f.write("#define EZP_PROTO_SPI            0x01\n")
        f.write("#define EZP_PROTO_I2C            0x02\n")
        f.write("#define EZP_PROTO_MICROWIRE      0x03\n")
        f.write("#define EZP_PROTO_SPI_4BYTE      0x06\n\n")

        f.write("#define EZP_CFG_NONE             0x00\n")
        f.write("#define EZP_CFG_QUAD_SPI         0x01\n")
        f.write("#define EZP_CFG_MW_8BIT_46       0x07\n")
        f.write("#define EZP_CFG_MW_8BIT_56       0x09\n")
        f.write("#define EZP_CFG_1V8_MODE         0x10\n")
        f.write("#define EZP_CFG_MW_16BIT_46      0x57\n")
        f.write("#define EZP_CFG_MW_16BIT_57      0x58\n")
        f.write("#define EZP_CFG_MW_16BIT_56      0x59\n")
        f.write("#define EZP_CFG_MW_16BIT_76      0x5A\n")
        f.write("#define EZP_CFG_MW_16BIT_86      0x5B\n")
        f.write("#define EZP_CFG_WP_DISABLE       0x80\n\n")

        f.write("#define EZP_OPFLAG_NONE          0x00\n")
        f.write("#define EZP_OPFLAG_LOW_VOLT      0x01\n\n")

        f.write("#define EZP_OP_UNKNOWN           0x00000002\n")
        f.write("#define EZP_OP_SUPPORTS_ERASE    0x00010000 /* Chip supports Erase functionality. Enables 'ToolErase' button in the UI. */\n")
        f.write("#define EZP_OP_SECTOR_ERASE      0x00040000 /* Chip supports 4KB Sector Erase. Enables the Sector Erase mode. */\n")
        f.write("#define EZP_OP_UI_SYNC_1V8       0x01000000 /* Syncs UI when 1.8V low-voltage chips are selected. Sets bit 0 at packet offset 0x1c. */\n\n")

        f.write("#define EZP_CMD_ERASE            0x04\n")
        f.write("#define EZP_CMD_READ_EE          0x05\n")
        f.write("#define EZP_CMD_READ_SPI         0x07\n")
        f.write("#define EZP_CMD_RESET            0x08\n")
        f.write("#define EZP_CMD_CONNECT          0x09\n")
        f.write("#define EZP_CMD_STATUS           0x0A\n")
        f.write("#define EZP_CMD_WRITE            0x0B\n")
        f.write("#define EZP_CMD_VERIFY           0x0C\n\n")

        f.write("#define EZP_ERASE_CHIP           0x00\n")
        f.write("#define EZP_ERASE_SECTOR         0x01\n\n")

        f.write("typedef enum {\n")
        f.write("    CHIP_ID_UNKNOWN = 0,\n")
        for c in chips_data:
            # Create a safe enum name: Replace special chars with underscores
            safe_name = c["name"].replace("-", "_").replace("(", "_").replace(")", "_").replace("@", "_").replace(".", "_").replace(" ", "_")
            f.write(f"    CHIP_ID_{c['manuf']}_{safe_name},\n")
        f.write("    CHIP_ID_COUNT\n")
        f.write("} ChipID;\n\n")

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
        f.write("_Static_assert(sizeof(Chip) == 48, \"Chip struct must be exactly 48 bytes\");\n\n")
        
        f.write("static const Chip ezp2019_chips[] = {\n")
        for row in rows:
            f.write("    { ")
            for i, val in enumerate(row):
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
