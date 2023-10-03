#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# mrv2
# Copyright Contributors to the mrv2 Project. All rights reserved.



#
# Turn on exit on error
#
set -o pipefail -e

#
# Get the auxiliary functions
#
. etc/functions.sh

extract_version
extract_python_version

#
# Determine OS Kernel, OS CPU architecture
#

. etc/build_cores.sh

. etc/parse_args.sh


# Build a build directory with that information
export BUILD_DIR=$BUILD_ROOT/${CMAKE_BUILD_TYPE}

if [[ $KERNEL == *Darwin* ]]; then
    export PATH="/usr/local/opt/gnu-sed/libexec/gnubin:${PATH}"
    if [[ $ARCH == arm64 ]]; then
	export CMAKE_OSX_ARCHITECTURES=$ARCH
    fi
    if [[ $MRV2_DIST_RELEASE == 1 ]]; then
	export CMAKE_FLAGS="-DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 ${CMAKE_FLAGS}"
    fi
fi

if [[ $MRV2_DIST_RELEASE == 1 ]]; then
    export CMAKE_FLAGS="-D MRV2_DIST_RELEASE=ON ${CMAKE_FLAGS}"
fi

if [[ $FLAGS == "" ]]; then
    export FLAGS="-j ${CPU_CORES}"
fi
export FLAGS="${FLAGS} $*"

if [[ $CLEAN_DIR == 1 && $0 == *runme.sh* ]]; then
    if [[ -d ${BUILD_DIR} ]]; then
	echo "Cleaning ${BUILD_DIR}.  Please wait..."
	run_cmd rm -rf $BUILD_DIR
    fi
fi

export PATH="$PWD/${BUILD_DIR}/install/bin:$PWD/$BUILD_DIR/install/bin/Scripts:${PATH}"
export LD_LIBRARY_PATH="$PWD/${BUILD_DIR}/install/lib64:$PWD/${BUILD_DIR}/install/lib:${LD_LIBRARY_PATH}"
export DYLD_LIBRARY_PATH="$PWD/${BUILD_DIR}/install/lib:${DYLD_LIBRARY_PATH}"
export PKG_CONFIG_PATH="$PWD/${BUILD_DIR}/install/lib/pkgconfig:${PKG_CONFIG_PATH}"

if [[ $RUNME == 1 && $0 != *runme.sh* ]]; then
    echo "Build directory is ${BUILD_DIR}"
    echo "Version to build is v${mrv2_VERSION}"
    echo "Architecture is ${ARCH}"
    echo "Building with ${CPU_CORES} cores"
    if [[ $FFMPEG_GPL == "" ]]; then
	if [[ $KERNEL == *Msys* ]]; then
	    echo "Will use pre-built FFmpeg ${FFMPEG_GPL}"
	else
	    echo "FFmpeg will be built as LGPL"
	fi
    else
	echo "FFmpeg will be built as ${FFMPEG_GPL}"
    fi
    echo "CMake flags are ${CMAKE_FLAGS}"
    echo "Compiler flags are ${FLAGS}"
    cmake --version


    mkdir -p $BUILD_DIR/install
    
fi

if [[ $RUNME == 1 && $0 == *runme_nolog.sh* ]]; then
    if [[ $KERNEL == *Msys* ]]; then
	. $PWD/etc/compile_windows_dlls.sh
    fi
fi
