# Copyright 2026 Sander Notenbaert. All rights reserved.
# Use of this source code is governed by the MIT License found in the License.md file.

cmake_minimum_required(VERSION 3.19)

set(C74_MIN_API_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../min-api)
include(${C74_MIN_API_DIR}/script/min-pretarget.cmake)


#############################################################
# MAX EXTERNAL
#############################################################


include_directories(
	"${C74_INCLUDES}"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../artnet"
)

find_package(Threads REQUIRED)


set( SOURCE_FILES
	${PROJECT_NAME}.cpp
)


add_library(
	${PROJECT_NAME}
	MODULE
	${SOURCE_FILES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE flux.artnet.core Threads::Threads)
add_dependencies(${PROJECT_NAME} flux.artnet.core)

if (APPLE)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		BUILD_WITH_INSTALL_RPATH TRUE
		INSTALL_RPATH "@loader_path/../../.."
	)
endif ()

if (NOT DEFINED PACKAGE_VERSION OR PACKAGE_VERSION STREQUAL "")
	target_compile_definitions(${PROJECT_NAME} PRIVATE FLUX_ARTNET_VERSION="unknown")
else ()
	target_compile_definitions(${PROJECT_NAME} PRIVATE FLUX_ARTNET_VERSION="${PACKAGE_VERSION}")
endif ()


include(${C74_MIN_API_DIR}/script/min-posttarget.cmake)


#############################################################
# UNIT TEST
#############################################################

include(${C74_MIN_API_DIR}/test/min-object-unittest.cmake)

if (TARGET ${PROJECT_NAME}_test)
	target_link_libraries(${PROJECT_NAME}_test PRIVATE flux.artnet.core Threads::Threads)
	add_dependencies(${PROJECT_NAME}_test flux.artnet.core)
	if (APPLE)
		set_target_properties(${PROJECT_NAME}_test PROPERTIES
			BUILD_WITH_INSTALL_RPATH TRUE
			INSTALL_RPATH "@loader_path;@loader_path/../externals"
		)
	endif ()
endif ()
