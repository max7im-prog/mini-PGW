#library
add_library(pgw_server_lib
    STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/server.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/threadPool.cpp
)
target_include_directories(pgw_server_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/)
target_link_libraries(pgw_server_lib
    PUBLIC
    nlohmann_json::nlohmann_json
    httplib
    spdlog
    pgw_common_lib
)

#executable
add_executable(
    pgw_server
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
target_link_libraries(pgw_server
    PRIVATE
    pgw_server_lib
)
