########################################
# Build Makefile for Examples
########################################

# --- Check for CHIP Parameter ---
ifneq ($(filter all, $(MAKECMDGOALS)),)
  ifndef CHIP
    $(error No 'CHIP' parameter provided, Usage: make all -j8 CHIP=ch32v205rct6)
  endif

# --- Locate Chip Makefile ---
CHIP_MK := $(wildcard $(ROOT_DIR)/chips/*/$(CHIP).mk)
FAMILY := $(notdir $(abspath $(CHIP_MK)/../))

  ifeq ($(filter $(FAMILY), $(SUPPORTED_FAMILIES)),)
    $(error This example does not support the family '$(FAMILY)', supported families are '$(SUPPORTED_FAMILIES)')
  endif

# --- Output Directories ---
OBJECT_DIR := build/$(CHIP)/object
OUTPUT_DIR := build/$(CHIP)/output

# --- Create Output Directories ---
$(shell mkdir -p $(OBJECT_DIR))
$(shell mkdir -p $(OUTPUT_DIR))

# --- Target and Output Files ---
TARGET := $(notdir $(CURDIR))
ELF_FILE := $(OUTPUT_DIR)/$(TARGET).elf
BIN_FILE := $(OUTPUT_DIR)/$(TARGET).bin
HEX_FILE := $(OUTPUT_DIR)/$(TARGET).hex
MAP_FILE := $(OUTPUT_DIR)/$(TARGET).map
LST_FILE := $(OUTPUT_DIR)/$(TARGET).lst

# --- Include Directories ---
INCLUDES := \
	$(ROOT_DIR)/include \
	$(ROOT_DIR)/port/usbhs

# --- Assembly Source Directories ---
ASM_DIR :=

# --- C Source Directories ---
SRC_DIR :=

# --- Library Directories ---
LIB_DIR :=

# --- Assembly Source Files ---
ASMS :=

# --- C Source Files ---
SRCS :=

# --- Libraries ---
LIBS :=

# --- Compiler Flags ---
CFLAGS :=

# --- Linker Flags ---
LDFLAGS :=

# --- Family Directories ---
FAMILY_DIR := $(abspath $(CHIP_MK)/../)

# --- Include Chip and Family Makefiles ---
include $(CHIP_MK)
include $(FAMILY_DIR)/family.mk

# --- Add C Source Files Directories ---
SRC_DIR += \
	$(abspath src) \
	$(ROOT_DIR)/src \
	$(ROOT_DIR)/port/usbhs

# --- Add Assembly Source Files ---
ASMS += $(foreach dir,$(ASM_DIR),$(wildcard $(dir)/*.S))

# --- Add C Source Files ---
SRCS += $(foreach dir,$(SRC_DIR),$(wildcard $(dir)/*.c))

# --- Object Files ---
OBJECT_FILES := \
	$(addprefix $(OBJECT_DIR)/,$(notdir $(ASMS:.S=.o))) \
	$(addprefix $(OBJECT_DIR)/,$(notdir $(SRCS:.c=.o)))

# --- Dependency Files ---
-include $(OBJECT_FILES:.o=.d)

# --- Vpath for Source Files ---
vpath %.S $(ASM_DIR)
vpath %.c $(SRC_DIR)

endif

# --- Build Targets ---
all: $(BIN_FILE) $(HEX_FILE) $(LST_FILE)

# --- Clear Target ---
clear:
	@rm -rf build
	@echo "Clear complete!"

# --- Compilation Rules ---
$(BIN_FILE): $(ELF_FILE)
	@$(OBJCOPY) -Obinary $< $(BIN_FILE)

$(HEX_FILE): $(ELF_FILE)
	@$(OBJCOPY) -Oihex $< $(HEX_FILE)

$(LST_FILE): $(ELF_FILE)
	@$(OBJDUMP) --all-headers --demangle --disassemble $< > $(LST_FILE)

$(ELF_FILE): $(OBJECT_FILES)
	@echo "[LINK] $@"
	@$(CC) $(LDFLAGS) $(OBJECT_FILES) -o $@
	@echo "Build complete!"

$(OBJECT_DIR)/%.o: %.c
	@echo "[CC]   $< -> $@"
	@$(CC) $(CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -c $< -o $@

$(OBJECT_DIR)/%.o: %.S
	@echo "[CC]   $< -> $@"
	@$(CC) $(CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -c $< -o $@

# --- Phony Targets ---
.PHONY: all clear
