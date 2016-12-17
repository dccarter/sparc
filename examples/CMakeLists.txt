CMAKE_MINIMUM_REQUIRED(VERSION 3.2)

macro(Sparc_Example name SOURCES)
    ADD_EXECUTABLE(${name} ${SOURCES})
    TARGET_LINK_LIBRARIES(${name} sparc-dev pthread dl ssl crypto)
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets/${name})
        TARGET_COMPILE_DEFINITIONS(${name} PUBLIC -D${name}_PUBLIC=\"/${name}/www\")
    endif()
    SET_TARGET_PROPERTIES(${name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/examples/)
    TARGET_COMPILE_DEFINITIONS(${name} PUBLIC -DSPARC_DOMAIN_NAME=\"sparc.suilteam.com\")
endmacro()

Sparc_Example(hello hello.cpp)
Sparc_Example(basic basic.cpp)
Sparc_Example(crud crud.cpp)
Sparc_Example(sauth sauth.cpp)
Sparc_Example(serve serve.cpp)
Sparc_Example(misc misc.cpp)


# Install the assets directory
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets)
    INSTALL( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets
             DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/examples)
endif()
