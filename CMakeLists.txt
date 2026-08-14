cmake_minimum_required(VERSION 3.16)
project(ifm_costintel VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Strict compiler policy
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wshadow
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wformat=2
    -Wundef
    -fno-common
    -D_GNU_SOURCE
)

# Optional sanitizer support
option(ENABLE_SANITIZERS "Enable Address and Undefined Behavior Sanitizers" OFF)
if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

# Core include directory
include_directories(include)

# Foundation and Engine library
set(LIB_SOURCES
    src/foundation/fak.c
    src/foundation/arena.c
    src/foundation/diagnostics.c
    src/ingress/stream_adapter.c
    src/ingress/json_decoder.c
    src/ingress/schema_validator.c
    src/ingress/traceability.c
    src/intelligence/rules.c
    src/intelligence/allocation.c
    src/intelligence/aggregation.c
    src/intelligence/variance.c
    src/intelligence/concentration.c
    src/intelligence/anomaly.c
    src/governance/reconciliation.c
    src/governance/fault_engine.c
    src/governance/telemetry.c
    src/governance/output.c
)

add_library(ifm_costintel_lib STATIC ${LIB_SOURCES})
target_include_directories(ifm_costintel_lib PUBLIC include)

# Main executable
add_executable(ifm-costintel src/main.c)
target_link_libraries(ifm-costintel PRIVATE ifm_costintel_lib m)

# Benchmark executable
add_executable(ifm_costintel_bench bench/bench_pipeline.c)
target_link_libraries(ifm_costintel_bench PRIVATE ifm_costintel_lib m)

# Enable testing
enable_testing()

macro(add_ifm_test test_name test_src)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE ifm_costintel_lib m)
    add_test(NAME ${test_name} COMMAND ${test_name})
endmacro()

add_ifm_test(test_fak tests/unit/test_fak.c)
add_ifm_test(test_ingress tests/unit/test_ingress.c)
add_ifm_test(test_allocation tests/unit/test_allocation.c)
add_ifm_test(test_aggregation tests/unit/test_aggregation.c)
add_ifm_test(test_variance tests/unit/test_variance.c)
add_ifm_test(test_governance tests/unit/test_governance.c)
add_ifm_test(test_end_to_end tests/integration/test_end_to_end.c)
add_ifm_test(fuzz_json_decoder tests/fuzz/fuzz_json_decoder.c)
