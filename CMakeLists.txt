cmake_minimum_required (VERSION 3.10)

project(directcapture)

#
# source files
#

file(GLOB_RECURSE DIRCAPDLL_SOURCES dircapdll/*.cpp)
add_library(dircapdll SHARED ${DIRCAPDLL_SOURCES})

file(GLOB_RECURSE DIRCAP_SOURCES directcapture/*.cpp)
file(GLOB_RECURSE MLIB_SOURCES extlib/mlib.1.1/*.cpp)
add_executable(directcapture ${MLIB_SOURCES} ${DIRCAP_SOURCES})

#
# compiler flags
#

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
endif()

set_target_properties(dircapdll directcapture PROPERTIES
	CXX_STANDARD 17
	CMAKE_CXX_STANDARD_REQUIRED ON)
	
#
# output paths
#

set_target_properties(dircapdll directcapture PROPERTIES
	DEBUG_POSTFIX "_d"
	RELEASE_POSTFIX "_r"
	MINSIZEREL_POSTFIX "_mr"
	RELWITHDEBINFO_POSTFIX "_dr"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_HOME_DIRECTORY}/bin"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_HOME_DIRECTORY}/bin"
	RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_HOME_DIRECTORY}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_HOME_DIRECTORY}/bin")
	
#
# external libraries
#
	
include_directories("${CMAKE_HOME_DIRECTORY}/extlib/cxxopts.2.2.0/include")
include_directories("${CMAKE_HOME_DIRECTORY}/extlib/mlib.1.1")

target_link_libraries(dircapdll dsound.lib)
