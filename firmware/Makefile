# Збірка проєкту без STM32CubeIDE - лише toolchain + make.
#
#   make            збірка (за замовчуванням release, -O2)
#   make BUILD=debug   налагоджувальна збірка (-O0 -g3)
#   make flash      прошити плату через ST-Link
#   make erase      стерти флеш
#   make reset      перезапустити МК
#   make size       розмір секцій
#   make clean      прибрати артефакти
#
# Якщо arm-none-eabi-gcc не в PATH, шлях береться з GCC_PATH нижче.

TARGET  = protask1
BUILD  ?= release

# ---- Розташування інструментів -------------------------------------------
# Шляхи всередині STM32CubeIDE. Замінити, якщо toolchain стоїть окремо.
CUBE_ROOT ?= C:/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins
GCC_PATH  ?= $(CUBE_ROOT)/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin
PROG_PATH ?= $(CUBE_ROOT)/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304/tools/bin

PREFIX  = arm-none-eabi-
ifneq ($(wildcard $(GCC_PATH)/.),)
  CC      = $(GCC_PATH)/$(PREFIX)gcc
  AS      = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
  CP      = $(GCC_PATH)/$(PREFIX)objcopy
  SZ      = $(GCC_PATH)/$(PREFIX)size
  OD      = $(GCC_PATH)/$(PREFIX)objdump
else
  CC      = $(PREFIX)gcc
  AS      = $(PREFIX)gcc -x assembler-with-cpp
  CP      = $(PREFIX)objcopy
  SZ      = $(PREFIX)size
  OD      = $(PREFIX)objdump
endif
PROG    = $(PROG_PATH)/STM32_Programmer_CLI

BUILD_DIR = build/$(BUILD)

# ---- Джерела --------------------------------------------------------------
C_SOURCES = \
  Core/Src/main.c \
  Core/Src/gpio.c \
  Core/Src/dma.c \
  Core/Src/tim.c \
  Core/Src/stm32g4xx_it.c \
  Core/Src/stm32g4xx_hal_msp.c \
  Core/Src/system_stm32g4xx.c \
  Core/Src/syscalls.c \
  Core/Src/sysmem.c \
  $(wildcard Drivers/STM32G4xx_HAL_Driver/Src/*.c)

ASM_SOURCES = Core/Startup/startup_stm32g474retx.s
LDSCRIPT    = STM32G474RETX_FLASH.ld

# ---- Прапорці -------------------------------------------------------------
CPU     = -mcpu=cortex-m4
FPU     = -mfpu=fpv4-sp-d16
FLOAT   = -mfloat-abi=hard
MCUFLAGS = $(CPU) -mthumb $(FPU) $(FLOAT)

C_DEFS  = -DUSE_HAL_DRIVER -DSTM32G474xx
C_INCS  = \
  -ICore/Inc \
  -IDrivers/STM32G4xx_HAL_Driver/Inc \
  -IDrivers/STM32G4xx_HAL_Driver/Inc/Legacy \
  -IDrivers/CMSIS/Device/ST/STM32G4xx/Include \
  -IDrivers/CMSIS/Include

ifeq ($(BUILD),debug)
  OPT     = -O0 -g3 -DDEBUG
else
  OPT     = -O2 -g
endif

CFLAGS  = $(MCUFLAGS) $(C_DEFS) $(C_INCS) $(OPT) -std=gnu11 \
          -Wall -Wextra -ffunction-sections -fdata-sections -fstack-usage \
          -MMD -MP --specs=nano.specs
ASFLAGS = $(MCUFLAGS) $(OPT) -Wall

LDFLAGS = $(MCUFLAGS) -T$(LDSCRIPT) --specs=nosys.specs --specs=nano.specs \
          -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections -static \
          -Wl,--start-group -lc -lm -Wl,--end-group

# ---- Об'єктні файли -------------------------------------------------------
OBJECTS  = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

# ---- Цілі -----------------------------------------------------------------
.PHONY: all clean flash erase reset size dis

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin size

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(CP) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(CP) -O binary -S $< $@

$(BUILD_DIR):
	@mkdir -p $@

size: $(BUILD_DIR)/$(TARGET).elf
	@$(SZ) $<

dis: $(BUILD_DIR)/$(TARGET).elf
	@$(OD) -d -S $< > $(BUILD_DIR)/$(TARGET).list
	@echo "дизасемблер -> $(BUILD_DIR)/$(TARGET).list"

flash: $(BUILD_DIR)/$(TARGET).elf
	"$(PROG)" -c port=SWD mode=UR -w $< -v -rst

erase:
	"$(PROG)" -c port=SWD mode=UR -e all

reset:
	"$(PROG)" -c port=SWD mode=HOTPLUG -rst

clean:
	@rm -rf build

-include $(wildcard $(BUILD_DIR)/*.d)
