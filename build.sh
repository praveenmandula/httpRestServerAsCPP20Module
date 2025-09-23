#!/bin/bash
set -e

SRC_DIR="httpRestServerAsCPP20Module"
OUT_DIR="build"
EXE="httpRestServer"

mkdir -p "$OUT_DIR"

# Find all .ixx files (module interfaces)
IXXS=($(find "$SRC_DIR" -name '*.ixx'))

# Compile each .ixx file to object file (and BMI if supported)
for ixx in "${IXXS[@]}"; do
    base=$(basename "$ixx" .ixx)
    g++ -std=c++20 -fmodules-ts -c "$ixx" -o "$OUT_DIR/$base.o" -x c++-module
done

# Find all .cpp files
CPPS=($(find "$SRC_DIR" -name '*.cpp'))

# Compile each .cpp file to object file
for cpp in "${CPPS[@]}"; do
    base=$(basename "$cpp" .cpp)
    g++ -std=c++20 -fmodules-ts -I"$SRC_DIR" -c "$cpp" -o "$OUT_DIR/$base.o"
done

# Link all object files
g++ -std=c++20 -fmodules-ts "$OUT_DIR"/*.o -o "$OUT_DIR/$EXE" -lpthread

echo "Build complete. Run with: $OUT_DIR/$EXE"