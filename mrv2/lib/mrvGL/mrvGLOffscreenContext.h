// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#pragma once

#include <memory>

#include <tlCore/Util.h>

namespace mrv
{
    //
    // This class implements an offscreen OpenGL context
    //
    class OffscreenContext
    {
    public:
        OffscreenContext();
        ~OffscreenContext();

        void init();
        void make_current();
        void release();

    protected:
        void create_gl_window();

        TLRENDER_PRIVATE();
    };
} // namespace mrv
