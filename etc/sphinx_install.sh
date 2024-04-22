#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# mrv2
# Copyright Contributors to the mrv2 Project. All rights reserved.

#
# This script installs with pip the needed modules for documentation
#

export PYTHON=`ls $BUILD_DIR/install/bin/python* | grep -o 'python.*' | head -1 `
if [[ ! -d $BUILD_DIR/install/lib/$PYTHON/site-packages/sphinx ]]; then
    run_cmd $BUILD_DIR/install/bin/$PYTHON -m pip install --upgrade pip --no-cache-dir
    run_cmd $BUILD_DIR/install/bin/$PYTHON -m pip install --upgrade sphinx sphinx_rtd_theme requests urllib3 --no-cache-dir
fi
