cmake_minimum_required(VERSION 3.10)
project(XClient)

set(PRODUCT_NAME "XClient")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-DNOMINMAX)

# main

if(BUILD_DEVELOPER)
	add_definitions(-DBUILD_DEVELOPER)
endif()

add_definitions(-DPROJECT_NAME="${PROJECT_NAME}")
add_definitions(-DPRODUCT_NAME="${PRODUCT_NAME}")

file(GLOB MAIN_SRC
	"src/*.cpp"
	"src/*.h"
)

source_group("all" FILES ${MAIN_SRC})

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:Console /ENTRY:mainCRTStartup")

if(XCLIENT_DLL)
	add_definitions(-DXCLIENT_DLL)
	add_library(${PROJECT_NAME} SHARED 
		${MAIN_SRC}
	)
else()
	if(XCLIENT_LUA)
		add_definitions(-DXCLIENT_LUA)
	endif()
	add_executable(${PROJECT_NAME} 
		${MAIN_SRC}
	)
endif()

# other

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# sky

add_subdirectory(sky)
target_link_libraries(${PROJECT_NAME} sky)

# hl

add_subdirectory(hl)
target_link_libraries(${PROJECT_NAME} hl)