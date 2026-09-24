########################################
# Makefile for CH32V205RCT6 Chip
########################################

# --- Supported IPs ---
SUPPORTED_IPS := usbhs usbfs

# --- Compiler Flags ---
CFLAGS += \
	-DUSB_NUM=2 \
	-DUSBHSD_BASE_ADDR=0x40023400 \
	-DUSBHSH_BASE_ADDR=0x40023500 \
	-DUSBFSD_BASE_ADDR=0x50000000 \
	-DUSBFSH_BASE_ADDR=0x50000000
