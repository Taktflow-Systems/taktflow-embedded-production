# =============================================================================
# libs/fault_lib/Makefile — POSIX build of the Fault Library C shim
#
# Produces:
#   build/posix/libs/fault_lib/libfaultlib.a    (static archive)
#   build/posix/libs/fault_lib/libfaultlib.so   (shared object)
#
# Usage (from repo root):
#   make -f libs/fault_lib/Makefile                 # build both libs
#   make -f libs/fault_lib/Makefile test            # build + run C unit tests
#   make -f libs/fault_lib/Makefile clean
#
# The POSIX firmware build (firmware/platform/posix/Makefile.posix)
# does not yet link against libfaultlib because no ECU consumes the
# shim API in this phase. Phase 4 Line B wires the ECUs in.
# =============================================================================

CC      ?= gcc
AR      ?= ar

# Match the project-wide POSIX flags where possible. We use -std=c99
# because the rest of the firmware builds with -std=c99, but this
# module is written to compile clean at -std=c11 as well.
CFLAGS  ?= -Wall -Wextra -Werror -pedantic -std=c99 -D_DEFAULT_SOURCE -fPIC -Og -g3
LDFLAGS ?=

ROOT      := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
FAULT_LIB := $(ROOT)/libs/fault_lib
UNITY_DIR := $(ROOT)/firmware/lib/vendor/unity

BUILD_DIR := $(ROOT)/build/posix/libs/fault_lib
OBJ_DIR   := $(BUILD_DIR)/obj
TEST_DIR  := $(BUILD_DIR)/test

INCLUDES := \
    -I$(FAULT_LIB)/include \
    -I$(FAULT_LIB)/src

LIB_SRCS := \
    $(FAULT_LIB)/src/postcard_c.c \
    $(FAULT_LIB)/src/wire_fault_record.c \
    $(FAULT_LIB)/src/fault_lib.c

LIB_OBJS := $(patsubst $(FAULT_LIB)/src/%.c,$(OBJ_DIR)/%.o,$(LIB_SRCS))

STATIC_LIB := $(BUILD_DIR)/libfaultlib.a
SHARED_LIB := $(BUILD_DIR)/libfaultlib.so

TEST_SRCS := \
    $(FAULT_LIB)/tests/test_postcard_c.c \
    $(FAULT_LIB)/tests/test_wire_fault_record.c
TEST_BINS := $(patsubst $(FAULT_LIB)/tests/%.c,$(TEST_DIR)/%,$(TEST_SRCS))

.PHONY: all test clean
all: $(STATIC_LIB) $(SHARED_LIB)

$(BUILD_DIR) $(OBJ_DIR) $(TEST_DIR):
	@mkdir -p $@

$(OBJ_DIR)/%.o: $(FAULT_LIB)/src/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(STATIC_LIB): $(LIB_OBJS) | $(BUILD_DIR)
	$(AR) rcs $@ $(LIB_OBJS)
	@echo "=== Built $@ ==="

$(SHARED_LIB): $(LIB_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o $@ $(LIB_OBJS)
	@echo "=== Built $@ ==="

# --- tests ----------------------------------------------------------------
UNITY_SRC := $(UNITY_DIR)/unity.c

$(TEST_DIR)/%: $(FAULT_LIB)/tests/%.c $(LIB_OBJS) $(UNITY_SRC) | $(TEST_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -I$(UNITY_DIR) \
	    $< $(UNITY_SRC) $(LIB_OBJS) -o $@

test: $(STATIC_LIB) $(TEST_BINS)
	@echo "=== Running fault_lib C unit tests ==="
	@cd $(ROOT) && pass=0; fail=0; total=0; \
	for t in $(TEST_BINS); do \
	    total=$$((total + 1)); \
	    real="$$t"; \
	    if [ ! -x "$$real" ] && [ -x "$${real}.exe" ]; then real="$${real}.exe"; fi; \
	    echo "--- $$real ---"; \
	    if "$$real"; then pass=$$((pass + 1)); else fail=$$((fail + 1)); fi; \
	done; \
	echo "=== fault_lib tests: $$pass/$$total passed ==="; \
	if [ $$fail -ne 0 ]; then exit 1; fi

clean:
	rm -rf $(BUILD_DIR)
	@echo "=== Cleaned fault_lib build artefacts ==="
