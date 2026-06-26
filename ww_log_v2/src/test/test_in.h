/**
 * @file test_in.h
 * @brief Public entry for the ww_log_v2 self-test suite (TEST module).
 */

#ifndef __TEST_IN_H__
#define __TEST_IN_H__

/**
 * @brief Run all log self-tests and print a PASS/FAIL report.
 * @return number of FAILED checks (0 == all passed). Safe to call on the
 *         firmware target too; it only touches the log RAM region and (when the
 *         EXT_MEM backend is on) the LOG partition.
 */
int test_log_run_all(void);

#endif /* __TEST_IN_H__ */
