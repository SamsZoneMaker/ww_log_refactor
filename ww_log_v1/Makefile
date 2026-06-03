# Makefile for ww_log v1 (PC simulation)
#
# On this machine `make` is MSYS2's mingw32-make; recipes run under /bin/sh.
# Mode (STR / ENCODE / DISABLED) is chosen in include/ww_log.h.
#
#   make            build
#   make run        build + run
#   make map        regenerate ww_log_map.json + derived artifacts
#   make clean      remove build artifacts

SHELL := /bin/sh
.DEFAULT_GOAL := all

CC = gcc
PY = python3

BUILD_DIR = build
BIN_DIR   = bin
TARGET    = $(BIN_DIR)/ww_log_sim

LOG_CONFIG  = log_config.json
WW_LOG_MAP  = ww_log_map.json
FILE_IDS_MK = $(BUILD_DIR)/file_ids.mk
AUTO_HEADER = include/auto_file_ids.h
GEN         = tools/gen_log_map.py

INCLUDES = -Iinclude -Isim -Isrc/demo -Isrc/drivers -Isrc/test
BASE_CFLAGS = -Wall -Wextra $(INCLUDES) -g -O2 \
              -Wno-unused-variable -Wno-unused-function \
              -D__USE_MINGW_ANSI_STDIO=0
STATIC_OPTS ?=
CFLAGS = $(BASE_CFLAGS) $(STATIC_OPTS)
LDFLAGS =

# Registered source dirs (scanned for LOG calls -> map regenerates on change)
LOG_SRCS = $(wildcard src/demo/*.c src/drivers/*.c src/test/*.c)

# All sources to compile
ALL_SRCS = $(wildcard core/*.c) \
           $(wildcard sim/*.c) \
           $(LOG_SRCS) \
           examples/main.c

OBJS = $(ALL_SRCS:%.c=$(BUILD_DIR)/%.o)

# ---- map + derived artifacts -------------------------------------------------
# file_ids.mk is a real (-included) target so GNU make builds it during its
# "remake makefiles" phase, before compiling anything.
$(FILE_IDS_MK): $(LOG_CONFIG) $(GEN) $(LOG_SRCS)
	@mkdir -p $(BUILD_DIR)
	@echo "[GEN ] $(WW_LOG_MAP)"
	@$(PY) $(GEN) $(LOG_CONFIG) --out $(WW_LOG_MAP)
	@$(PY) $(GEN) $(LOG_CONFIG) --makefile > $(FILE_IDS_MK)
	@$(PY) $(GEN) $(LOG_CONFIG) --header   > $(AUTO_HEADER)

-include $(FILE_IDS_MK)

.PHONY: map
map: $(LOG_CONFIG) $(GEN)
	@mkdir -p $(BUILD_DIR)
	@$(PY) $(GEN) $(LOG_CONFIG) --out $(WW_LOG_MAP)
	@$(PY) $(GEN) $(LOG_CONFIG) --makefile > $(FILE_IDS_MK)
	@$(PY) $(GEN) $(LOG_CONFIG) --header   > $(AUTO_HEADER)
	@echo "Map regenerated."

# ---- build -------------------------------------------------------------------
.PHONY: all
all: $(TARGET)
	@echo "Build complete: $(TARGET)  (mode set in include/ww_log.h)"

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	@echo "[LINK] $@"
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Per-file compile with file_id / module_id / static-enable injection.
$(BUILD_DIR)/%.o: %.c $(FILE_IDS_MK)
	@mkdir -p $(dir $@)
	@echo "[CC  ] $<"
	$(eval V := $(subst /,_,$(subst .,_,$<)))
	$(eval FID := $(FILE_ID_$(V)))
	$(eval MID := $(MODULE_ID_$(V)))
	$(eval SEN := $(MODULE_STATIC_EN_$(V)))
	@if [ -n "$(FID)" ]; then \
		$(CC) $(CFLAGS) \
			-DCURRENT_FILE_ID=$(FID) \
			-DCURRENT_MODULE_ID=$(MID) \
			-DCURRENT_MODULE_STATIC_EN=$(SEN) \
			-D__NOTDIR_FILE__=\"$(notdir $<)\" \
			-MMD -MP -c $< -o $@; \
	else \
		$(CC) $(CFLAGS) \
			-DCURRENT_MODULE_STATIC_EN=0 \
			-D__NOTDIR_FILE__=\"$(notdir $<)\" \
			-MMD -MP -c $< -o $@; \
	fi

-include $(OBJS:.o=.d)

# ---- run / clean -------------------------------------------------------------
.PHONY: run
run: all
	@echo ""
	@echo "===== Running $(TARGET) ====="
	@./$(TARGET)

.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@rm -f $(AUTO_HEADER)
	@echo "Clean complete."

.PHONY: distclean
distclean: clean
	@rm -f $(WW_LOG_MAP)
	@rm -rf sim_data
	@echo "Distclean complete."
