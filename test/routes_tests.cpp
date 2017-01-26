//
// Created by dc on 11/29/16.
//

#include <sparc.h>
#include "router.h"
#include "routes_test.h"

using namespace sparc;

SUT_TEST(RouterCreateTest)
{
    detail::Router r;
    SUT_ASSERT_NEQ(r.root_, NULL);
    SUT_ASSERT_EQ(r.root_->tag, '/');
    SUT_ASSERT_EQ(r.root_->route, NULL);
    SUT_ASSERT_TRUE(TAILQ_EMPTY(&r.root_->children));
    // the router should be freed by default
}

SUT_TEST(RouteCreateTest)
{
    detail::Route r;
    SUT_ASSERT_EQ(r.prefix, NULL);
    SUT_ASSERT_EQ(r.methods, 0);
    SUT_ASSERT_TRUE(TAILQ_EMPTY(&r.handlers));
}

SUT_TEST(RouteAddHandlerTest)
{
    detail::Route r;
    detail::RouteHandler *rh;
    // add an empty handler
    rh = r.add(POST, NULL, 0);
    SUT_ASSERT_NEQ(rh, NULL);
    SUT_ASSERT_EQ(rh->m, POST);
    SUT_ASSERT_EQ(rh->params, NULL);
    SUT_ASSERT_EQ(rh->nparams, 0);
    SUT_ASSERT_EQ(rh->h, NULL);
    // methods should have been updated
    SUT_ASSERT_EQ(r.methods, (1<<POST));

    /** there is no protection against duplicate in router,
     *  protection should be implemented by caller by using the r.find(...) method
     */

    // add another route
    rh = r.add(UPDATE, NULL, 0);
    SUT_ASSERT_NEQ(rh, NULL);
    SUT_ASSERT_EQ(rh->m, UPDATE);
    SUT_ASSERT_EQ(rh->params, NULL);
    SUT_ASSERT_EQ(rh->nparams, 0);
    SUT_ASSERT_EQ(rh->h, NULL);
    // methods should have been updated
    SUT_ASSERT_TRUE((r.methods & (1<<POST)));
}

SUT_TEST(RouteFindHandlerTest)
{
    detail::Route r;
    detail::RouteHandler *rh;
    // add several handlers
    r.add(POST, NULL, 0);
    r.add(GET, NULL, 0);
    r.add(PUT, NULL, 1);
    r.add(POST, NULL, 4);   // route like /route/:id1/:id2:/id3/:id4 (params won't be NULL)

    // POST handler with 0 handlers
    rh = r.find(POST, 0);
    SUT_ASSERT_NEQ(rh, NULL);
    SUT_ASSERT_EQ(rh->m, POST);
    SUT_ASSERT_EQ(rh->nparams, 0);
    // POST handler with 4 parameters
    rh = r.find(POST, 4);
    SUT_ASSERT_NEQ(rh, NULL);
    SUT_ASSERT_EQ(rh->m, POST);
    SUT_ASSERT_EQ(rh->nparams, 4);
    // POST handler with 1 parameter
    rh = r.find(POST, 1);
    SUT_ASSERT_EQ(rh, NULL);

    // PUT handler with 1 handler
    rh = r.find(PUT, 1);
    SUT_ASSERT_NEQ(rh, NULL);
    SUT_ASSERT_EQ(rh->m, PUT);
    SUT_ASSERT_EQ(rh->nparams, 1);

    // GET handler with 0 handlers
    rh = r.find(GET, 0);
    SUT_ASSERT_NEQ(rh, NULL);
    SUT_ASSERT_EQ(rh->m, GET);
    SUT_ASSERT_EQ(rh->nparams, 0);

    // UPDATE handler which doesn't exit
    rh = r.find(UPDATE, 0);
    SUT_ASSERT_EQ(rh, NULL);

    // verify set methods
    SUT_ASSERT_EQ(r.methods, ((1<<GET)|(1<<POST)|(1<<PUT)));
}

#define cstrlen(str) sizeof(str)-1

SUT_TEST(RouterAddTest)
{
    detail::Route           *rr1, *rr2;
    detail::RouteHandler    *rh;
    detail::Router::Node    *n, *n1, *n2;

    // test private add function which deals trie data structure
    {
        detail::Router          r;
        detail::Router::Node    *root = r.root_;
        // test private add implementation
        n = r.add(root, "/", cstrlen("/"));
        SUT_ASSERT_EQ(root, n);
        SUT_ASSERT_EQ(r.depth, cstrlen("/"));

        // Should fail is ally cannot be added to /
        n = r.add(root, "ally", cstrlen("ally"));
        SUT_ASSERT_EQ(r.depth, 1);
        SUT_ASSERT_EQ(n, NULL);

        // depth should be equal to string with maximum length
        r.add(root, "/home/", cstrlen("/home/"));
        SUT_ASSERT_EQ(r.depth, cstrlen("/home/"));
        r.add(root, "/home", cstrlen("/home"));
        SUT_ASSERT_EQ(r.depth, cstrlen("/home/"));
        r.add(root, "/home/users/", cstrlen("/home/users"));
        SUT_ASSERT_EQ(r.depth, cstrlen("/home/users"));
    }

    // test the actual add which encapsulates checking duplicates
    {
        detail::Router          r;
        rr1 = r.add(POST, "/", NULL, NULL, NULL);
        SUT_ASSERT_NEQ(rr1, NULL);
        SUT_ASSERT_TRUE(rr1->methods&(1<<POST));
        SUT_ASSERT_STRING_EQ(rr1->prefix, "/");

        rr2 = r.add(GET, "/", NULL, NULL, NULL);
        SUT_ASSERT_NEQ(rr2, NULL);
        SUT_ASSERT_TRUE(rr2->methods&(1<<GET));
        SUT_ASSERT_STRING_EQ(rr2->prefix, "/");

        // and most importantly rr1 = rr2
        SUT_ASSERT_EQ(rr1, rr2);
        SUT_ASSERT_EQ(rr1->methods, rr2->methods);

        // duplicates are not allowed
        rr1 = r.add(GET, "/user/", NULL, NULL, NULL);
        SUT_ASSERT_NEQ(rr1, NULL);
        SUT_ASSERT_TRUE(rr1->methods&(1<<GET));
        SUT_ASSERT_STRING_EQ(rr1->prefix, "/user/");

        rr2 = r.add(GET, "/user/", NULL, NULL, NULL);
        SUT_ASSERT_EQ(rr2, NULL);

        // different parameter configurations are allowed
        rr2 = r.add(GET, "/user/:name", NULL, NULL, NULL);
        SUT_ASSERT_NEQ(rr2, NULL);
        SUT_ASSERT_TRUE(rr2->methods&(1<<GET));
        SUT_ASSERT_STRING_EQ(rr2->prefix, "/user/");
        // and most importantly it's still the same route but different configuration
        SUT_ASSERT_EQ(rr1, rr2);
        SUT_ASSERT_EQ(rr1->methods, rr2->methods);

        rr1 = r.add(GET, "/user/:name/:age", NULL, NULL, NULL);
        SUT_ASSERT_NEQ(rr1, NULL);
        SUT_ASSERT_TRUE(rr1->methods&(1<<GET));
        SUT_ASSERT_STRING_EQ(rr1->prefix, "/user/");
        // and most importantly it's still the same route but different configuration
        SUT_ASSERT_EQ(rr1, rr2);
        SUT_ASSERT_EQ(rr1->methods, rr2->methods);

        // match must satisify method and parameter, thus is valid, same route, different handlers
        rr2 = r.add(POST, "/user/:name/:age", NULL, NULL, NULL);
        SUT_ASSERT_NEQ(rr2, NULL);
        SUT_ASSERT_TRUE(rr2->methods&(1<<POST));
        SUT_ASSERT_STRING_EQ(rr2->prefix, "/user/");
        // and most importantly it's still the same route but different configuration
        SUT_ASSERT_EQ(rr1, rr2);
        SUT_ASSERT_EQ(rr1->methods, rr2->methods);

        // although trying to duplicate handler will result in an error
        rr1 = r.add(POST, "/user/:name/:age", NULL, NULL, NULL);
        SUT_ASSERT_EQ(rr1, NULL);
        rr2 = r.add(GET, "/user/:name", NULL, NULL, NULL);
        SUT_ASSERT_EQ(rr2, NULL);
    }
}