##############################################################################
# Makefile for ThreadX CAN experiment — NUCLEO-G474RE
# Build: PATH="/c/tools/msys64/ucrt64/bin:$PATH" make -j$(nproc)
# Flash: PATH="/c/tools/msys64/ucrt64/bin:$PATH" make flash
##############################################################################

# Toolchain
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
CP      = $(PREFIX)objcopy
SZ      = $(PREFIX)size

# Target
TARGET  = fzc_threadx
BUILD   = build

# MCU flags
CPU     = -mcpu=cortex-m4
FPU     = -mfpu=fpv4-sp-d16
FLOAT   = -mfloat-abi=hard
MCU     = $(CPU) -mthumb $(FPU) $(FLOAT)

# Defines
DEFS    = -DSTM32G474xx -DUSE_HAL_DRIVER -DTX_INCLUDE_USER_DEFINE_FILE

##############################################################################
# Sources
##############################################################################

# Project application sources
C_SOURCES  = \
  Src/main.c \
  Src/app_azure_rtos.c \
  Src/app_threadx.c \
  Src/stm32g4xx_hal_msp.c \
  Src/stm32g4xx_hal_timebase_tim.c \
  Src/stm32g4xx_it.c \
  Src/system_stm32g4xx.c \
  STM32CubeIDE/Application/User/syscalls.c \
  STM32CubeIDE/Application/User/sysmem.c

# HAL drivers (only enabled modules)
C_SOURCES += \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_cortex.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma_ex.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_exti.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash_ex.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash_ramfunc.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_fdcan.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_gpio.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr_ex.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc_ex.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_tim.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_tim_ex.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_uart.c \
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_uart_ex.c

# ThreadX kernel sources (all .c in common/src)
C_SOURCES += $(wildcard threadx/common/src/*.c)

# Assembly sources
ASM_SOURCES = \
  STM32CubeIDE/Application/Startup/startup_stm32g474retx.s \
  Src/tx_initialize_low_level.s

# ThreadX port assembly (.S, needs preprocessor)
ASM_SOURCES_S = $(wildcard threadx/ports/cortex_m4/gnu/src/*.S)

##############################################################################
# Include paths
##############################################################################

INCLUDES = \
  -IInc \
  -IDrivers/STM32G4xx_HAL_Driver/Inc \
  -IDrivers/STM32G4xx_HAL_Driver/Inc/Legacy \
  -IDrivers/CMSIS/Include \
  -IDrivers/CMSIS/Device/ST/STM32G4xx/Include \
  -Ithreadx/common/inc \
  -Ithreadx/ports/cortex_m4/gnu/inc

##############################################################################
# Compiler / Linker flags
##############################################################################

CFLAGS   = $(MCU) $(DEFS) $(INCLUDES) -Wall -Werror -fdata-sections -ffunction-sections -Os -g
ASFLAGS  = $(MCU) -g

LDSCRIPT = STM32CubeIDE/STM32G474RETX_FLASH.ld
LDFLAGS  = $(MCU) -specs=nosys.specs -specs=nano.specs -T$(LDSCRIPT) \
           -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map,--cref

##############################################################################
# Object lists
##############################################################################

# C objects
OBJECTS  = $(addprefix $(BUILD)/,$(C_SOURCES:.c=.o))
# .s objects (lowercase, no preprocessor)
OBJECTS += $(addprefix $(BUILD)/,$(ASM_SOURCES:.s=.o))
# .S objects (uppercase, preprocessor)
OBJECTS += $(addprefix $(BUILD)/,$(ASM_SOURCES_S:.S=.o))

# Directories to create
OBJDIRS = $(sort $(dir $(OBJECTS)))

##############################################################################
# Rules
##############################################################################

.PHONY: all clean flash size

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin size

# Create output directories
$(OBJDIRS):
	mkdir -p $@

# C sources — project code (strict warnings)
$(BUILD)/%.o: %.c | $(OBJDIRS)
	$(CC) $(CFLAGS) -c $< -o $@

# ThreadX kernel C sources — relax unused-parameter
$(BUILD)/threadx/common/src/%.o: threadx/common/src/%.c | $(OBJDIRS)
	$(CC) $(CFLAGS) -Wno-error=unused-parameter -c $< -o $@

# Lowercase .s assembly (no preprocessor)
$(BUILD)/%.o: %.s | $(OBJDIRS)
	$(AS) $(ASFLAGS) -c $< -o $@

# Uppercase .S assembly (with preprocessor)
$(BUILD)/%.o: %.S | $(OBJDIRS)
	$(AS) $(ASFLAGS) -c $< -o $@

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
	$(PROGRAMMER) --connect port=SWD index=2 --download $< --verify --start

# Clean
clean:
	rm -rf $(BUILD)
