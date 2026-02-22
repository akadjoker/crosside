project(runner)
cmake_policy(SET CMP0072 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

file(GLOB LIBBU_RUNTIME_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/libbu/src/*.cpp")
list(FILTER LIBBU_RUNTIME_SOURCES EXCLUDE REGEX ".*/compiler\\.cpp$")
list(FILTER LIBBU_RUNTIME_SOURCES EXCLUDE REGEX ".*/compiler_expressions\\.cpp$")
list(FILTER LIBBU_RUNTIME_SOURCES EXCLUDE REGEX ".*/compiler_statements\\.cpp$")

add_library(libbu_runtime STATIC ${LIBBU_RUNTIME_SOURCES})
set_target_properties(libbu_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(libbu_runtime PUBLIC
    ${CMAKE_SOURCE_DIR}/libbu/include
    ${CMAKE_SOURCE_DIR}/libbu/src
)
target_compile_definitions(libbu_runtime PUBLIC BU_RUNTIME_ONLY=1)

if (UNIX AND NOT APPLE)
    target_link_libraries(libbu_runtime PRIVATE miniz pthread)
endif()

file(GLOB RUNNER_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/main/src/*.cpp")
file(GLOB GRAPHICS_RUNTIME_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/graphics/src/*.cpp")

add_library(graphics_runtime STATIC ${GRAPHICS_RUNTIME_SOURCES})
target_include_directories(graphics_runtime PUBLIC
    ${CMAKE_SOURCE_DIR}/graphics/include
    ${CMAKE_SOURCE_DIR}/graphics/src
)
target_link_libraries(graphics_runtime PUBLIC libbu_runtime raylib poly2tri)

if (UNIX)
    target_link_libraries(graphics_runtime PUBLIC m)
endif()

add_executable(runner ${RUNNER_SOURCES})
target_include_directories(runner PUBLIC
    ${CMAKE_SOURCE_DIR}/main/include
    ${CMAKE_SOURCE_DIR}/main/src
)
target_compile_definitions(runner PRIVATE BU_RUNNER_ONLY=1)
target_link_libraries(runner miniz poly2tri raylib box2d libbu_runtime graphics_runtime)

if (WIN32)
    target_link_libraries(runner Winmm.lib)
endif()

if (UNIX)
    target_link_libraries(runner m)
endif()
