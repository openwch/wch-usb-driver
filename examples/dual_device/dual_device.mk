########################################
# Makefile for Dual Device Examples
########################################

# --- Compiler Flags ---
ifneq ($(USBD0_INDEX),)
CFLAGS += -DUSBD0_INDEX=$(USBD0_INDEX)
endif

ifneq ($(USBD1_INDEX),)
CFLAGS += -DUSBD1_INDEX=$(USBD1_INDEX)
endif

# --- Include Build Makefile ---
include ../../build.mk
