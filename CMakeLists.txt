# $Id$
# EOSERV is released under the zlib license.
# See LICENSE.txt for more info.

cmake_minimum_required(VERSION 3.0)
project(eoserv C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ---------
#  Options
# ---------

option(EOSERV_WANT_MYSQL "Enables MariaDB/MySQL server database support." ON)
option(EOSERV_WANT_SQLITE "Enables SQLite3 embedded database support." ON)

option(EOSERV_USE_PRECOMPILED_HEADERS "Uses a precompiled header to speed up compilation." ON)
option(EOSERV_USE_UNITY_BUILD "Compiles multiple source files in one translation unit to speed up compilation." OFF)

option(EOSERV_GEN_PRECOMPILED_HEADERS "Generated precompiled header automatically. Requires a shell with basic binutils including grep and awk." OFF)
option(EOSERV_NO_DATA "Disables copying of data files in to build directory" OFF)

option(EOSERV_DEBUG_QUERIES "Enables printing of database queries to debug output" OFF)

# --------------
#  Source files
# --------------

include(SourceFileList)

# Platform-specific source files
# Unity builds handle their own platform checking for now
if(WIN32)
	list(APPEND eoserv_ALL_SOURCE_FILES ${eoserv_WIN32_SOURCE_FILES})
endif()

if(EOSERV_USE_UNITY_BUILD)
	set(eoserv_SOURCE_FILES ${eoserv_UNITY_SOURCE_FILES})
else()
	set(eoserv_SOURCE_FILES ${eoserv_ALL_SOURCE_FILES})
endif()

# Fancy icon on Windows
if(WIN32)
	list(APPEND eoserv_SOURCE_FILES "project/winres.rc")
endif()

set(eoserv_LIBRARIES)

# ----------------
#  Compiler flags
# ----------------

# These non-standard anti-optimizations are currently required for a correctly functioning server
if(eoserv_GCC OR eoserv_CLANG)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fwrapv -fno-strict-aliasing")
endif()

if(EOSERV_DEBUG_QUERIES)
	add_definitions(-DDATABASE_DEBUG)
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" BuildType)

if(BuildType STREQUAL "debug")
	add_definitions(-DDEBUG)
endif()

# ---------
#  Outputs
# ---------

add_executable(eoserv
	${eoserv_SOURCE_FILES}
)

set_target_properties(eoserv PROPERTIES CXX_STANDARD 14)

# -----------
#  Libraries
# -----------

find_package(PThreads REQUIRED)

include_directories(${PTHREADS_INCLUDE_DIR})
list(APPEND eoserv_LIBRARIES ${PTHREADS_LIBRARY})

if(EOSERV_WANT_MYSQL)
	find_package(MariaDB REQUIRED)
	include_directories(${MARIADB_INCLUDE_DIR})
	list(APPEND eoserv_LIBRARIES ${MARIADB_LIBRARY})
	add_definitions(-DDATABASE_MYSQL)
endif()

if(EOSERV_WANT_SQLITE)
	find_package(SQLite3 REQUIRED)
	include_directories(${SQLITE3_INCLUDE_DIR})
	list(APPEND eoserv_LIBRARIES ${SQLITE3_LIBRARY})
	add_definitions(-DDATABASE_SQLITE)
endif()

if (NOT EOSERV_WANT_MYSQL AND NOT EOSERV_WANT_SQLITE)
	message(FATAL_ERROR "Either MySQL or SQLite support must be enabled.")
endif()

if(WIN32)
        list(APPEND eoserv_LIBRARIES winmm ws2_32)
endif()

target_link_libraries(eoserv ${eoserv_LIBRARIES})

install(TARGETS eoserv RUNTIME DESTINATION .)

foreach (File ${ExtraFiles})
	get_filename_component(Dir "${File}" DIRECTORY)

	if(Dir STREQUAL "")
		set(Dir ".")
	endif()

	install(FILES ${File} DESTINATION ${Dir})
endforeach()

# ---------------------
#  Precompiled Headers
# ---------------------

include(EOSERV_PCH)

if(EOSERV_USE_PRECOMPILED_HEADERS)
	if(EOSERV_GEN_PRECOMPILED_HEADERS)
		add_custom_command(OUTPUT ${CMAKE_CACHEFILE_DIR}/eoserv-pch.h
			COMMAND ${CMAKE_SOURCE_DIR}/autogen-pch.sh ${eoserv_ALL_SOURCE_FILES} > ${CMAKE_CACHEFILE_DIR}/eoserv-pch.h
			MAIN_DEPENDENCY autogen-pch.sh
			# This dependency is commented out to avoid constant rebuilding on any source file change
			# DEPENDS autogen-pch.sh ${eoserv_ALL_SOURCE_FILES}
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		)
	else()
		add_custom_command(OUTPUT ${CMAKE_CACHEFILE_DIR}/eoserv-pch.h
			COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/src/stdafx.h ${CMAKE_CACHEFILE_DIR}/eoserv-pch.h
			MAIN_DEPENDENCY src/stdafx.h
			DEPENDS src/stdafx.h
		)
	endif()

	add_custom_target(eoserv-pch-autogen
		DEPENDS ${CMAKE_CACHEFILE_DIR}/eoserv-pch.h
	)

	compile_pch(eoserv-pch ${CMAKE_CACHEFILE_DIR}/eoserv-pch.h)

	add_dependencies(eoserv-pch-autogen eoserv-pch)
	add_dependencies(eoserv eoserv-pch)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${eoserv-pch_INCLUDE_FLAG}")
endif()

# ------------
#  Data files
# ------------

if (NOT EOSERV_NO_DATA)
	foreach (File ${ExtraFiles})
		configure_file(${File} ${CMAKE_CURRENT_BINARY_DIR}/${File} COPYONLY)
	endforeach()
endif()
