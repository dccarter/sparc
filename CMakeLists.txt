CMAKE_MINIMUM_REQUIRED(VERSION 3.5)
PROJECT(sparc C CXX)

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

OPTION(SPARC_TESTS      "Build sparc unit tests" ON)
OPTION(SPARC_EXAMPLES   "Build sparc examples" ON)

SET(SPARC_FLAGS "${SPARC_FLAGS} -Wmissing-declarations -Wshadow -Wpointer-arith")
SET(SPARC_FLAGS "${SPARC_FLAGS} -Wsign-compare -Wno-pedantic -rdynamic")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SPARC_FLAGS} -std=c++14 -Wno-write-strings")
SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${SPARC_FLAGS} -Wstrict-prototypes -Wmissing-prototypes")

SET(KORE_SOURCES
        base/kore/accesslog.c
        base/kore/auth.c
        base/kore/buf.c
        base/kore/connection.c
        base/kore/domain.c
        base/kore/http.c
        base/kore/keymgr.c
        base/kore/linux.c
        base/kore/mem.c
        base/kore/module.c
        base/kore/msg.c
        base/kore/net.c
        base/kore/pool.c
        base/kore/tasks.c
        base/kore/timer.c
        base/kore/utils.c
        base/kore/validator.c
        base/kore/websocket.c
        base/kore/worker.c)


FILE(GLOB SPARC_SOURCES sparc/*.cpp sparc/*.c)
FILE(GLOB SPARC_DETAIL_SOURCES sparc/detail/*.cpp sparc/detail/*.c)
FILE(GLOB EXT_BASE_SOURCES    base/*.c base/*.cpp)

SET(SOURCE_FILES
        ${KORE_SOURCES}
        ${EXT_BASE_SOURCES}
        ${SPARC_DETAIL_SOURCES}
        ${SPARC_SOURCES})

ADD_DEFINITIONS("-DKORE_NO_TLS -DTASKS -DKORE_DEBUG -DKORE_SINGLE_BINARY -D_GNU_SOURCE=1")
INCLUDE_DIRECTORIES(base/ base/kore sparc sparc/detail)

ADD_LIBRARY(sparc-dev STATIC ${SOURCE_FILES})
TARGET_LINK_LIBRARIES(sparc-dev pthread dl ssl crypto)

ADD_EXECUTABLE(sparc main.cpp)
TARGET_LINK_LIBRARIES(sparc sparc-dev pthread dl ssl crypto)

if (SPARC_EXAMPLES)
    ADD_SUBDIRECTORY(examples)
endif()

if (SPARC_TESTS)
    FILE(GLOB SPARC_TEST_SOURCES test/*.c test/*.cpp)
    INCLUDE_DIRECTORIES(test)
    ADD_EXECUTABLE(sparc_test ${SPARC_TEST_SOURCES})
    TARGET_LINK_LIBRARIES(sparc_test sparc-dev pthread dl ssl crypto)
endif()