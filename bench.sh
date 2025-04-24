#!/bin/bash

INPUT_DIR="inputs"
OUTPUT_DIR="outputs"
CFS_EXEC="bin/CFS"
DPS_EXEC="bin/DPS-DTQ"
REF_EXEC="bin/REF_PAPER_ALGO"

CFS_OUT_DIR="$OUTPUT_DIR/CFS"
DPS_OUT_DIR="$OUTPUT_DIR/DPS-DTQ"
REF_OUT_DIR="$OUTPUT_DIR/REF_PAPER_ALGO"

mkdir -p "$CFS_OUT_DIR"
mkdir -p "$DPS_OUT_DIR"
mkdir -p "$REF_OUT_DIR"

for input_file in "$INPUT_DIR"/*.txt; do
    base_name=$(basename "$input_file" .txt)
    "$CFS_EXEC" "$input_file" > "$CFS_OUT_DIR/$base_name.csv"
    "$DPS_EXEC" "$input_file" > "$DPS_OUT_DIR/$base_name.csv"
    "$REF_EXEC" "$input_file" > "$REF_OUT_DIR/$base_name.csv"
done

echo "Execution completed. Outputs written to $OUTPUT_DIR."
