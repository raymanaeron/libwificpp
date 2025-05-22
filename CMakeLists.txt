cmake_minimum_required(VERSION 3.15)
project(wificpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Platform-specific source files
set(PLATFORM_SOURCES "")
if(WIN32)
    list(APPEND PLATFORM_SOURCES src/wifi_windows.cpp)
    add_compile_definitions(WIFICPP_PLATFORM_WINDOWS)
elseif(UNIX AND NOT APPLE)
    list(APPEND PLATFORM_SOURCES src/wifi_linux.cpp)
    add_compile_definitions(WIFICPP_PLATFORM_LINUX)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Library target
add_library(wificpp SHARED
    src/wifi_manager.cpp
    src/wifi_scanner.cpp
    src/wifi_logger.cpp
    src/wifi_c_api.cpp
    ${PLATFORM_SOURCES}
)

target_include_directories(wificpp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(wificpp PRIVATE wlanapi)
elseif(UNIX AND NOT APPLE)
    # NetworkManager dependencies for Linux
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBNM REQUIRED libnm>=1.14)
    pkg_check_modules(GLIB REQUIRED glib-2.0>=2.32)
    
    target_include_directories(wificpp PRIVATE 
        ${LIBNM_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS}
    )
    target_link_libraries(wificpp PRIVATE 
        ${LIBNM_LIBRARIES}
        ${GLIB_LIBRARIES}
    )
    target_compile_options(wificpp PRIVATE 
        ${LIBNM_CFLAGS}
        ${GLIB_CFLAGS}
    )
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
