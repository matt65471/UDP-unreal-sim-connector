cmake_minimum_required(VERSION 3.16)

project(udp_unreal_sim_connector VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(rocket_telemetry
    src/telemetry.cpp
)
target_include_directories(rocket_telemetry PUBLIC include)

add_executable(udp_receiver
    src/main.cpp
)
target_link_libraries(udp_receiver PRIVATE rocket_telemetry)

if(WIN32)
    target_link_libraries(udp_receiver PRIVATE ws2_32)
endif()

function(enable_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

enable_project_warnings(rocket_telemetry)
enable_project_warnings(udp_receiver)

include(CTest)
if(BUILD_TESTING)
    add_executable(telemetry_tests tests/telemetry_tests.cpp)
    target_link_libraries(telemetry_tests PRIVATE rocket_telemetry)
    enable_project_warnings(telemetry_tests)
    add_test(NAME telemetry_tests COMMAND telemetry_tests)
endif()
