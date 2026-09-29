########################################
# Makefile for Device Examples
########################################

# --- Compiler Flags ---
ifneq ($(USBD),)
CFLAGS += -DUSBD=$(USBD)
endif

# --- Include Build Makefile ---
include ../../build.mk
