# OSEK OS bootstrap host test runner (Unity, gcc on host).
#
# Usage (from repo root or anywhere):
#   make -f firmware/bsw/os/bootstrap/test/Makefile          # build + run all
#   make -f firmware/bsw/os/bootstrap/test/Makefile build    # build only
#   make -f firmware/bsw/os/bootstrap/test/Makefile clean
#
# Recipe (see docs/plans/plan-os-test-suite.md, step S-OSTS-01):
#  - Kernel-only suites: every TU with -DUNIT_TEST, no platform define.
#  - Port suites: kernel TUs with -DUNIT_TEST -DPLATFORM_<X>, EXCEPT
#    Os_Interrupt.c / Os_MemProt.c / Os_TimingProt.c (their host stubs are
#    behind `#if defined(UNIT_TEST) && !defined(PLATFORM_*)`; the real shims
#    exist only in target asm). The platform port source, the task-binding
#    boundary, and the test TU get both defines.

THIS_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT     := $(abspath $(THIS_DIR)/../../../../..)

BOOT     := $(ROOT)/firmware/bsw/os/bootstrap
UNITY    := $(ROOT)/firmware/lib/vendor/unity
DET      := $(ROOT)/firmware/bsw/services/Det
BUILD    := $(ROOT)/build/os-tests

CC       ?= gcc
PYTHON   ?= python
CFLAGS   ?= -Wall -Wextra -Wshadow -Wdouble-promotion -Wformat=2 -Wundef -g

INC := -I$(ROOT)/firmware/bsw/include \
       -I$(BOOT)/include \
       -I$(BOOT)/port/include \
       -I$(BOOT)/src \
       -I$(UNITY) \
       -I$(DET)/include

KERNEL_SRC := $(wildcard $(BOOT)/src/*.c)
KERNEL_STEMS := $(notdir $(basename $(KERNEL_SRC)))
SUPPORT_SRC := $(UNITY)/unity.c $(DET)/src/Det.c

# Kernel TUs whose host stubs disappear under a platform define - always
# compiled WITHOUT the platform define, even in port-suite builds.
NO_PLATFORM_TUS := Os_Interrupt Os_MemProt Os_TimingProt

KERNEL_SUITES := test_Os_asild \
                 test_Os_ConfigValidation \
                 test_Os_Interrupt \
                 test_Os_MemProt \
                 test_Os_ProtectionHook \
                 test_Os_ScheduleTable \
                 test_Os_ServiceProt \
                 test_Os_TimingProt \
                 test_Os_Conformance_Sched

# Port suites: <suite-stem> <platform-dir> <platform-define>
PORT_SPECS := Tms570:tms570:PLATFORM_TMS570 \
              Stm32:stm32:PLATFORM_STM32 \
              Stm32L5:stm32l5:PLATFORM_STM32L5

PORT_NAMES  := $(foreach s,$(PORT_SPECS),$(word 1,$(subst :, ,$(s))))
PORT_OBJDIRS := $(addprefix $(BUILD)/obj_,$(PORT_NAMES))
KERNEL_BINS := $(addprefix $(BUILD)/,$(KERNEL_SUITES))
# The former monolithic TMS570 smoke binary test_Os_Port_Tms570_bootstrap was
# superseded by the harvested split family (every test name it contained
# exists in the test_Os_Port_Tms570_bootstrap_* files), so Tms570 stays in
# PORT_SPECS only to produce the platform kernel objects for the split binary.
PORT_BIN_NAMES := $(filter-out Tms570,$(PORT_NAMES))
PORT_BINS   := $(addprefix $(BUILD)/test_Os_Port_,$(addsuffix _bootstrap,$(PORT_BIN_NAMES)))

# Harvested split port suites (fix/hil-scenarios bringup line):
#  - STM32: one standalone Unity binary per test_Os_Port_Stm32_bootstrap_*.c
#  - TMS570: all test_Os_Port_Tms570_bootstrap_*.c TUs link into ONE binary
#    (shared support.c + _main.c runner registers the split suites)
STM32_SPLIT_SUITES := $(notdir $(basename $(wildcard $(BOOT)/test/test_Os_Port_Stm32_bootstrap_*.c)))
STM32_SPLIT_BINS   := $(addprefix $(BUILD)/,$(STM32_SPLIT_SUITES))
TMS570_SPLIT_SRC   := $(wildcard $(BOOT)/test/test_Os_Port_Tms570_bootstrap_*.c)
TMS570_SPLIT_BIN   := $(BUILD)/test_Os_Port_Tms570_bootstrap_split

RUNNER      := $(BUILD)/run_os_tests.py

define RUNNER_PY
import pathlib
import subprocess
import sys

def unity_summary(output):
    for line in output.splitlines():
        if line[:1].isdigit() and " Tests " in line:
            return line
    return "no Unity summary"

def main(test_bins):
    fail = 0
    for test_bin in test_bins:
        path = pathlib.Path(test_bin)
        result = subprocess.run(
            [str(path)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        output = result.stdout
        pathlib.Path(str(path) + ".out").write_text(output)
        summary = unity_summary(output)
        status = "PASS" if result.returncode == 0 else "FAIL"
        print(f"{status}  {path.name}  ({summary})")
        if result.returncode != 0:
            fail = 1
            for line in output.splitlines():
                if ":FAIL" in line:
                    print(line)
    return fail

if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
endef

.PHONY: all build run clean

all: run

build: $(KERNEL_BINS) $(PORT_BINS) $(STM32_SPLIT_BINS) $(TMS570_SPLIT_BIN)

run: build $(RUNNER)
	$(PYTHON) $(RUNNER) $(KERNEL_BINS) $(PORT_BINS) $(STM32_SPLIT_BINS) $(TMS570_SPLIT_BIN)

$(BUILD):
	$(PYTHON) -c "import os, sys; os.makedirs(sys.argv[1], exist_ok=True)" "$@"

$(PORT_OBJDIRS): | $(BUILD)
	$(PYTHON) -c "import os, sys; os.makedirs(sys.argv[1], exist_ok=True)" "$@"

$(RUNNER): | $(BUILD)
	$(file >$@,$(RUNNER_PY))
	@echo Wrote $@

# --- kernel-only suites: single-shot compile, -DUNIT_TEST only ---------------
$(BUILD)/test_Os_%: $(BOOT)/test/test_Os_%.c $(KERNEL_SRC) $(SUPPORT_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -DUNIT_TEST $(INC) \
	  $(KERNEL_SRC) $(SUPPORT_SRC) $< -o $@

# --- port suites: split-TU compile -------------------------------------------
define NO_PLATFORM_OBJ_RULE
$(BUILD)/obj_$(1)/$(2).o: $(BOOT)/src/$(2).c | $(BUILD)/obj_$(1)
	$(CC) $(CFLAGS) -DUNIT_TEST $(INC) -I$(ROOT)/firmware/platform/$(3)/include \
	  -c $$< -o $$@
endef

define PORT_RULE
PORT_OBJS_$(1) := $$(addprefix $(BUILD)/obj_$(1)/,$$(addsuffix .o,$$(KERNEL_STEMS))) \
                  $(BUILD)/obj_$(1)/unity.o \
                  $(BUILD)/obj_$(1)/Det.o

$(BUILD)/obj_$(1)/%.o: $(BOOT)/src/%.c | $(BUILD)/obj_$(1)
	$(CC) $(CFLAGS) -DUNIT_TEST -D$(3) $(INC) -I$(ROOT)/firmware/platform/$(2)/include \
	  -c $$< -o $$@

$$(foreach t,$$(NO_PLATFORM_TUS),$$(eval $$(call NO_PLATFORM_OBJ_RULE,$(1),$$(t),$(2))))

$(BUILD)/obj_$(1)/unity.o: $(UNITY)/unity.c | $(BUILD)/obj_$(1)
	$(CC) $(CFLAGS) -DUNIT_TEST $(INC) -c $$< -o $$@

$(BUILD)/obj_$(1)/Det.o: $(DET)/src/Det.c | $(BUILD)/obj_$(1)
	$(CC) $(CFLAGS) -DUNIT_TEST $(INC) -c $$< -o $$@

$(BUILD)/test_Os_Port_$(1)_bootstrap: $(BOOT)/test/test_Os_Port_$(1)_bootstrap.c \
    $(KERNEL_SRC) $(SUPPORT_SRC) \
    $(ROOT)/firmware/platform/$(2)/src/Os_Port_$(1).c \
    $(BOOT)/port/src/Os_Port_TaskBinding.c \
    $$(PORT_OBJS_$(1)) | $(BUILD)
	$(CC) $(CFLAGS) -DUNIT_TEST -D$(3) $(INC) -I$(ROOT)/firmware/platform/$(2)/include \
	  $$(PORT_OBJS_$(1)) \
	  $(ROOT)/firmware/platform/$(2)/src/Os_Port_$(1).c \
	  $(BOOT)/port/src/Os_Port_TaskBinding.c \
	  $$< -o $$@
endef

$(foreach s,$(PORT_SPECS),$(eval $(call PORT_RULE,$(word 1,$(subst :, ,$(s))),$(word 2,$(subst :, ,$(s))),$(word 3,$(subst :, ,$(s))))))

# --- harvested split suites ---------------------------------------------------
define STM32_SPLIT_RULE
$(BUILD)/$(1): $(BOOT)/test/$(1).c \
    $(ROOT)/firmware/platform/stm32/src/Os_Port_Stm32.c \
    $(BOOT)/port/src/Os_Port_TaskBinding.c \
    $$(PORT_OBJS_Stm32) | $(BUILD)
	$(CC) $(CFLAGS) -DUNIT_TEST -DPLATFORM_STM32 $(INC) -I$(ROOT)/firmware/platform/stm32/include \
	  $$(PORT_OBJS_Stm32) \
	  $(ROOT)/firmware/platform/stm32/src/Os_Port_Stm32.c \
	  $(BOOT)/port/src/Os_Port_TaskBinding.c \
	  $$< -o $$@
endef

$(foreach t,$(STM32_SPLIT_SUITES),$(eval $(call STM32_SPLIT_RULE,$(t))))

$(TMS570_SPLIT_BIN): $(TMS570_SPLIT_SRC) \
    $(ROOT)/firmware/platform/tms570/src/Os_Port_Tms570.c \
    $(BOOT)/port/src/Os_Port_TaskBinding.c \
    $(PORT_OBJS_Tms570) | $(BUILD)
	$(CC) $(CFLAGS) -DUNIT_TEST -DPLATFORM_TMS570 $(INC) -I$(ROOT)/firmware/platform/tms570/include \
	  $(PORT_OBJS_Tms570) \
	  $(ROOT)/firmware/platform/tms570/src/Os_Port_Tms570.c \
	  $(BOOT)/port/src/Os_Port_TaskBinding.c \
	  $(TMS570_SPLIT_SRC) -o $@

clean:
	$(PYTHON) -c "import shutil, sys; shutil.rmtree(sys.argv[1], ignore_errors=True)" "$(BUILD)"
