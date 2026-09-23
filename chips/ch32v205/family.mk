########################################
# Makefile for CH32V205 Family
########################################

# --- Toolchain ---
PREFIX  := riscv32-wch-elf
CC      := $(PREFIX)-gcc
AR      := $(PREFIX)-ar
OBJCOPY := $(PREFIX)-objcopy
OBJDUMP := $(PREFIX)-objdump

# --- Include Directories ---
INCLUDES += \
	$(FAMILY_DIR)/sdk/Core \
	$(FAMILY_DIR)/sdk/Debug \
	$(FAMILY_DIR)/sdk/Peripheral/inc \
	$(FAMILY_DIR)/board

# --- Assembly Source Directories ---
ASM_DIR += $(FAMILY_DIR)/sdk/Startup

# --- C Source Directories ---
SRC_DIR += \
	$(FAMILY_DIR)/sdk/Core \
	$(FAMILY_DIR)/sdk/Debug \
	$(FAMILY_DIR)/sdk/Peripheral/src \
	$(FAMILY_DIR)/board

# --- Compiler Flags ---
CFLAGS += \
	-march=rv32imc_zba_zbb_zbc_zbs_xw \
	-mabi=ilp32 \
	-msmall-data-limit=8 \
	-mno-save-restore \
	-Os \
	-fmessage-length=0 \
	-fsigned-char \
	-ffunction-sections \
	-fdata-sections \
	-fno-common \
	-Wunused \
	-Wuninitialized \
	-g \
	-std=gnu11 \
	$(addprefix -I,$(INCLUDES)) \
	$(addprefix -L,$(LIB_DIR))

# --- Linker Flags ---
LDFLAGS += \
	$(CFLAGS) \
	-nostartfiles \
	-Xlinker \
	--gc-sections \
	-Wl,--print-memory-usage \
	-Wl,-Map,$(MAP_FILE) \
	--specs=nano.specs \
	--specs=nosys.specs \
	-T "$(FAMILY_DIR)/sdk/Ld/Link.ld"
