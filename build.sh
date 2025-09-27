#!/bin/bash
set -euo pipefail

CXX="g++-13"
SRC_DIR="httpRestServerAsCPP20Module"
OUT_DIR="build"
BMI_DIR="gcm.cache"
EXE="httpRestServer"

rm -rf "$OUT_DIR" "$BMI_DIR"
mkdir -p "$OUT_DIR" "$BMI_DIR"

# --- Step 1: collect modules and dependencies ---
declare -A MOD_DEPS
declare -A MOD_PATHS
declare -a MODULES

for ixx in $(find "$SRC_DIR" -name '*.ixx'); do
    base=$(basename "$ixx" .ixx)
    MODULES+=("$base")
    MOD_PATHS["$base"]="$ixx"

    deps=()
    # Match lines like: import Foo; or import Foo:part;
    while read -r line; do
        dep=$(echo "$line" | sed -n 's/^[[:space:]]*import[[:space:]]\+\([a-zA-Z0-9_]\+\).*/\1/p')
        if [[ -n "$dep" && -f "$SRC_DIR/$dep.ixx" ]]; then
            deps+=("$dep")
        fi
    done < <(grep -E '^[[:space:]]*import[[:space:]]+' "$ixx" || true)

    MOD_DEPS["$base"]="${deps[*]}"
done

# --- Step 2: topological sort ---
declare -A visited
build_order=()

visit() {
    local mod="$1"
    [[ "${visited[$mod]}" == "1" ]] && return
    visited[$mod]=1
    for dep in ${MOD_DEPS[$mod]}; do
        visit "$dep"
    done
    build_order+=("$mod")
}

for mod in "${MODULES[@]}"; do
    visit "$mod"
done

# --- Step 3: build modules in order ---
for mod in "${build_order[@]}"; do
    ixx="${MOD_PATHS[$mod]}"
    BMI_FLAGS=""
    for dep in ${MOD_DEPS[$mod]}; do
        BMI_FLAGS="$BMI_FLAGS -fmodule-file=$BMI_DIR/$dep.gcm"
    done
    $CXX -std=c++20 -fmodules $BMI_FLAGS -c "$ixx" -o "$OUT_DIR/$mod.o"
    # Move BMI file if generated
    if [ -f "$mod.gcm" ]; then
        mv "$mod.gcm" "$BMI_DIR/"
    elif [ -f "$OUT_DIR/$mod.gcm" ]; then
        mv "$OUT_DIR/$mod.gcm" "$BMI_DIR/"
    fi
done

# --- Step 4: gather BMI flags ---
BMI_FLAGS=""
for bmi in "$BMI_DIR"/*.gcm; do
    BMI_FLAGS="$BMI_FLAGS -fmodule-file=$bmi"
done

# --- Step 5: compile .cpp files ---
for cpp in $(find "$SRC_DIR" -name '*.cpp'); do
    base=$(basename "$cpp" .cpp)
    $CXX -std=c++20 -fmodules -I"$SRC_DIR" $BMI_FLAGS -c "$cpp" -o "$OUT_DIR/$base.o"
done

# --- Step 6: link everything ---
$CXX -std=c++20 -fmodules "$OUT_DIR"/*.o -o "$OUT_DIR/$EXE" -lpthread

echo "✅ Build complete. Run with: $OUT_DIR/$EXE"
