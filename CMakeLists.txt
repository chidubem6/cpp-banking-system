# 3.24 is required by FetchContent's DOWNLOAD_EXTRACT_TIMESTAMP in tests/.
# On 3.20-3.23 that keyword is silently absorbed as a URL mirror rather than
# rejected, so the floor has to be honest about it.
cmake_minimum_required(VERSION 3.24)
project(banking_system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warning settings live on one INTERFACE target so every consumer gets them.
# Previously they were PRIVATE on banking_core, which meant the test target
# compiled the same headers with warnings switched off - the exact place a
# self-initialisation bug can hide.
add_library(banking_warnings INTERFACE)
if(MSVC)
    target_compile_options(banking_warnings INTERFACE /W4 /utf-8)
else()
    target_compile_options(banking_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_library(banking_core
    src/Account.cpp
    src/Bank.cpp
    src/Money.cpp
    src/Transaction.cpp
)
target_include_directories(banking_core
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
target_link_libraries(banking_core PUBLIC banking_warnings)

add_executable(bank src/cli/main.cpp)
target_link_libraries(bank PRIVATE banking_core)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
