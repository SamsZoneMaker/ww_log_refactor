# Makefile for Log System Test (New Design with Auto File ID)
# Date: 2025-12-17

# Compiler and flags
CC = gcc
BASE_CFLAGS = -Wall -Wextra -Iinclude -I src/demo -I src/brom -I src/test -I src/app -I src/drivers -g -O2 -Wno-unused-variable -Wno-unused-function
LDFLAGS =

# Color output
PREFIX_C = \033[0;33m
RESET_C = \033[0m
GREEN = \033[0;32m
YELLOW = \033[0;33m
BLUE = \033[0;34m
RED = \033[0;31m
NC = \033[0m

# Directories
BUILD_DIR = build
BIN_DIR = bin
OBJ_DIR = $(BUILD_DIR)

# Log configuration
LOG_CONFIG = log_config.json
FILE_IDS_MK = $(BUILD_DIR)/file_ids.mk

# Source files - include all modules
ALL_SRCS = $(wildcard core/*.c) \
           $(wildcard src/demo/*.c) \
           $(wildcard src/brom/*.c) \
           $(wildcard src/test/*.c) \
           $(wildcard src/app/*.c) \
           $(wildcard src/drivers/*.c) \
           examples/main.c

# Object files
OBJS = $(ALL_SRCS:%.c=$(BUILD_DIR)/%.o)

# Mode is now defined in ww_log.h, not via Makefile
# Makefile only handles compilation flags

# Enable RAM buffer for encode mode (optional)
# CFLAGS += -DWW_LOG_ENCODE_RAM_BUFFER_EN

# Static module switches (compile-time enable/disable)
# Usage: make STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0"
# Multiple modules: STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0 -DWW_LOG_STATIC_MODULE_TEST_EN=0"
STATIC_OPTS ?=

# Combine base flags with static options
CFLAGS = $(BASE_CFLAGS) $(STATIC_OPTS)

# Output executable
TARGET = $(BIN_DIR)/log_test

# Generate file ID mappings
# This target creates the file_ids.mk file which defines FILE_ID_xxx and MODULE_ID_xxx variables
$(FILE_IDS_MK): $(LOG_CONFIG) tools/gen_file_ids.py
	@echo -e "$(BLUE)Generating file ID mappings...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@python3 tools/gen_file_ids.py $(LOG_CONFIG) --makefile > $(FILE_IDS_MK)
	@python3 tools/gen_file_ids.py $(LOG_CONFIG) --header > include/auto_file_ids.h
	@echo -e "$(GREEN)File ID mappings generated.$(NC)"

# Phony target for manual regeneration
.PHONY: gen-log-ids
gen-log-ids: $(FILE_IDS_MK)

# Include generated file ID mappings (will trigger generation if missing)
-include $(FILE_IDS_MK)

# Default target
.PHONY: all
all: gen-log-ids $(TARGET)
	@echo -e "$(GREEN)Build complete: $(TARGET)$(NC)"
	@echo -e "$(BLUE)Mode is configured in include/ww_log.h$(NC)"

# Create directories
$(BUILD_DIR) $(BIN_DIR):
	@mkdir -p $(BUILD_DIR)/core
	@mkdir -p $(BUILD_DIR)/src/demo
	@mkdir -p $(BUILD_DIR)/src/brom
	@mkdir -p $(BUILD_DIR)/src/test
	@mkdir -p $(BUILD_DIR)/src/app
	@mkdir -p $(BUILD_DIR)/src/drivers
	@mkdir -p $(BUILD_DIR)/examples
	@mkdir -p $(BIN_DIR)

# Build executable
$(TARGET): $(BUILD_DIR) $(BIN_DIR) $(OBJS)
	@echo -e "$(BLUE)Linking $@...$(NC)"
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compile source files with automatic file ID and module ID injection
$(BUILD_DIR)/%.o: %.c $(FILE_IDS_MK)
	@echo -e "${PREFIX_C}[CC   ] $<${RESET_C}"
	@mkdir -p $(dir $@)
	$(eval FILE_VAR := $(subst /,_,$(subst .,_,FILE_ID_$<)))
	$(eval MODULE_VAR := $(subst /,_,$(subst .,_,MODULE_ID_$<)))
	$(eval STATIC_VAR := $(subst /,_,$(subst .,_,MODULE_STATIC_EN_$<)))
	$(eval FILE_ID_VAL := $($(FILE_VAR)))
	$(eval MODULE_ID_VAL := $($(MODULE_VAR)))
	$(eval STATIC_EN_VAL := $($(STATIC_VAR)))
	@if [ -n "$(FILE_ID_VAL)" ]; then \
		$(CC) $(BASE_CFLAGS) $(STATIC_OPTS) \
			-DCURRENT_FILE_ID=$(FILE_ID_VAL) \
			-DCURRENT_MODULE_ID=$(MODULE_ID_VAL) \
			-DCURRENT_MODULE_STATIC_EN=$(STATIC_EN_VAL) \
			-D__NOTDIR_FILE__=\"$(notdir $<)\" \
			-MMD -MP -c $< -o $@; \
	else \
		$(CC) $(BASE_CFLAGS) $(STATIC_OPTS) \
			-DCURRENT_MODULE_STATIC_EN=0 \
			-D__NOTDIR_FILE__=\"$(notdir $<)\" \
			-MMD -MP -c $< -o $@; \
	fi

# Include dependency files
-include $(OBJS:.o=.d)

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@rm -f include/auto_file_ids.h
	@echo "Clean complete."

# Deep clean
.PHONY: distclean
distclean:
	@echo "Deep cleaning..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@rm -f include/auto_file_ids.h
	@echo "Deep clean complete."

# Run test program
.PHONY: run
run: all
	@echo ""
	@echo -e "$(GREEN)=========================================$(NC)"
	@echo -e "$(GREEN)  Running Test$(NC)"
	@echo -e "$(GREEN)=========================================$(NC)"
	@echo ""
	@./$(TARGET)

# Help
.PHONY: help
help:
	@echo -e "$(GREEN)Log System Makefile (Auto File ID)$(NC)"
	@echo ""
	@echo -e "$(BLUE)Usage:$(NC)"
	@echo "  make [STATIC_OPTS=\"...\"] [target]"
	@echo ""
	@echo -e "$(BLUE)Mode Configuration:$(NC)"
	@echo "  Log mode is configured in include/ww_log.h by uncommenting one of:"
	@echo "    - WW_LOG_MODE_STR      (String mode - printf-style)"
	@echo "    - WW_LOG_MODE_ENCODE   (Encode mode - binary encoding)"
	@echo "    - WW_LOG_MODE_DISABLED (All logging disabled)"
	@echo ""
	@echo -e "$(BLUE)Static Module Control:$(NC)"
	@echo "  Disable modules at compile time (zero code size):"
	@echo "    make STATIC_OPTS=\"-DWW_LOG_STATIC_MODULE_DEMO_EN=0\""
	@echo "  Multiple modules:"
	@echo "    make STATIC_OPTS=\"-DWW_LOG_STATIC_MODULE_DEMO_EN=0 -DWW_LOG_STATIC_MODULE_TEST_EN=0\""
	@echo ""
	@echo -e "$(BLUE)Targets:$(NC)"
	@echo "  make all- Build the project (default)"
	@echo "  make run          - Build and run"
	@echo "  make gen-log-ids  - Regenerate file ID mappings"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make distclean    - Remove all generated files"
	@echo "  make help         - Show this help"
	@echo ""
	@echo -e "$(BLUE)Examples:$(NC)"
	@echo "  make              - Build with current mode"
	@echo "  make run          - Build and run"
	@echo "  make clean all    - Clean rebuild"
	@echo ""
