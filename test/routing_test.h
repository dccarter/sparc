//
// Created by dc on 12/21/16.
//

#ifndef SPARC_ROUTING_TEST_H
#define SPARC_ROUTING_TEST_H

#include "sut.h"

#ifdef __cplusplus
extern "C" {
#endif

/***
 * @test tests routing dynamic routes
 */
SUT_TEST(RoutingDynamicRoot);

/***
 * @test tests routing dynamic routes
 */
SUT_TEST(RoutingDynamicMethods);

/***
 * @test tests routing dynamic routes
 */
SUT_TEST(RoutingDynamicParamters);

#ifdef __cplusplus
}
#endif

#endif //SPARC_ROUTING_TEST_H
