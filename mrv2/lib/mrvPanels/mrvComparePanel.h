// SPDX-License-Identifier: BSD-3-Clause
// mrv2 (mrViewer2)
// Copyright Contributors to the mrv2 Project. All rights reserved.

#pragma once

#include <tlCore/Time.h>

#include <vector>

#include "mrvPanelWidget.h"

class ViewerUI;
class Fl_RGB_Image;

namespace mrv {
using namespace tl;

class ClipButton;
class HorSlider;

class ComparePanel : public PanelWidget {
public:
  HorSlider *wipeX;
  HorSlider *wipeY;
  HorSlider *wipeRotation;
  HorSlider *overlay;

public:
  ComparePanel(ViewerUI *ui);
  ~ComparePanel();

  void clear_controls();
  void add_controls() override;

  void redraw();

  void refresh();
  void compareThumbnail(
      const int64_t id,
      const std::vector<std::pair<otime::RationalTime, Fl_RGB_Image *>>
          &thumbnails,
      ClipButton *w);

protected:
  void cancel_thumbnails();

private:
  struct Private;
  std::unique_ptr<Private> _r;
};

} // namespace mrv
