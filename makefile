########################################
# Root Makefile for the project
########################################

CHIP ?= all
SERIES_LIST :=  $(notdir $(wildcard ./chips/*))
TARGET_SERIES := $(if $(filter all,$(CHIP)),$(SERIES_LIST),$(CHIP))

# --- Example Directories ---
EXAMPLE_DIRS := $(sort $(patsubst %/makefile,%,$(wildcard ./examples/*/*/makefile)))

make_evt:
	@for chip in $(TARGET_SERIES); do \
		python tools/gen_mrs_project.py $$chip evt/$$chip/USB/New; \
	done

clear_evt:
	@rm -rf evt

build_all:
	@for dir in $(EXAMPLE_DIRS); do \
		if grep -qw "$(CHIP)" $$dir/makefile; then \
			$(MAKE) --no-print-directory -C $$dir all CHIP=$(CHIP) || exit 1; \
		fi; \
	done
	@echo "Build all complete!"

clear_all:
	@rm -rf $(addsuffix /build,$(EXAMPLE_DIRS))
	@echo "Clear all complete!"

.PHONY: make_evt clear_evt build_all clear_all
