/**
 * @file test_log.c
 * @brief ww_log_v2 self-test suite (TEST module, registered in log_config.json).
 *
 * Each check prints [PASS]/[FAIL]; test_log_run_all() returns the fail count.
 * The suite is mode/backend-gated so the same binary is meaningful in every
 * build, and it only touches the log RAM region (+ the LOG partition when the
 * EXT_MEM backend is on), so it is safe to run on the firmware target too.
 *
 * Storage-layer tests drive log_ram_write()/log_ram_flush() directly (mode
 * independent); the filter tests drive the N_LOG_* path (encode mode only).
 */

#include "ww_std.h"
#include "log/n_ww_log.h"
#include "log/n_ww_log_storage.h"
#include "dlm_layout.h"
#include "test_in.h"

/* ====================================================================== */
/* tiny check framework                                                    */
/* ====================================================================== */

static int s_pass;
static int s_fail;

static void check_(int ok, const char *name)
{
    if (ok) { s_pass++; ww_printf("  [PASS] %s\n", name); }
    else    { s_fail++; ww_printf("  [FAIL] %s\n", name); }
}
#define CHECK(cond, name)   check_((cond) ? 1 : 0, (name))

static void section(const char *title)
{
    ww_printf("\n--- %s ---\n", title);
}

/* Write one synthetic encoded entry with `pcnt` U32 params straight into the
 * RAM ring (bypasses the N_LOG_* macros for deterministic sizing). */
static void t_write(U8 pcnt)
{
    U32 p[N_WW_LOG_ENCODE_MAX_PARAMS];
    U8  i;
    for (i = 0; i < pcnt; i++)
    {
        p[i] = 0xA0000000u + i;
    }
    (void)log_ram_write(N_WW_LOG_ENCODE(CURRENT_FILE_ID, 123, pcnt), p, pcnt);
}

/* ====================================================================== */
/* RAM ring tests (mode independent; need the RAM backend)                 */
/* ====================================================================== */
#if (CONFIG_N_LOG_BACKEND_RAM == 1)

static void test_ram_init(void)
{
    section("RAM init / clear");
    log_ram_init(WW_TRUE);
    CHECK(log_ram_get_write_index() == 0,   "write_index == 0 after clear");
    CHECK(log_ram_get_read_index() == 0,    "read_index == 0 after clear");
    CHECK(log_ram_validate() == WW_OK,      "header validates after clear");
    CHECK(log_ram_get_log_count() == 0,     "log_count == 0 after clear");
    CHECK(log_ram_get_flags() == 0,         "flags == 0 after clear");
}

static void test_ram_basic_write(void)
{
    section("RAM basic write + accounting");
    log_ram_init(WW_TRUE);
    t_write(0);  /* 4 bytes  */
    t_write(1);  /* 8 bytes  */
    t_write(2);  /* 12 bytes */
    CHECK(log_ram_get_write_index() == 24,  "write_index advanced by 24");
    CHECK(get_current_usage() == 24,        "usage == 24");
    CHECK(log_ram_get_log_count() == 3,     "log_count == 3");
    CHECK(log_ram_validate_data() == WW_OK, "entry chain validates");
    CHECK((U32)log_ram_get_available() + get_current_usage()
              == (U32)log_ram_get_data_size() - 1,
          "available + usage == data_size - 1");
}

static void test_ram_overflow(void)
{
    section("RAM ring overflow / wrap (FIFO evict + flag)");
    log_ram_init(WW_TRUE);
    U16 ds = log_ram_get_data_size();
    U32 n  = (U32)(ds / 12) + 50;       /* overshoot capacity with 12B entries */
    U32 i;
    for (i = 0; i < n; i++)
    {
        t_write(2);
    }
    CHECK((log_ram_get_flags() & LOG_FLAG_OVERFLOW) != 0, "OVERFLOW flag set");
    CHECK(log_ram_get_overflow_count() > 0,               "overflow_count > 0");
    CHECK(get_current_usage() <= (U16)(ds - 1),           "usage within capacity");
    CHECK(log_ram_validate_data() == WW_OK,               "ring still valid after wrap");
    CHECK(log_ram_get_log_count() == n,                   "log_count counts every write");
}

static void test_ram_corruption(void)
{
    section("RAM structural data validation");
    log_ram_init(WW_TRUE);
    t_write(1); t_write(1); t_write(1);
    CHECK(log_ram_validate_data() == WW_OK, "valid before corruption");
    /* Smash the first entry header to claim 63 params -> overruns the data. */
    U8 *d = log_ram_get_data_ptr();
    *(U32 *)d = N_WW_LOG_ENCODE(CURRENT_FILE_ID, 1, 63);
    CHECK(log_ram_validate_data() != WW_OK, "corruption detected (pcnt overrun)");
    log_ram_init(WW_TRUE);  /* cleanup */
}

static void test_hot_restart(void)
{
    section("Hot restart (preserve on valid header)");
    log_ram_init(WW_TRUE);
    t_write(1); t_write(2); t_write(0);
    U16 wi = log_ram_get_write_index();
    log_ram_init(WW_FALSE);                 /* warm re-init */
    CHECK(log_ram_get_write_index() == wi,  "write_index preserved across warm init");
    CHECK(log_ram_validate_data() == WW_OK, "data preserved + valid");
    CHECK(log_ram_get_log_count() == 0,     "log_count reset to 0 (per-boot)");
}

static void test_cold_fallback(void)
{
    section("Cold fallback (re-init on bad header)");
    log_ram_init(WW_TRUE);
    t_write(1);
    *(volatile U32 *)DLM_MAINTAIN_LOG_BASE_ADDR = 0xDEADBEEF;  /* smash magic */
    log_ram_init(WW_FALSE);
    CHECK(log_ram_validate() == WW_OK,     "header re-initialized after corruption");
    CHECK(log_ram_get_write_index() == 0,  "buffer reset on cold fallback");
}

/* ====================================================================== */
/* N_LOG_* path: level / module filters (encode mode feeds the RAM ring)   */
/* ====================================================================== */
#if defined(CONFIG_N_LOG_MODE_ENCODE)

static void test_level_filter(void)
{
    section("Storage level filter (DBG not persisted) + ERR flag");
    log_ram_init(WW_TRUE);
    n_ww_log_set_level_threshold(N_WW_LOG_LEVEL_DBG);   /* allow all at runtime */
    U32 before = log_ram_get_log_count();
    N_LOG_ERR("selftest err %d", 1);
    N_LOG_WRN("selftest wrn %d", 2);
    N_LOG_INF("selftest inf %d", 3);
    N_LOG_DBG("selftest dbg %d", 4);
    CHECK(log_ram_get_log_count() - before == 3,
          "ERR/WRN/INF persisted, DBG dropped (storage threshold INF)");
    CHECK((log_ram_get_flags() & LOG_FLAG_ERROR) != 0, "ERR sets LOG_FLAG_ERROR");
}

static void test_module_mask(void)
{
    section("Dynamic module mask");
    log_ram_init(WW_TRUE);
    U32 b0 = log_ram_get_log_count();
    n_ww_log_disable_module(CURRENT_MODULE_ID);
    N_LOG_INF("masked %d", 0);
    CHECK(log_ram_get_log_count() == b0, "disabled module emits nothing");
    n_ww_log_enable_module(CURRENT_MODULE_ID);
    N_LOG_INF("unmasked %d", 0);
    CHECK(log_ram_get_log_count() == b0 + 1, "enabled module emits again");
}

static void test_level_threshold(void)
{
    section("Dynamic level threshold");
    log_ram_init(WW_TRUE);
    n_ww_log_set_level_threshold(N_WW_LOG_LEVEL_ERR);
    U32 b0 = log_ram_get_log_count();
    N_LOG_INF("inf %d", 0);
    N_LOG_WRN("wrn %d", 0);
    N_LOG_DBG("dbg %d", 0);
    CHECK(log_ram_get_log_count() == b0, "INF/WRN/DBG suppressed at ERR threshold");
    N_LOG_ERR("err %d", 0);
    CHECK(log_ram_get_log_count() == b0 + 1, "ERR still emitted");
    n_ww_log_set_level_threshold(N_WW_LOG_LEVEL_DBG);   /* restore */
}

#endif /* CONFIG_N_LOG_MODE_ENCODE */

/* ====================================================================== */
/* External storage block ring                                             */
/* ====================================================================== */
#if defined(CONFIG_N_LOG_BACKEND_EXT_MEM)

static void test_ext_flush(void)
{
    section("External storage: block flush + payload CRC");
    if (log_ext_mem_available() != WW_TRUE)
    {
        CHECK(0, "external storage available");
        return;
    }
    log_ext_mem_clear();
    log_ram_init(WW_TRUE);

    U32 i;
    for (i = 0; i < 40; i++)        /* 40 * 8B = 320B -> fits one block payload */
    {
        t_write(1);
    }
    U16 slot0 = log_ext_get_write_slot();
    int r = log_ram_flush();
    CHECK(r == LOG_EXT_OK, "flush returns OK");
    CHECK(log_ext_get_write_slot() == (U16)(slot0 + 1), "write_slot advanced by 1");

    static U8 buf[LOG_EXT_BLOCK_SIZE];
    log_ext_mem_read(buf, LOG_EXT_BLOCK_SIZE);
    LOG_BLOCK_HEADER_T *bh = (LOG_BLOCK_HEADER_T *)buf;
    CHECK(bh->magic == LOG_BLOCK_MAGIC, "block 0 has 'LOGH' magic");
    CHECK(bh->entry_count == 40,        "block records 40 entries");
    U32 crc = log_calc_checksum(buf + LOG_EXT_BLOCK_HEADER_SIZE, bh->data_size);
    CHECK(crc == bh->crc,               "block payload CRC matches");
}

static void test_ext_ring_wrap(void)
{
    section("External storage: ring wrap (RING keeps most recent)");
    if (log_ext_mem_available() != WW_TRUE)
    {
        CHECK(0, "external storage available");
        return;
    }
    log_ext_mem_clear();
    log_ram_init(WW_TRUE);
    U16 bc = log_ext_get_block_count();

    U32 i;
    for (i = 0; i < (U32)(bc + 3) * 45; i++)   /* > block_count blocks of traffic */
    {
        t_write(1);
    }
    int guard = 0;
    while (log_ram_get_pending_len() > 0 && guard++ < 2000)
    {
        if (log_ram_flush() != LOG_EXT_OK)
        {
            break;
        }
    }
    CHECK(log_ext_get_wrap_count() > 0, "ring wrapped (wrap_count > 0)");
    CHECK(log_ext_get_block_count() == bc, "block_count stable");
    CHECK(log_ext_get_write_slot() < bc, "write_slot within ring");
}

static void test_ext_resume(void)
{
    section("External storage: footer resume across reboot (no erase)");
    if (log_ext_mem_available() != WW_TRUE)
    {
        CHECK(0, "external storage available");
        return;
    }
    log_ext_mem_clear();
    log_ram_init(WW_TRUE);

    /* Flush a few blocks, then snapshot the ring cursor. */
    U32 i;
    for (i = 0; i < 80; i++)        /* 80 * 8B -> ~2 blocks */
    {
        t_write(1);
    }
    int guard = 0;
    while (log_ram_get_pending_len() > 0 && guard++ < 2000)
    {
        if (log_ram_flush() != LOG_EXT_OK) { break; }
    }
    U16 slot = log_ext_get_write_slot();
    U32 seq  = log_ext_get_next_seq();
    U32 wrap = log_ext_get_wrap_count();
    CHECK(seq > 0, "blocks flushed before reboot");

    /* Simulate a reboot: ext ctx (RAM) is lost, device bytes persist. */
    log_ext_force_reinit();
    CHECK(log_ext_get_initialized() == WW_FALSE, "ext ctx dropped (reboot sim)");

    /* First access re-inits -> must RESUME from footer, not erase. */
    CHECK(log_ext_mem_available() == WW_TRUE, "re-init succeeds after reboot");
    CHECK(log_ext_get_write_slot() == slot,  "write_slot resumed from footer");
    CHECK(log_ext_get_next_seq()  == seq,   "next_seq resumed from footer");
    CHECK(log_ext_get_wrap_count() == wrap,  "wrap_count resumed from footer");

    /* The previously flushed block 0 must still be intact (proves no erase). */
    static U8 buf[LOG_EXT_BLOCK_SIZE];
    log_ext_mem_read(buf, LOG_EXT_BLOCK_SIZE);
    LOG_BLOCK_HEADER_T *bh = (LOG_BLOCK_HEADER_T *)buf;
    CHECK(bh->magic == LOG_BLOCK_MAGIC, "prior block preserved (not erased)");

    /* Cleanup so later runs start from a known-empty archive. */
    log_ext_mem_clear();
}

#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */

#endif /* CONFIG_N_LOG_BACKEND_RAM */

/* ====================================================================== */
/* runner                                                                  */
/* ====================================================================== */

int test_log_run_all(void)
{
    s_pass = 0;
    s_fail = 0;
    ww_printf("\n========== ww_log v2 self-test ==========\n");

#if (CONFIG_N_LOG_BACKEND_RAM == 1)
    test_ram_init();
    test_ram_basic_write();
    test_ram_overflow();
    test_ram_corruption();
    test_hot_restart();
    test_cold_fallback();
#if defined(CONFIG_N_LOG_MODE_ENCODE)
    test_level_filter();
    test_module_mask();
    test_level_threshold();
#endif
#if defined(CONFIG_N_LOG_BACKEND_EXT_MEM)
    test_ext_flush();
    test_ext_ring_wrap();
    test_ext_resume();
#endif
    /* leave the log in a clean state for whatever runs next */
    n_ww_log_set_level_threshold(N_WW_LOG_LEVEL_DBG);
    log_ram_init(WW_TRUE);
#else
    ww_printf("  (RAM backend off -> storage self-tests skipped)\n");
#endif

    ww_printf("\n========== self-test: %d passed, %d failed ==========\n",
              s_pass, s_fail);
    return s_fail;
}
