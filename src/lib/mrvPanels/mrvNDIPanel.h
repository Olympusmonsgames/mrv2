// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#pragma once

#include "mrvPanelWidget.h"

class ViewerUI;
class Fl_Menu_Item;
class Fl_Toggle_Button;

namespace mrv
{
#ifdef TLRENDER_NDI
    namespace panel
    {
        class NDIPanel : public PanelWidget
        {
        public:
            NDIPanel(ViewerUI* ui);
            ~NDIPanel();

            void add_controls() override;

            static void refresh_sources_cb(void* v);
            void refresh_sources();

        protected:
            void _ndi_input(const Fl_Menu_Item*);
            void _ndi_output(Fl_Toggle_Button*);

        private:
            MRV2_PRIVATE();
        };

    } // namespace panel
#endif // TLRENDER_NDI
} // namespace mrv
