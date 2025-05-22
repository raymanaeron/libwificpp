cmake_minimum_required(VERSION 3.15)
project(wificpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Library target
add_library(wificpp SHARED
    src/wifi_manager.cpp
    src/wifi_scanner.cpp
    src/wifi_logger.cpp
    src/wifi_c_api.cpp
)

target_include_directories(wificpp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(wificpp PRIVATE wlanapi)
endif()

# Test executable
add_executable(test_wifi test/test_wifi.cpp)
target_link_libraries(test_wifi PRIVATE wificpp)

# Install rules
install(TARGETS wificpp
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
)
