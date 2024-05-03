# SPDX-License-Identifier: BSD-3-Clause
# mrv2 
# Copyright Contributors to the mrv2 Project. All rights reserved.

include(ExternalProject)

set(mrv2_ARGS
    ${TLRENDER_EXTERNAL_ARGS}
    -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHIECTURES}
    -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
    -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
    -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
    -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}

    -DCMAKE_INSTALL_MESSAGE=${CMAKE_INSTALL_MESSAGE}

    -DTLRENDER_PYTHON=${TLRENDER_PYTHON}
    -DTLRENDER_API=${TLRENDER_API}
    -DTLRENDER_BMD=${TLRENDER_BMD}
    -DTLRENDER_BMD_SDK=${TLRENDER_BMD_SDK}
    -DTLRENDER_EXR=${TLRENDER_EXR}
    -DTLRENDER_FFMPEG=${TLRENDER_FFMPEG}
    -DTLRENDER_GL=${TLRENDER_GL}
    -DTLRENDER_JPEG=${TLRENDER_JPEG}
    -DTLRENDER_NDI=${TLRENDER_NDI}
    -DTLRENDER_NDI_SDK=${TLRENDER_NDI_SDK}
    -DTLRENDER_NET=${TLRENDER_NET}
    -DTLRENDER_OCIO=${TLRENDER_OCIO}
    -DTLRENDER_PNG=${TLRENDER_PNG}
    -DTLRENDER_RAW=${TLRENDER_RAW}
    -DTLRENDER_STB=${TLRENDER_STB}
    -DTLRENDER_TIFF=${TLRENDER_TIFF}
    -DTLRENDER_USD=${TLRENDER_USD}
    -DTLRENDER_WAYLAND=${TLRENDER_WAYLAND}
    -DTLRENDER_X11=${TLRENDER_X11}
    -DTLRENDER_X264=${TLRENDER_X264}

    -DBUILD_PYTHON=${BUILD_PYTHON}
    -DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE}
    -DPYBIND11_PYTHON_VERSION=${PYTHON_VERSION}

    -DMRV2_NETWORK=${MRV2_NETWORK}
    -DMRV2_PYBIND11=${MRV2_PYBIND11}
    -DMRV2_PDF=${MRV2_PDF}
    -DMRV2_PYFLTK=${MRV2_PYFLTK}

    # defined when FLTK is built
    -DFLTK_BUILD_SHARED_LIBS=${FLTK_BUILD_SHARED_LIBS}  
)

ExternalProject_Add(
    mrv2
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/mrv2
    DEPENDS tlRender ${FLTK_DEP} ${PYTHON_DEP} ${PYBIND11_DEP} ${pyFLTK_DEP} ${POCO_DEP} ${LibHaru_DEP} ${Gettext}
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/mrv2
    LIST_SEPARATOR |
    CMAKE_ARGS ${mrv2_ARGS}
)
