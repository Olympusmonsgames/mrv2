// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#include "mrvFl/mrvContextObject.h"

#include <tlCore/Context.h>

#include <FL/Fl.H>

#include <atomic>

#include "mrvFl/mrvIO.h"

namespace mrv
{
    using namespace tl;

    namespace
    {
        const double kTimeout = 0.005;
        const char* kModule = "ctxobj";
    } // namespace

    struct ContextObject::Private
    {
        std::shared_ptr<system::Context> context;
    };

    ContextObject::ContextObject(
        const std::shared_ptr<system::Context>& context) :
        _p(new Private)
    {
        _p->context = context;

        Fl::add_timeout(kTimeout, (Fl_Timeout_Handler)timerEvent_cb, this);
    }

    ContextObject::~ContextObject()
    {
        Fl::remove_timeout((Fl_Timeout_Handler)timerEvent_cb, this);
    }

    const std::shared_ptr<system::Context>& ContextObject::context() const
    {
        return _p->context;
    }

    void ContextObject::timerEvent()
    {
        _p->context->tick();

        Fl::repeat_timeout(kTimeout, (Fl_Timeout_Handler)timerEvent_cb, this);
    }

    void ContextObject::timerEvent_cb(void* d)
    {
        ContextObject* t = static_cast< ContextObject* >(d);
        t->timerEvent();
    }
} // namespace mrv
