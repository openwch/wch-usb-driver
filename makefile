########################################
# Root Makefile for the project
########################################

CHIP ?= all
SERIES_LIST :=  $(notdir $(wildcard ./chips/*))
TARGET_SERIES := $(if $(filter all,$(CHIP)),$(SERIES_LIST),$(CHIP))

make_evt:
	@for chip in $(TARGET_SERIES); do \
		python tools/gen_mrs_project.py $$chip evt/$$chip/USB/New; \
	done

clear_evt:
	@rm -rf evt

.PHONY: make_evt clear_evt
