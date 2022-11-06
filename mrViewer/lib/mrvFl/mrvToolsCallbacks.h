#pragma once

#include <memory>
#include <vector>

#include "tlCore/Util.h"

#include "mrvFilesTool.h"
#include "mrvColorTool.h"
#include "mrvCompareTool.h"
#include "mrvSettingsTool.h"

class ViewerUI;
class Fl_Widget;

namespace mrv
{
    
    extern ColorTool*    colorTool;
    extern FilesTool*     filesTool;
    extern CompareTool*  compareTool;
    extern SettingsTool* settingsTool;
    
    void color_tool_grp( Fl_Widget* w, ViewerUI* ui );
    void files_tool_grp( Fl_Widget* w, ViewerUI* ui );
    void compare_tool_grp( Fl_Widget* w, ViewerUI* ui );
    void settings_tool_grp( Fl_Widget* w, ViewerUI* ui );
    
} // namespace mrv
