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
DEFS    = -DSTM32G474xx -DUSE_HAL_DRIVER -DTX_INCLUDE_USER_DEFINE_FILE \
          -DPLATFORM_STM32

# Path to production firmware tree (relative to experiment dir)
FW      = ../../firmware

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

# Experiment glue sources (ThreadX-compatible SchM + stubs)
C_SOURCES += \
  Src/SchM_ThreadX.c \
  Src/bsw_stubs.c

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

# ---- Step 7a: MCAL CAN + platform backend ----
C_SOURCES += \
  $(FW)/bsw/mcal/Can/src/Can.c \
  $(FW)/platform/stm32/src/Can_Hw_STM32.c

# ---- Step 7a: Det + CanSM + SchM_Timing ----
C_SOURCES += \
  $(FW)/bsw/services/Det/src/Det.c \
  $(FW)/bsw/services/CanSM/src/CanSM.c \
  $(FW)/bsw/services/SchM/src/SchM_Timing.c

# ---- Step 7b: CanIf ----
C_SOURCES += \
  $(FW)/bsw/ecual/CanIf/src/CanIf.c

# ---- Step 7c: Com + PduR + FZC configs ----
C_SOURCES += \
  $(FW)/bsw/ecual/PduR/src/PduR.c \
  $(FW)/bsw/services/Com/src/Com.c \
  $(FW)/ecu/fzc/cfg/Com_Cfg_Fzc.c

# ---- Step 7d: Rte + Swc_Heartbeat + FZC Rte config ----
C_SOURCES += \
  $(FW)/bsw/rte/src/Rte.c \
  $(FW)/ecu/fzc/cfg/Rte_Cfg_Fzc.c \
  $(FW)/ecu/fzc/src/Swc_Heartbeat.c

# ---- Step 9: All FZC SWCs + MCAL + IoHwAb ----
C_SOURCES += \
  $(FW)/ecu/fzc/src/Swc_Steering.c \
  $(FW)/ecu/fzc/src/Swc_Brake.c \
  $(FW)/ecu/fzc/src/Swc_Lidar.c \
  $(FW)/ecu/fzc/src/Swc_FzcSafety.c \
  $(FW)/ecu/fzc/src/Swc_Buzzer.c \
  $(FW)/ecu/fzc/src/Swc_FzcCanMonitor.c \
  $(FW)/ecu/fzc/src/Swc_FzcCom.c \
  $(FW)/ecu/fzc/src/Swc_FzcDcm.c \
  $(FW)/ecu/fzc/src/Swc_FzcNvm.c \
  $(FW)/ecu/fzc/src/Swc_FzcSensorFeeder.c \
  $(FW)/ecu/fzc/src/Swc_FzcScheduler.c \
  $(FW)/bsw/ecual/IoHwAb/src/IoHwAb.c \
  $(FW)/bsw/mcal/Spi/src/Spi.c \
  $(FW)/bsw/mcal/Adc/src/Adc.c \
  $(FW)/bsw/mcal/Pwm/src/Pwm.c \
  $(FW)/bsw/mcal/Dio/src/Dio.c \
  $(FW)/bsw/mcal/Uart/src/Uart.c \
  $(FW)/bsw/services/NvM/src/NvM.c \
  $(FW)/platform/stm32/src/Spi_Hw_STM32.c \
  $(FW)/platform/stm32/src/Adc_Hw_STM32.c \
  $(FW)/platform/stm32/src/Pwm_Hw_STM32.c \
  $(FW)/platform/stm32/src/Dio_Hw_STM32.c \
  $(FW)/platform/stm32/src/Uart_Hw_STM32.c \
  $(FW)/platform/stm32/src/Gpt_Hw_STM32.c

# ---- Step 8: UDS Diagnostics + XCP ----
C_SOURCES += \
  $(FW)/bsw/services/CanTp/src/CanTp.c \
  $(FW)/bsw/services/Dcm/src/Dcm.c \
  $(FW)/bsw/services/E2E/src/E2E.c \
  $(FW)/bsw/services/E2E/src/E2E_Sm.c \
  $(FW)/bsw/services/BswM/src/BswM.c \
  $(FW)/bsw/services/Dem/src/Dem.c \
  $(FW)/bsw/services/WdgM/src/WdgM.c \
  $(FW)/bsw/services/Xcp/src/Xcp.c \
  $(FW)/ecu/fzc/cfg/Dcm_Cfg_Fzc.c \
  $(FW)/ecu/fzc/cfg/E2E_Cfg_Fzc.c \
  $(FW)/ecu/fzc/cfg/PduR_Cfg_Fzc.c \
  $(FW)/ecu/fzc/cfg/CanIf_Cfg_Fzc.c \
  $(FW)/ecu/fzc/cfg/CanTp_Cfg_Fzc.c

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
  -Ithreadx/ports/cortex_m4/gnu/inc \
  -I$(FW)/bsw/include \
  -I$(FW)/bsw/mcal/Can/include \
  -I$(FW)/bsw/ecual/CanIf/include \
  -I$(FW)/bsw/ecual/PduR/include \
  -I$(FW)/bsw/services/Com/include \
  -I$(FW)/bsw/services/SchM/include \
  -I$(FW)/bsw/services/Det/include \
  -I$(FW)/bsw/services/Dem/include \
  -I$(FW)/bsw/services/E2E/include \
  -I$(FW)/bsw/services/WdgM/include \
  -I$(FW)/bsw/services/Dcm/include \
  -I$(FW)/bsw/services/BswM/include \
  -I$(FW)/bsw/services/CanSM/include \
  -I$(FW)/bsw/services/CanTp/include \
  -I$(FW)/bsw/services/Xcp/include \
  -I$(FW)/bsw/services/NvM/include \
  -I$(FW)/bsw/rte/include \
  -I$(FW)/bsw/ecual/IoHwAb/include \
  -I$(FW)/ecu/fzc/include \
  -I$(FW)/ecu/fzc/cfg/platform_target \
  -I$(FW)/bsw/mcal/Spi/include \
  -I$(FW)/bsw/mcal/Adc/include \
  -I$(FW)/bsw/mcal/Pwm/include \
  -I$(FW)/bsw/mcal/Dio/include \
  -I$(FW)/bsw/mcal/Uart/include \
  -I$(FW)/bsw/mcal/Gpt/include \
  -I$(FW)/bsw/os/include \
  -I$(FW)/platform/stm32/include

##############################################################################
# Compiler / Linker flags
##############################################################################

CFLAGS   = $(MCU) $(DEFS) $(INCLUDES) -Wall -Werror -fdata-sections -ffunction-sections -Os -g
ASFLAGS  = $(MCU) -g

LDSCRIPT = STM32CubeIDE/STM32G474RETX_FLASH.ld
LDFLAGS  = $(MCU) -specs=nosys.specs -specs=nano.specs -T$(LDSCRIPT) \
           -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map,--cref

##############################################################################
# Object lists — strip ../../ so all objects stay INSIDE build/
##############################################################################

# Strip leading ../../ from firmware paths → objects go to build/firmware/...
strip_dotdot = $(patsubst ../../%,%,$1)

# C objects
OBJECTS  = $(addprefix $(BUILD)/,$(call strip_dotdot,$(C_SOURCES:.c=.o)))
# .s objects (lowercase, no preprocessor)
OBJECTS += $(addprefix $(BUILD)/,$(call strip_dotdot,$(ASM_SOURCES:.s=.o)))
# .S objects (uppercase, preprocessor)
OBJECTS += $(addprefix $(BUILD)/,$(call strip_dotdot,$(ASM_SOURCES_S:.S=.o)))

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

# BSW/firmware sources — map ../../firmware/X to build/firmware/X
$(BUILD)/firmware/%.o: $(FW)/%.c | $(OBJDIRS)
	$(CC) $(CFLAGS) -Wno-error=unused-parameter -Wno-error=unused-variable -c $< -o $@

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
	$(PROGRAMMER) --connect port=SWD sn=STLINK-CVC-PLACEHOLDER reset=HWrst --download $< --verify --start

# Clean — rm -rf build/ is sufficient now (no escaped paths)
clean:
	rm -rf $(BUILD)
