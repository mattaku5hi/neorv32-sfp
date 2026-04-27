#pragma once


#include <stdio.h>
#include <string.h>


#define TEST_MAX 64


// Tiny test harness: no external framework needed.  Each test function is
// registered by TEST(name) and appended to a global table; the runner in
// main() iterates and prints results.


typedef struct
{
    const char *name;
    int       (*fn)(void);  // returns 0 on success, non-zero on failure
} test_case_t;

extern test_case_t g_tests[TEST_MAX];
extern int         g_tests_n;

void test_register(const char *name, int (*fn)(void));

#define TEST(name)                                                   \
    static int name(void);                                           \
    __attribute__((constructor)) static void reg_##name(void) {      \
        test_register(#name, name);                                  \
    }                                                                \
    static int name(void)

#define ASSERT_TRUE(cond) do {                                        \
        if (!(cond)) {                                                \
            fprintf(stderr, "  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                 \
        }                                                             \
    } while (0)

#define ASSERT_EQ_INT(a, b) do {                                      \
        long long _a = (long long)(a), _b = (long long)(b);           \
        if (_a != _b) {                                               \
            fprintf(stderr, "  [FAIL] %s:%d: %s=%lld != %s=%lld\n",   \
                    __FILE__, __LINE__, #a, _a, #b, _b);              \
            return 1;                                                 \
        }                                                             \
    } while (0)

#define ASSERT_EQ_STR(a, b) do {                                      \
        const char *_a = (a), *_b = (b);                              \
        if (strcmp(_a, _b) != 0) {                                    \
            fprintf(stderr, "  [FAIL] %s:%d: %s=\"%s\" != %s=\"%s\"\n",\
                    __FILE__, __LINE__, #a, _a, #b, _b);              \
            return 1;                                                 \
        }                                                             \
    } while (0)

#define ASSERT_EQ_MEM(a, b, n) do {                                   \
        if (memcmp((a), (b), (n)) != 0) {                             \
            fprintf(stderr, "  [FAIL] %s:%d: memcmp(%s, %s, %zu) != 0\n", \
                    __FILE__, __LINE__, #a, #b, (size_t)(n));         \
            return 1;                                                 \
        }                                                             \
    } while (0)

