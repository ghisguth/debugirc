include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(debugirc SHARED debugirc.hpp debugirc.cpp)
target_link_libraries(debugirc m)
