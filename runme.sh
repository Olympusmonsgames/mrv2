#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# mrv2
# Copyright Contributors to the mrv2 Project. All rights reserved.


#
#
# Main build script for mrv2.  It builds all dependencies and will install the
# main executable on BUILD_DIR (by default
#                               BUILD-OS-ARCH/BUILD_TYPE/install/bin).
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

# Reset library paths
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:/usr/local/lib64:/usr/local/lib:/usr/lib/x86_64-linux-gnu:/usr/lib64:/usr/lib"
export DYLD_LIBRARY_PATH="/usr/local/lib:/usr/lib"

#
# Find out our build dir
#
. etc/build_dir.sh

mkdir -p $BUILD_DIR


#
# Clear the flags, as they will be set again by runme_nolog.sh.
#
export FLAGS=""
export CMAKE_FLAGS=""


echo
echo "Saving compile log to $BUILD_DIR/compile.log ..."
cmd="./etc/runme_nolog.sh --ask $params 2>&1 | tee $BUILD_DIR/compile.log"
run_cmd $cmd
