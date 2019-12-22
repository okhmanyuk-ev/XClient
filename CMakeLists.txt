cmake_minimum_required(VERSION 3.8.0)
project(XClient)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:Console /ENTRY:mainCRTStartup")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-DNOMINMAX)

# main

file(GLOB MAIN_SRC
	"src/*.cpp"
	"src/*.h"
)

add_executable(${PROJECT_NAME} 
#add_library(${PROJECT_NAME} SHARED 
	${MAIN_SRC}
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# sky

include(sky/CMakeLists.txt)

# hl

include(hl/CMakeLists.txt)