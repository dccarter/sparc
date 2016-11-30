//
// Created by dc on 10/7/16.
//

#ifndef SUIL_SUT_H
#define SUIL_SUT_H

#include <memory.h>
#include <stdint.h>
#include <time.h>
#include "ccan_json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SUT_DEFAULT =   0x0000,
    SUT_WAIVE   =   0x0001,
    SUT_SKIP    =   0x0002
} sut_attr;

typedef enum sut_result
{
    SUT_PASSED  = 0,
    SUT_FAILED  = 1,
    SUT_SKIPPED = 3,
    SUT_WAIVED  = 4
} sut_result;

typedef struct sut_test sut_test_t;
typedef void (*sut_body)(sut_test_t *__t);
typedef void (*sut_setup)(void);
typedef void (*sut_teardown)(void);

struct sut_test
{
    sut_body        body;
    const char      *name;
    uint32_t        result;
    uint32_t        line;
    const char      *func;
    const char      *file;
    char            *msg;
    uint32_t        attrs;
    time_t          duration_us;
    sut_setup       setup;
    sut_teardown    teardown;
};

typedef struct sut_fixture
{
    const char          *name;
    const sut_test_t    *tests;
    uint32_t            result;
    time_t              duration_us;
    sut_setup           setup;
    sut_teardown        teardown;
} sut_fixture_t;

#define SUT_FIXTURE_ST(fname, setupFn, teardownFn)              \
    static sut_fixture_t fname = {                      \
        .name  = #fname, .tests = NULL, .result = 0,    \
        .setup = setupFn, .teardown = teardownFn };     \
    static sut_test_t __##fname##__tests[] =

#define SUT_FIXTURE(fname)  SUT_FIXTURE_ST(fname, NULL, NULL)

#define SUT_FIXTURE_TERM(fname)  {NULL}}; \
    { (fname).tests = __##fname##__tests;

#define SUT_TEST_ADD_ST(test, attr, setupFN, teardownFN)    \
    { .body = test, .name = #test,  \
      .msg = NULL, .result = 0, .attrs = attr, \
      .line = 0, .func = NULL, .file = NULL, \
      .duration_us = 0, .setup = setupFN, \
      .teardown = teardownFN }

#define SUT_TEST_ADD(test, attr)    SUT_TEST_ADD_ST(test, attr, NULL, NULL)

#define SUT_TEST(test)      void test(sut_test_t *__this)
#define SUT_SETUP(setup)    void setup()

#define sut_assert(message, test)       \
do {                                    \
    if (!(test)) {                      \
        __this->msg = message ;         \
        __this->result = SUT_FAILED;    \
        __this->line = __LINE__;        \
        __this->func = __func__;        \
        __this->file = __FILE__;        \
        return;                         \
    }                                   \
} while (0)

#define SUT_ASSERT_TRUE(cond)       sut_assert(#cond, cond)
#define SUT_ASSERT_FALSE(cond)      sut_assert(#cond, !(cond))
#define SUT_ASSERT_STRING_EQ(expected, result)  \
    sut_assert(#result "==" #expected , strcmp(expected , result)==0)
#define SUT_ASSERT_EQ(expected, result) \
    sut_assert("comparing " #expected " == " #result , (expected)==(result))
#define SUT_ASSERT_NEQ(expected, result) \
    sut_assert("comparing " #expected " == " #result , (expected)!=(result))
#define SUT_ASSERT_BYTES_EQ(expected, result, len) \
    sut_assert("memory compare, size=" #len ", do " #expected " == " #result, \
                0==memcmp((expected), (result), len))

typedef struct sut_suite_t {
    char            *name;
    time_t          duration_ms;
    uint32_t        result;
    sut_fixture_t   **fixtures;
} sut_suite_t;

#define SUT_FIXTURE_ADD(fname) &(fname)

#define SUT_SUITE(sname) \
static sut_suite_t sname = { \
    .name = #sname, .duration_ms = 0, .result = 0 };\
static sut_fixture_t *__##sname##__fixtures[] =

#define SUT_SUIT_TERM(sname) NULL}; \
    { (sname).fixtures = __##sname##__fixtures;

/**
 * runs the given test suite, depending on test settings
 * @param suite the test suite to run
 * @return 1 if all tests passed and -1 if any test failed
 */
int
sut_run_suite(sut_suite_t* suite);
void
sut_report_console(sut_suite_t* suite);
JsonNode*
sut_report_json(sut_suite_t *suite);
#ifdef __cplusplus
};
#endif

#endif //SUIL_SUT_H
