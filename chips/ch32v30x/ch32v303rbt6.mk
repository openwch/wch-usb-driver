########################################
# Makefile for CH32V303RBT6 Chip
########################################

# --- Assembly Source Files ---
ASMS += $(FAMILY_DIR)/sdk/Startup/startup_ch32v30x_D8.S

# --- Compiler Flags ---
CFLAGS += -DCH32V30x_D8 -DUSB_COUNT=1

# --- Linker Flags ---
LDFLAGS += -T "$(FAMILY_DIR)/sdk/Ld/Link_xb.ld"
