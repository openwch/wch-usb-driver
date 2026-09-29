########################################
# Makefile for Dual Device Examples
########################################

# --- Compiler Flags ---
ifneq ($(USBD0),)
CFLAGS += -DUSBD0=$(USBD0)
endif

ifneq ($(USBD1),)
CFLAGS += -DUSBD1=$(USBD1)
endif

# --- Include Build Makefile ---
include ../../build.mk
