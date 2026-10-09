########################################
# Makefile for Host Examples
########################################

# --- Compiler Flags ---
ifneq ($(USBH_INDEX),)
CFLAGS += -DUSBH_INDEX=$(USBH_INDEX)
endif

# --- Include Build Makefile ---
include ../../build.mk
