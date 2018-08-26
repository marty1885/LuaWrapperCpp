project(LuaWrapperCpp)

file(GLOB HEADERS *.h)
file(GLOB LUA_WRAPPER_SOURCES *.cpp)

add_library(LuaWrapperCpp SHARED ${LUA_WRAPPER_SOURCES})

option(USE_SYSTEM_LUA "Use Lua installed on the system instead of building it from source" OFF)
if(USE_SYSTEM_LUA MATCHES ON)
	target_link_libraries(LuaWrapperCpp lua)
else()
	file(GLOB LUA_SOURCE lua_lib/*.c)
	include_directories(lua_lib)
	target_sources(LuaWrapperCpp PUBLIC ${LUA_SOURCE})
endif(USE_SYSTEM_LUA MATCHES ON)

install(TARGETS LuaWrapperCpp DESTINATION lib)
install(FILES ${HEADERS} DESTINATION include/LuaWrapperCpp)
