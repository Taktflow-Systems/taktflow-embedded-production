# OSEK bootstrap demo application runner.
#
# Usage from repo root:
#   make -f firmware/bsw/os/bootstrap/demo/Makefile
#   make -f firmware/bsw/os/bootstrap/demo/Makefile clean
#
# This builds the same demo task graph in four host-executable variants:
#   - host kernel model
#   - TMS570 port model
#   - STM32 port model
#   - STM32L5 port model

THIS_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT     := $(abspath $(THIS_DIR)/../../../../..)

BOOT     := $(ROOT)/firmware/bsw/os/bootstrap
DET      := $(ROOT)/firmware/bsw/services/Det
BUILD    := $(ROOT)/build/os-demo

CC       ?= gcc
PYTHON   ?= python
CFLAGS   ?= -Wall -Wextra -Wshadow -Wdouble-promotion -Wformat=2 -Wundef -g

INC := -I$(ROOT)/firmware/bsw/include \
       -I$(BOOT)/include \
       -I$(BOOT)/port/include \
       -I$(BOOT)/src \
       -I$(BOOT)/demo \
       -I$(DET)/include

KERNEL_SRC := $(wildcard $(BOOT)/src/*.c)
KERNEL_STEMS := $(notdir $(basename $(KERNEL_SRC)))
DEMO_SRC := $(BOOT)/demo/Os_Demo_App.c $(BOOT)/demo/Os_Demo_Main.c
SUPPORT_SRC := $(DET)/src/Det.c

NO_PLATFORM_TUS := Os_Interrupt Os_MemProt Os_TimingProt
PORT_SPECS := Tms570:tms570:PLATFORM_TMS570 \
              Stm32:stm32:PLATFORM_STM32 \
              Stm32L5:stm32l5:PLATFORM_STM32L5
PORT_NAMES := $(foreach s,$(PORT_SPECS),$(word 1,$(subst :, ,$(s))))
PORT_OBJDIRS := $(addprefix $(BUILD)/obj_,$(PORT_NAMES))

HOST_BIN := $(BUILD)/osek_demo_host
PORT_BINS := $(addprefix $(BUILD)/osek_demo_,$(PORT_NAMES))
RUNNER := $(BUILD)/run_demo.py

define RUNNER_PY
import pathlib
import subprocess
import sys

def main(args):
    fail = 0
    for demo_bin in args:
        path = pathlib.Path(demo_bin)
        result = subprocess.run(
            [str(path)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        output = result.stdout.strip()
        pathlib.Path(str(path) + ".out").write_text(result.stdout)
        print(f"{path.name}: {output}")
        if result.returncode != 0:
            fail = 1
    return fail

if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
endef

.PHONY: all build run clean

all: run

build: $(HOST_BIN) $(PORT_BINS)

run: build $(RUNNER)
	$(PYTHON) $(RUNNER) $(HOST_BIN) $(PORT_BINS)

$(BUILD):
	$(PYTHON) -c "import os, sys; os.makedirs(sys.argv[1], exist_ok=True)" "$@"

$(PORT_OBJDIRS): | $(BUILD)
	$(PYTHON) -c "import os, sys; os.makedirs(sys.argv[1], exist_ok=True)" "$@"

$(RUNNER): | $(BUILD)
	$(file >$@,$(RUNNER_PY))
	@echo Wrote $@

$(HOST_BIN): $(KERNEL_SRC) $(SUPPORT_SRC) $(DEMO_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -DUNIT_TEST $(INC) \
	  $(KERNEL_SRC) $(SUPPORT_SRC) $(DEMO_SRC) -o $@

define NO_PLATFORM_OBJ_RULE
$(BUILD)/obj_$(1)/$(2).o: $(BOOT)/src/$(2).c | $(BUILD)/obj_$(1)
	$(CC) $(CFLAGS) -DUNIT_TEST $(INC) -I$(ROOT)/firmware/platform/$(3)/include \
	  -c $$< -o $$@
endef

define PORT_RULE
PORT_OBJS_$(1) := $$(addprefix $(BUILD)/obj_$(1)/,$$(addsuffix .o,$$(KERNEL_STEMS))) \
                  $(BUILD)/obj_$(1)/Det.o \
                  $(BUILD)/obj_$(1)/Os_Demo_App.o \
                  $(BUILD)/obj_$(1)/Os_Demo_Main.o

$(BUILD)/obj_$(1)/%.o: $(BOOT)/src/%.c | $(BUILD)/obj_$(1)
	$(CC) $(CFLAGS) -DUNIT_TEST -D$(3) $(INC) -I$(ROOT)/firmware/platform/$(2)/include \
	  -c $$< -o $$@

$$(foreach t,$$(NO_PLATFORM_TUS),$$(eval $$(call NO_PLATFORM_OBJ_RULE,$(1),$$(t),$(2))))

$(BUILD)/obj_$(1)/Det.o: $(DET)/src/Det.c | $(BUILD)/obj_$(1)
	$(CC) $(CFLAGS) -DUNIT_TEST $(INC) -c $$< -o $$@

$(BUILD)/obj_$(1)/Os_Demo_App.o: $(BOOT)/demo/Os_Demo_App.c | $(BUILD)/obj_$(1)
	$(CC) $(CFLAGS) -DUNIT_TEST -D$(3) $(INC) -I$(ROOT)/firmware/platform/$(2)/include \
	  -c $$< -o $$@

$(BUILD)/obj_$(1)/Os_Demo_Main.o: $(BOOT)/demo/Os_Demo_Main.c | $(BUILD)/obj_$(1)
	$(CC) $(CFLAGS) -DUNIT_TEST -D$(3) $(INC) -I$(ROOT)/firmware/platform/$(2)/include \
	  -c $$< -o $$@

$(BUILD)/osek_demo_$(1): \
    $$(PORT_OBJS_$(1)) \
    $(ROOT)/firmware/platform/$(2)/src/Os_Port_$(1).c \
    $(BOOT)/port/src/Os_Port_TaskBinding.c | $(BUILD)
	$(CC) $(CFLAGS) -DUNIT_TEST -D$(3) $(INC) -I$(ROOT)/firmware/platform/$(2)/include \
	  $$(PORT_OBJS_$(1)) \
	  $(ROOT)/firmware/platform/$(2)/src/Os_Port_$(1).c \
	  $(BOOT)/port/src/Os_Port_TaskBinding.c \
	  -o $$@
endef

$(foreach s,$(PORT_SPECS),$(eval $(call PORT_RULE,$(word 1,$(subst :, ,$(s))),$(word 2,$(subst :, ,$(s))),$(word 3,$(subst :, ,$(s))))))

clean:
	$(PYTHON) -c "import shutil, sys; shutil.rmtree(sys.argv[1], ignore_errors=True)" "$(BUILD)"
