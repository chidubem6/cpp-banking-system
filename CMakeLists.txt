cmake_minimum_required(VERSION 3.20)
project(banking_system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(banking_core
    src/Account.cpp
    src/Bank.cpp
    src/Transaction.cpp
)
target_include_directories(banking_core PUBLIC src)

if(MSVC)
    target_compile_options(banking_core PRIVATE /W4 /utf-8)
else()
    target_compile_options(banking_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(bank src/cli/main.cpp)
target_link_libraries(bank PRIVATE banking_core)
if(MSVC)
    target_compile_options(bank PRIVATE /W4 /utf-8)
else()
    target_compile_options(bank PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
