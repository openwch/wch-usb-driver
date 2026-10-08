########################################
# Makefile for CH32V4x7 Family
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
	$(FAMILY_DIR)/board \
	$(ROOT_DIR)/port/usbhs

# --- Assembly Source Directories ---
ASM_DIR += $(FAMILY_DIR)/sdk/Startup

# --- Assembly Source Files ---
ASMS += $(FAMILY_DIR)/sdk/Startup/startup_ch32v4x7.S

# --- C Source Directories ---
SRC_DIR += \
	$(FAMILY_DIR)/sdk/Core \
	$(FAMILY_DIR)/sdk/Debug \
	$(FAMILY_DIR)/sdk/Peripheral/src \
	$(FAMILY_DIR)/board \
	$(ROOT_DIR)/port/usbhs

# --- Compiler Flags ---
CFLAGS += \
	-march=rv32imac_zba_zbb_zbc_zbs_zve64x_zvl64b_zvbb_xw \
	-mabi=ilp32 \
	-msmall-data-limit=8 \
	-msave-restore \
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
