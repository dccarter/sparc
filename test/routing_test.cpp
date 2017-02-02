//
// Created by dc on 12/21/16.
//
#include <detail/app.h>
#include "kore.h"
#include "routing_test.h"

using namespace sparc;
using namespace detail;

struct route_match {
    c_string        tokenized;
    c_string        tokens[16];
    c_string        params[10];
    size_t          ntoks;
    ~route_match() {
        if (tokenized)
            kore_free(tokenized);
    }
};

static inline RouteHandler*
find_route(Method m, cc_string route, Router &r, route_match& rm)
{
    rm.ntoks = sizeof(rm.tokens);
    return r.find(m, route, &rm.tokenized, rm.tokens, &rm.ntoks, (c_string **)&rm.params);
}

SUT_TEST(RoutingDynamicRoot)
{
    Router          r;
    Route           *rr;
    RouteHandler    *rh;
    rr = r.add(GET, "/", NULL, NULL, NULL, NULL);
    SUT_ASSERT_NEQ(rr, NULL);
    // we try different combinations to match the root
    {
        route_match     rm = {0};
        rh = find_route(GET, "/", r, rm);
        SUT_ASSERT_NEQ(rh, NULL);
        SUT_ASSERT_EQ(rh->nparams, 0);
        SUT_ASSERT_STRING_EQ(rh->prefix, "/");
    }
    // we try different combinations to match the root
    {
        route_match     rm = {0};
        rh = find_route(GET, "/test", r, rm);
        SUT_ASSERT_EQ(rh, NULL);
    }
}

SUT_TEST(RoutingDynamicMethods)
{
    Router          r;
    Route           *rr;
    RouteHandler    *rh;
    rr = r.add(GET, "/", NULL, NULL, NULL, NULL);
    SUT_ASSERT_NEQ(rr, NULL);
    rr = r.add(POST, "/", NULL, NULL, NULL, NULL);
    SUT_ASSERT_NEQ(rr, NULL);
    rr = r.add(DELETE, "/", NULL, NULL, NULL, NULL);

    SUT_ASSERT_NEQ(rr, NULL);
    {
        route_match     rm = {0};
        rh = find_route(GET, "/", r, rm);
        SUT_ASSERT_NEQ(rh, NULL);
        SUT_ASSERT_EQ(rh->nparams, 0);
        SUT_ASSERT_STRING_EQ(rh->prefix, "/");
    }
    {
        route_match     rm = {0};
        rh = find_route(POST, "/", r, rm);
        SUT_ASSERT_NEQ(rh, NULL);
        SUT_ASSERT_EQ(rh->nparams, 0);
        SUT_ASSERT_STRING_EQ(rh->prefix, "/");
    }
    {
        route_match     rm = {0};
        rh = find_route(DELETE, "/", r, rm);
        SUT_ASSERT_NEQ(rh, NULL);
        SUT_ASSERT_EQ(rh->nparams, 0);
        SUT_ASSERT_STRING_EQ(rh->prefix, "/");
    }

    rr = r.add(GET, "/:name", NULL, NULL, NULL, NULL);
    SUT_ASSERT_NEQ(rr, NULL);
    rr = r.add(POST, "/:name", NULL, NULL, NULL, NULL);
    SUT_ASSERT_NEQ(rr, NULL);
    rr = r.add(DELETE, "/:name", NULL, NULL, NULL, NULL);

    SUT_ASSERT_NEQ(rr, NULL);
    {
        route_match     rm = {0};
        rh = find_route(GET, "/Carter", r, rm);
        SUT_ASSERT_NEQ(rh, NULL);
        SUT_ASSERT_EQ(rh->nparams, 1);
        SUT_ASSERT_STRING_EQ(rh->prefix, "/");
    }
    {
        route_match     rm = {0};
        rh = find_route(POST, "/Carter", r, rm);
        SUT_ASSERT_NEQ(rh, NULL);
        SUT_ASSERT_EQ(rh->nparams, 1);
        SUT_ASSERT_STRING_EQ(rh->prefix, "/");
    }
    {
        route_match     rm = {0};
        rh = find_route(DELETE, "/Carter", r, rm);
        SUT_ASSERT_NEQ(rh, NULL);
        SUT_ASSERT_EQ(rh->nparams, 1);
        SUT_ASSERT_STRING_EQ(rh->prefix, "/");
    }
    {
        route_match     rm = {0};
        rh = find_route(PUT, "/Carter", r, rm);
        SUT_ASSERT_EQ(rh, NULL);
    }
}

SUT_TEST(RoutingDynamicParamters)
{
    Router          r;
    Route           *rr;
    RouteHandler    *rh;

    rr = r.add(GET, "/params/:param1/:param2", NULL, NULL, NULL, NULL);
    SUT_ASSERT_NEQ(rr, NULL);
    {
        route_match     rm = {0};
        rh = find_route(GET, "/params/value1/value2", r, rm);
        SUT_ASSERT_NEQ(rh, NULL);
        SUT_ASSERT_EQ(rh->nparams, 2);
        SUT_ASSERT_STRING_EQ(rh->prefix, "/params/");
    }
    {
        route_match     rm = {0};
        rh = find_route(GET, "/params/string/5656/", r, rm);
        SUT_ASSERT_NEQ(rh, NULL);
        SUT_ASSERT_EQ(rh->nparams, 2);
        SUT_ASSERT_STRING_EQ(rh->prefix, "/params/");
    }
    {
        route_match     rm = {0};
        rh = find_route(GET, "/params/value1/value2/value3", r, rm);
        SUT_ASSERT_EQ(rh, NULL);
    }
    {
        route_match     rm = {0};
        rh = find_route(GET, "/params//value2", r, rm);
        SUT_ASSERT_EQ(rh, NULL);
    }

    rr = r.add(GET, "/params/:param1/:param2/:param3/:param4/:param5/:param6", NULL, NULL, NULL, NULL);
    SUT_ASSERT_NEQ(rr, NULL);
    {
        route_match     rm = {0};
        rh = find_route(GET, "/params/1/2/3/4/5/6", r, rm);
        SUT_ASSERT_NEQ(rh, NULL);
        SUT_ASSERT_EQ(rh->nparams, 6);
        SUT_ASSERT_STRING_EQ(rh->prefix, "/params/");
    }

    {
        route_match     rm = {0};
        rh = find_route(GET, "/params/1/2/3/4/5/6/7", r, rm);
        SUT_ASSERT_EQ(rh, NULL);
    }
    {
        route_match     rm = {0};
        rh = find_route(GET, "/params/1/2/3/", r, rm);
        SUT_ASSERT_EQ(rh, NULL);
    }
    {
        route_match rm = {0};
        rh = find_route(GET, "/params/1/2/3/4/5/", r, rm);
        SUT_ASSERT_EQ(rh, NULL);
    }

    rr = r.add(GET, "/params/:param1", NULL, NULL, NULL, NULL);
    SUT_ASSERT_NEQ(rr, NULL);
    {
        route_match     rm = {0};
        rh = find_route(GET, "/params////", r, rm);
        SUT_ASSERT_EQ(rh, NULL);
    }
}

