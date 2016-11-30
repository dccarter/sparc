//
// Created by dc on 10/7/16.
//

#include <kore/kore.h>
#include "sut.h"
#include "util.h"

int
sut_run_suite(sut_suite_t* suite)
{
    int ret             = 0;
    uint32_t    fres    = 0;
    time_t  sstart;
    time_t  fstart;
    time_t  tstart;

    sut_fixture_t   **f;
    sut_test_t      *t;
    sstart  = sparc::datetime::now(MICROSECONDS);
    f = suite->fixtures;
    while (*f && (*f)->name)
    {
        ret = 0;

        fstart = sparc::datetime::now(MICROSECONDS);
        t = (sut_test_t *) (*f)->tests;

        if ((*f)->setup) (*f)->setup();

        while (t && t->name)
        {
            if (t->attrs & SUT_SKIP)
            {
                t->result = SUT_SKIPPED;
                t++;
                continue;
            }

            tstart = sparc::datetime::now(MICROSECONDS);

            if(t->setup) t->setup();

            t->body(t);

            if (t->teardown) t->teardown();

            t->duration_us = sparc::datetime::now(MICROSECONDS) - tstart;
            if (t->result == SUT_FAILED && t->attrs & SUT_WAIVE)
                t->result = SUT_WAIVED;
            ret = ret? ret : (t->result==SUT_FAILED? 1 : 0);
            t++;
        }
        (*f)->result = (uint32_t)ret;

        // teardown the fixture if teardown function is provided
        if ((*f)->teardown) (*f)->teardown();

        (*f)->duration_us = sparc::datetime::now(MICROSECONDS) - fstart;
        fres = fres? fres : (ret? 1 : 0);
        f++;
    }
    suite->result = fres;
    suite->duration_ms = sparc::datetime::now(MICROSECONDS) - sstart;
    return ret;
}

void
sut_report_console(sut_suite_t* suite)
{
    sut_fixture_t **f;
    sut_test_t     *t;
    char           *tab = "    ";
    uint32_t        passed = 0, failed = 0, skipped = 0, waived = 0;

    if (suite->result)
        sparc::cprint(CRED, "Suite:%s, Status: FAILED, Duration = %lld us\n\n", suite->name, suite->duration_ms);
    else
        sparc::cprint(CGREEN, "Suite:%s Status: PASSED, Duration = %lld us\n\n", suite->name, suite->duration_ms);

    f = suite->fixtures;
    while (*f && (*f)->name)
    {
        if ((*f)->result)
            sparc::cprint(CRED, "%sFixture:%s, Status: FAILED, Duration = %lld us\n", tab, (*f)->name, (*f)->duration_us);
        else
            sparc::cprint(CGREEN, "%sFixture:%s, Status: PASSED, Duration = %lld us\n", tab, (*f)->name, (*f)->duration_us);

        t = (sut_test_t *) (*f)->tests;
        while (t && t->name)
        {
            if (t->result ==  SUT_FAILED)
            {
                failed++;
                sparc::cprint(CRED, "%s%sFAILED: %s Duration %lld us\n", tab, tab, t->name, t->duration_us);
                sparc::cprint(CDEFAULT, "%s%s%s%s - %s:%s:%d\n", tab, tab, tab, t->msg, t->file, t->func, t->line);
            }
            else if (t->result == 0)
            {
                passed++;
                sparc::cprint(CGREEN, "%s%sPASSED: %s Duration %lld us\n", tab, tab, t->name, t->duration_us);
            }
            else if (t->result == SUT_SKIPPED)
            {
                skipped++;
                sparc::cprint(CYELLOW, "%s%sSKIPPED: %s\n", tab, tab, t->name);
            }
            else if (t->result == SUT_WAIVED)
            {
                waived++;
                sparc::cprint(CCYAN, "%s%sWAIVED: %s Duration %lld us\n", tab, tab, t->name, t->duration_us);
                sparc::cprint(CDEFAULT, "%s%s%s%s - %s:%s:%d\n", tab, tab, tab, t->msg, t->file, t->func, t->line);
            }

            t++;
        }
        f++;
    }
    sparc::cprint(CDEFAULT, "\nSummary: Passed=%u, Skipped=%u, Waived=%u, Failed=%d\n", passed, skipped, waived, failed);
}

JsonNode*
sut_report_json(sut_suite_t *suite)
{
    uint32_t        passed = 0, failed = 0, skipped = 0, waived = 0;
    JsonNode *jsuite,
             *jfx, *jfxs,
             *jt, *jts,
             *jsum;
    sut_fixture_t **f;
    sut_test_t     *t;

    jsuite = json_mkobject();
    json_append_member(jsuite, "name", json_mkstring(suite->name));
    json_append_member(jsuite, "status", json_mknumber(suite->result));
    json_append_member(jsuite, "durationMS", json_mknumber(suite->duration_ms));

    // array of fixtures
    jfxs = json_mkarray();
    f = suite->fixtures;
    while ((*f) && (*f)->name)
    {
        jfx = json_mkobject();
        json_append_member(jfx, "name", json_mkstring((*f)->name));
        json_append_member(jfx, "durationUS", json_mknumber((*f)->duration_us));
        json_append_member(jfx, "status", json_mknumber((*f)->result));

        // array of tests
        jts = json_mkarray();
        t = (sut_test_t *) (*f)->tests;
        while ( t && t->name)
        {
            jt = json_mkobject();
            json_append_member(jt, "name", json_mkstring(t->name));
            json_append_member(jt, "status", json_mknumber(t->result));
            if (t->result != SUT_SKIPPED)
            {
                json_append_member(jt, "durationUS", json_mknumber(t->duration_us));
                if (t->result)
                {
                    if (t->result == SUT_FAILED) failed++;
                    else waived++;

                    json_append_member(jt, "msg", json_mkstring(t->msg));
                    json_append_member(jt, "func", json_mkstring(t->func));
                    json_append_member(jt, "file", json_mkstring(t->file));
                    json_append_member(jt, "line", json_mknumber(t->line));
                } else passed++;
            } else skipped++;

            json_append_element(jts, jt);
            t++;
        }
        json_append_member(jfx, "tests", jts);
        json_append_element(jfxs, jfx);
        f++;
    }
    json_append_member(jsuite, "fixtures", jfxs);
    jsum = json_mkobject();
    json_append_member(jsum, "passed", json_mknumber(passed));
    json_append_member(jsum, "failed", json_mknumber(failed));
    json_append_member(jsum, "waived", json_mknumber(waived));
    json_append_member(jsum, "skipped", json_mknumber(skipped));
    json_append_member(jsuite, "summary", jsum);

    return jsuite;
}