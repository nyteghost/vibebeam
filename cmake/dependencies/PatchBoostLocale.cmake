# Narrow, idempotent fix for the checksum-pinned Boost 1.89 fallback archive.
# This changes IDE header grouping only; no library sources or backends change.
set(locale_cmake "${BOOST_SOURCE_DIR}/libs/locale/CMakeLists.txt")
file(READ "${locale_cmake}" contents)
set(before [=[source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include/boost/locale" PREFIX "headers" FILES ${sources_inc})]=])
set(after [=[source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include/boost" PREFIX "headers" FILES ${sources_inc})]=])
string(FIND "${contents}" "${before}" old_position)
string(FIND "${contents}" "${after}" new_position)
if(NOT old_position EQUAL -1)
    string(REPLACE "${before}" "${after}" contents "${contents}")
    file(WRITE "${locale_cmake}" "${contents}")
elseif(new_position EQUAL -1)
    message(FATAL_ERROR "Boost.Locale source_group changed; review the pinned fallback patch")
endif()
