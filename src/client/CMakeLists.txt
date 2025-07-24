add_executable(
    pgw_client
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_SOURCE_DIR}/src/common/imsi.cpp
)

target_include_directories(pgw_client PRIVATE ${CMAKE_SOURCE_DIR}/src/common)