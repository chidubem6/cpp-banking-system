include(FetchContent)

FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    # Pinned so a repointed tag or a truncated download fails loudly with a hash
    # mismatch rather than silently building against different content.
    URL_HASH SHA256=40d4ec942217dcc84a9ebe2a68584ada7d4a33a8ee958755763278ea1c5e18ff
    # Use extraction time for file timestamps so a changed URL triggers a rebuild.
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
# Prevents an MSVC runtime-library mismatch when linking the test executable.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(banking_tests
    test_smoke.cpp
    test_transaction.cpp
)
target_link_libraries(banking_tests PRIVATE banking_core banking_warnings GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(banking_tests)
