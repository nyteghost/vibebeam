cmake_minimum_required(VERSION 3.20)
project(VibebeamSteamLibraryTests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(STEAM_TEST_JSON_INCLUDE "" CACHE PATH "Directory containing nlohmann/json.hpp")
if(NOT EXISTS "${STEAM_TEST_JSON_INCLUDE}/nlohmann/json.hpp")
    message(FATAL_ERROR "Set STEAM_TEST_JSON_INCLUDE to the JSON include directory")
endif()
add_subdirectory("${REPO}/third-party/googletest" googletest)
enable_testing()

add_library(steam_library STATIC
    "${REPO}/src/steam_integration.cpp"
    "${REPO}/src/steam_sync_policy.cpp"
    "${REPO}/src/steam_auto_sync_policy.cpp"
    "${REPO}/src/provider_scan_protocol.cpp")
target_include_directories(steam_library PUBLIC "${REPO}" "${STEAM_TEST_JSON_INCLUDE}")
if(WIN32)
    target_compile_definitions(steam_library PUBLIC NOMINMAX)
    target_link_libraries(steam_library PUBLIC advapi32 shell32 wtsapi32)
endif()

add_executable(steam_library_tests
    "${REPO}/tests/test_steam_integration.cpp"
    "${REPO}/tests/test_steam_sync.cpp"
    "${REPO}/tests/test_steam_auto_sync.cpp"
    "${REPO}/tests/test_provider_scan_protocol.cpp")
target_link_libraries(steam_library_tests PRIVATE steam_library GTest::gtest_main)
if(MSVC)
    target_compile_options(steam_library PRIVATE /utf-8)
    target_compile_options(steam_library_tests PRIVATE /utf-8)
endif()
add_test(NAME steam_library COMMAND steam_library_tests)
set_tests_properties(steam_library PROPERTIES TIMEOUT 120)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(session_policy_tests "${REPO}/tests/unit/platform/linux/test_session_exec_policy.c")
    target_link_libraries(session_policy_tests PRIVATE cap)
    add_test(NAME session_policy COMMAND session_policy_tests)
    set_tests_properties(session_policy PROPERTIES TIMEOUT 120)
endif()
