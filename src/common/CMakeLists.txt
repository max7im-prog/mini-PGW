#library
add_library(pgw_common_lib
    STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imsi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CDREvent.cpp
)

target_include_directories(pgw_common_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/)