# ThreadX on STM32L552ZE (Cortex-M33) — NUCLEO-L552ZE-Q
# Step 1: LED blink + UART (bare-metal registers, no HAL)
#
# Prerequisites:
#   arm-none-eabi-gcc (via MSYS2 or standalone)
#
# Build: PATH="/c/tools/msys64/ucrt64/bin:$PATH" make -j$(nproc)
# Flash: make flash
#
# TODO:HARDWARE — Once STM32CubeL5 is available, add HAL sources
# and switch from bare-register access to HAL for CAN/SPI/ADC.

PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
CP      = $(PREFIX)objcopy
SZ      = $(PREFIX)size

MCU = -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard

CFLAGS  = $(MCU) -Wall -Wextra -std=c99 -Os -g
CFLAGS += -DTX_INCLUDE_USER_DEFINE_FILE -DPLATFORM_STM32
CFLAGS += -fdata-sections -ffunction-sections

ASFLAGS = $(MCU) -g -DTX_INCLUDE_USER_DEFINE_FILE

ROOT      = ../..
FW        = $(ROOT)/firmware
TX_COMMON = $(ROOT)/firmware/lib/threadx/common
TX_PORT   = $(ROOT)/firmware/lib/threadx/ports/cortex_m33/gnu

INCLUDES  = -ISrc -IInc
INCLUDES += -I$(TX_COMMON)/inc
INCLUDES += -I$(TX_PORT)/inc
INCLUDES += -I$(FW)/bsw/include
INCLUDES += -I$(FW)/bsw/services/SchM/include
INCLUDES += -I$(FW)/bsw/os/include

LDFLAGS  = $(MCU) -Wl,--gc-sections -specs=nosys.specs -specs=nano.specs
LDFLAGS += -T STM32L552ZETX_FLASH.ld
LDFLAGS += -Wl,-Map=$(BUILD)/$(TARGET).map,--cref

BUILD  = build
TARGET = l5_threadx

# Application sources
C_SOURCES = \
  Src/main.c \
  Src/SchM_ThreadX.c \
  Src/bsw_stubs.c

# ThreadX kernel sources
C_SOURCES += $(wildcard $(TX_COMMON)/src/*.c)

# Assembly: startup + ThreadX low-level init
ASM_SOURCES = \
  Src/startup_stm32l552xx.s \
  Src/tx_initialize_low_level.s

# ThreadX Cortex-M33 port assembly (.S with preprocessor)
ASM_SOURCES_S = $(wildcard $(TX_PORT)/src/*.S)

# Object lists — strip ../../ so all objects stay INSIDE build/
strip_dotdot = $(patsubst ../../%,%,$1)

OBJECTS  = $(addprefix $(BUILD)/,$(call strip_dotdot,$(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD)/,$(call strip_dotdot,$(ASM_SOURCES:.s=.o)))
OBJECTS += $(addprefix $(BUILD)/,$(call strip_dotdot,$(ASM_SOURCES_S:.S=.o)))
OBJDIRS  = $(sort $(dir $(OBJECTS)))

.PHONY: all clean flash size

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin size

$(OBJDIRS):
	mkdir -p $@

# C sources
$(BUILD)/%.o: %.c | $(OBJDIRS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Firmware/ThreadX kernel — map ../../firmware/X to build/firmware/X
$(BUILD)/firmware/%.o: $(FW)/%.c | $(OBJDIRS)
	$(CC) $(CFLAGS) $(INCLUDES) -Wno-error=unused-parameter -c $< -o $@

# Lowercase .s assembly (no preprocessor)
$(BUILD)/%.o: %.s | $(OBJDIRS)
	$(AS) $(ASFLAGS) -c $< -o $@

# Uppercase .S assembly (local, with preprocessor)
$(BUILD)/%.o: %.S | $(OBJDIRS)
	$(AS) $(ASFLAGS) $(INCLUDES) -c $< -o $@

# ThreadX port .S assembly — map ../../firmware/X to build/firmware/X
$(BUILD)/firmware/%.o: $(FW)/%.S | $(OBJDIRS)
	$(AS) $(ASFLAGS) $(INCLUDES) -c $< -o $@

# Link
$(BUILD)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

# Binary
$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(CP) -O binary $< $@

# Size report
size: $(BUILD)/$(TARGET).elf
	$(SZ) $<

# Flash via STM32CubeProgrammer
PROGRAMMER = "C:/Program Files (x86)/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe"
flash: $(BUILD)/$(TARGET).elf
	$(PROGRAMMER) --connect port=SWD --download $< --verify --start

clean:
	rm -rf $(BUILD)
