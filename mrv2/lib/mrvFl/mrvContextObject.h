// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#pragma once

#include <tlCore/Util.h>

#include <memory>

namespace tl
{
    namespace system
    {
        class Context;
    }
} // namespace tl

namespace mrv
{
    using namespace tl;

    //! Context object.
    class ContextObject
    {
    public:
        ContextObject(const std::shared_ptr<system::Context>&);

        ~ContextObject();

        //! Get the context.
        const std::shared_ptr<system::Context>& context() const;

        static void timerEvent_cb(void*);

    protected:
        void timerEvent();

    private:
        TLRENDER_PRIVATE();
    };
} // namespace mrv
