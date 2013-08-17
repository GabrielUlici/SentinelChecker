cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(SentinelChecker)
find_package(qibuild)

set(SentinelChecker_VERSION "1.0")

# Searching a valid setenv.cmake, if any
IF (NOT "$ENV{RDK_BUILD_DIR}" STREQUAL "")
	EXECUTE_PROCESS(COMMAND rdk-config --cmakeinclude OUTPUT_VARIABLE openrdkExternalInclude OUTPUT_STRIP_TRAILING_WHITESPACE)
	MESSAGE(STATUS "Using ${openrdkExternalInclude}")
	INCLUDE("${openrdkExternalInclude}")
ELSE(NOT "$ENV{RDK_BUILD_DIR}" STREQUAL "")
	MESSAGE(FATAL_ERROR "It seems I cannot find your copy of OpenRDK. This could be because I cannot determine the path of \"FindOpenRDK.cmake\" file that will setup correct building flags. Try using this command (did you do make install_rdkenv in your OpenRDK directory?): . rdkenv set")

ENDIF (NOT "$ENV{RDK_BUILD_DIR}" STREQUAL "")

MESSAGE(STATUS "..:: OpenRDK variables ::..")
RDK_PRINT_VARIABLES()

# Create an executable named SentinelChecker
set(SentinelChecker_srcs
	src/SentinelCheckerMain.cpp
	src/SentinelChecker.h
	src/SentinelChecker.cpp
	${OpenRDK_ROOT}/src/libraries/rdkcore/sharedmemory/sharedmemory.cpp
)

qi_create_lib(SentinelChecker SHARED ${SentinelChecker_srcs} SUBFOLDER naoqi)
set_target_properties(SentinelChecker PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include_directories(${OpenRDK_INCLUDE_DIR};${CMAKE_CURRENT_SOURCE_DIR}/../../../../libraries)
add_definitions(${OpenRDK_DEFINITIONS})

qi_use_lib(SentinelChecker alcommon almath alcore alaudio altools alvalue almemoryfastaccess NAOQI-PLUGINS-TOOLS)
