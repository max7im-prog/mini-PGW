enable_testing()

add_executable(unitTests
    ${CMAKE_SOURCE_DIR}/test/tests.cpp
)

target_link_libraries(unitTests
    PRIVATE
    GTest::gtest_main
    GTest::gmock
    pgw_common_lib
    pgw_server_lib
    pgw_client_lib
)

gtest_discover_tests(unitTests)

add_custom_target(copy_test_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/res
        ${CMAKE_CURRENT_BINARY_DIR}/res
)

add_dependencies(unitTests copy_test_resources)

