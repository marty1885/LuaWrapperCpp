add_subdirectory(LUA)
include_directories(LUA)


project(Benchmark)
add_executable(Benchmark Benchmarks.cpp main.cpp)
target_link_libraries(Benchmark LuaWrapperCpp)
