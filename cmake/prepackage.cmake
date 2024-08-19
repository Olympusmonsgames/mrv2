# SPDX-License-Identifier: BSD-3-Clause
# mrv2
# Copyright Contributors to the mrv2 Project. All rights reserved.

message( STATUS "----------------------------------------prepackage------------------------------------" )
message( STATUS "CMAKE_CURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}" )
message( STATUS "CMAKE_CURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}" )
message( STATUS "CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" )

#
# Find mrv2's root dir by brute force, looking for directory that has
# our "cmake/version.cmake" file.
#
set( _current_dir ${CMAKE_CURRENT_BINARY_DIR})
set( _found_root_dir FALSE)
while( NOT _found_root_dir )
    set(_current_dir "${_current_dir}/..")
    file(REAL_PATH ${_current_dir} _current_dir )
    set(_mrv2_version "${_current_dir}/cmake/version.cmake" )
    if (EXISTS ${_mrv2_version})
	set(ROOT_DIR ${_current_dir})
	set(_found_root_dir TRUE)
    endif()
endwhile()

file(REAL_PATH ${ROOT_DIR} ROOT_DIR )
message( STATUS "mrv2 ROOT_DIR=${ROOT_DIR}" )


include( "${ROOT_DIR}/cmake/version.cmake" )
include( "${ROOT_DIR}/cmake/functions.cmake" )


# Function to remove __pycache__ directories
function(remove_pycache_directories dir)
    file(GLOB_RECURSE pycache_dirs RELATIVE ${dir} "__pycache__")

    foreach(pycache_dir IN LISTS pycache_dirs)
        file(REMOVE_RECURSE "${dir}/${pycache_dir}")
    endforeach()
endfunction()

if( UNIX AND NOT APPLE )
    #
    # \@bug: Linux CMAKE_INSTALL_PREFIX is broken and not pointing to
    #        pre-packaging directory!!!
    #
    if (EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_PREFIX}")
	set( CPACK_PREPACKAGE "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_PREFIX}" )
    else()
	set( CPACK_PREPACKAGE "${CMAKE_INSTALL_PREFIX}" )
    endif()
else()
    set( CPACK_PREPACKAGE "${CMAKE_INSTALL_PREFIX}" )
endif()

message( STATUS "CPACK_PREPACKAGE=${CPACK_PREPACKAGE}" )

#
# Remove usd directory from lib/ directory on Windows
#
if(WIN32)
    message( STATUS "Removing ${CPACK_PREPACKAGE}/lib/usd")
    file( REMOVE_RECURSE "${CPACK_PREPACKAGE}/lib/usd" )
endif()

#
# Remove .a, .lib and .dll files from packaging lib/ directory
#
message( STATUS "Removing static and DLLs from ${CPACK_PREPACKAGE}/lib")
file( GLOB STATIC_LIBS "${CPACK_PREPACKAGE}/lib/*.a"
		       "${CPACK_PREPACKAGE}/lib/*.lib"
		       "${CPACK_PREPACKAGE}/lib/*.dll" )
		   

if ( NOT "${STATIC_LIBS}" STREQUAL "" )
    file( REMOVE ${STATIC_LIBS} )
endif()


#
# Remove include files from packaging directory
#
message( STATUS "Removing ${CPACK_PREPACKAGE}/include" )
file( REMOVE_RECURSE "${CPACK_PREPACKAGE}/include" )

#
# Install system .SO dependencies
#
if(UNIX)

    #
    # Under Rocky Linux, DSOs sometimes go to lib64/.
    # Copy them to lib/ directory
    #
    set(linux_lib64_dir "${CPACK_PREPACKAGE}/lib64")
    if (EXISTS "${linux_lib64_dir}" )
	# For pyFLTK we need to install all libfltk DSOs including those we
	# do not use, like forms.
	message( NOTICE "${linux_lib64_dir} exists...")
	file(GLOB fltk_dsos "${linux_lib64_dir}/libfltk*.so")
	file(INSTALL
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/lib"
	    TYPE SHARED_LIBRARY
	    FOLLOW_SYMLINK_CHAIN
	    FILES ${fltk_dsos}
	)
	file(REMOVE_RECURSE ${linux_lib64_dir})
    else()
	message( NOTICE "${linux_lib64_dir} does not exist...")
    endif()
	
    #
    # Glob python directories as we don't know the version of python in this
    # script, as it does not inherit variables from the main CMakeLists.
    #
    file(GLOB MRV2_PYTHON_LIB_DIRS "${CPACK_PREPACKAGE}/lib/python*")

    #
    # Grab the last version, in case there are several
    #
    set(MRV2_PYTHON_LIBDIR "")
    if (MRV2_PYTHON_LIB_DIRS STREQUAL "")
	message(WARNING "Could not locate any python version in:")
	message(FATAL_ERROR "${CPACK_PREPACKAGE}/lib/python*")
    else()
	list(GET MRV2_PYTHON_LIB_DIRS -1 MRV2_PYTHON_LIB_DIR)
    endif()
    
    set(MRV2_PYTHON_SITE_PACKAGES_DIR "${MRV2_PYTHON_LIB_DIR}/site-packages")

    set( MRV2_EXES "${CPACK_PREPACKAGE}/bin/mrv2" )
    
	
    #
    # We need to get the dependencies of the python DSOs to avoid
    # issues like openssl and libcrypto changing between Rocky Linux
    # and Ubuntu 22.04.5 or macOS's libb2.
    #
    set(MRV2_PYTHON_DSO_DIR "${MRV2_PYTHON_LIB_DIR}/lib-dynload")
    file(GLOB python_dsos "${MRV2_PYTHON_DSO_DIR}/*.so")
    list(APPEND MRV2_EXES ${python_dsos} )
	
    if ( APPLE )
	#
	# Get DYLIB dependencies of componenets
	#
	get_macos_runtime_dependencies( "${MRV2_EXES}" )
    else()
	#
	# Get DSO dependencies of componenets
	#
	get_runtime_dependencies( "${MRV2_EXES}" )
    endif()
elseif(WIN32)
    #
    # Set python's site-packages dir for .exe installer.
    #
    # When building an .exe installer on Windows, the site-packages will
    # be inside an applications component directory.
    #
    set(MRV2_APP_DIR "${CPACK_PREPACKAGE}/applications")

    if (EXISTS "${MRV2_APP_DIR}/bin/mrv2.exe")
	file(CREATE_LINK
	    "${MRV2_APP_DIR}/bin/mrv2.exe"
	    "${MRV2_APP_DIR}/bin/mrv2-v${mrv2_VERSION}.exe") 
    endif()
    
    set(MRV2_PYTHON_APP_LIB_DIR "${MRV2_APP_DIR}/bin/Lib")
    set(MRV2_PYTHON_SITE_PACKAGES_DIR
	"${MRV2_PYTHON_APP_LIB_DIR}/site-packages")
    
    #
    # Don't pack sphinx and other auxiliary documentation libs in .exe
    #
    file(GLOB MRV2_UNUSED_PYTHON_DIRS
	"${MRV2_APP_DIR}/lib/Lib/"
	"${MRV2_PYTHON_APP_LIB_DIR}/test*"
	"${MRV2_PYTHON_APP_LIB_DIR}/config*"
	"${MRV2_PYTHON_APP_LIB_DIR}/ctypes/test*"
	"${MRV2_PYTHON_APP_LIB_DIR}/distutils/test*"
	"${MRV2_PYTHON_APP_LIB_DIR}/idlelib/idle_test*"
	"${MRV2_PYTHON_APP_LIB_DIR}/lib2to3/test*"
	"${MRV2_PYTHON_APP_LIB_DIR}/sqlite3/test*"
	"${MRV2_PYTHON_APP_LIB_DIR}/tkinter/test*"
	"${MRV2_PYTHON_APP_LIB_DIR}/unittest*"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/alabaster*"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/babel*"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/colorama*"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/docutils*"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/fltk14/*.cpp"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/fltk14/*.h"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/imagesize*"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/Jinja*"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/MarkupSafe*"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/pillow*"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/PIL*"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/polib*"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/Pygments*"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/snowballstemmer*"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/sphinx*"
	"${MRV2_PYTHON_SITE_PACKAGES_DIR}/unittest*")

    foreach( _dir ${MRV2_UNUSED_PYTHON_DIRS} )
	remove_pycache_directories(${_dir})
	message(STATUS "Removing ${_dir}")
    endforeach()
	
    if ( NOT "${MRV2_UNUSED_PYTHON_DIRS}" STREQUAL "" )
	file( REMOVE_RECURSE ${MRV2_UNUSED_PYTHON_DIRS} )
    endif()

    #
    # Set python's site-packages and lib dir for .zip.
    #
    # When building an .exe on Windows, the site-packages will
    # be inside an application directory.
    #
    set(MRV2_PYTHON_LIB_DIR "${CPACK_PREPACKAGE}/bin/Lib")
    set(MRV2_PYTHON_SITE_PACKAGES_DIR
	"${CPACK_PREPACKAGE}/bin/Lib/site-packages")
endif()


#
# Don't pack sphinx and other auxiliary documentation libs nor the tests
# for the libraries.
#
message(STATUS "Removing tests from ${MRV2_PYTHON_LIB_DIR}")
message(STATUS "Removing unneeded packages from ${MRV2_PYTHON_SITE_PACKAGES_DIR}")
file(GLOB MRV2_UNUSED_PYTHON_DIRS
    "${CPACK_PREPACKAGE}/lib/Lib/"
    "${MRV2_PYTHON_LIB_DIR}/test*"
    "${MRV2_PYTHON_LIB_DIR}/config*"
    "${MRV2_PYTHON_LIB_DIR}/ctypes/test*"
    "${MRV2_PYTHON_LIB_DIR}/distutils/test*"
    "${MRV2_PYTHON_LIB_DIR}/idlelib/idle_test*"
    "${MRV2_PYTHON_LIB_DIR}/lib2to3/test*"
    "${MRV2_PYTHON_LIB_DIR}/sqlite3/test*"
    "${MRV2_PYTHON_LIB_DIR}/tkinter/test*"
    "${MRV2_PYTHON_LIB_DIR}/unittest*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/alabaster*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/babel*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/Babel*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/colorama*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/docutils*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/fltk14/*.cpp"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/fltk14/*.h"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/imagesize*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/Jinja*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/jinja2*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/markupsafe*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/MarkupSafe*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/pillow*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/PIL*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/polib*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/pygments*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/Pygments*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/snowballstemmer*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/sphinx*"
    "${MRV2_PYTHON_SITE_PACKAGES_DIR}/unittest*")

if ( NOT "${MRV2_UNUSED_PYTHON_DIRS}" STREQUAL "" )
    foreach( _dir ${MRV2_UNUSED_PYTHON_DIRS} )
	remove_pycache_directories(${_dir})
	message(STATUS "Removing ${_dir}")
    endforeach()
    file( REMOVE_RECURSE ${MRV2_UNUSED_PYTHON_DIRS} )
endif()
