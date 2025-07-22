add_executable(
    pgw_server
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/server.cpp
)

target_include_directories(pgw_server
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(pgw_server
    PRIVATE
    nlohmann_json::nlohmann_json
)