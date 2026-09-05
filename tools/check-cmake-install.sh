#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Release CMake build with -Werror (so NDEBUG is exercised), install into a
# scratch prefix, then build and run one consumer through
# find_package(maelys-json CONFIG) and one through pkg-config.
# Usage: check-cmake-install.sh WORKDIR
set -eu
root=$(pwd)
work=$1
case "$work" in /*) ;; *) work="$root/$work" ;; esac
version=$(cat VERSION)
rm -rf "$work"
mkdir -p "$work/consumer"
cmake -S "$root" -B "$work/build" -DCMAKE_BUILD_TYPE=Release \
    -DMAELYS_JSON_WERROR=ON -DCMAKE_INSTALL_PREFIX="$work/prefix" >/dev/null
cmake --build "$work/build" >/dev/null
ctest --test-dir "$work/build" --output-on-failure >/dev/null
cmake --install "$work/build" >/dev/null

cat > "$work/consumer/main.c" <<'C'
#include <maelys/json.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static_assert(MAELYS_JSON_ABI_VERSION == 1u, "maelys-json ABI mismatch");
int main(void) {
    maelys_json_document_t *document = NULL;
    const char text[] = "{\"a\":[1,2]}";
    int canonical = 0;
    if (maelys_json_document_parse(text, sizeof(text) - 1u,
            MAELYS_JSON_PROFILE_RFC8259, NULL, &document, NULL) != MAELYS_JSON_OK ||
        maelys_json_document_is_canonical(document, 0u, &canonical) != MAELYS_JSON_OK ||
        !canonical) {
        return 1;
    }
    maelys_json_document_release(document);
    puts(maelys_json_version());
    return strcmp(maelys_json_version(), MAELYS_JSON_VERSION_STRING) != 0;
}
C
cat > "$work/consumer/CMakeLists.txt" <<C
cmake_minimum_required(VERSION 3.16)
project(consumer C)
set(CMAKE_C_STANDARD 11)
find_package(maelys-json $version CONFIG REQUIRED)
add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE maelys::json)
C
cmake -S "$work/consumer" -B "$work/consumer/build" \
    -DCMAKE_PREFIX_PATH="$work/prefix" >/dev/null
cmake --build "$work/consumer/build" >/dev/null
found=$("$work/consumer/build/consumer")
test "$found" = "$version" || { echo "find_package consumer reported $found" >&2; exit 1; }
echo "cmake-check: find_package(maelys-json $version CONFIG) OK"

if command -v pkg-config >/dev/null 2>&1; then
    pcdir=$(dirname "$(find "$work/prefix" -name maelys-json.pc | head -1)")
    flags=$(PKG_CONFIG_PATH="$pcdir" pkg-config --cflags --libs maelys-json)
    # shellcheck disable=SC2086
    ${CC:-cc} -std=c11 "$work/consumer/main.c" $flags -o "$work/consumer/pc-consumer"
    found=$("$work/consumer/pc-consumer")
    test "$found" = "$version" || { echo "pkg-config consumer reported $found" >&2; exit 1; }
    echo "cmake-check: pkg-config maelys-json OK"
else
    echo "cmake-check: pkg-config not installed, consumer skipped"
fi
