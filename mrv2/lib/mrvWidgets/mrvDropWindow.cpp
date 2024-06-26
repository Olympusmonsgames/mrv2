// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#include <FL/Fl.H>

#include "mrvDropWindow.h"

namespace mrv
{

    // basic fltk constructors
    DropWindow::DropWindow(int x, int y, int w, int h, const char* l) :
        Fl_Double_Window(x, y, w, h, l)
    {
        init();
    }

    DropWindow::DropWindow(int w, int h, const char* l) :
        Fl_Double_Window(w, h, l)
    {
        init();
    }

    void DropWindow::init(void)
    {
        dock = (DockGroup*)nullptr;
        workspace = (Fl_Flex*)nullptr;
    }

    int DropWindow::valid_drop()
    {
        int out = 0;
        
        // Is this a dock_drop event?
        if (dock)
        {
            // Did the drop happen on us?
            // Get our co-ordinates
            int ex = x_root() + dock->x();
            int ey = y_root() + dock->y();
            int ew = dock->w();
            int eh = dock->h();
            
            // get the drop event co-ordinates
            int cx = Fl::event_x_root();
            int cy = Fl::event_y_root();

            // Is the event inside the boundary of this window?
            if (visible() && (cx > ex) && (cy > ey) && (cx < (ew + ex)) &&
                (cy < (eh + ey)))
            {
                out = 1;
            }
        }
        return out;
    }

} // namespace mrv
