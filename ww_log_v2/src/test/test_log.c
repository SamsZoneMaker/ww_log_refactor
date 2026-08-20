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

#if defined(CONFIG_N_LOG) && (CONFIG_N_LOG_MODE == N_WW_LOG_MODE_ENCODE)
#include "log_map_id.h"   /* generated: N_WW_LOG_MAP_ID (boot-record test) */
#include "version.h"      /* project:   BUILD_VERSION / BUILD_GIT_ID       */
#endif

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

#if defined(CONFIG_N_LOG) && (CONFIG_N_LOG_MODE != N_WW_LOG_MODE_DISABLE)
static void test_runtime_default(void)
{
    section("Configured runtime level");
    CHECK(n_ww_log_get_level_threshold() == CONFIG_N_LOG_RUNTIME_THRESHOLD,
          "initial runtime level comes from .conf");
}
#endif

#ifdef CONFIG_N_LOG_BACKEND_RAM

/* Write one synthetic encoded entry with `pcnt` U32 params straight into the
 * RAM ring (bypasses the N_LOG_* macros for deterministic sizing). */
/* Write one entry at an explicit level (so ext level-filter paths are testable). */
static void t_write_lvl(U8 level, U8 pcnt)
{
    U32 p[N_WW_LOG_ENCODE_MAX_PARAMS];
    U8  i;
    for (i = 0; i < pcnt; i++)
    {
        p[i] = 0xA0000000u + i;
    }
    (void)log_ram_write(N_WW_LOG_ENCODE(CURRENT_FILE_ID, 123, level, pcnt), p, pcnt);
}

/* Default helper: ERR level -> always passes the ext persist threshold. */
static void t_write(U8 pcnt)
{
    t_write_lvl(N_WW_LOG_LEVEL_ERR, pcnt);
}

/* Bytes the flush path prepends to the FIRST batch a freshly cleared archive
 * receives: one boot record, so the archive can always name the map that
 * decodes it (n_ww_log_storage.c). Zero when there is no encode stream. */
#if defined(CONFIG_N_LOG) && \
    (CONFIG_N_LOG_MODE == N_WW_LOG_MODE_ENCODE) && \
    defined(CONFIG_N_LOG_BACKEND_EXT_MEM)
#define EXT_FIRST_BATCH_LEAD    N_WW_LOG_BOOT_RECORD_SIZE
#else
#define EXT_FIRST_BATCH_LEAD    0
#endif

/* ====================================================================== */
/* RAM ring tests (mode independent; need the RAM backend)                 */
/* ====================================================================== */
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
    /* Smash the first entry header to claim the max params -> overruns the data. */
    U8 *d = log_ram_get_data_ptr();
    *(U32 *)d = N_WW_LOG_ENCODE(CURRENT_FILE_ID, 1, N_WW_LOG_LEVEL_ERR, 15);
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
#if CONFIG_N_LOG_MODE == N_WW_LOG_MODE_ENCODE

static void test_level_filter(void)
{
    section("RAM keeps all levels + ERR flag");
    log_ram_init(WW_TRUE);
    n_ww_log_set_level_threshold(N_WW_LOG_LEVEL_DBG);   /* allow all at runtime */
    U32 before = log_ram_get_log_count();
    N_LOG_ERR("selftest err %d", 1);
    N_LOG_WRN("selftest wrn %d", 2);
    N_LOG_INF("selftest inf %d", 3);
    N_LOG_DBG("selftest dbg %d", 4);
    /* The runtime threshold is wide open above, so what survives is decided by
     * the COMPILE-time threshold, which drops the macros entirely. Levels are
     * 0..3, so "threshold + 1" of the four calls are still in the binary --
     * derive it rather than hardcode 4, or the suite only passes at the default
     * setting. */
    CHECK(log_ram_get_log_count() - before == CONFIG_N_LOG_COMPILE_THRESHOLD + 1,
          "RAM keeps every level that survived the compile threshold");
    CHECK((log_ram_get_flags() & LOG_FLAG_ERROR) != 0, "ERR sets LOG_FLAG_ERROR");
}

static void test_module_mask(void)
{
    section("Dynamic module mask");
    log_ram_init(WW_TRUE);
    U32 b0 = log_ram_get_log_count();
    n_ww_log_disable_module(CURRENT_MODULE_ID);
    N_LOG_ERR("masked %d", 0);  /* ERR survives every valid compile threshold */
    CHECK(log_ram_get_log_count() == b0, "disabled module emits nothing");
    n_ww_log_enable_module(CURRENT_MODULE_ID);
    N_LOG_ERR("unmasked %d", 0);
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

#endif /* encode mode */

/* ====================================================================== */
/* External storage block ring                                             */
/* ====================================================================== */
#if defined(CONFIG_N_LOG_BACKEND_EXT_MEM)

static void test_ext_flush(void)
{
    section("External storage: append flush + partition header");
    if (log_ext_mem_available() != WW_TRUE)
    {
        CHECK(0, "external storage available");
        return;
    }
    log_ext_mem_clear();
    log_ram_init(WW_TRUE);

    U32 base_off = log_ext_get_write_offset();
    CHECK(base_off == log_ext_get_log_offset() + LOG_EXT_PART_HDR_SIZE,
          "write_off starts just past the 8B partition header");

    U32 i;
    for (i = 0; i < 20; i++)        /* 20 * 8B = 160B, all ERR -> all persisted */
    {
        t_write(1);
    }
    int guard = 0;
    while (log_ram_get_pending_len() > 0 && guard++ < 2000)
    {
        if (log_ram_flush() != LOG_EXT_OK) { break; }
    }
    CHECK(log_ext_get_write_offset() == base_off + EXT_FIRST_BATCH_LEAD + 20 * 8,
          "write_off advanced by boot record + 20 entries * 8B");

    /* Partition header 'XLOG' present at the base, first entry right after it. */
    static U8 buf[64];
    log_ext_mem_read(buf, sizeof(buf));
    LOG_EXT_PART_HDR_T *ph = (LOG_EXT_PART_HDR_T *)buf;
    CHECK(ph->magic == LOG_EXTMEM_MAGIC, "partition header has 'XLOG' magic");
    U32 e0 = *(U32 *)(buf + LOG_EXT_PART_HDR_SIZE + EXT_FIRST_BATCH_LEAD);
    CHECK(N_WW_LOG_PCNT_OF(e0) == 1,             "first appended entry has pcnt=1");
    CHECK(N_WW_LOG_LEVEL_OF(e0) == N_WW_LOG_LEVEL_ERR, "first entry keeps ERR level");
}

static void test_ext_level_filter(void)
{
    section("External storage: only ERR/WRN appended (INF/DBG filtered)");
    if (log_ext_mem_available() != WW_TRUE)
    {
        CHECK(0, "external storage available");
        return;
    }
    log_ext_mem_clear();
    log_ram_init(WW_TRUE);

    U32 base_off = log_ext_get_write_offset();
    /* 4 entries, 1 param each (8B): ERR + WRN persist, INF + DBG do not. */
    t_write_lvl(N_WW_LOG_LEVEL_ERR, 1);
    t_write_lvl(N_WW_LOG_LEVEL_INF, 1);
    t_write_lvl(N_WW_LOG_LEVEL_WRN, 1);
    t_write_lvl(N_WW_LOG_LEVEL_DBG, 1);
    int guard = 0;
    while (log_ram_get_pending_len() > 0 && guard++ < 2000)
    {
        if (log_ram_flush() != LOG_EXT_OK) { break; }
    }
    /* t_write_lvl bypasses the call macros, so all four reach the ring; the ext
     * threshold then decides how many are copied on. Levels are 0..3, so
     * "threshold + 1" of them persist at 8 bytes each. */
    CHECK(log_ext_get_write_offset()
              == base_off + EXT_FIRST_BATCH_LEAD
                 + (CONFIG_N_LOG_EXT_LEVEL_THRESHOLD + 1) * 8,
          "only entries passing the ext level threshold were appended");
    CHECK(log_ram_get_pending_len() == 0,
          "all 4 entries consumed from RAM (INF/DBG dropped, not stuck)");
}

static void test_ext_resume(void)
{
    section("External storage: cold-boot scan resume (no erase)");
    if (log_ext_mem_available() != WW_TRUE)
    {
        CHECK(0, "external storage available");
        return;
    }
    log_ext_mem_clear();
    log_ram_init(WW_TRUE);

    /* Append a few entries, then snapshot the write cursor. */
    U32 i;
    for (i = 0; i < 30; i++)
    {
        t_write(1);
    }
    int guard = 0;
    while (log_ram_get_pending_len() > 0 && guard++ < 2000)
    {
        if (log_ram_flush() != LOG_EXT_OK) { break; }
    }
    U32 woff = log_ext_get_write_offset();
    CHECK(woff > log_ext_get_log_offset() + LOG_EXT_PART_HDR_SIZE,
          "entries appended before reboot");

    /* Simulate a reboot: ext ctx (RAM) is lost, device bytes persist. */
    log_ext_force_reinit();
    CHECK(log_ext_get_initialized() == WW_FALSE, "ext ctx dropped (reboot sim)");

    /* First access re-inits -> must rebuild write_off by SCANNING, not erase. */
    CHECK(log_ext_mem_available() == WW_TRUE, "re-init succeeds after reboot");
    CHECK(log_ext_get_write_offset() == woff, "write_off rebuilt by cold-boot scan");

    /* The partition header + first entry must still be intact (proves no erase). */
    static U8 buf[64];
    log_ext_mem_read(buf, sizeof(buf));
    LOG_EXT_PART_HDR_T *ph = (LOG_EXT_PART_HDR_T *)buf;
    CHECK(ph->magic == LOG_EXTMEM_MAGIC, "partition header preserved (not erased)");

    /* Cleanup so later runs start from a known-empty archive. */
    log_ext_mem_clear();
}

#if CONFIG_N_LOG_MODE == N_WW_LOG_MODE_ENCODE
/**
 * The boot record is what lets a host decode an archive that spans firmware
 * updates: n_ww_log_init() stamps one into the stream, naming the map that can
 * decode everything after it. Two properties have to hold or cross-version
 * decoding silently produces plausible-but-wrong lines:
 *   - it is a well-formed entry (pcnt=3), so every walker steps over it;
 *   - it reaches EXTERNAL storage, not just the RAM ring -- which it only does
 *     because its level is ERR and therefore always passes the ext threshold.
 */
static void test_boot_record(void)
{
    section("Boot record: map identity reaches the archive");
    if (log_ext_mem_available() != WW_TRUE)
    {
        CHECK(0, "external storage available");
        return;
    }
    log_ext_mem_clear();
    log_ram_init(WW_TRUE);

    U32 base_off = log_ext_get_write_offset();

    /* Re-run just the stamping half of init (the ring is already up). */
    n_ww_log_init();

    CHECK(get_current_usage() == N_WW_LOG_BOOT_RECORD_SIZE,
          "boot record is 16B in the RAM ring");
    CHECK(*(U32 *)log_ram_get_data_ptr() == (U32)N_WW_LOG_BOOT_RECORD_HDR,
          "ring holds the boot record header");
    CHECK(N_WW_LOG_PCNT_OF(N_WW_LOG_BOOT_RECORD_HDR) == 3,
          "boot record declares pcnt=3 (walkers skip it)");
    CHECK(N_WW_LOG_LEVEL_OF(N_WW_LOG_BOOT_RECORD_HDR) == N_WW_LOG_LEVEL_ERR,
          "boot record is ERR (always passes the ext threshold)");

    int guard = 0;
    while (log_ram_get_pending_len() > 0 && guard++ < 2000)
    {
        if (log_ram_flush() != LOG_EXT_OK) { break; }
    }

    /* No flush marker is armed here, so the record sits right after the
     * partition header (the flush path stamps one for a fresh archive whether or
     * not init already put one in the ring). */
    static U8 buf[64];
    log_ext_mem_read(buf, sizeof(buf));
    U32 *w = (U32 *)(buf + LOG_EXT_PART_HDR_SIZE);
    CHECK(log_ext_get_write_offset() > base_off, "boot record persisted to ext");
    CHECK(w[0] == (U32)N_WW_LOG_BOOT_RECORD_HDR, "archive starts with the boot record");
    CHECK(w[1] == (U32)N_WW_LOG_MAP_ID,          "archive carries this build's map_id");
    CHECK(w[2] == (U32)BUILD_VERSION,            "archive carries BUILD_VERSION");
    CHECK(w[3] == (U32)BUILD_GIT_ID,             "archive carries BUILD_GIT_ID");

    log_ext_mem_clear();
}
#endif /* encode mode */

#if CONFIG_N_LOG_EXT_FULL == N_WW_LOG_EXT_FULL_FREEZE
/**
 * When the archive fills, the last batch must still top up the tail with as
 * many WHOLE entries as fit rather than being discarded outright (which used to
 * strand up to one staging buffer of partition), and the condition has to be
 * visible afterwards -- hence LOG_FLAG_EXT_FULL in the RAM header, which
 * outlives the RAM-resident ext ctx.
 */
static void test_ext_full_freeze(void)
{
    section("External storage: FREEZE tops up the tail, then flags full");
    if (log_ext_mem_available() != WW_TRUE)
    {
        CHECK(0, "external storage available");
        return;
    }
    log_ext_mem_clear();
    log_ram_init(WW_TRUE);

    U32 end = log_ext_get_log_offset() + log_ext_get_log_size();
    int guard = 0;

    /* Keep writing 8B entries and draining until the archive freezes. */
    while (log_ext_mem_is_full() != WW_TRUE && guard++ < 20000)
    {
        t_write(1);
        if (log_ram_get_pending_len() >= LOG_EXT_FLUSH_STAGE_SIZE)
        {
            (void)log_ram_flush();
        }
    }
    while (log_ram_get_pending_len() > 0 && guard++ < 20000)
    {
        if (log_ram_flush() == LOG_EXT_ERR_WRITE_FAIL) { break; }
    }

    CHECK(log_ext_mem_is_full() == WW_TRUE, "archive reports full");
    CHECK((log_ram_get_flags() & LOG_FLAG_EXT_FULL) != 0,
          "LOG_FLAG_EXT_FULL recorded in the RAM header");
    /* Tail topped up: less than one whole entry of room may remain, not a whole
     * discarded batch. */
    CHECK(end - log_ext_get_write_offset() < 8,
          "partition tail filled to within one entry");
    CHECK(log_ext_get_write_offset() <= end, "write_off never runs past the end");

    log_ext_mem_clear();
    CHECK((log_ram_get_flags() & LOG_FLAG_EXT_FULL) == 0,
          "clearing the archive clears LOG_FLAG_EXT_FULL");
    log_ram_init(WW_TRUE);
}
#endif /* freeze policy */

#if (CONFIG_N_LOG_EXT_FULL == N_WW_LOG_EXT_FULL_ERASE) && \
    (CONFIG_N_LOG_MODE == N_WW_LOG_MODE_ENCODE)
static void test_ext_full_erase(void)
{
    section("External storage: ERASE restarts with a boot record");
    if (log_ext_mem_available() != WW_TRUE)
    {
        CHECK(0, "external storage available");
        return;
    }
    log_ext_mem_clear();
    log_ram_init(WW_TRUE);

    U32 previous = log_ext_get_write_offset();
    int wrapped = 0;
    int guard = 0;

    while (!wrapped && guard++ < 20000)
    {
        t_write(1);
        if (log_ram_get_pending_len() >= LOG_EXT_FLUSH_STAGE_SIZE)
        {
            U32 before = log_ext_get_write_offset();
            if (log_ram_flush() != LOG_EXT_OK)
            {
                break;
            }
            previous = log_ext_get_write_offset();
            wrapped = previous < before;
        }
    }

    CHECK(wrapped, "partition erased and append cursor restarted");
    CHECK(previous <= log_ext_get_log_offset() + log_ext_get_log_size(),
          "write_off remains inside the LOG partition");

    static U8 buf[64];
    log_ext_mem_read(buf, sizeof(buf));
    U32 *w = (U32 *)(buf + LOG_EXT_PART_HDR_SIZE);
    CHECK(w[0] == (U32)N_WW_LOG_BOOT_RECORD_HDR,
          "restarted archive begins with a boot record");
    CHECK(w[1] == (U32)N_WW_LOG_MAP_ID,
          "restarted archive carries the current map_id");

    /* Regression: if the previous firmware filled the archive exactly and the
     * device then rebooted, the cold scan reconstructs ctx.full=true. ERASE
     * must still accept the next entry and recycle the partition; treating
     * ctx.full like FREEZE here used to leave the archive permanently stuck. */
    log_ext_mem_clear();
    log_ram_init(WW_TRUE);
    U32 end = log_ext_get_log_offset() + log_ext_get_log_size();
    guard = 0;
    while (log_ext_get_write_offset() < end && guard++ < 20000)
    {
        t_write(1);
        if (log_ram_flush() != LOG_EXT_OK)
        {
            break;
        }
    }
    CHECK(log_ext_get_write_offset() == end,
          "test archive can be filled exactly to the partition end");

    log_ext_force_reinit();
    CHECK(log_ext_mem_available() == WW_TRUE,
          "cold scan resumes an exactly-full ERASE archive");
    t_write(1);
    CHECK(log_ram_flush() == LOG_EXT_OK,
          "ERASE accepts a new batch after resuming a full archive");
    CHECK(log_ext_get_write_offset()
              == log_ext_get_log_offset() + LOG_EXT_PART_HDR_SIZE
                 + N_WW_LOG_BOOT_RECORD_SIZE + 8,
          "post-reboot erase restarts at header + boot record + entry");

    log_ext_mem_clear();
    log_ram_init(WW_TRUE);
}
#endif

#if defined(CONFIG_N_LOG_EXT_FLUSH_MARKER)
static void test_ext_flush_marker(void)
{
    section("External storage: flush marker (once per armed drain)");
    if (log_ext_mem_available() != WW_TRUE)
    {
        CHECK(0, "external storage available");
        return;
    }
    log_ext_mem_clear();
    log_ram_init(WW_TRUE);

    U32 base_off = log_ext_get_write_offset();
    log_ext_flush_marker_arm();          /* simulate one flush-task wake */
    t_write(1);                          /* two ERR entries (8B each) -> persisted */
    t_write(1);
    int guard = 0;
    while (log_ram_get_pending_len() > 0 && guard++ < 2000)
    {
        if (log_ram_flush() != LOG_EXT_OK) { break; }
    }
    CHECK(log_ext_get_write_offset()
              == base_off + EXT_FIRST_BATCH_LEAD + LOG_EXT_FLUSH_MARKER_SIZE + 2 * 8,
          "write_off = boot record + marker(8B) + 2 entries(16B)");

    static U8 buf[64];
    log_ext_mem_read(buf, sizeof(buf));
    U32 m0 = *(U32 *)(buf + LOG_EXT_PART_HDR_SIZE + EXT_FIRST_BATCH_LEAD);
    CHECK(m0 == LOG_EXT_FLUSH_MARKER_HDR, "flush marker follows the boot record");
    U32 e0 = *(U32 *)(buf + LOG_EXT_PART_HDR_SIZE + EXT_FIRST_BATCH_LEAD
                          + LOG_EXT_FLUSH_MARKER_SIZE);
    CHECK(N_WW_LOG_PCNT_OF(e0) == 1, "a real entry follows right after marker+tick");

    /* A second drain that was NOT re-armed must not add another marker. */
    U32 off2 = log_ext_get_write_offset();
    t_write(1);
    guard = 0;
    while (log_ram_get_pending_len() > 0 && guard++ < 2000)
    {
        if (log_ram_flush() != LOG_EXT_OK) { break; }
    }
    CHECK(log_ext_get_write_offset() == off2 + 8,
          "unarmed drain appends the entry only (no second marker)");

    log_ext_mem_clear();
}
#endif /* CONFIG_N_LOG_EXT_FLUSH_MARKER */

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

#if defined(CONFIG_N_LOG) && (CONFIG_N_LOG_MODE != N_WW_LOG_MODE_DISABLE)
    test_runtime_default();
#endif

#ifdef CONFIG_N_LOG_BACKEND_RAM
    test_ram_init();
    test_ram_basic_write();
    test_ram_overflow();
    test_ram_corruption();
    test_hot_restart();
    test_cold_fallback();
#if CONFIG_N_LOG_MODE == N_WW_LOG_MODE_ENCODE
    test_level_filter();
    test_module_mask();
    test_level_threshold();
#endif
#if defined(CONFIG_N_LOG_BACKEND_EXT_MEM)
    test_ext_flush();
    test_ext_level_filter();
    test_ext_resume();
#if CONFIG_N_LOG_MODE == N_WW_LOG_MODE_ENCODE
    test_boot_record();
#endif
#if CONFIG_N_LOG_EXT_FULL == N_WW_LOG_EXT_FULL_FREEZE
    test_ext_full_freeze();
#endif
#if (CONFIG_N_LOG_EXT_FULL == N_WW_LOG_EXT_FULL_ERASE) && \
    (CONFIG_N_LOG_MODE == N_WW_LOG_MODE_ENCODE)
    test_ext_full_erase();
#endif
#if defined(CONFIG_N_LOG_EXT_FLUSH_MARKER)
    test_ext_flush_marker();
#endif
#endif
    /* leave the log in a clean state for whatever runs next */
    n_ww_log_set_level_threshold(CONFIG_N_LOG_RUNTIME_THRESHOLD);
    log_ram_init(WW_TRUE);
#else
    ww_printf("  (RAM backend off -> storage self-tests skipped)\n");
#endif

    ww_printf("\n========== self-test: %d passed, %d failed ==========\n",
              s_pass, s_fail);
    return s_fail;
}
