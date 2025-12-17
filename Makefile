# Makefile for Log System Test (New Design)
# Date: 2025-11-29

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -I src/demo -I src/brom -I src/test -I src/app -I src/drivers -g -O2
LDFLAGS =

# Color output
GREEN = \033[0;32m
YELLOW = \033[0;33m
BLUE = \033[0;34m
RED = \033[0;31m
NC = \033[0m

# Directories
BUILD_DIR = build
BIN_DIR = bin

# Source files - include all modules
ALL_SRCS = $(wildcard src/core/*.c) \
           $(wildcard src/demo/*.c) \
           $(wildcard src/brom/*.c) \
           $(wildcard src/test/*.c) \
           $(wildcard src/app/*.c) \
           $(wildcard src/drivers/*.c) \
           examples/main.c

# Object files
OBJS = $(ALL_SRCS:%.c=$(BUILD_DIR)/%.o)

# Mode selection (str, encode, or disabled)
# Usage: make MODE=encode
MODE ?= str

# Static module switches (compile-time enable/disable)
# Usage: make MODE=str STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0"
# Multiple modules: STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0 -DWW_LOG_STATIC_MODULE_TEST_EN=0"
STATIC_OPTS ?=

# Set mode-specific flags
ifeq ($(MODE),str)
    CFLAGS += -DWW_LOG_MODE_STR
    MODE_STR = "STRING MODE"
else ifeq ($(MODE),encode)
    CFLAGS += -DWW_LOG_MODE_ENCODE
    CFLAGS += -DWW_LOG_ENCODE_RAM_BUFFER_EN
    MODE_STR = "ENCODE MODE"
else ifeq ($(MODE),disabled)
    CFLAGS += -DWW_LOG_MODE_DISABLED
    MODE_STR = "DISABLED MODE"
else
    $(error Invalid MODE=$(MODE). Use: MODE=str, MODE=encode, or MODE=disabled)
endif

# Add static switches to CFLAGS
ifneq ($(STATIC_OPTS),)
    CFLAGS += $(STATIC_OPTS)
endif

# Output executable
TARGET = $(BIN_DIR)/log_test_$(MODE)

# Default target
.PHONY: all
all: $(TARGET)
	@echo -e "$(GREEN)Build complete: $(TARGET)$(NC)"
	@echo -e "$(BLUE)Mode: $(MODE_STR)$(NC)"

# Create directories
$(BUILD_DIR) $(BIN_DIR):
	@mkdir -p $(BUILD_DIR)/src/core
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

# Compile source files
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)Compiling $<...$(NC)"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Include dependency files
-include $(OBJS:.o=.d)

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@echo "Clean complete."

# Deep clean
.PHONY: distclean
distclean:
	@echo "Deep cleaning..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Deep clean complete."

# Run test program
.PHONY: run
run: all
	@echo ""
	@echo -e "$(GREEN)=========================================$(NC)"
	@echo -e "$(GREEN)  Running Test ($(MODE_STR))$(NC)"
	@echo -e "$(GREEN)=========================================$(NC)"
	@echo ""
	@./$(TARGET)

# Build and run string mode (clean build)
.PHONY: test-str
test-str: clean
	@$(MAKE) MODE=str run

# Build and run encode mode (clean build)
.PHONY: test-encode
test-encode: clean
	@$(MAKE) MODE=encode run

# Build and run disabled mode (clean build)
.PHONY: test-disabled
test-disabled: clean
	@$(MAKE) MODE=disabled run

# Test all modes
.PHONY: test-all
test-all:
	@echo -e "$(GREEN)=========================================$(NC)"
	@echo -e "$(GREEN)  Testing All Modes$(NC)"
	@echo -e "$(GREEN)=========================================$(NC)"
	@echo ""
	@$(MAKE) test-str
	@echo ""
	@$(MAKE) test-encode
	@echo ""
	@$(MAKE) test-disabled
	@echo ""
	@echo -e "$(GREEN)=========================================$(NC)"
	@echo -e "$(GREEN)  All Mode Tests Complete$(NC)"
	@echo -e "$(GREEN)=========================================$(NC)"

# Size comparison
.PHONY: size-compare
size-compare:
	@echo -e "$(GREEN)=========================================$(NC)"
	@echo -e "$(GREEN)  Binary Size Comparison$(NC)"
	@echo -e "$(GREEN)=========================================$(NC)"
	@echo ""
	@echo -e "$(YELLOW)Building all three modes...$(NC)"
	@$(MAKE) clean MODE=str > /dev/null 2>&1
	@$(MAKE) all MODE=str > /dev/null 2>&1
	@$(MAKE) clean MODE=encode > /dev/null 2>&1
	@$(MAKE) all MODE=encode > /dev/null 2>&1
	@$(MAKE) clean MODE=disabled > /dev/null 2>&1
	@$(MAKE) all MODE=disabled > /dev/null 2>&1
	@echo ""
	@echo -e "$(BLUE)Size comparison:$(NC)"
	@size $(BIN_DIR)/log_test_str $(BIN_DIR)/log_test_encode $(BIN_DIR)/log_test_disabled
	@echo ""
	@ls -lh $(BIN_DIR)/log_test_* | awk '{print "  " $$9 " - " $$5}'
	@echo ""
	@echo -e "$(GREEN)Done!$(NC)"

# Help
.PHONY: help
help:
	@echo -e "$(GREEN)Log System Makefile (New Design)$(NC)"
	@echo ""
	@echo -e "$(BLUE)Usage:$(NC)"
	@echo "  make [MODE=str|encode|disabled] [target]"
	@echo ""
	@echo -e "$(BLUE)Modes:$(NC)"
	@echo "  MODE=str       - String mode (printf-style, human-readable)"
	@echo "  MODE=encode    - Encode mode (binary encoding, minimal code size)"
	@echo "  MODE=disabled  - All logging disabled"
	@echo ""
	@echo -e "$(BLUE)Targets:$(NC)"
	@echo "  make              - Build with string mode (default)"
	@echo "  make MODE=encode  - Build with encode mode"
	@echo "  make run          - Build and run with current mode"
	@echo "  make test-str     - Test string mode"
	@echo "  make test-encode  - Test encode mode"
	@echo "  make test-all     - Test all modes"
	@echo "  make size-compare - Compare binary sizes across modes"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make distclean    - Remove all generated files"
	@echo "  make help         - Show this help"
	@echo ""
	@echo -e "$(BLUE)Examples:$(NC)"
	@echo "  make MODE=str run       - Build and run string mode"
	@echo "  make MODE=encode run    - Build and run encode mode"
	@echo "  make test-all           - Test all three modes"
	@echo "  make size-compare       - Compare binary sizes"
	@echo ""
