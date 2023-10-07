// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#pragma once

#include "mrvPanelWidget.h"

class ViewerUI;
class Fl_Menu_;

namespace mrv
{
    class PythonOutput;
    class PythonEditor;

    class PythonPanel : public PanelWidget
    {
    public:
        PythonPanel(ViewerUI* ui);
        ~PythonPanel();

        void add_controls() override;

        void create_menu();
        void create_menu(Fl_Menu_*);

        void run_code();

        void open_python_file(const std::string& file);
        void save_python_file(std::string file);

        void clear_output();
        void clear_editor();

        void toggle_line_numbers(Fl_Menu_*);

        void cut_text();
        void copy_text();
        void paste_text();

        void comment_text();
        void uncomment_text();

        void add_to_script_list(const std::string& file);
        void script_shortcut(unsigned int idx);

        static PythonOutput* output();
        static void run_code_cb(Fl_Menu_*, PythonPanel* o);
        static void open_python_file_cb(Fl_Menu_*, PythonPanel* o);
        static void save_python_file_cb(Fl_Menu_*, PythonPanel* o);

        static void cut_text_cb(Fl_Menu_*, PythonPanel* o);
        static void copy_text_cb(Fl_Menu_*, PythonPanel* o);
        static void paste_text_cb(Fl_Menu_*, PythonPanel* o);

        static void delete_cb(Fl_Menu_*, PythonEditor* o);

        static void find_cb(Fl_Menu_*, PythonEditor* o);
        static void find2_cb(Fl_Menu_*, PythonEditor* o);

        static void replall_cb(Fl_Menu_*, PythonEditor* o);
        static void replace_cb(Fl_Menu_*, PythonEditor* o);
        static void replace2_cb(Fl_Menu_*, PythonEditor* o);

        static void comment_text_cb(Fl_Menu_*, PythonPanel* o);
        static void uncomment_text_cb(Fl_Menu_*, PythonPanel* o);

        static void search_text_cb(Fl_Menu_*, PythonPanel* o);
        static void replace_text_cb(Fl_Menu_*, PythonPanel* o);

        static void clear_output_cb(Fl_Menu_*, PythonPanel* o);
        static void clear_editor_cb(Fl_Menu_*, PythonPanel* o);

        static void toggle_line_numbers_cb(Fl_Menu_*, PythonPanel* o);

        static void add_to_script_list_cb(Fl_Menu_* m, PythonPanel* p);
        static void script_shortcut_cb(Fl_Menu_* m, PythonPanel* d);

    private:
        void style_update(
            int pos,                 // I - Position of update
            int nInserted,           // I - Number of inserted chars
            int nDeleted,            // I - Number of deleted chars
            int nRestyled,           // I - Number of restyled chars
            const char* deletedText, // I - Text that was deleted
            void* cbArg);            // I - Callback data
        static void style_update_cb(
            int pos,                 // I - Position of update
            int nInserted,           // I - Number of inserted chars
            int nDeleted,            // I - Number of deleted chars
            int nRestyled,           // I - Number of restyled chars
            const char* deletedText, // I - Text that was deleted
            void* cbArg);            // I - Callback data

        MRV2_PRIVATE();
    };

} // namespace mrv
