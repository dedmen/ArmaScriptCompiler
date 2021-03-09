cmake_minimum_required (VERSION 3.13)
if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
	set (CMAKE_SYSTEM_VERSION 8.1 CACHE TYPE INTERNAL FORCE) #Force 8.1 SDK, to keep it compatible with win7
endif()
project (ArmaScriptCompiler CXX)
find_package (Threads)


if(MSVC)
	set( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /std:c++latest" )
#elseif(MSVC)
#	message(FATAL_ERROR "ERROR: You need a C++17 compatible compiler")
endif()

message("GENERATOR USED: '${CMAKE_GENERATOR}'")
message("COMPILER USED: '${CMAKE_CXX_COMPILER_ID}'")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/release")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

SET(CMAKE_INCLUDE_CURRENT_DIR ON) 
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)


set(LIBRARY_PATH_ZSTD "${CMAKE_SOURCE_DIR}/lib/zstd")
set(LIBRARY_PATH_SQFVM "${CMAKE_SOURCE_DIR}/lib/sqfvm")
set(LIBRARY_PATH_B64  "${CMAKE_SOURCE_DIR}/lib/cpp-base64")
set(LIBRARY_PATH_LZO  "${CMAKE_SOURCE_DIR}/lib/lzokay")
set(LIBRARY_PATH_JSON "${CMAKE_SOURCE_DIR}/lib/json/single_include")
set(LIBRARY_PATH_SOL2 "${CMAKE_SOURCE_DIR}/lib/sol2")

add_definitions(/DNOMINMAX)
add_definitions(/D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS) #No I don't cheat!

#don't ask me

if(NOT CMAKE_C_COMPILE_OBJECT)
  set(CMAKE_C_COMPILE_OBJECT
    "<CMAKE_C_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -o <OBJECT>   -c <SOURCE>")
endif()






file(GLOB_RECURSE SOURCES_ASC "src/*.hpp" "src/*.cpp" "${LIBRARY_PATH_B64}/base64.cpp")
SOURCE_GROUP("src" FILES ${SOURCES_ASC})

file(GLOB_RECURSE SOURCES_ASC_OPT "src/optimizer/*.hpp" "src/optimizer/*.cpp")
SOURCE_GROUP("src" FILES ${SOURCES_ASC_OPT})

#zstd
SET(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "no programs" FORCE)
set(ZSTD_USE_STATIC_RUNTIME ON CACHE BOOL "yes" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "no" FORCE)

add_subdirectory("${LIBRARY_PATH_ZSTD}/build/cmake")

set(INCLUDE_PATH_ZSTD "${LIBRARY_PATH_ZSTD}/lib")

#sqfvm
#Adding manually because I don't want commands

#include_directories("${LIBRARY_PATH_SQFVM}/include/json/include")
#include_directories("${LIBRARY_PATH_SQFVM}/include/tclap-1.2.2/include")
#
#file(GLOB SOURCES_SQFVM 
#    "${LIBRARY_PATH_SQFVM}/src/*.h" "${LIBRARY_PATH_SQFVM}/src/*.cpp" "${LIBRARY_PATH_SQFVM}/src/*.c"
#)
#SOURCE_GROUP("sqf_vm" FILES ${SOURCES_SQFVM})
#
#set(INCLUDE_PATH_SQFVM "${LIBRARY_PATH_SQFVM}/src")
#
#
#
#list(REMOVE_ITEM SOURCES_SQFVM "${LIBRARY_PATH_SQFVM}/src/Entry.cpp")
#list(REMOVE_ITEM SOURCES_SQFVM "${LIBRARY_PATH_SQFVM}/src/dllexports.cpp")


set(SQFVM_BUILD_EXECUTABLE OFF CACHE BOOL "no" FORCE)
set(SQFVM_BUILD_EXECUTABLE_ARMA2_LOCALKEYWORD OFF CACHE BOOL "no" FORCE)
set(SQFVM_BUILD_LIBRARY OFF CACHE BOOL "no" FORCE)
set(SQFVM_BUILD_LIBRARY_SQC_SUPPORT OFF CACHE BOOL "no" FORCE)
set(SQFVM_BUILD_STATIC_LIBRARY ON CACHE BOOL "yes" FORCE)
set(SQFVM_BUILD_STATIC_LIBRARY_SQC_SUPPORT OFF CACHE BOOL "no" FORCE)
set(SQFVM_BUILD_EXECUTABLE_SQC_SUPPORT OFF CACHE BOOL "no" FORCE)
set(SQFVM_BUILD_EXECUTABLE_FULL_DIAGNOSE OFF CACHE BOOL "no" FORCE)
set(SQFVM_BUILD_EXECUTABLE_ARMA2_LOCALKEYWORD_FULL_DIAGNOSE OFF CACHE BOOL "no" FORCE)

set(INCLUDE_PATH_SQFVM "${LIBRARY_PATH_SQFVM}/src")
add_subdirectory(${LIBRARY_PATH_SQFVM})
add_subdirectory(${LIBRARY_PATH_SOL2})

set(LUA_BUILD_AS_CXX ON CACHE BOOL "yes" FORCE)
add_subdirectory("lib/lua")
add_definitions(/DLOADFILE_CACHE) #SQF VM fileio.cpp cache



add_executable("ArmaScriptCompiler" ${SOURCES_ASC} ${SOURCES_SQFVM} ${SOURCES_ASC_OPT} "${LIBRARY_PATH_LZO}/lzokay.cpp")

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${INCLUDE_PATH_ZSTD} ${LIBRARY_PATH_B64} ${INCLUDE_PATH_SQFVM} ${LIBRARY_PATH_LZO} ${LIBRARY_PATH_JSON}) # ${INCLUDE_PATH_ZSTD} ${INCLUDE_PATH_SQFVM}

#target_link_libraries("ArmaScriptCompiler" ${CMAKE_THREAD_LIBS_INIT})

target_link_libraries("ArmaScriptCompiler" slibsqfvm libzstd_static sol2 lua_static)

set_target_properties("ArmaScriptCompiler" PROPERTIES PREFIX "")
set_target_properties("ArmaScriptCompiler" PROPERTIES FOLDER ArmaScriptCompiler)



target_compile_definitions("ArmaScriptCompiler" PUBLIC SOL_ALL_SAFETIES_ON=1 SOL_PRINT_ERRORS=1 SOL_USING_CXX_LUA=1)

if(CMAKE_COMPILER_IS_GNUCXX)
	add_compile_definitions(__linux__)

	target_compile_options(
		"ArmaScriptCompiler" PRIVATE
		"-std=c++1z"
		"$<$<CONFIG:Release>:-O3>"
		"-s"
		"-fPIC"
		"-fpermissive"
		"-static-libgcc"
		"-static-libstdc++"
		"-Wno-ignored-attributes"
		"-static"
		"$<$<CONFIG:Release>:-ffunction-sections>"
		"$<$<CONFIG:Release>:-fdata-sections>"

	)
	target_link_options("ArmaScriptCompiler" PRIVATE "-fPIC" "-static" "-static-libgcc" "-static-libstdc++"
	"$<$<CONFIG:Release>:-Wl,--gc-sections>"
	"$<$<CONFIG:Release>:-Wl,--strip-all>"
	) 
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
else()
	target_compile_options(lua_static PRIVATE "$<$<CONFIG:Release>:/MT>")
	target_compile_options(
		"ArmaScriptCompiler" PRIVATE 
		"/MP" "/Zi"
		"$<$<CONFIG:Release>:/MT>"
		"$<$<CONFIG:Release>:/Ox>"
		"$<$<CONFIG:Release>:/Ob2>"
		"$<$<CONFIG:Release>:/Oi>"
		"$<$<CONFIG:Release>:/Ot>"
		"$<$<CONFIG:Release>:/GL>"
		"/F4194304" #Stack size
		"/FS"
		"/bigobj"
	)
	
	target_compile_options(
		slibsqfvm PRIVATE 
		"/MP" "/Zi"
		"$<$<CONFIG:Release>:/MT>"
		"$<$<CONFIG:Release>:/Ox>"
		"$<$<CONFIG:Release>:/Ob2>"
		"$<$<CONFIG:Release>:/Oi>"
		"$<$<CONFIG:Release>:/Ot>"
		"$<$<CONFIG:Release>:/GL>"
		"/DLOADFILE_CACHE"
	)
	target_link_options("ArmaScriptCompiler" PRIVATE "/OPT:REF" "/OPT:ICF" "/DEBUG:FULL" "/LTCG") 
	target_link_options(slibsqfvm PRIVATE "/OPT:REF" "/OPT:ICF" "/DEBUG:FULL" "/LTCG") 
endif()

#Binary signing
#if(EXISTS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sign.bat" AND MSVC)
#	add_custom_command(TARGET ${INTERCEPT_PLUGIN_NAME}
#		POST_BUILD
#		COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sign.bat
#		WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
#	)
#endif()