########################################
# Makefile for Device Examples
########################################

# --- Compiler Flags ---
ifneq ($(USBD_INDEX),)
CFLAGS += -DUSBD_INDEX=$(USBD_INDEX)
endif

# --- Include Build Makefile ---
include ../../build.mk
