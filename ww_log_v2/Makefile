# Makefile for ww_log v2 (PC simulation)
#
# Log mode and backends are chosen in sim/log.conf (Kconfig .conf syntax).
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
MAP_ID_H    = $(OUT_DIR)/log_map_id.h
GEN         = scripts/log/gen_log_map.py

# Sim-only: stands in for the firmware's Kconfig .conf -> autoconf.h step.
# Deliberately NOT part of gen_log_map.py, which stays purely about file IDs and
# the map so it merges into the firmware tree with nothing dead in it.
LOG_AUTOCONF = $(OUT_DIR)/log_autoconf.h
LOG_CONF     = sim/log.conf
CONF2AUTOCONF= sim/conf_to_autoconf.py

# Archive of released maps, indexed by map_id. The boot record in a log stream
# names the map that decodes it; this is where the decoder finds that map for
# firmware that is no longer the current build. Populated by `make map-archive`.
MAP_ARCHIVE = maps
VERSION_H   = sim/version.h

# ---- Include paths ---------------------------------------------------------
# sim/ comes FIRST so its ww_type.h, autoconf.h, FreeRTOS.h etc. shadow any
# fw-specific headers.  include/ provides log/n_ww_log*.h.
INCLUDES = \
    -Isim \
    -Iinclude \
    -I$(OUT_DIR) \
    -Isrc/demo \
    -Isrc/drivers \
    -Isrc/test

# Force-include autoconf.h on every TU, the way the Kconfig build does on target
# (CONFIG_N_LOG_MODE / CONFIG_N_LOG_BACKEND_* come from there). This lets the
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
    log/n_ww_log_output.c \
    log/n_ww_log_ram.c \
    log/n_ww_log_storage.c \
    log/n_ww_log_task.c

# Sim device backend (not a fork of module logic; just the hardware shims).
SIM_IMPL_SRCS = \
    sim/sim_dlm_ram.c \
    sim/sim_ext_storage.c

# Registered source dirs (scanned by gen_log_map.py). Walked recursively, the
# same way the generator does -- a plain src/demo/*.c wildcard would hand a
# file_id to src/demo/sub/x.c and then never compile it.
LOG_SRC_ROOTS = src/demo src/drivers src/test
LOG_SRCS = $(shell find $(LOG_SRC_ROOTS) -name '*.c' 2>/dev/null)
# The directories themselves are prerequisites too: a DELETED source drops out
# of $(LOG_SRCS), leaving nothing newer than file_ids.mk, so the map would keep
# listing it as present. A directory's mtime moves when an entry is added or
# removed, which catches exactly that case.
LOG_SRC_DIRS = $(shell find $(LOG_SRC_ROOTS) -type d 2>/dev/null)

ALL_SRCS = $(LOG_MODULE_SRCS) $(SIM_IMPL_SRCS) $(LOG_SRCS) examples/main.c

OBJS = $(ALL_SRCS:%.c=$(OUT_DIR)/%.o)

# ---- Map + derived artifacts -----------------------------------------------
# Rebuild scope matters here. Editing a source can move log line numbers (map +
# map_id change) without changing FILE IDS (the per-file -D values). Those
# outputs must not share one timestamp, or every edit costs a full rebuild:
#
#   file_ids.mk   regenerated on every scan, always touched -> safe to -include
#                 (a target that keeps an older mtime than its prerequisite
#                 makes GNU make restart forever). NOTHING compiles against it.
#   auto_file_ids.h  written ONLY when its content changed, i.e. only when a
#                 file was added/removed or a module ID moved. This is what the
#                 objects depend on. A non-log edit rebuilds just its object; a
#                 map-changing edit also rebuilds the two map_id consumers.
$(FILE_IDS_MK): $(LOG_CONFIG) $(GEN) $(LOG_CONF) $(CONF2AUTOCONF) $(LOG_SRCS) $(LOG_SRC_DIRS)
	@mkdir -p $(OUT_DIR)
	@echo "[GEN ] $(WW_LOG_MAP)"
	@$(PY) $(GEN) $(LOG_CONFIG) --root $(CURDIR) --out $(WW_LOG_MAP)
	@$(PY) $(GEN) $(LOG_CONFIG) --root $(CURDIR) --out $(WW_LOG_MAP) --makefile > $(FILE_IDS_MK)

$(AUTO_HEADER): $(FILE_IDS_MK)
	@$(PY) $(GEN) $(LOG_CONFIG) --root $(CURDIR) --out $(WW_LOG_MAP) --header --write $@

$(MAP_ID_H): $(FILE_IDS_MK)
	@$(PY) $(GEN) $(LOG_CONFIG) --root $(CURDIR) --out $(WW_LOG_MAP) --mapid --write $@

$(LOG_AUTOCONF): $(FILE_IDS_MK) $(LOG_CONF) $(CONF2AUTOCONF)
	@$(PY) $(CONF2AUTOCONF) $(LOG_CONF) --write $@

-include $(FILE_IDS_MK)

.PHONY: map
map: $(LOG_CONFIG) $(GEN)
	@mkdir -p $(OUT_DIR)
	@$(PY) $(GEN) $(LOG_CONFIG) --root $(CURDIR) --out $(WW_LOG_MAP)
	@$(PY) $(GEN) $(LOG_CONFIG) --root $(CURDIR) --out $(WW_LOG_MAP) --makefile > $(FILE_IDS_MK)
	@$(PY) $(GEN) $(LOG_CONFIG) --root $(CURDIR) --out $(WW_LOG_MAP) --header --write $(AUTO_HEADER)
	@$(PY) $(GEN) $(LOG_CONFIG) --root $(CURDIR) --out $(WW_LOG_MAP) --mapid  --write $(MAP_ID_H)
	@$(PY) $(CONF2AUTOCONF) $(LOG_CONF) --write $(LOG_AUTOCONF)
	@echo "Map regenerated."

# Keep a copy of the CURRENT map under its map_id, for decoding logs produced by
# this build after it stops being the current build. Run it when you cut a
# release -- not from the normal build, where every edit yields a fresh map_id
# and would bury the releases under hundreds of throwaway maps.
.PHONY: map-archive
map-archive: $(FILE_IDS_MK)
	@$(PY) $(GEN) $(LOG_CONFIG) --root $(CURDIR) --out $(WW_LOG_MAP) \
		--archive $(MAP_ARCHIVE) --version-header $(VERSION_H)

# ---- Build -----------------------------------------------------------------
.PHONY: all
all: $(TARGET)
	@echo "Build complete: $(TARGET)"

$(TARGET): $(OBJS)
	@mkdir -p $(OUT_DIR)
	@echo "[LINK] $@"
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Per-file compile with file_id / module_id / static-enable injection.
# Depends on AUTO_HEADER (write-if-changed) rather than FILE_IDS_MK (always
# touched) so only a real ID change forces a recompile -- see the note above.
# __NOTDIR_FILE__ takes the generator's disambiguated short name, which equals
# $(notdir $<) unless two sources share a basename (several target/*/main.c);
# STR mode has no file_id to fall back on, so those must not print alike.
$(OUT_DIR)/%.o: %.c $(AUTO_HEADER) $(LOG_AUTOCONF)
	@mkdir -p $(dir $@)
	@echo "[CC  ] $<"
	$(eval V  := $(subst -,_,$(subst /,_,$(subst .,_,$<))))
	$(eval FID := $(FILE_ID_$(V)))
	$(eval MID := $(MODULE_ID_$(V)))
	$(eval SEN := $(MODULE_STATIC_EN_$(V)))
	$(eval SHORT := $(or $(SHORT_NAME_$(V)),$(notdir $<)))
	@if [ -n "$(FID)" ]; then \
		$(CC) $(CFLAGS) \
			-DCURRENT_FILE_ID=$(FID) \
			-DCURRENT_MODULE_ID=$(MID) \
			-DCURRENT_MODULE_STATIC_EN=$(SEN) \
			-D__NOTDIR_FILE__=\"$(SHORT)\" \
			-MMD -MP -c $< -o $@; \
	else \
		$(CC) $(CFLAGS) \
			-DCURRENT_MODULE_STATIC_EN=0 \
			-D__NOTDIR_FILE__=\"$(SHORT)\" \
			-MMD -MP -c $< -o $@; \
	fi

# These two translation units include the generated map identity directly.
# State the dependency explicitly so a clean build creates it before their
# first compile (there is no .d file yet on that first build).
$(OUT_DIR)/log/n_ww_log_control.o $(OUT_DIR)/src/test/test_log.o: $(MAP_ID_H)

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
