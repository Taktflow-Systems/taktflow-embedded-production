# ThreadX + CVC on STM32G474RE
# Usage: make build flash

PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
CP      = $(PREFIX)objcopy
SZ      = $(PREFIX)size

MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

CFLAGS  = $(MCU) -Wall -Wextra -std=c99 -Og -g3 -DDEBUG
CFLAGS += -DSTM32G474xx -DUSE_HAL_DRIVER -DTX_INCLUDE_USER_DEFINE_FILE -DCANTP_ENABLED -DPLATFORM_HIL
# TODO:HARDWARE — remove PLATFORM_HIL after bench CAN bus stabilized (termination, powered hub)
CFLAGS += -fdata-sections -ffunction-sections

ASFLAGS = $(MCU) -DTX_INCLUDE_USER_DEFINE_FILE

ROOT      = ../..
BSW       = $(ROOT)/firmware/bsw
ECU_CVC   = $(ROOT)/firmware/ecu/cvc
PLAT_G4   = $(ROOT)/firmware/platform/stm32
# Use FZC experiment's HAL drivers and ThreadX (same G474RE hardware)
FZC_EXP   = $(ROOT)/experiments/threadx-can
HAL_DRV   = $(FZC_EXP)/Drivers/STM32G4xx_HAL_Driver
CMSIS     = $(FZC_EXP)/Drivers/CMSIS
TX_COMMON = $(FZC_EXP)/threadx/common
TX_PORT   = $(FZC_EXP)/threadx/ports/cortex_m4/gnu

INCLUDES  = -ISrc
INCLUDES += -I$(FZC_EXP)/Inc
INCLUDES += -I$(HAL_DRV)/Inc
INCLUDES += -I$(CMSIS)/Include
INCLUDES += -I$(CMSIS)/Device/ST/STM32G4xx/Include
INCLUDES += -I$(TX_COMMON)/inc -I$(TX_PORT)/inc
INCLUDES += -I$(PLAT_G4)/include
INCLUDES += -I$(BSW)/include -I$(BSW)/mcal/Can/include -I$(BSW)/services/Det/include
INCLUDES += -I$(BSW)/services/SchM/include -I$(BSW)/services/Com/include
INCLUDES += -I$(BSW)/ecual/CanIf/include -I$(BSW)/ecual/PduR/include
INCLUDES += -I$(BSW)/services/E2E/include -I$(BSW)/services/Dem/include
INCLUDES += -I$(BSW)/services/WdgM/include -I$(BSW)/services/BswM/include
INCLUDES += -I$(BSW)/services/Dcm/include -I$(BSW)/services/NvM/include
INCLUDES += -I$(BSW)/services/CanSM/include -I$(BSW)/services/Xcp/include
INCLUDES += -I$(BSW)/services/CanTp/include -I$(BSW)/services/Sil/include
INCLUDES += -I$(BSW)/rte/include -I$(BSW)/ecual/IoHwAb/include
INCLUDES += -I$(BSW)/mcal/Spi/include -I$(BSW)/mcal/Adc/include
INCLUDES += -I$(BSW)/mcal/Pwm/include -I$(BSW)/mcal/Dio/include
INCLUDES += -I$(BSW)/mcal/Gpt/include -I$(BSW)/mcal/Uart/include
INCLUDES += -I$(BSW)/os/include -I$(BSW)/os/bootstrap/include
INCLUDES += -I$(ECU_CVC)/include -I$(ECU_CVC)/cfg -I$(ECU_CVC)/cfg/platform_target

LDFLAGS  = $(MCU) -Wl,--gc-sections -specs=nosys.specs -specs=nano.specs
LDFLAGS += -T $(FZC_EXP)/STM32CubeIDE/STM32G474RETX_FLASH.ld

BUILD = build

# VPATH for all source directories
vpath %.c Src \
    $(BSW)/mcal/Can/src $(BSW)/services/Det/src \
    $(BSW)/ecual/CanIf/src $(BSW)/ecual/PduR/src \
    $(BSW)/services/Com/src $(BSW)/services/E2E/src \
    $(BSW)/services/CanTp/src $(BSW)/services/Dcm/src \
    $(BSW)/services/BswM/src $(BSW)/services/WdgM/src $(BSW)/services/Dem/src \
    $(BSW)/services/CanSM/src $(BSW)/services/Xcp/src $(BSW)/services/SchM/src \
    $(BSW)/rte/src $(PLAT_G4)/src \
    $(ECU_CVC)/cfg $(ECU_CVC)/src \
    $(HAL_DRV)/Src $(FZC_EXP)/Src
vpath %.c $(TX_COMMON)/src
vpath %.S $(TX_PORT)/src Src

# Step 9: Full BSW + UDS + CVC SWCs
APP_SRCS = main.c SchM_ThreadX.c bsw_stubs.c \
    Can.c Can_Hw_STM32.c Det.c \
    CanIf.c CanIf_Cfg_Cvc.c \
    PduR.c PduR_Cfg_Cvc.c \
    Com.c Com_Cfg_Cvc.c \
    E2E.c E2E_Sm.c E2E_Cfg_Cvc.c \
    CanTp.c CanTp_Cfg_Cvc.c \
    Dcm.c Dcm_Cfg_Cvc.c \
    BswM.c WdgM.c Dem.c \
    CanSM.c Xcp.c SchM_Timing.c \
    Rte.c Rte_Cfg_Cvc.c \
    Swc_Heartbeat.c Swc_VehicleState.c Swc_Pedal.c \
    Swc_EStop.c Swc_Dashboard.c Swc_CvcCom.c \
    Swc_CvcDcm.c Swc_CanMonitor.c Swc_Watchdog.c Swc_Nvm.c \
    Swc_Scheduler.c Swc_SelfTest.c Ssd1306.c

# HAL
HAL_SRCS = stm32g4xx_hal.c stm32g4xx_hal_cortex.c \
    stm32g4xx_hal_rcc.c stm32g4xx_hal_rcc_ex.c \
    stm32g4xx_hal_pwr.c stm32g4xx_hal_pwr_ex.c \
    stm32g4xx_hal_gpio.c stm32g4xx_hal_fdcan.c \
    stm32g4xx_hal_uart.c stm32g4xx_hal_uart_ex.c \
    system_stm32g4xx.c stm32g4xx_hal_msp.c

# ThreadX
include tx_sources.mk

# Assembly
TX_ASM_FLAT = tx_thread_context_restore.S tx_thread_context_save.S \
    tx_thread_interrupt_control.S tx_thread_interrupt_disable.S \
    tx_thread_interrupt_restore.S tx_thread_schedule.S \
    tx_thread_stack_build.S tx_thread_system_return.S \
    tx_timer_interrupt.S

ALL_C = $(APP_SRCS) $(HAL_SRCS) $(TX_SRCS)
C_OBJS = $(addprefix $(BUILD)/,$(patsubst %.c,%.o,$(ALL_C)))
ASM_OBJS = $(addprefix $(BUILD)/,$(patsubst %.S,%.o,$(TX_ASM_FLAT)))
STARTUP_OBJ = $(BUILD)/startup.o
TX_LOW_OBJ = $(BUILD)/tx_initialize_low_level.o

ELF = $(BUILD)/cvc_threadx.elf
BIN = $(BUILD)/cvc_threadx.bin

.PHONY: build flash clean size

build: $(ELF) size

$(ELF): $(C_OBJS) $(ASM_OBJS) $(STARTUP_OBJ) $(TX_LOW_OBJ)
	@mkdir -p $(BUILD)
	$(CC) $(LDFLAGS) $^ -o $@
	$(CP) -O binary $@ $(BIN)

$(BUILD)/%.o: %.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(BUILD)
	$(AS) $(ASFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/startup.o: $(FZC_EXP)/STM32CubeIDE/Application/Startup/startup_stm32g474retx.s
	@mkdir -p $(BUILD)
	$(AS) $(MCU) -c $< -o $@

$(TX_LOW_OBJ): Src/tx_initialize_low_level.s
	@mkdir -p $(BUILD)
	$(AS) $(MCU) -DTX_INCLUDE_USER_DEFINE_FILE $(INCLUDES) -c $< -o $@

size: $(ELF)
	$(SZ) $<

flash: $(ELF)
	"C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" \
		--connect port=SWD sn=001A00363235510B37333439 --download $(ELF) --verify --start

clean:
	rm -rf $(BUILD)
