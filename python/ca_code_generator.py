"""
GPS C/A (PRN) code generator.

Generates the 1023-chip Gold code for GPS satellites PRN 1-32 using two
10-bit LFSRs (G1 and G2) with the standard G2 tap pairs, and writes them to a
CSV file as BPSK chips (+1 / -1). The CSV is consumed by the LabVIEW
"PRN C/A Code Generator" VI (labview/new-cacode.vi).

Usage:
    python ca_code_generator.py                    # writes prn_output.csv
    python ca_code_generator.py -o my_codes.csv    # custom output path
"""

import argparse
import csv

import numpy as np

# G2 tap pairs for PRN 1-32 (1-based register positions)
TAPS = {
    1: (2, 6),   2: (3, 7),   3: (4, 8),   4: (5, 9),   5: (1, 9),
    6: (2, 10),  7: (1, 8),   8: (2, 9),   9: (3, 10), 10: (2, 3),
    11: (3, 4), 12: (5, 6),  13: (6, 7),  14: (7, 8),  15: (8, 9),
    16: (9, 10), 17: (1, 4),  18: (2, 5),  19: (3, 6),  20: (4, 7),
    21: (5, 8),  22: (6, 9),  23: (1, 3),  24: (4, 6),  25: (5, 7),
    26: (6, 8),  27: (7, 9),  28: (8, 10), 29: (1, 6),  30: (2, 7),
    31: (3, 8),  32: (4, 9),
}


def generate_ca(prn):
    """Return the 1023-chip C/A code for `prn` as a list of +1/-1 chips."""
    if prn not in TAPS:
        raise ValueError(f"Invalid PRN {prn}: must be 1-32")

    g1 = np.ones(10, dtype=int)
    g2 = np.ones(10, dtype=int)
    tap1, tap2 = TAPS[prn]
    chips = []

    for _ in range(1023):
        ca_bit = g1[9] ^ (g2[tap1 - 1] ^ g2[tap2 - 1])
        chips.append(1 if ca_bit == 0 else -1)  # BPSK mapping: 0 -> +1, 1 -> -1

        g1_feedback = g1[2] ^ g1[9]
        g2_feedback = g2[1] ^ g2[2] ^ g2[5] ^ g2[7] ^ g2[8] ^ g2[9]

        g1[1:] = g1[:-1]
        g2[1:] = g2[:-1]
        g1[0] = g1_feedback
        g2[0] = g2_feedback

    return chips


def generate_all_to_csv(filepath):
    """Write PRN 1-32 to a CSV: one row per PRN, 1023 chip columns."""
    with open(filepath, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["PRN"] + [f"Chip{i}" for i in range(1, 1024)])
        for prn in range(1, 33):
            writer.writerow([prn] + generate_ca(prn))


def main():
    parser = argparse.ArgumentParser(description="Generate GPS C/A codes (PRN 1-32) as a CSV.")
    parser.add_argument("-o", "--output", default="prn_output.csv", help="output CSV path")
    args = parser.parse_args()

    generate_all_to_csv(args.output)
    print(f"Done - saved {args.output}")


if __name__ == "__main__":
    main()
