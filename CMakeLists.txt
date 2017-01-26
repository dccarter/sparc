CMAKE_MINIMUM_REQUIRED(VERSION 3.2)
PROJECT(sparc C CXX)

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

OPTION(SPARC_TESTS      "Build sparc unit tests" ON)
OPTION(SPARC_EXAMPLES   "Build sparc examples" ON)
OPTION(SPARC_PQ_V96     "Build PgSQL version will default to 9.6" OFF)

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
        base/kore/worker.c
        base/kore/pgsql.c)

FILE(GLOB SPARC_SOURCES sparc/*.cpp sparc/*.c)
FILE(GLOB SPARC_DETAIL_SOURCES sparc/detail/*.cpp sparc/detail/*.c)
FILE(GLOB EXT_BASE_SOURCES    base/*.c base/*.cpp)

SET(SOURCE_FILES
        ${KORE_SOURCES}
        ${EXT_BASE_SOURCES}
        ${SPARC_DETAIL_SOURCES}
        ${SPARC_SOURCES})

SET(PGSQL_VERSION 9.5)
if (SPARC_PQ_V96)
    SET(PGSQL_VERSION 9.6)
endif()
FIND_PACKAGE(PostgreSQL ${PGSQL_VERSION} REQUIRED)

ADD_DEFINITIONS("-DKORE_NO_TLS -DTASKS -DKORE_DEBUG -DKORE_SINGLE_BINARY -DKORE_USE_PGSQL=1 -D_GNU_SOURCE=1")
INCLUDE_DIRECTORIES(base/ base/kore sparc sparc/detail ${PostgreSQL_INCLUDE_DIRS})
ADD_LIBRARY(sparc-dev STATIC ${SOURCE_FILES})
TARGET_LINK_LIBRARIES(sparc-dev pthread dl ssl crypto ${PostgreSQL_LIBRARIES})

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
