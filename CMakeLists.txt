cmake_minimum_required(VERSION 3.20)
project(realtime_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(realtime_app INTERFACE)
add_library(realtime_app::realtime_app ALIAS realtime_app)

target_include_directories(realtime_app INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

if (MSVC)
  target_compile_options(realtime_app INTERFACE /W4 /permissive-)
else()
  target_compile_options(realtime_app INTERFACE -Wall -Wextra -Wpedantic)
endif()

include(.vix/vix_deps.cmake)
target_link_libraries(realtime_app INTERFACE vix::api_app vix::web_app vix::app)

include(CTest)
enable_testing()

add_executable(realtime_app_basic_test tests/test_basic.cpp)
target_link_libraries(realtime_app_basic_test PRIVATE realtime_app::realtime_app)
add_test(NAME realtime_app.basic COMMAND realtime_app_basic_test)
