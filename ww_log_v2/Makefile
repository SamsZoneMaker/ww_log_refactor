# Makefile for ww_log v2 (PC simulation)
#
# Log mode and backends are chosen in sim/autoconf.h.
# The two original FW-specific files (n_ww_log_storage.c, n_ww_log_task.c)
# have syntax errors (#include <>) and are replaced by sim/ equivalents.
#
#   make            build
#   make run        build + run
#   make map        regenerate ww_log_map.json + derived artifacts
#   make clean      remove build artifacts

SHELL := /bin/sh
.DEFAULT_GOAL := all

CC  = gcc
PY  = python3

OUT_DIR = output
TARGET  = $(OUT_DIR)/ww_log_v2_sim

LOG_CONFIG  = scripts/log/log_config.json
WW_LOG_MAP  = ww_log_map.json
FILE_IDS_MK = $(OUT_DIR)/file_ids.mk
AUTO_HEADER = $(OUT_DIR)/auto_file_ids.h
GEN         = scripts/log/gen_log_map.py

# ---- Include paths ---------------------------------------------------------
# sim/ comes FIRST so its ww_type.h, autoconf.h, FreeRTOS.h etc. shadow any
# fw-specific headers.  include/ provides log/n_ww_log*.h.
INCLUDES = \
    -Isim \
    -Iinclude \
    -I$(OUT_DIR) \
    -Isrc/demo \
    -Isrc/drivers

# Force-include autoconf.h on every TU, the way the Kconfig build does on target
# (CONFIG_N_LOG_MODE_* / CONFIG_N_LOG_BACKEND_* come from there). This lets the
# real firmware .c files compile unmodified -- they don't #include autoconf.h.
BASE_CFLAGS = -Wall -Wextra $(INCLUDES) -g -O2 \
              -DSIMULATION_MODE \
              -include autoconf.h \
              -Wno-unused-variable -Wno-unused-function \
              -Wno-unused-parameter
STATIC_OPTS ?=
CFLAGS = $(BASE_CFLAGS) $(STATIC_OPTS)
LDFLAGS =

# ---- Source lists ----------------------------------------------------------
# The real firmware log core, compiled unmodified. The only sim-side piece is
# the device backend (sim/), which provides the DLM RAM region and (when the
# EXT_MEM backend is on) file-backed flash/eeprom + partition table.
LOG_MODULE_SRCS = \
    log/n_ww_log_control.c \
    log/n_ww_logoutput.c \
    log/n_ww_log_storage.c \
    log/n_ww_log_task.c

# Sim device backend (not a fork of module logic; just the hardware shims).
SIM_IMPL_SRCS = \
    sim/sim_dlm_ram.c \
    sim/sim_ext_storage.c

# Registered source dirs (scanned by gen_log_map.py)
LOG_SRCS = $(wildcard src/demo/*.c src/drivers/*.c)

ALL_SRCS = $(LOG_MODULE_SRCS) $(SIM_IMPL_SRCS) $(LOG_SRCS) examples/main.c

OBJS = $(ALL_SRCS:%.c=$(OUT_DIR)/%.o)

# ---- Map + derived artifacts -----------------------------------------------
$(FILE_IDS_MK): $(LOG_CONFIG) $(GEN) $(LOG_SRCS)
	@mkdir -p $(OUT_DIR)
	@echo "[GEN ] $(WW_LOG_MAP)"
	@$(PY) $(GEN) $(LOG_CONFIG) --root $(CURDIR) --out $(WW_LOG_MAP)
	@$(PY) $(GEN) $(LOG_CONFIG) --root $(CURDIR) --out $(WW_LOG_MAP) --makefile > $(FILE_IDS_MK)
	@$(PY) $(GEN) $(LOG_CONFIG) --root $(CURDIR) --out $(WW_LOG_MAP) --header   > $(AUTO_HEADER)

-include $(FILE_IDS_MK)

.PHONY: map
map: $(LOG_CONFIG) $(GEN)
	@mkdir -p $(OUT_DIR)
	@$(PY) $(GEN) $(LOG_CONFIG) --root $(CURDIR) --out $(WW_LOG_MAP)
	@$(PY) $(GEN) $(LOG_CONFIG) --root $(CURDIR) --out $(WW_LOG_MAP) --makefile > $(FILE_IDS_MK)
	@$(PY) $(GEN) $(LOG_CONFIG) --root $(CURDIR) --out $(WW_LOG_MAP) --header   > $(AUTO_HEADER)
	@echo "Map regenerated."

# ---- Build -----------------------------------------------------------------
.PHONY: all
all: $(TARGET)
	@echo "Build complete: $(TARGET)"

$(TARGET): $(OBJS)
	@mkdir -p $(OUT_DIR)
	@echo "[LINK] $@"
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Per-file compile with file_id / module_id / static-enable injection.
$(OUT_DIR)/%.o: %.c $(FILE_IDS_MK)
	@mkdir -p $(dir $@)
	@echo "[CC  ] $<"
	$(eval V  := $(subst /,_,$(subst .,_,$<)))
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

# ---- Run / clean ------------------------------------------------------------
.PHONY: run
run: all
	@echo ""
	@echo "===== Running $(TARGET) ====="
	@./$(TARGET)

.PHONY: clean
clean:
	@rm -rf $(OUT_DIR)
	@echo "Clean complete."

.PHONY: distclean
distclean: clean
	@rm -f $(WW_LOG_MAP)
	@echo "Distclean complete."
