# SPDX-License-Identifier: BSD-3-Clause
# mrv2 (mrv2)
# Copyright Contributors to the mrv2 Project. All rights reserved.

#
# Common CPACK options to all generators
#
string(TIMESTAMP THIS_YEAR "%Y")

set( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/../LICENSE" )
set( CPACK_PACKAGE_VERSION_MAJOR "${mrv2_VERSION_MAJOR}" )
set( CPACK_PACKAGE_VERSION_MINOR "${mrv2_VERSION_MINOR}" )
set( CPACK_PACKAGE_VERSION_PATCH "${mrv2_VERSION_PATCH}" )
set( CPACK_PACKAGE_VERSION "${mrv2_VERSION}")
set( CPACK_PACKAGE_CONTACT "ggarra13@gmail.com")


#
# Experimental support in CPack for multithreading. 0 uses all cores.
#
set( CPACK_THREADS 0 )
    
set( MRV2_OS_BITS 32 )
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set( MRV2_OS_BITS 64 )
        set( MRV2_ARCHITECTURE "arm64")
    else()
        set( MRV2_ARCHITECTURE "arm")
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^mips.*")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set( MRV2_OS_BITS 64 )
        set( MRV2_ARCHITECTURE "mips64el")
    else()
        set( MRV2_ARCHITECTURE "mipsel")
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^ppc.*")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set( MRV2_ARCHITECTURE "ppc64le")
    else()
        message(FATAL_ERROR "Architecture is not supported")
    endif()
else()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set( MRV2_OS_BITS 64 )
        set( MRV2_ARCHITECTURE "amd64")
    else()
        set( MRV2_ARCHITECTURE "x86")
    endif()
endif()

set( mrv2ShortName "mrv2-v${mrv2_VERSION}-${CMAKE_SYSTEM_NAME}-${MRV2_OS_BITS}" )
set( CPACK_PACKAGE_NAME mrv2 )
set( CPACK_PACKAGE_VENDOR "Film Aura, S.A." )
set( CPACK_PACKAGE_DESCRIPTION_SUMMARY "Professional media player.")
set( CPACK_PACKAGE_INSTALL_DIRECTORY ${mrv2ShortName} )
set( CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-v${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${MRV2_ARCHITECTURE} )

#
# This is the mrv2/ subdir
#
set(MRV2_DIR ${CMAKE_SOURCE_DIR})

#
# This is the root of mrv2
#
file(REAL_PATH "${MRV2_DIR}/.." MRV2_ROOT)

#
# \@bug:
# This dummy (empty) install script is needed so variables get passed to
# the CPACK_PRE_BUILD_SCRIPTS.
#
set( CPACK_INSTALL_SCRIPT ${MRV2_ROOT}/cmake/dummy.cmake )

#
# This pre-build script does some cleaning of files in packaging area to
# keep installers smaller.
#
set( CPACK_PRE_BUILD_SCRIPTS ${MRV2_ROOT}/cmake/prepackage.cmake )

if( APPLE )
    set(CPACK_GENERATOR Bundle )

    set(CPACK_DMG_FORMAT "UDBZ")
    set(CPACK_DMG_FILESYSTEM "APFS")

    set( INSTALL_NAME ${PROJECT_NAME} )

    configure_file(
	${MRV2_DIR}/etc/macOS/Info.plist.in
	${PROJECT_BINARY_DIR}/Info.plist )

    set(CPACK_PACKAGE_ICON ${MRV2_DIR}/etc/macOS/mrv2.icns )
    set(CPACK_BUNDLE_NAME ${INSTALL_NAME} )
    set(CPACK_BUNDLE_ICON ${MRV2_DIR}/etc/macOS/mrv2.icns )
    set(CPACK_BUNDLE_PLIST ${PROJECT_BINARY_DIR}/Info.plist )
    set(CPACK_BUNDLE_STARTUP_COMMAND ${MRV2_DIR}/etc/macOS/startup.sh)
elseif(UNIX)
    
    #
    # Linux generators
    #
    set(CPACK_GENERATOR DEB RPM TGZ)
    
    #
    # Linux icon and .desktop shortcut
    #

    #
    # This desktop is the one placed on the desktop for X11/Wayland version
    # shortcuts.
    #
    configure_file( ${MRV2_DIR}/etc/Linux/mrv2.desktop.in
	"${PROJECT_BINARY_DIR}/etc/mrv2-v${mrv2_VERSION}.desktop" )

    #
    # This desktop file is for Wayland to set its icon correctly.
    #
    configure_file( ${MRV2_DIR}/etc/Linux/mrv2.main.desktop.in
	"${PROJECT_BINARY_DIR}/etc/mrv2.desktop" )

    install(FILES "${PROJECT_BINARY_DIR}/etc/mrv2-v${mrv2_VERSION}.desktop"
	DESTINATION share/applications COMPONENT applications)
    install(FILES "${PROJECT_BINARY_DIR}/etc/mrv2.desktop"
	DESTINATION share/applications COMPONENT applications)
    install(DIRECTORY ${MRV2_DIR}/share/icons
	DESTINATION share/ COMPONENT applications)

    set(CPACK_INSTALL_PREFIX /usr/local/${mrv2ShortName})

    #
    # Linux post-install and post-remove scripts to handle versioning and
    # installation of .desktop shortcut on the user's Desktop.
    #
    configure_file(
	${MRV2_DIR}/etc/Linux/postinst.in
	${PROJECT_BINARY_DIR}/etc/Linux/postinst
	@ONLY)
    configure_file(
	${MRV2_DIR}/etc/Linux/postrm.in
	${PROJECT_BINARY_DIR}/etc/Linux/postrm
	@ONLY)

    #
    # set Debian options.
    #
    execute_process(
	COMMAND dpkg --print-architecture
	OUTPUT_VARIABLE DEB_ARCHITECTURE
	OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(CPACK_DEBIAN_PACKAGE_NAME ${PROJECT_NAME}-v${mrv2_VERSION})
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${DEB_ARCHITECTURE})
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
	"${PROJECT_BINARY_DIR}/etc/Linux/postinst"
	"${PROJECT_BINARY_DIR}/etc/Linux/postrm")

    set(CPACK_DEBIAN_FILE_NAME	"${CPACK_PACKAGE_FILE_NAME}.deb" )

    #
    # Set RPM options.
    #
    set(CPACK_RPM_PACKAGE_NAME ${PROJECT_NAME}-${mrv2_VERSION})

    set(CPACK_RPM_PACKAGE_RELOCATABLE true)
    set(CPACK_RPM_PACKAGE_AUTOREQ false)
    set(CPACK_RPM_PACKAGE_AUTOPROV true)
    set(CPACK_RPM_COMPRESSION_TYPE gzip )
    execute_process(
	COMMAND uname -m
	OUTPUT_VARIABLE RPM_ARCHITECTURE
	OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set( CPACK_RPM_PACKAGE_ARCHITECTURE ${RPM_ARCHITECTURE} )

    set(
	CPACK_RPM_POST_INSTALL_SCRIPT_FILE
	${PROJECT_BINARY_DIR}/etc/Linux/postinst)
    set(
	CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE
	${PROJECT_BINARY_DIR}/etc/Linux/postrm)
      
     # Undocumented option used to avoid .build-id libs listing
     set(CPACK_RPM_SPEC_MORE_DEFINE "%define _build_id_links none")

     # \@bug: According to docs it is not needed, but
     #        RPM packaging won't work properly without it.   
     set(CPACK_SET_DESTDIR true) 
else()

    # Create debug directory for .pdb files
    if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR
	    CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
	list(PREPEND CPACK_PRE_BUILD_SCRIPTS ${MRV2_ROOT}/cmake/copy_pdbs.cmake )
    endif()

    
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "mrv2-v${mrv2_VERSION}" )
    
    # There is a bug in NSIS that does not handle full unix paths properly. Make
    # sure there is at least one set of four (4) backlasshes.
    set(CPACK_NSIS_MODIFY_PATH ON)

    set(CPACK_GENERATOR NSIS ZIP)

    #
    # This sets the title at the top of the installer.
    #
    set(CPACK_NSIS_PACKAGE_NAME "mrv2 v${mrv2_VERSION} ${CMAKE_SYSTEM_NAME}-${MRV2_OS_BITS}" )
    
    #
    # Set the executable
    #
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/mrv2.exe")

    #
    # Set the MUI Installer icon
    #
    set(CPACK_NSIS_MUI_ICON "${MRV2_DIR}/main/app.ico")
    set(CPACK_NSIS_MUI_UNICON "${MRV2_DIR}/main/app.ico")

    #
    # Set the MUI banner to use a custom mrv2 one.
    #
    set(MUI_HEADERIMAGE "${MRV2_ROOT}/cmake/nsis/NSIS_background.bmp")
    file(TO_NATIVE_PATH "${MUI_HEADERIMAGE}" MUI_HEADERIMAGE)
    string(REPLACE "\\" "\\\\" MUI_HEADERIMAGE "${MUI_HEADERIMAGE}")
    set(CPACK_NSIS_MUI_HEADERIMAGE "${MUI_HEADERIMAGE}")

    #
    # Default location for installation.
    #
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")

    #
    # This sets the name in Windows Apps and Control Panel.
    #
    set(mrv2_DISPLAY_NAME "mrv2-${MRV2_OS_BITS} v${mrv2_VERSION}")
    
    set(CPACK_NSIS_DISPLAY_NAME "${mrv2_DISPLAY_NAME}" )

    set(CPACK_PACKAGE_EXECUTABLES "mrv2" "${mrv2_DISPLAY_NAME}")
    set(CPACK_CREATE_DESKTOP_LINKS "mrv2" "${mrv2_DISPLAY_NAME}")

    #
    # To call uninstall first if the same version has been installed.
    #
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON )

    include("${MRV2_ROOT}/cmake/nsis/NSISRegistry.cmake")

endif()

#
# For Windows installer, handle the components
#
set(mrv2_COMPONENTS
    applications
    documentation
)

if(BUILD_PYTHON)
    list(APPEND mrv2_COMPONENTS 
	python_demos
	python_tk
    )
endif()

set(CPACK_COMPONENTS_ALL ${mrv2_COMPONENTS})
set(CPACK_COMPONENT_APPLICATIONS_DISPLAY_NAME "mrv2 Application")
set(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "mrv2 Documentation")
if(BUILD_PYTHON)
    set(CPACK_COMPONENT_PYTHON_DEMOS_DISPLAY_NAME "mrv2 Python Demos")
    set(CPACK_COMPONENT_PYTHON_TK_DISPLAY_NAME "Python TK Libraries")
    set(CPACK_COMPONENT_PYTHON_TK_DISABLED TRUE)
endif()


#
# Create the 'package' target
#
include(CPack)

