cmake_minimum_required(VERSION 3.19)

# Build universal binaries on macOS by default so the externals run natively
# on both Apple Silicon and Intel machines. Override with
# -DCMAKE_OSX_ARCHITECTURES=arm64 (or x86_64) if desired.
if (APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures to build")
endif ()

project(flux.artnet)

enable_testing()

string(REGEX REPLACE "(.*)/" "" THIS_PACKAGE_NAME "${CMAKE_CURRENT_SOURCE_DIR}")


if (${CMAKE_GENERATOR} MATCHES "Xcode")
         if (${XCODE_VERSION} VERSION_LESS 10)
              message(STATUS "Xcode 10 or higher is required. Please install from the Mac App Store.")
            return ()
        endif ()
endif ()


# Fetch the correct version of the min-api
message(STATUS "Updating Git Submodules")
execute_process(
    COMMAND				git submodule update --init --recursive
    WORKING_DIRECTORY	"${CMAKE_CURRENT_SOURCE_DIR}"
)


# Read the package metadata so that the generated macOS Info.plist gets the
# right version, copyright, and bundle identifier. The Min API only reads
# package-info.json automatically when a package-info.json.in template is
# present, so we do it here for the hand-maintained file.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/package-info.json")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/package-info.json" C74_PKGINFO)
    string(JSON PACKAGE_VERSION GET "${C74_PKGINFO}" "version")
    string(JSON C74_PKG_EXTRA GET "${C74_PKGINFO}" "package_extra")
    string(JSON AUTHOR_DOMAIN GET "${C74_PKG_EXTRA}" "reverse_domain")
    string(JSON COPYRIGHT_STRING GET "${C74_PKG_EXTRA}" "copyright")
endif ()


# Misc setup and subroutines
include(${CMAKE_CURRENT_SOURCE_DIR}/source/min-api/script/min-package.cmake)


# Shared networking core
#
# Max loads each .mxo as a separate module, so a singleton compiled into both
# externals would give them separate UDP listeners. This dylib is linked by
# both modules and owns the one listener on port 6454 plus the discovery
# registry.
find_package(Threads REQUIRED)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/externals")

add_library(flux.artnet.core SHARED
    source/artnet/discovery.cpp
    source/artnet/receiver_advertisement.cpp
    source/artnet/shared_core.cpp
)

target_include_directories(flux.artnet.core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/source/artnet")
target_link_libraries(flux.artnet.core PRIVATE Threads::Threads)

set_target_properties(flux.artnet.core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    OUTPUT_NAME "flux.artnet.core"
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/externals"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/externals"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/externals"
)

if (APPLE)
    set_target_properties(flux.artnet.core PROPERTIES
        SUFFIX ".dylib"
        INSTALL_NAME_DIR "@rpath"
        BUILD_WITH_INSTALL_NAME_DIR ON
        MACOSX_RPATH ON
    )
    add_custom_command(TARGET flux.artnet.core POST_BUILD
        COMMAND codesign -s - -f "$<TARGET_FILE:flux.artnet.core>"
        VERBATIM
        COMMENT "Code sign flux.artnet.core"
    )
endif ()


# Add the Lib, if it exists
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/min-lib/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source/min-lib)
endif ()


# Generate a project for every folder in the "source/projects" folder
SUBDIRLIST(PROJECT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/source/projects)
foreach (project_dir ${PROJECT_DIRS})
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/projects/${project_dir}/CMakeLists.txt")
        message("Generating: ${project_dir}")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source/projects/${project_dir})
    endif ()
endforeach ()

# Comment the line below if you want automatic cmake regneration enabled
set(CMAKE_SUPPRESS_REGENERATION true)
