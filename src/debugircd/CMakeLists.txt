include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_executable(debugircd main.cpp)
target_link_libraries(debugircd ${Boost_LIBRARIES})
