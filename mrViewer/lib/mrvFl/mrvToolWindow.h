#pragma once

/* fltk includes */
#include <FL/Fl.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Double_Window.H>

#include "mrvPack.h"

namespace mrv
{

    class ToolWindow : public Fl_Double_Window
    {
#define TW_MAX_FLOATERS	16

    protected:
	void create_dockable_window(void);
	short idx;
	static ToolWindow* active_list[TW_MAX_FLOATERS];
	static short active;

        enum Direction
        {
            kNone = 0,
            kRight = 1,
            kLeft  = 2,
            kTop   = 4,
            kBottom = 8,
            kTopRight = kTop | kRight,
            kTopLeft  = kTop | kLeft,
            kBottomRight = kBottom | kRight,
            kBottomLeft  = kBottom | kLeft
        };
        
        int last_x, last_y;
        Direction dir;
        int    valid;


        void set_cursor(int ex, int ey);
        
    public:
	// Normal FLTK constructors
	ToolWindow(int w, int h, const char *l = 0);
	ToolWindow(int x, int y, int w, int h, const char *l = 0);
	
	// destructor
	~ToolWindow();

        int handle( int event ) override;

	// methods for hiding/showing *all* the floating windows
	static void show_all(void);
	static void hide_all(void);

    };

} // namespace mrv
