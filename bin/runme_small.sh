#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# mrv2
# Copyright Contributors to the mrv2 Project. All rights reserved.


#
#
# Small build script for mrv2.  Run it from the root of the mrv2 dir, like:
#
# ./bin/runme_small.sh
#
# It builds all dependencies and will install
# the main executable on BUILD_DIR (by default
#                                   BUILD-OS-ARCH/BUILD_TYPE/install/bin).
#
# The small build does not have Python, USD, PDF, NDI or NETWORK support.
# It is intended for solo artists.
# 
#
# On Linux and macOS, it will also create a mrv2 or mrv2-dbg symbolic link
# in $HOME/bin if the directory exists.
#
# It will also log the compilation on $BUILD_DIR/compile.log
#


#
# Store the parameters for passing them later
#
params=$*

#
# Find out our build dir
#
. etc/build_dir.sh


#
# Clear the flags, as they will be set by runme_nolog.sh.
#
export FLAGS=""
export CMAKE_FLAGS=""

#
# Common flags
#
if [ -z "$BUILD_PYTHON" ]; then
    export BUILD_PYTHON=OFF
fi

#
# These are some of the expensive mrv2 options
#

if [ -z "$MRV2_PYFLTK" ]; then
    export MRV2_PYFLTK=OFF
fi

if [ -z "$MRV2_PYBIND11" ]; then
    export MRV2_PYBIND11=OFF
fi

if [ -z "$MRV2_NETWORK" ]; then
    export MRV2_NETWORK=OFF
fi

if [ -z "$MRV2_PDF" ]; then
    export MRV2_PDF=OFF
fi

#
# These are some of the expensive TLRENDER options
#
export TLRENDER_AV1=ON
export TLRENDER_FFMPEG=ON
export TLRENDER_FFMPEG_MINIMAL=ON
export TLRENDER_EXR=ON
export TLRENDER_JPEG=ON
export TLRENDER_NDI=OFF
export TLRENDER_NET=OFF
export TLRENDER_RAW=ON
export TLRENDER_SGI=ON
export TLRENDER_STB=ON
export TLRENDER_TIFF=ON
export TLRENDER_USD=OFF
export TLRENDER_VPX=ON
export TLRENDER_WAYLAND=ON
export TLRENDER_X11=ON
export TLRENDER_YASM=ON

echo
echo "Saving compile log to $BUILD_DIR/compile.log ..."
echo
cmd="./etc/runme_nolog.sh --ask $params 2>&1 | tee $BUILD_DIR/compile.log"
run_cmd $cmd
