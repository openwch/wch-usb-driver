########################################
# Makefile for Host Examples
########################################

# --- Compiler Flags ---
ifneq ($(USBH),)
CFLAGS += -DUSBH=$(USBH)
endif

# --- Include Build Makefile ---
include ../../build.mk
