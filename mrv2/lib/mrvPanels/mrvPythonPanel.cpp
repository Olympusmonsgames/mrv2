// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#include <FL/Fl_Tile.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Box.H>
#include <FL/fl_ask.H>

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
namespace fs = std::filesystem;

#include <pybind11/embed.h>
namespace py = pybind11;

#include <tlCore/StringFormat.h>

#include "mrvCore/mrvHome.h"

#include "mrvWidgets/mrvFunctional.h"
#include "mrvWidgets/mrvPythonOutput.h"
#include "mrvWidgets/mrvPythonEditor.h"

#include "mrvFl/mrvFileRequester.h"
#include "mrvFl/mrvIO.h"

#include "mrvPanels/mrvPanelsCallbacks.h"

#include "mrvPy/PyStdErrOutRedirect.h"

#include "mrvApp/mrvSettingsObject.h"

#include "mrViewer.h"

namespace
{
    const char* kModule = "pypanel";
}

namespace mrv
{

    static Fl_Text_Display::Style_Table_Entry kCodeStyles[] = {
        // Style table
        {FL_BLACK, FL_COURIER, FL_NORMAL_SIZE},             // A - Plain
        {FL_DARK_GREEN, FL_COURIER_ITALIC, FL_NORMAL_SIZE}, // B - Line comments
        {FL_DARK_GREEN, FL_COURIER_ITALIC,
         FL_NORMAL_SIZE},                                // C - Block comments
        {FL_BLUE, FL_COURIER, FL_NORMAL_SIZE},           // D - Strings
        {FL_DARK_RED, FL_COURIER, FL_NORMAL_SIZE},       // E - Directives
        {FL_RED, FL_COURIER_BOLD, FL_NORMAL_SIZE},       // F - Types
        {FL_BLUE, FL_COURIER_BOLD, FL_NORMAL_SIZE},      // G - Keywords
        {FL_DARK_GREEN, FL_COURIER_BOLD, FL_NORMAL_SIZE} // H - Functions
    };

    // Style table
    static Fl_Text_Display::Style_Table_Entry kLogStyles[] = {
        // FONT COLOR       FONT FACE   SIZE  ATTR
        // --------------- ------------ ---- ------
        {FL_BLACK, FL_HELVETICA, 14, 0},       // A - Info
        {FL_DARK_YELLOW, FL_HELVETICA, 14, 0}, // B - Warning
        {FL_RED, FL_HELVETICA, 14, 0}          // C - Error
    };

    //! We keep this global so the content won't be erased when the user
    //! closes the Python Panel
    static Fl_Text_Buffer* textBuffer = nullptr;
    static Fl_Text_Buffer* styleBuffer = nullptr;
    static PythonOutput* outputDisplay = nullptr;

    struct PythonPanel::Private
    {
        Fl_Tile* tile = nullptr;
        PythonEditor* pythonEditor = nullptr;
        Fl_Menu_Bar* menu = nullptr;
    };

    void PythonPanel::style_update_cb(
        int pos,                 // I - Position of update
        int nInserted,           // I - Number of inserted chars
        int nDeleted,            // I - Number of deleted chars
        int nRestyled,           // I - Number of restyled chars
        const char* deletedText, // I - Text that was deleted
        void* cbArg)             // I - Callback data
    {
        PythonPanel* p = static_cast< PythonPanel* >(cbArg);
        p->style_update(
            pos, nInserted, nDeleted, nRestyled, deletedText, nullptr);
    }

    //!
    //! 'style_update()' - Update the style buffer...
    //!
    void PythonPanel::style_update(
        int pos,                 // I - Position of update
        int nInserted,           // I - Number of inserted chars
        int nDeleted,            // I - Number of deleted chars
        int nRestyled,           // I - Number of restyled chars
        const char* deletedText, // I - Text that was deleted
        void* cbArg)             // I - Callback data
    {
        int start,  // Start of text
            end,    // End of text
            len;    // Length of text change
        char last,  // Last style on line
            *style, // Style data
            *text;  // Text data

        // If this is just a selection change, just unselect the style buffer...
        if (nInserted == 0 && nDeleted == 0)
        {
            styleBuffer->unselect();
            return;
        }

        // Track changes in the text buffer...
        if (nInserted > 0)
        {
            // Insert characters into the style buffer...
            style = new char[nInserted + 1];
            memset(style, 'A', nInserted);
            style[nInserted] = '\0';

            styleBuffer->replace(pos, pos + nDeleted, style);
            delete[] style;
        }
        else
        {
            // Just delete characters in the style buffer...
            styleBuffer->remove(pos, pos + nDeleted);
        }

        // Select the area that was just updated to avoid unnecessary
        // callbacks...
        styleBuffer->select(pos, pos + nInserted - nDeleted);

        // Re-parse the changed region; we do this by parsing from the
        // beginning of the line of the changed region to the end of
        // the line of the changed region...  Then we check the last
        // style character and keep updating if we have a multi-line
        // comment character...
        start = textBuffer->line_start(pos);
        end = textBuffer->line_end(pos + nInserted - nDeleted);
        text = textBuffer->text_range(start, end);
        style = styleBuffer->text_range(start, end);
        last = style[end - start - 1];
        len = end - start;

        PythonEditor::style_parse(text, style, len);

        if (len > 0)
        {
            styleBuffer->replace(start, end, style);
            _r->pythonEditor->redisplay_range(start, end);
        }

        if (last != style[end - start - 1])
        {
            // The last character on the line changed styles, so reparse the
            // remainder of the buffer...
            free(text);
            free(style);

            end = textBuffer->length();
            text = textBuffer->text_range(start, end);
            style = styleBuffer->text_range(start, end);
            len = end - start;

            PythonEditor::style_parse(text, style, len);

            if (len > 0)
            {
                styleBuffer->replace(start, end, style);
                _r->pythonEditor->redisplay_range(start, end);
            }
        }

        free(text);
        free(style);
    }

    PythonPanel::PythonPanel(ViewerUI* ui) :
        _r(new Private),
        PanelWidget(ui)
    {

        if (!textBuffer)
        {
            styleBuffer = new Fl_Text_Buffer;
            textBuffer = new Fl_Text_Buffer;
            outputDisplay = new PythonOutput(0, 0, 100, 100);
        }

        add_group("Python");

        Fl_SVG_Image* svg = load_svg("Python.svg");
        g->image(svg);

        g->callback(
            [](Fl_Widget* w, void* d)
            {
                ViewerUI* ui = static_cast< ViewerUI* >(d);
                delete pythonPanel;
                pythonPanel = nullptr;
                ui->uiMain->fill_menu(ui->uiMenuBar);
            },
            ui);
    }

    PythonPanel::~PythonPanel()
    {
#if __APPLE__
        TLRENDER_P();
        if (!p.ui->uiPrefs->uiPrefsMacOSMenus->value())
        {
            g->remove(_r->menu);
            if (_r->menu)
                g->remove(_r->menu);
            _r->menu = nullptr;
        }
#else
        _r->menu = new Fl_Menu_Bar(g->x(), g->y() + 20, g->w(), 20);
        if (_r->menu)
            g->remove(_r->menu);
        _r->menu = nullptr;
#endif
        textBuffer->remove_modify_callback(style_update_cb, this);
        _r->tile->remove(outputDisplay); // we make sure not to delete this
    }

    void PythonPanel::create_menu()
    {
        TLRENDER_P();

#if __APPLE__
        if (p.ui->uiPrefs->uiPrefsMacOSMenus->value())
        {
            _r->menu = p.ui->uiMenuBar;
        }
        else
        {
            delete _r->menu;
            _r->menu = new Fl_Menu_Bar(g->x(), g->y() + 20, g->w(), 20);
        }
#else
        delete _r->menu;
        _r->menu = new Fl_Menu_Bar(g->x(), g->y() + 20, g->w(), 20);
#endif
        create_menu(_r->menu);
    }

    void PythonPanel::create_menu(Fl_Menu_* menu)
    {
        TLRENDER_P();

        menu->clear();
        menu->add(
            _("&File/&Open"), FL_COMMAND + 'o',
            (Fl_Callback*)open_python_file_cb, this, FL_MENU_DIVIDER);
        menu->add(
            _("&File/&Save"), FL_COMMAND + 's',
            (Fl_Callback*)save_python_file_cb, this);
        menu->add(
            _("Edit/Cu&t"), FL_COMMAND + 'x', (Fl_Callback*)cut_text_cb, this);
        menu->add(
            _("Edit/&Copy"), FL_COMMAND + 'c', (Fl_Callback*)copy_text_cb,
            this);
        menu->add(
            _("Edit/&Paste"), FL_COMMAND + 'p', (Fl_Callback*)paste_text_cb,
            this);
        menu->add(
            _("Clear/&Output"), FL_COMMAND + 'k', (Fl_Callback*)clear_output_cb,
            this, FL_MENU_DIVIDER);
        menu->add(
            _("Clear/&Editor"), FL_COMMAND + 'e', (Fl_Callback*)clear_editor_cb,
            this);
        menu->add(
            _("Editor/&Run Code"), FL_KP_Enter, (Fl_Callback*)run_code_cb, this,
            FL_MENU_DIVIDER);
        menu->add(
            _("Editor/Toggle &Line Numbers"), 0,
            (Fl_Callback*)toggle_line_numbers_cb, this, FL_MENU_TOGGLE);
        menu->add(
            _("Scripts/Add To Script List"), 0,
            (Fl_Callback*)add_to_script_list_cb, this, FL_MENU_DIVIDER);

        auto settingsObject = p.ui->app->settingsObject();
        for (const auto fullfile : settingsObject->pythonScripts())
        {
            fs::path fullPath = fullfile;

            // Use the filename() member function to get only the filename
            fs::path filename = fullPath.filename();

            char buf[256];
            snprintf(buf, 256, _("Scripts/%s"), filename.string().c_str());

            menu->add(buf, 0, (Fl_Callback*)script_shortcut_cb, this);
        }

        menu->menu_end();
        Fl_Menu_Bar* bar = dynamic_cast< Fl_Menu_Bar* >(menu);
        if (bar)
            bar->update();
    }

    void PythonPanel::add_controls()
    {
        TLRENDER_P();

        g->clear();

        g->begin();

        create_menu();

        int H = g->h() - 20;
        int Y = 20;
        int M = (H - Y) / 2;

        _r->tile = new Fl_Tile(g->x(), g->y() + Y, g->w(), H + Y + 3);
        _r->tile->labeltype(FL_NO_LABEL);

        int dx = 20, dy = dx; // border width of resizable() - see below
        Fl_Box r(
            _r->tile->x() + dx, _r->tile->y() + dy, _r->tile->w() - 2 * dx,
            _r->tile->h() - 2 * dy);
        _r->tile->resizable(r);

        _r->tile->add(outputDisplay);

        outputDisplay->resize(g->x(), g->y() + Y, g->w(), M);

        PythonEditor* e;
        _r->pythonEditor = e =
            new PythonEditor(g->x(), g->y() + M + Y, g->w(), _r->tile->h() - M);
        e->box(FL_DOWN_BOX);
        e->textfont(FL_COURIER);
        e->textcolor(FL_BLACK);
        e->textsize(14);
        e->tooltip(
            _("Type in your python code here.  Select an area to execute just "
              "a portion of it.  Press Keypad Enter to run it."));
        Fl_Text_Buffer* oldBuffer = e->buffer();
        if (oldBuffer != textBuffer)
            delete oldBuffer;
        e->buffer(textBuffer);
        oldBuffer = e->style_buffer();
        if (oldBuffer != styleBuffer)
            delete oldBuffer;
        e->highlight_data(
            styleBuffer, kCodeStyles,
            sizeof(kCodeStyles) / sizeof(kCodeStyles[0]), 'A', 0, 0);
        textBuffer->add_modify_callback(style_update_cb, this);
        if (textBuffer->length() == 0)
        {
            textBuffer->append(R"PYTHON(
import mrv2
from mrv2 import annotations, cmd, math, image, io, media
from mrv2 import playlist, timeline, usd, settings

)PYTHON");
        }

        _r->tile->end();

        Fl_Scroll* s = g->get_scroll();
        Pack* pack = g->get_pack();
        s->resizable(pack);
    }

    void PythonPanel::run_code()
    {
        PythonEditor* e = _r->pythonEditor;
        e->split_code();

        std::string code = e->code();
        std::string eval = e->eval();
        std::string var = e->variable();
        outputDisplay->warning(code.c_str());
        if (!eval.empty() && var != eval)
        {
            eval += '\n';
            outputDisplay->warning(eval.c_str());
        }
        try
        {
            PyStdErrOutStreamRedirect pyRedirect;
            py::exec(code);
            if (!eval.empty())
            {
                py::object result = py::eval(eval);
                py::print(result);
            }
            const std::string& out = pyRedirect.stdoutString();
            if (!out.empty())
            {
                outputDisplay->info(out.c_str());
            }
            const std::string& err = pyRedirect.stderrString();
            if (!err.empty())
            {
                outputDisplay->error(out.c_str());
            }
        }
        catch (const std::exception& e)
        {
            outputDisplay->error(e.what());
            outputDisplay->error("\n");
        }
    }

    void PythonPanel::open_python_file(const std::string& file)
    {
        std::ifstream is(file);
        std::stringstream s;
        s << is.rdbuf();
        clear_editor();
        Fl_Text_Buffer* buffer = _r->pythonEditor->buffer();
        std::string nocr;
        std::string text = s.str();
        for (auto c : text)
        {
            if (c == '\r')
                continue;
            nocr += c;
        }
        buffer->append(nocr.c_str());
    }

    void PythonPanel::save_python_file(std::string file)
    {
        if (file.substr(file.size() - 3, file.size()) != ".py")
            file += ".py";

        if (fs::exists(file))
        {
            std::string msg =
                tl::string::Format(_("Python file {0} already "
                                     "exists.  Do you want to overwrite it?"))
                    .arg(file);
            int ok = fl_choice(msg.c_str(), _("No"), _("Yes"), NULL, NULL);
            if (!ok)
                return;
        }

        std::ofstream ofs(file);
        if (!ofs.is_open())
        {
            fl_alert("%s", _("Failed to open the file for writing."));
            return;
        }
        Fl_Text_Buffer* buffer = _r->pythonEditor->buffer();
        char* text = buffer->text();
        ofs << text;
        free(text);
        if (ofs.fail())
        {
            fl_alert("%s", _("Failed to write to the file."));
            return;
        }
        if (ofs.bad())
        {
            fl_alert("%s", _("The stream is in an unrecoverable error state."));
            return;
        }
        ofs.close();
    }

    void PythonPanel::clear_output()
    {
        outputDisplay->clear();
    }

    void PythonPanel::clear_editor()
    {
        Fl_Text_Buffer* buffer = _r->pythonEditor->buffer();
        buffer->remove(0, buffer->length());
    }

    void PythonPanel::toggle_line_numbers(Fl_Menu_* m)
    {
        const Fl_Menu_Item* i = m->mvalue();
        PythonEditor* e = _r->pythonEditor;
        if (i->value())
        {
            e->linenumber_width(75);
            e->linenumber_size(e->textsize());
        }
        else
        {
            e->linenumber_width(0);
        }
        e->redraw();
    }

    PythonOutput* PythonPanel::output()
    {
        return outputDisplay;
    }

    void PythonPanel::run_code_cb(Fl_Menu_*, PythonPanel* o)
    {
        o->run_code();
    }

    void PythonPanel::save_python_file_cb(Fl_Menu_*, PythonPanel* o)
    {
        std::string file = mrv::save_python_file(mrv::rootpath().c_str());
        if (file.empty())
            return;
        o->save_python_file(file);
    }

    void PythonPanel::open_python_file_cb(Fl_Menu_*, PythonPanel* o)
    {
        std::string file = mrv::open_python_file(mrv::pythonpath().c_str());
        if (file.empty())
            return;
        o->open_python_file(file);
    }

    void PythonPanel::clear_output_cb(Fl_Menu_*, PythonPanel* o)
    {
        o->clear_output();
    }

    void PythonPanel::clear_editor_cb(Fl_Menu_*, PythonPanel* o)
    {
        o->clear_editor();
    }

    void PythonPanel::toggle_line_numbers_cb(Fl_Menu_* m, PythonPanel* o)
    {
        o->toggle_line_numbers(m);
    }

    void PythonPanel::cut_text()
    {
        if (Fl::belowmouse() == outputDisplay)
            copy_text();
        else
            PythonEditor::kf_cut(0, _r->pythonEditor);
    }

    void PythonPanel::copy_text()
    {
        if (Fl::belowmouse() == outputDisplay)
        {
            Fl_Text_Buffer* buffer = outputDisplay->buffer();
            if (!buffer->selected())
                return;
            const char* copy = buffer->selection_text();
            if (*copy)
                Fl::copy(copy, (int)strlen(copy), 1);
            free((void*)copy);
        }
        else
        {
            PythonEditor::kf_copy(0, _r->pythonEditor);
        }
    }

    void PythonPanel::paste_text()
    {
        PythonEditor::kf_paste(0, _r->pythonEditor);
    }

    void PythonPanel::cut_text_cb(Fl_Menu_* m, PythonPanel* o)
    {
        o->cut_text();
    }

    void PythonPanel::copy_text_cb(Fl_Menu_* m, PythonPanel* o)
    {
        o->copy_text();
    }

    void PythonPanel::paste_text_cb(Fl_Menu_* m, PythonPanel* o)
    {
        o->paste_text();
    }

    void PythonPanel::add_to_script_list(const std::string& file)
    {
        TLRENDER_P();

        auto settingsObject = p.ui->app->settingsObject();
        settingsObject->addPythonScript(file);

        create_menu(_r->menu);
    }

    void PythonPanel::add_to_script_list_cb(Fl_Menu_* m, PythonPanel* o)
    {
        std::string file = mrv::open_python_file(mrv::pythonpath().c_str());
        if (file.empty())
            return;
        o->add_to_script_list(file);
    }

    void PythonPanel::script_shortcut(unsigned int idx)
    {
        TLRENDER_P();

        auto settingsObject = p.ui->app->settingsObject();

        const std::string script = settingsObject->pythonScript(idx);

        Fl_Text_Buffer* buffer = _r->pythonEditor->buffer();
        char* text = buffer->text();
        open_python_file(script);
        run_code();
        clear_editor();
        buffer->append(text);
        free(text);
    }

    void PythonPanel::script_shortcut_cb(Fl_Menu_* m, PythonPanel* o)
    {
        unsigned base = m->find_index(_("Scripts/Add To Script List"));
        const Fl_Menu_Item* item = m->mvalue();
        unsigned idx = m->find_index(item) - base - 1;
        o->script_shortcut(idx);
    }

} // namespace mrv
