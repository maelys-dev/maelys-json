# SPDX-License-Identifier: MPL-2.0
# maelys-json build. `make check` is the gate used by CI.
#
# CFLAGS/CXXFLAGS/CPPFLAGS belong to the user and are appended last; the
# project adds its own standard, warnings and include flags. WERROR is empty
# by default so a newer compiler never breaks a downstream build; the check
# and sanitizer targets set it.

CC ?= cc
CXX ?= c++
AR ?= ar
PREFIX ?= /usr/local
DESTDIR ?=
BUILD ?= build
VERSION := $(shell cat VERSION)

CFLAGS ?= -O2 -g
CXXFLAGS ?= -O2 -g
CPPFLAGS ?=
WERROR ?=
CSTD := -std=c11
CXXSTD := -std=c++17
WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes \
	-Wmissing-prototypes -Wconversion -Wsign-conversion -Wcast-qual \
	-Wformat=2 -Wvla -Wundef -Wswitch-enum -Wimplicit-fallthrough
INCLUDES := -Iinclude
ALL_CFLAGS = $(CPPFLAGS) $(INCLUDES) $(CSTD) $(WARNINGS) $(WERROR) $(CFLAGS)
ALL_CXXFLAGS = $(CPPFLAGS) $(INCLUDES) $(CXXSTD) -Wall -Wextra -Wpedantic $(WERROR) $(CXXFLAGS)

LLVM_PREFIX ?= $(shell if test -d /opt/homebrew/opt/llvm/bin; then \
	printf '%s' /opt/homebrew/opt/llvm/bin/; fi)
FUZZ_CC ?= $(shell if test -x $(LLVM_PREFIX)clang; then \
	printf '%s' $(LLVM_PREFIX)clang; else printf '%s' '$(CC)'; fi)
CLANG_TIDY ?= $(shell command -v $(LLVM_PREFIX)clang-tidy 2>/dev/null || \
	command -v clang-tidy 2>/dev/null)
CLANG_FORMAT ?= $(shell command -v $(LLVM_PREFIX)clang-format 2>/dev/null || \
	command -v clang-format 2>/dev/null)

SOURCES := src/common.c src/utf8.c src/keyset.c src/parser.c src/document.c \
	src/helpers.c src/writer.c src/serialize.c src/stdio.c
HEADERS := include/maelys/json.h src/internal.h src/keyset.h \
	src/writer_internal.h
TEST_SOURCES := tests/main.c tests/test_parser.c tests/test_reader.c \
	tests/test_writer.c tests/test_vectors.c tests/test_conformance.c
OBJECTS := $(SOURCES:src/%.c=$(BUILD)/obj/%.o)
LIBRARY := $(BUILD)/lib/libmaelys-json.a
TEST := $(BUILD)/bin/test-json
PC := $(BUILD)/lib/pkgconfig/maelys-json.pc
FLAGS_STAMP := $(BUILD)/cflags.stamp

.PHONY: all test check lint tidy format asan ubsan coverage conformance \
	cmake-check fuzz fuzz-smoke install clean force

all: $(LIBRARY) $(PC)

# Objects depend on the exact flags used, so `make check` rebuilds with
# -Werror even after a plain `make`.
$(FLAGS_STAMP): force
	@mkdir -p $(@D)
	@printf '%s\n' '$(ALL_CFLAGS)' | cmp -s - $@ 2>/dev/null || \
		printf '%s\n' '$(ALL_CFLAGS)' > $@

$(BUILD)/obj/%.o: src/%.c $(HEADERS) $(FLAGS_STAMP)
	@mkdir -p $(@D)
	$(CC) $(ALL_CFLAGS) -MMD -MP -c $< -o $@

$(LIBRARY): $(OBJECTS)
	@mkdir -p $(@D)
	rm -f $@
	ZERO_AR_DATE=1 $(AR) rcs $@ $^

$(PC): pkgconfig/maelys-json.pc.in VERSION
	@mkdir -p $(@D)
	sed -e 's|@PREFIX@|$(PREFIX)|g' -e 's|@VERSION@|$(VERSION)|g' $< >$@

$(TEST): $(TEST_SOURCES) tests/framework.h $(LIBRARY) $(FLAGS_STAMP)
	@mkdir -p $(@D)
	$(CC) $(ALL_CFLAGS) $(TEST_SOURCES) $(LIBRARY) -o $@

SUITE := tests/conformance/JSONTestSuite/test_parsing

test: $(TEST)
	MAELYS_JSON_VECTORS=tests/vectors MAELYS_JSON_TEST_SUITE=$(SUITE) $(TEST)

# Full gate: tests with -Werror, C++ header check, strict lint, size and
# version policies, whitespace hygiene.
check: WERROR := -Werror
check: test lint
	@mkdir -p $(BUILD)
	$(CXX) $(ALL_CXXFLAGS) tests/header_cpp.cpp -c -o $(BUILD)/header-cpp.o
	@find src include tests fuzz -type f \( -name '*.c' -o -name '*.h' \) \
		-exec sh -c 'for f do test "$$(wc -l < "$$f")" -le 1000 || { echo "$$f exceeds 1000 lines" >&2; exit 1; }; done' sh {} +
	sh tools/check-version.sh
	sh tools/check-cmake-sources.sh $(SOURCES)
	sh tools/check-spdx.sh
	@if git rev-parse --git-dir >/dev/null 2>&1; then git diff --check; fi
	@echo "check: OK"

# Syntax-only pass with the strict warning set and -Werror, independent of
# cached objects, with and without NDEBUG (assertions must not be the only
# use of a parameter).
lint:
	@for define in "" -DNDEBUG; do for f in $(SOURCES) $(TEST_SOURCES) fuzz/*.c; do \
		$(CC) $(CPPFLAGS) $$define $(INCLUDES) $(CSTD) $(WARNINGS) -Werror -fsyntax-only $$f || exit 1; \
	done; done
	@echo "lint: OK"

# Release CMake build with -Werror, install into a scratch prefix, then
# build a consumer with find_package and one with pkg-config.
cmake-check:
	sh tools/check-cmake-install.sh $(BUILD)/cmake-install

tidy:
	@test -n "$(CLANG_TIDY)" || { echo "clang-tidy not found"; exit 1; }
	$(CLANG_TIDY) --quiet $(SOURCES) -- $(CPPFLAGS) $(INCLUDES) $(CSTD)

format:
	@test -n "$(CLANG_FORMAT)" || { echo "clang-format not found"; exit 1; }
	$(CLANG_FORMAT) -i $(SOURCES) $(HEADERS) $(TEST_SOURCES) tests/framework.h fuzz/*.c

asan:
	$(MAKE) check BUILD=$(BUILD)-asan WERROR=-Werror \
		CFLAGS='-O1 -g -fsanitize=address -fno-omit-frame-pointer'

ubsan:
	$(MAKE) check BUILD=$(BUILD)-ubsan WERROR=-Werror \
		CFLAGS='-O1 -g -fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer'

# Line coverage of the library sources under the test suite (clang/llvm).
coverage:
	$(MAKE) $(BUILD)-coverage/bin/test-json BUILD=$(BUILD)-coverage \
		CFLAGS='-O0 -g -fprofile-instr-generate -fcoverage-mapping'
	sh tools/coverage-report.sh $(BUILD)-coverage "$(CC)" $(LLVM_PREFIX)

# Conformance only (the vendored JSONTestSuite corpus, also part of `test`).
# MAELYS_JSON_TEST_SUITE overrides the corpus directory.
conformance: $(TEST)
	MAELYS_JSON_VECTORS=tests/vectors \
	MAELYS_JSON_TEST_SUITE=$${MAELYS_JSON_TEST_SUITE:-$(SUITE)} $(TEST) conformance

fuzz:
	@mkdir -p $(BUILD)/bin
	$(FUZZ_CC) $(CPPFLAGS) $(INCLUDES) $(CSTD) -O1 -g -fsanitize=fuzzer,address,undefined \
		fuzz/fuzz_parser.c $(SOURCES) -o $(BUILD)/bin/fuzz-parser
	$(FUZZ_CC) $(CPPFLAGS) $(INCLUDES) $(CSTD) -O1 -g -fsanitize=fuzzer,address,undefined \
		fuzz/fuzz_roundtrip.c $(SOURCES) -o $(BUILD)/bin/fuzz-roundtrip
	$(FUZZ_CC) $(CPPFLAGS) $(INCLUDES) $(CSTD) -O1 -g -fsanitize=fuzzer,address,undefined \
		fuzz/fuzz_writer.c $(SOURCES) -o $(BUILD)/bin/fuzz-writer

FUZZ_TIME ?= 15
fuzz-smoke: fuzz
	@mkdir -p $(BUILD)/corpus-parser $(BUILD)/corpus-roundtrip $(BUILD)/corpus-writer
	$(BUILD)/bin/fuzz-parser -max_total_time=$(FUZZ_TIME) -timeout=2 \
		-rss_limit_mb=1024 -artifact_prefix=$(BUILD)/ -dict=fuzz/json.dict \
		$(BUILD)/corpus-parser fuzz/corpus
	$(BUILD)/bin/fuzz-roundtrip -max_total_time=$(FUZZ_TIME) -timeout=2 \
		-rss_limit_mb=1024 -artifact_prefix=$(BUILD)/ -dict=fuzz/json.dict \
		$(BUILD)/corpus-roundtrip fuzz/corpus
	$(BUILD)/bin/fuzz-writer -max_total_time=$(FUZZ_TIME) -timeout=2 \
		-rss_limit_mb=1024 -artifact_prefix=$(BUILD)/ $(BUILD)/corpus-writer

install: all
	install -d $(DESTDIR)$(PREFIX)/include/maelys $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -m 0644 include/maelys/json.h $(DESTDIR)$(PREFIX)/include/maelys/json.h
	install -m 0644 $(LIBRARY) $(DESTDIR)$(PREFIX)/lib/libmaelys-json.a
	install -m 0644 $(PC) $(DESTDIR)$(PREFIX)/lib/pkgconfig/maelys-json.pc

clean:
	rm -rf $(BUILD) $(BUILD)-asan $(BUILD)-ubsan $(BUILD)-coverage

-include $(OBJECTS:.o=.d)
