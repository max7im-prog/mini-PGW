enable_testing()

add_executable(unitTests
    ${CMAKE_SOURCE_DIR}/test/tests.cpp
)

target_link_libraries(unitTests PRIVATE GTest::gtest_main GTest::gmock)

gtest_discover_tests(unitTests)
