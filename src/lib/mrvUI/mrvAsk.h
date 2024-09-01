// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#pragma once

#include <FL/Enumerations.H>
#include <FL/fl_attr.h>

class Fl_Widget;

namespace mrv
{

    // fl_ask() is deprecated since it uses "Yes" and "No" for the buttons,
    // which does not conform to the current FLTK Human Interface Guidelines.
    // Use fl_choice() instead with the appropriate verbs instead.
    int fl_choice(
        const char* q, const char* b0, const char* b1, const char* b2, ...)
        __fl_attr((__format__(__printf__, 1, 5)));
    const char* fl_input(const char* label, const char* deflt = 0, ...)
        __fl_attr((__format__(__printf__, 1, 3)));
    void fl_alert(const char*, ...) __fl_attr((__format__(__printf__, 1, 2)));

    Fl_Widget* fl_message_icon();
    extern Fl_Font fl_message_font_;
    extern Fl_Fontsize fl_message_size_;
    inline void fl_message_font(Fl_Font f, Fl_Fontsize s)
    {
        fl_message_font_ = f;
        fl_message_size_ = s;
    }

    // pointers you can use to change FLTK to another language:
    extern const char* fl_no;
    extern const char* fl_yes;
    extern const char* fl_ok;
    extern const char* fl_cancel;
    extern const char* fl_close;

} // namespace mrv
