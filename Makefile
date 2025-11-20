# Makefile for Log System Test
# Date: 2025-11-18

# Enable parallel compilation by default (use -j option)
MAKEFLAGS += --no-print-directory

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -g -O2
LDFLAGS =

# Detect number of CPU cores for parallel compilation
NPROCS := $(shell nproc 2>/dev/null || echo 4)

# Color output (optional, for better readability)
RED = \033[0;31m
GREEN = \033[0;32m
YELLOW = \033[0;33m
BLUE = \033[0;34m
NC = \033[0m # No Color

# Directories
SRC_DIR = src
INC_DIR = include
EXAMPLES_DIR = examples
BUILD_DIR = build
BIN_DIR = bin

# Output executable
TARGET = $(BIN_DIR)/log_test

# Source files
CORE_SRCS = $(SRC_DIR)/core/ww_log_common.c \
            $(SRC_DIR)/core/ww_log_str.c \
            $(SRC_DIR)/core/ww_log_encode.c

MODULE_SRCS = $(SRC_DIR)/demo/demo_init.c \
              $(SRC_DIR)/demo/demo_process.c \
              $(SRC_DIR)/test/test_unit.c \
              $(SRC_DIR)/test/test_integration.c \
              $(SRC_DIR)/test/test_stress.c \
              $(SRC_DIR)/app/app_main.c \
              $(SRC_DIR)/app/app_config.c \
              $(SRC_DIR)/drivers/drv_uart.c \
              $(SRC_DIR)/drivers/drv_spi.c \
              $(SRC_DIR)/drivers/drv_i2c.c \
              $(SRC_DIR)/brom/brom_boot.c \
              $(SRC_DIR)/brom/brom_loader.c

MAIN_SRC = $(EXAMPLES_DIR)/main.c

ALL_SRCS = $(CORE_SRCS) $(MODULE_SRCS) $(MAIN_SRC)

# Object files
OBJS = $(ALL_SRCS:%.c=$(BUILD_DIR)/%.o)

# Default target: build with current config
.PHONY: all
all: $(TARGET)

# Create directories
$(BUILD_DIR) $(BIN_DIR):
	@mkdir -p $(BUILD_DIR)/src/core
	@mkdir -p $(BUILD_DIR)/src/demo
	@mkdir -p $(BUILD_DIR)/src/test
	@mkdir -p $(BUILD_DIR)/src/app
	@mkdir -p $(BUILD_DIR)/src/drivers
	@mkdir -p $(BUILD_DIR)/src/brom
	@mkdir -p $(BUILD_DIR)/examples
	@mkdir -p $(BIN_DIR)

# Build executable
$(TARGET): $(BUILD_DIR) $(BIN_DIR) $(OBJS)
	@echo "$(BLUE)Linking $@...$(NC)"
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "$(GREEN)Build complete: $@$(NC)"

# Compile source files (with dependency generation)
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "$(YELLOW)Compiling $<...$(NC)"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Include dependency files
-include $(OBJS:.o=.d)

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@rm -f $(BIN_DIR)/log_test
	@echo "Clean complete."

# Deep clean (remove everything including comparison binaries)
.PHONY: distclean
distclean:
	@echo "Deep cleaning all build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Deep clean complete."

# Run test program
.PHONY: run
run: $(TARGET)
	@echo ""
	@echo "Running test program..."
	@echo ""
	@./$(TARGET)

# Build and run with STRING mode
.PHONY: test-str
test-str: clean
	@echo "Building with STRING mode..."
	@# Edit config to enable STR_MODE
	@sed -i 's|^// #define CONFIG_WW_LOG_STR_MODE|#define CONFIG_WW_LOG_STR_MODE|' include/ww_log_config.h
	@sed -i 's|^#define CONFIG_WW_LOG_ENCODE_MODE|// #define CONFIG_WW_LOG_ENCODE_MODE|' include/ww_log_config.h
	@sed -i 's|^#define CONFIG_WW_LOG_DISABLED|// #define CONFIG_WW_LOG_DISABLED|' include/ww_log_config.h
	@$(MAKE) all
	@echo ""
	@echo "========================================="
	@echo "  Running STRING MODE Test"
	@echo "========================================="
	@./$(TARGET)

# Build and run with ENCODE mode
.PHONY: test-encode
test-encode: clean
	@echo "Building with ENCODE mode..."
	@# Edit config to enable ENCODE_MODE
	@sed -i 's|^#define CONFIG_WW_LOG_STR_MODE|// #define CONFIG_WW_LOG_STR_MODE|' include/ww_log_config.h
	@sed -i 's|^// #define CONFIG_WW_LOG_ENCODE_MODE|#define CONFIG_WW_LOG_ENCODE_MODE|' include/ww_log_config.h
	@sed -i 's|^#define CONFIG_WW_LOG_DISABLED|// #define CONFIG_WW_LOG_DISABLED|' include/ww_log_config.h
	@$(MAKE) all
	@echo ""
	@echo "========================================="
	@echo "  Running ENCODE MODE Test"
	@echo "========================================="
	@./$(TARGET)

# Build and run with DISABLED mode
.PHONY: test-disabled
test-disabled: clean
	@echo "Building with DISABLED mode..."
	@# Edit config to enable DISABLED
	@sed -i 's|^#define CONFIG_WW_LOG_STR_MODE|// #define CONFIG_WW_LOG_STR_MODE|' include/ww_log_config.h
	@sed -i 's|^#define CONFIG_WW_LOG_ENCODE_MODE|// #define CONFIG_WW_LOG_ENCODE_MODE|' include/ww_log_config.h
	@sed -i 's|^// #define CONFIG_WW_LOG_DISABLED|#define CONFIG_WW_LOG_DISABLED|' include/ww_log_config.h
	@$(MAKE) all
	@echo ""
	@echo "========================================="
	@echo "  Running DISABLED MODE Test"
	@echo "========================================="
	@./$(TARGET)

# Run all three modes sequentially
.PHONY: test-all
test-all:
	@echo ""
	@echo "========================================="
	@echo "  Testing All Three Modes"
	@echo "========================================="
	@echo ""
	@$(MAKE) test-str
	@echo ""
	@echo ""
	@$(MAKE) test-encode
	@echo ""
	@echo ""
	@$(MAKE) test-disabled
	@echo ""
	@echo "========================================="
	@echo "  All Mode Tests Complete"
	@echo "========================================="

# Build and run encode mode, saving output to file
.PHONY: test-encode-save
test-encode-save: test-encode
	@echo ""
	@echo "Running encode mode and saving output..."
	@./$(TARGET) 2>&1 | grep "^0x" > encode_output.txt
	@echo "Encoded logs saved to: encode_output.txt"
	@echo ""
	@echo "Decoding logs..."
	@python3 tools/log_decoder.py encode_output.txt
	@echo ""
	@echo "Raw encoded output is in: encode_output.txt"
	@echo "Use: python3 tools/log_decoder.py encode_output.txt"

# Help
.PHONY: help
help:
	@echo "$(GREEN)Log System Makefile$(NC)"
	@echo ""
	@echo "$(BLUE)Targets:$(NC)"
	@echo "  make [-j$(NPROCS)]    - Build with current config (parallel compilation)"
	@echo "  make run              - Build and run with current config"
	@echo "  make test-str         - Build and test STRING mode"
	@echo "  make test-encode      - Build and test ENCODE mode"
	@echo "  make test-disabled    - Build and test DISABLED mode"
	@echo "  make test-all         - Test all three modes"
	@echo "  make test-encode-save - Test encode mode and decode output"
	@echo "  make size-compare     - Compare binary sizes across modes"
	@echo "  make clean            - Remove build artifacts"
	@echo "  make help             - Show this help"
	@echo ""
	@echo "$(BLUE)Parallel Compilation:$(NC)"
	@echo "  make -j$(NPROCS)               # Use $(NPROCS) parallel jobs (detected CPU cores)"
	@echo "  make -j8               # Use 8 parallel jobs"
	@echo "  make test-str -j4      # Test string mode with 4 parallel jobs"
	@echo ""
	@echo "$(BLUE)Examples:$(NC)"
	@echo "  make test-str -j$(NPROCS)      # Fast test string mode"
	@echo "  make test-encode-save  # Test encode mode with decoding"
	@echo "  make test-all          # Test all modes"
	@echo "  make size-compare      # Compare binary sizes"

.PHONY: show-config
show-config:
	@echo "Current configuration in include/ww_log_config.h:"
	@grep -E "^(#define|// #define) CONFIG_WW_LOG_(STR_MODE|ENCODE_MODE|DISABLED)" include/ww_log_config.h || true
.PHONY: size-compare
size-compare:
	@echo "$(GREEN)===== Binary Size Comparison =====$(NC)"
	@echo ""
	@echo "$(YELLOW)Building all three modes...$(NC)"
	@echo ""
	
	@# Build STRING mode
	@echo "$(BLUE)1. Building STRING MODE...$(NC)"
	@sed -i "s|^// #define CONFIG_WW_LOG_STR_MODE|#define CONFIG_WW_LOG_STR_MODE|" include/ww_log_config.h
	@sed -i "s|^#define CONFIG_WW_LOG_ENCODE_MODE|// #define CONFIG_WW_LOG_ENCODE_MODE|" include/ww_log_config.h
	@sed -i "s|^#define CONFIG_WW_LOG_DISABLED|// #define CONFIG_WW_LOG_DISABLED|" include/ww_log_config.h
	@$(MAKE) clean > /dev/null 2>&1
	@$(MAKE) all > /dev/null 2>&1
	@cp $(TARGET) $(BIN_DIR)/log_test_str
	@SIZE_STR=$$(size $(BIN_DIR)/log_test_str | tail -1 | awk '{print $$1}'); echo "  Text size: $$SIZE_STR bytes"
	
	@# Build ENCODE mode
	@echo ""
	@echo "$(BLUE)2. Building ENCODE MODE...$(NC)"
	@sed -i "s|^#define CONFIG_WW_LOG_STR_MODE|// #define CONFIG_WW_LOG_STR_MODE|" include/ww_log_config.h
	@sed -i "s|^// #define CONFIG_WW_LOG_ENCODE_MODE|#define CONFIG_WW_LOG_ENCODE_MODE|" include/ww_log_config.h
	@sed -i "s|^#define CONFIG_WW_LOG_DISABLED|// #define CONFIG_WW_LOG_DISABLED|" include/ww_log_config.h
	@$(MAKE) clean > /dev/null 2>&1
	@$(MAKE) all > /dev/null 2>&1
	@cp $(TARGET) $(BIN_DIR)/log_test_encode
	@SIZE_ENC=$$(size $(BIN_DIR)/log_test_encode | tail -1 | awk '{print $$1}'); echo "  Text size: $$SIZE_ENC bytes"
	
	@# Build DISABLED mode
	@echo ""
	@echo "$(BLUE)3. Building DISABLED MODE...$(NC)"
	@sed -i "s|^#define CONFIG_WW_LOG_STR_MODE|// #define CONFIG_WW_LOG_STR_MODE|" include/ww_log_config.h
	@sed -i "s|^#define CONFIG_WW_LOG_ENCODE_MODE|// #define CONFIG_WW_LOG_ENCODE_MODE|" include/ww_log_config.h
	@sed -i "s|^// #define CONFIG_WW_LOG_DISABLED|#define CONFIG_WW_LOG_DISABLED|" include/ww_log_config.h
	@$(MAKE) clean > /dev/null 2>&1
	@$(MAKE) all > /dev/null 2>&1
	@cp $(TARGET) $(BIN_DIR)/log_test_disabled
	@SIZE_DIS=$$(size $(BIN_DIR)/log_test_disabled | tail -1 | awk '{print $$1}'); echo "  Text size: $$SIZE_DIS bytes"
	
	@# Generate detailed comparison
	@echo ""
	@echo "$(GREEN)===== Size Comparison Report =====$(NC)"
	@echo ""
	@size $(BIN_DIR)/log_test_str $(BIN_DIR)/log_test_encode $(BIN_DIR)/log_test_disabled
	@echo ""
	@echo "$(YELLOW)Detailed Analysis:$(NC)"
	@python3 tools/size_compare.py $(BIN_DIR)/log_test_str $(BIN_DIR)/log_test_encode $(BIN_DIR)/log_test_disabled
	@echo ""
	@echo "$(GREEN)Binary files saved in $(BIN_DIR)/:$(NC)"
	@ls -lh $(BIN_DIR)/log_test_* | awk '{print "  " $$9 " - " $$5}'
	@echo ""
	
	@# Restore to STR mode
	@sed -i "s|^// #define CONFIG_WW_LOG_STR_MODE|#define CONFIG_WW_LOG_STR_MODE|" include/ww_log_config.h
	@sed -i "s|^#define CONFIG_WW_LOG_ENCODE_MODE|// #define CONFIG_WW_LOG_ENCODE_MODE|" include/ww_log_config.h
	@sed -i "s|^#define CONFIG_WW_LOG_DISABLED|// #define CONFIG_WW_LOG_DISABLED|" include/ww_log_config.h

