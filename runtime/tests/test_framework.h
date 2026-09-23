/*
 * Minimal, dependency-free unit test harness for the runtime's C code.
 *
 * hace-corba intentionally avoids pulling in a third-party test
 * framework (e.g. Unity) as a build dependency: for a DAL C-oriented
 * codebase it's preferable to know exactly what's in the toolchain, and
 * a harness this small (no dynamic allocation, no network fetch) is
 * easy to review in full. Each test .c file defines its own main() that
 * calls HACE_RUN_TEST for each test function and finishes with
 * HACE_TEST_REPORT().
 */
#ifndef HACE_TEST_FRAMEWORK_H
#define HACE_TEST_FRAMEWORK_H

#include <stdio.h>

static int hace_test_failures = 0;
static int hace_test_count = 0;

#define HACE_RUN_TEST(fn)                                                    \
    do {                                                                     \
        hace_test_count++;                                                   \
        printf("  %-48s", #fn);                                             \
        if (fn()) {                                                          \
            printf("PASS\n");                                               \
        } else {                                                             \
            printf("FAIL\n");                                               \
            hace_test_failures++;                                            \
        }                                                                    \
    } while (0)

#define HACE_TEST_REPORT()                                                   \
    do {                                                                     \
        printf("%d/%d tests passed\n", hace_test_count - hace_test_failures, \
               hace_test_count);                                             \
        return (hace_test_failures == 0) ? 0 : 1;                            \
    } while (0)

#define HACE_ASSERT_TRUE(cond)                                               \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("\n    assertion failed at %s:%d: %s\n", __FILE__,        \
                   __LINE__, #cond);                                         \
            return 0;                                                       \
        }                                                                    \
    } while (0)

#define HACE_ASSERT_EQ_INT(expected, actual)                                 \
    do {                                                                     \
        long long e_ = (long long)(expected);                               \
        long long a_ = (long long)(actual);                                 \
        if (e_ != a_) {                                                      \
            printf("\n    assertion failed at %s:%d: expected %lld, got "    \
                   "%lld\n",                                                 \
                   __FILE__, __LINE__, e_, a_);                             \
            return 0;                                                       \
        }                                                                    \
    } while (0)

#endif /* HACE_TEST_FRAMEWORK_H */
