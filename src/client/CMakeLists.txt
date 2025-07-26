#library
add_library(pgw_client_lib
    STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/client.cpp
    ${CMAKE_SOURCE_DIR}/src/common/imsi.cpp
)
target_include_directories(pgw_client_lib PUBLIC ${CMAKE_SOURCE_DIR}/src/common)
target_link_libraries(pgw_client_lib
    PUBLIC
    nlohmann_json::nlohmann_json
    spdlog
)

#executable
add_executable(
    pgw_client
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
target_link_libraries(pgw_client
    PRIVATE
    pgw_client_lib
)
