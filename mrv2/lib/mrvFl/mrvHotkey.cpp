// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#include <regex>
#include <filesystem>
namespace fs = std::filesystem;

#include <FL/Enumerations.H>

#include <tlCore/StringFormat.h>

#include "mrvCore/mrvI8N.h"
#include "mrvCore/mrvHotkey.h"
#include "mrvCore/mrvHome.h"
#include "mrvCore/mrvUtil.h"

#include "mrvWidgets/mrvBrowser.h"

#include "mrvUI/mrvAsk.h"

#include "mrvFl/mrvIO.h"
#include "mrvFl/mrvHotkey.h"
#include "mrvFl/mrvFileRequester.h"

#include "keyboard_ui.h"
#include "mrvHotkeyUI.h"
#include "mrViewer.h"

namespace
{
    const char* kModule = "keys";
}

namespace mrv
{

    void fill_ui_hotkeys(mrv::Browser* b)
    {
        int r = b->vposition();
        b->type(FL_SELECT_BROWSER);
        b->clear();

        int w2 = b->w() / 4;
        int w1 = w2 * 3;
        static int widths[] = {w1 - 20, w2, 0};
        b->column_widths(widths);
        // b->showcolsep(1);
        // b->colsepcolor(FL_RED);
        b->column_char('\t'); // tabs as column delimiters

        // Labels
        b->add(_("@B12@C7@b@.Function\t@B12@C7@b@.Hotkey"));

        for (int i = 0; hotkeys[i].name != "END"; ++i)
        {
            HotkeyEntry& h = hotkeys[i];
            std::string row(_(h.name.c_str()));
            const std::string hotkey = h.hotkey.to_s();
            row += "\t" + hotkey;

            b->add(row.c_str());
        }

        b->vposition(r);
    }

    void select_hotkey(HotkeyUI* b)
    {
        int idx =
            b->uiFunction->value() - 2; // 1 for browser offset, 1 for title
        if (idx < 0)
            return;

        const std::string& name = hotkeys[idx].name;
        Hotkey& hk = hotkeys[idx].hotkey;

        ChooseHotkey* h = new ChooseHotkey(hk);
        h->make_window(name);
        h->fill();

        Fl_Window* window = h->uiMain;
        Fl_Group::current(0);
        window->show();

        while (window->visible())
            Fl::check();

        for (int i = 0; hotkeys[i].name != "END"; ++i)
        {
            if (h->hk == hotkeys[i].hotkey && idx != i &&
                hotkeys[i].hotkey.to_s() != "[" &&
                hotkeys[i].hotkey.to_s() != "]")
            {
                int ok = fl_choice(
                    _("Hotkey \"%s\" already used in \"%s\".\n"
                      "Do you want to override it?"),
                    _("Yes"), _("No"), 0L, h->hk.to_s().c_str(),
                    _(hotkeys[i].name.c_str()));
                if (ok)
                {
                    delete h;
                    return select_hotkey(b);
                }
                else
                {
                    hotkeys[i].hotkey.clear();
                }
            }
        }

        hk = h->hk;

        delete h;

        fill_ui_hotkeys(b->uiFunction);
    }

    void searchFunction(const std::string& searchText, mrv::Browser* o)
    {
        if (searchText.empty())
        {
            o->select(o->value(), 0);
            o->topline(0);
            o->redraw();
            return;
        }
        try
        {
            std::regex regex{searchText, std::regex_constants::icase};
            int start = o->value() + 1;
            for (int i = start; i <= o->size(); ++i)
            {
                std::string function = o->text(i);
                std::size_t pos = function.find('\t');
                function = function.substr(0, pos);
                if (std::regex_search(function, regex))
                {
                    o->topline(i);
                    o->select(i);
                    o->redraw();
                    return;
                }
            }
        }
        catch (const std::exception& e)
        {
        }
    }

    void searchHotkey(const std::string& text, mrv::Browser* o)
    {
        if (text.empty())
        {
            o->select(o->value(), 0);
            o->topline(0);
            o->redraw();
            return;
        }
        try
        {
            std::string prepareForRegEx = commentCharacter(text, '+');
            std::regex regex{text, std::regex_constants::icase};
            int start = o->value() + 1;
            for (int i = start; i <= o->size(); ++i)
            {
                std::string hotkey = o->text(i);
                std::size_t pos = hotkey.find('\t');
                hotkey = hotkey.substr(pos + 1, hotkey.size());
                if (std::regex_search(hotkey, regex))
                {
                    o->topline(i);
                    o->select(i);
                    o->redraw();
                    return;
                }
            }
        }
        catch (const std::exception& e)
        {
        }
    }

    void save_hotkeys(Fl_Preferences& keys)
    {
        keys.set("version", 11);
        for (int i = 0; hotkeys[i].name != "END"; ++i)
        {
            keys.set(
                (hotkeys[i].name + " ctrl").c_str(), hotkeys[i].hotkey.ctrl);
            keys.set((hotkeys[i].name + " alt").c_str(), hotkeys[i].hotkey.alt);
            keys.set(
                (hotkeys[i].name + " meta").c_str(), hotkeys[i].hotkey.meta);
            keys.set(
                (hotkeys[i].name + " shift").c_str(), hotkeys[i].hotkey.shift);
            keys.set(
                (hotkeys[i].name + " key").c_str(), (int)hotkeys[i].hotkey.key);
            keys.set(
                (hotkeys[i].name + " text").c_str(),
                hotkeys[i].hotkey.text.c_str());
        }
    }

    void load_hotkeys(ViewerUI* ui, Fl_Preferences* keys)
    {
        int version = 0;
        keys->get("version", version, 11);
        int tmp = 0;
        char tmpS[2048];

        for (int i = 0; hotkeys[i].name != "END"; ++i)
        {
            if (hotkeys[i].force == false)
                continue;
            hotkeys[i].hotkey.shift = hotkeys[i].hotkey.ctrl =
                hotkeys[i].hotkey.alt = hotkeys[i].hotkey.meta = false;
            hotkeys[i].hotkey.key = 0;
            hotkeys[i].hotkey.text.clear();
        }

        for (int i = 0; hotkeys[i].name != "END"; ++i)
        {
            Hotkey saved = hotkeys[i].hotkey;

            keys->get(
                (hotkeys[i].name + " key").c_str(), tmp,
                (int)hotkeys[i].hotkey.key);
            if (tmp)
                hotkeys[i].force = false;
            hotkeys[i].hotkey.key = unsigned(tmp);

            keys->get(
                (hotkeys[i].name + " text").c_str(), tmpS,
                hotkeys[i].hotkey.text.c_str(), 16);
            if (strlen(tmpS) > 0)
            {
                hotkeys[i].force = false;
                hotkeys[i].hotkey.text = tmpS;
            }
            else
                hotkeys[i].hotkey.text.clear();

            if (hotkeys[i].force)
            {
                hotkeys[i].hotkey = saved;
                continue;
            }
            keys->get(
                (hotkeys[i].name + " ctrl").c_str(), tmp,
                (int)hotkeys[i].hotkey.ctrl);
            if (tmp)
                hotkeys[i].hotkey.ctrl = true;
            else
                hotkeys[i].hotkey.ctrl = false;
            keys->get(
                (hotkeys[i].name + " alt").c_str(), tmp,
                (int)hotkeys[i].hotkey.alt);
            if (tmp)
                hotkeys[i].hotkey.alt = true;
            else
                hotkeys[i].hotkey.alt = false;

            keys->get(
                (hotkeys[i].name + " meta").c_str(), tmp,
                (int)hotkeys[i].hotkey.meta);
            if (tmp)
                hotkeys[i].hotkey.meta = true;
            else
                hotkeys[i].hotkey.meta = false;

            keys->get(
                (hotkeys[i].name + " shift").c_str(), tmp,
                (int)hotkeys[i].hotkey.shift);
            if (tmp)
                hotkeys[i].hotkey.shift = true;
            else
                hotkeys[i].hotkey.shift = false;

            for (int j = 0; hotkeys[j].name != "END"; ++j)
            {
                if (j != i && hotkeys[j].hotkey == hotkeys[i].hotkey)
                {
                    if (hotkeys[j].force != true)
                    {
                        std::string err =
                            tl::string::Format(
                                _("Corruption in hotkeys preferences. "
                                  "Hotkey {0} for {1} will not be "
                                  "available.  "
                                  "Already used in {2}."))
                                .arg(hotkeys[j].hotkey.to_s())
                                .arg(_(hotkeys[j].name.c_str()))
                                .arg(_(hotkeys[i].name.c_str()));
                        LOG_ERROR(err);
                    }
                    hotkeys[j].hotkey = Hotkey();
                }
            }
        }

        HotkeyUI* h = ui->uiHotkey;
        fill_ui_hotkeys(h->uiFunction);
    }

    void load_hotkeys(ViewerUI* ui)
    {
        Fl_Preferences* keys = new Fl_Preferences(
            prefspath().c_str(), "filmaura", Preferences::hotkeys_file.c_str());
        load_hotkeys(ui, keys);
        delete keys;

        HotkeyUI* h = ui->uiHotkey;
        fill_ui_hotkeys(h->uiFunction);
    }

} // namespace mrv
