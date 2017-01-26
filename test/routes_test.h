//
// Created by dc on 11/29/16.
//

#ifndef SPARC_ROUTES_TEST_H
#define SPARC_ROUTES_TEST_H

#include "sut.h"

#ifdef __cplusplus
extern "C" {
#endif

/***
 * @test tests the creation of a router, each created
 * router should by default have a node "/" with empty
 * handler
 */
SUT_TEST(RouterCreateTest);

/**
 * @test a router is made up of routes which have different kinds of.
 * this will test the creation of a route
 */
SUT_TEST(RouteCreateTest);

/**
 * @test a router is made up of routes which have different kinds of
 * handlers. This will test that adding an entry to the route works as expected
 */
SUT_TEST(RouteAddHandlerTest);

/**
 * @test a router is made up of routes which have different kinds of
 * handlers. This will test that finding an entry on the handler works as expected
 */
SUT_TEST(RouteFindHandlerTest);

/**
 * @test tests adding route's and handlers to the router
 */
SUT_TEST(RouterAddTest);

#ifdef __cplusplus
}
#endif

#endif //SPARC_ROUTES_TEST_H
