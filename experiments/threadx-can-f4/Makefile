# ThreadX + bxCAN experiment for STM32F413ZH
# Usage: make build flash

PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
CP      = $(PREFIX)objcopy
SZ      = $(PREFIX)size

MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

CFLAGS  = $(MCU) -Wall -Wextra -std=c99 -Og -g3 -DDEBUG
CFLAGS += -DSTM32F413xx -DUSE_HAL_DRIVER -DTX_INCLUDE_USER_DEFINE_FILE
CFLAGS += -fdata-sections -ffunction-sections

ASFLAGS = $(MCU) -DTX_INCLUDE_USER_DEFINE_FILE

# ThreadX tick = 1000 Hz (defined in tx_user.h)

# Paths
ROOT      = ../..
CUBEMX    = $(ROOT)/firmware/ecu/rzc_f4/cuberzccf4fg
HAL_DRV   = $(CUBEMX)/Drivers/STM32F4xx_HAL_Driver
CMSIS     = $(CUBEMX)/Drivers/CMSIS
TX_COMMON = $(ROOT)/experiments/threadx-can/threadx/common
TX_PORT   = $(ROOT)/experiments/threadx-can/threadx/ports/cortex_m4/gnu
TX_INC    = $(ROOT)/firmware/platform/stm32/include

INCLUDES  = -ISrc -IInc
INCLUDES += -I$(CUBEMX)/Core/Inc
INCLUDES += -I$(HAL_DRV)/Inc
INCLUDES += -I$(CMSIS)/Include
INCLUDES += -I$(CMSIS)/Device/ST/STM32F4xx/Include
INCLUDES += -I$(TX_COMMON)/inc
INCLUDES += -I$(TX_PORT)/inc
INCLUDES += -I$(TX_INC)

LDFLAGS  = $(MCU) -Wl,--gc-sections -specs=nosys.specs -specs=nano.specs
LDFLAGS += -T $(CUBEMX)/STM32F413XX_FLASH.ld

BUILD = build

# Sources (local app + BSW modules)
BSW      = $(ROOT)/firmware/bsw
PLAT_F4  = $(ROOT)/firmware/platform/stm32f4
ECU_RZC  = $(ROOT)/firmware/ecu/rzc

CUBEMX_SRC = $(ROOT)/firmware/ecu/rzc_f4/cuberzccf4fg/Core/Src

vpath %.c Src $(BSW)/mcal/Can/src $(BSW)/services/Det/src $(BSW)/services/SchM/src \
         $(BSW)/ecual/CanIf/src $(BSW)/ecual/PduR/src $(BSW)/services/Com/src \
         $(BSW)/services/E2E/src $(BSW)/services/CanTp/src $(BSW)/services/Dcm/src \
         $(BSW)/services/BswM/src $(BSW)/services/WdgM/src $(BSW)/services/Dem/src \
         $(BSW)/services/CanSM/src $(BSW)/services/Xcp/src \
         $(BSW)/rte/src $(PLAT_F4)/src \
         $(ECU_RZC)/cfg $(ECU_RZC)/src $(CUBEMX_SRC)

INCLUDES += -I$(BSW)/include -I$(BSW)/mcal/Can/include -I$(BSW)/services/Det/include \
            -I$(BSW)/services/SchM/include -I$(BSW)/services/Com/include \
            -I$(BSW)/ecual/CanIf/include -I$(BSW)/ecual/PduR/include \
            -I$(BSW)/services/E2E/include -I$(BSW)/services/Dem/include \
            -I$(BSW)/services/WdgM/include -I$(BSW)/services/BswM/include \
            -I$(BSW)/services/CanSM/include -I$(BSW)/services/Xcp/include \
            -I$(BSW)/services/Dcm/include -I$(BSW)/services/NvM/include \
            -I$(BSW)/services/CanTp/include -I$(BSW)/services/Sil/include \
            -I$(BSW)/rte/include -I$(BSW)/ecual/IoHwAb/include \
            -I$(BSW)/mcal/Spi/include -I$(BSW)/mcal/Adc/include \
            -I$(BSW)/mcal/Pwm/include -I$(BSW)/mcal/Dio/include \
            -I$(BSW)/mcal/Gpt/include -I$(BSW)/mcal/Uart/include \
            -I$(BSW)/os/include -I$(BSW)/os/bootstrap/include \
            -I$(PLAT_F4)/include -I$(ECU_RZC)/include -I$(ECU_RZC)/cfg

# Step 9: Full BSW + UDS + all RZC SWCs
APP_SRCS = main.c SchM_ThreadX.c bsw_stubs.c \
           stm32f4xx_hal_msp.c \
           Can.c Can_Hw_STM32F4.c Det.c \
           CanIf.c CanIf_Cfg_Rzc.c \
           PduR.c PduR_Cfg_Rzc.c \
           Com.c Com_Cfg_Rzc.c \
           E2E.c E2E_Sm.c E2E_Cfg_Rzc.c \
           CanTp.c CanTp_Cfg_Rzc.c \
           Dcm.c Dcm_Cfg_Rzc.c \
           BswM.c WdgM.c Dem.c \
           CanSM.c Xcp.c SchM_Timing.c \
           Rte.c Rte_Cfg_Rzc.c \
           Swc_Battery.c Swc_CurrentMonitor.c Swc_Encoder.c \
           Swc_Heartbeat.c Swc_Motor.c Swc_RzcCom.c \
           Swc_RzcDcm.c Swc_RzcNvm.c Swc_RzcSafety.c \
           Swc_RzcScheduler.c Swc_RzcSelfTest.c \
           Swc_RzcSensorFeeder.c Swc_TempMonitor.c

HAL_SRCS = \
    $(HAL_DRV)/Src/stm32f4xx_hal.c \
    $(HAL_DRV)/Src/stm32f4xx_hal_cortex.c \
    $(HAL_DRV)/Src/stm32f4xx_hal_rcc.c \
    $(HAL_DRV)/Src/stm32f4xx_hal_rcc_ex.c \
    $(HAL_DRV)/Src/stm32f4xx_hal_pwr.c \
    $(HAL_DRV)/Src/stm32f4xx_hal_pwr_ex.c \
    $(HAL_DRV)/Src/stm32f4xx_hal_gpio.c \
    $(HAL_DRV)/Src/stm32f4xx_hal_can.c \
    $(CUBEMX)/Core/Src/system_stm32f4xx.c

# ThreadX sources — generated list (Chocolatey Make can't expand $(shell) or $(wildcard) with relative paths)
include tx_sources.mk

# ThreadX low-level init provided in main.c (C override, not .S)

# ThreadX port assembly
TX_ASM = $(TX_PORT)/src/tx_thread_context_restore.S \
         $(TX_PORT)/src/tx_thread_context_save.S \
         $(TX_PORT)/src/tx_thread_interrupt_control.S \
         $(TX_PORT)/src/tx_thread_interrupt_disable.S \
         $(TX_PORT)/src/tx_thread_interrupt_restore.S \
         $(TX_PORT)/src/tx_thread_schedule.S \
         $(TX_PORT)/src/tx_thread_stack_build.S \
         $(TX_PORT)/src/tx_thread_system_return.S \
         $(TX_PORT)/src/tx_timer_interrupt.S

# Startup + ThreadX low-level init
STARTUP = $(CUBEMX)/startup_stm32f413xx.s
TX_LOW_LEVEL = Src/tx_initialize_low_level.s

# VPATH for sources outside this directory
vpath %.c $(TX_COMMON)/src $(HAL_DRV)/Src $(CUBEMX)/Core/Src
vpath %.S $(TX_PORT)/src
vpath %.s $(CUBEMX)

# Flatten paths for object files
TX_SRCS_FLAT = $(notdir $(TX_SRCS))
HAL_SRCS_FLAT = $(notdir $(HAL_SRCS))

ALL_C = $(APP_SRCS) $(HAL_SRCS_FLAT) $(TX_SRCS_FLAT)
C_OBJS = $(addprefix $(BUILD)/,$(patsubst %.c,%.o,$(ALL_C)))
TX_ASM_FLAT = $(notdir $(TX_ASM))
ASM_OBJS = $(addprefix $(BUILD)/,$(patsubst %.S,%.o,$(TX_ASM_FLAT)))
STARTUP_OBJ = $(BUILD)/startup.o

ELF = $(BUILD)/threadx_f4.elf
BIN = $(BUILD)/threadx_f4.bin

.PHONY: build flash clean size

build: $(ELF) size

TX_LOW_OBJ = $(BUILD)/tx_initialize_low_level.o

$(ELF): $(C_OBJS) $(ASM_OBJS) $(STARTUP_OBJ) $(TX_LOW_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -o $@
	$(CP) -O binary $@ $(BIN)

$(BUILD)/%.o: %.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(BUILD)
	$(AS) $(ASFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/startup.o: $(STARTUP)
	@mkdir -p $(BUILD)
	$(AS) $(MCU) -c $< -o $@

$(BUILD)/tx_initialize_low_level.o: $(TX_LOW_LEVEL)
	@mkdir -p $(BUILD)
	$(AS) $(MCU) -DTX_INCLUDE_USER_DEFINE_FILE $(INCLUDES) -c $< -o $@

size: $(ELF)
	$(SZ) $<

flash: $(ELF)
	"C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" \
		--connect port=SWD sn=0670FF383930434B43202436 --download $(ELF) --verify --start

clean:
	rm -rf $(BUILD)
