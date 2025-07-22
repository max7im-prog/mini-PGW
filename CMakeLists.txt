cmake_minimum_required(VERSION 3.15.0)
project(protey-course-work VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    gtest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(gtest)

FetchContent_Declare(
    cpp-http
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.23.1
)
FetchContent_MakeAvailable(cpp-http)

FetchContent_Declare(
  nlohmann-json 
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann-json)

FetchContent_Declare(
  spdlog 
  URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.3.tar.gz
)
FetchContent_MakeAvailable(spdlog)

include(CTest)
include(GoogleTest)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/common)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/client)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/server)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)

