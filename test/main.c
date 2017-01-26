//
// Created by dc on 11/29/16.
//
#include <kore/kore.h>
#include "sut.h"
#include "routes_test.h"
#include "routing_test.h"

int main(int argc, char *argv[])
{
    kore_mem_init();

    SUT_FIXTURE(Router) {
        SUT_TEST_ADD(RouterCreateTest, SUT_DEFAULT),
        SUT_TEST_ADD(RouteCreateTest, SUT_DEFAULT),
        SUT_TEST_ADD(RouteAddHandlerTest, SUT_DEFAULT),
        SUT_TEST_ADD(RouteFindHandlerTest, SUT_DEFAULT),
        SUT_TEST_ADD(RouterAddTest, SUT_DEFAULT),
        SUT_FIXTURE_TERM(Router)
    };

    SUT_FIXTURE(Routing) {
        SUT_TEST_ADD(RoutingDynamicRoot, SUT_DEFAULT),
        SUT_TEST_ADD(RoutingDynamicMethods, SUT_DEFAULT),
        SUT_TEST_ADD(RoutingDynamicParamters, SUT_DEFAULT),
        SUT_FIXTURE_TERM(Routing)
    };

    SUT_SUITE(SparcSuite) {
        SUT_FIXTURE_ADD(Router),
        SUT_FIXTURE_ADD(Routing),
        SUT_SUIT_TERM(SparcSuite)
    };

    sut_run_suite(&SparcSuite);
    sut_report_console(&SparcSuite);
}
