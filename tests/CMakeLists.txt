include(FetchContent)

FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    # Use extraction time for file timestamps so a changed URL triggers a rebuild.
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
# Prevents an MSVC runtime-library mismatch when linking the test executable.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(banking_tests
    test_smoke.cpp
)
target_link_libraries(banking_tests PRIVATE banking_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(banking_tests)
