// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#include <string>

#include "mrViewer.h"

namespace mrv
{
    std::string getMatchingLayer(const std::string& layer, const ViewerUI* ui)
    {
        size_t pos;
        std::string out = layer;
        if ((pos = out.find("left")) != std::string::npos)
            out.replace(pos, 4, "right");
        if (out != layer)
            return out;
        if ((pos = out.find("right")) != std::string::npos)
            out.replace(pos, 5, "left");
        if (out != layer)
            return out;
        if ((pos = out.find("L.")) != std::string::npos)
            out.replace(pos, 2, "R.");
        if (out != layer)
            return out;
        if ((pos = out.find("R.")) != std::string::npos)
            out.replace(pos, 2, "L.");
        if (out != layer)
            return out;

        size_t numLayers = ui->uiColorChannel->children();
        for (size_t i = 0; i < numLayers; ++i)
        {
            const char* label = ui->uiColorChannel->child(i)->label();
            if (!label)
                continue;
            out = label;
            if (((pos = out.find("left")) != std::string::npos) ||
                ((pos = out.find("right")) != std::string::npos) ||
                ((pos = out.find("L.")) != std::string::npos) ||
                ((pos = out.find("R.")) != std::string::npos))
                return out;
        }

        return layer;
    }
} // namespace mrv
