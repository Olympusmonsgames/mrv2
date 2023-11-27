
#include <cstring>

#include <FL/Fl.H>

#include "mrvCore/mrvI8N.h"
#include "mrvCore/mrvHotkey.h"

#include "mrvWidgets/mrvHotkeyInput.h"

#include "keyboard_ui.h"

namespace mrv
{
    HotkeyInput::HotkeyInput(int X, int Y, int W, int H, const char* L) :
        Fl_Input(X, Y, W, H, L)
    {
    }

    int HotkeyInput::handle(int e)
    {
        if (e == FL_KEYBOARD)
        {
            char buffer[1000];
            buffer[0] = '\0';
            m_text.clear();
            m_key = static_cast<unsigned>(Fl::event_key());
            if (m_key < 128)
            { // ASCII
                snprintf(buffer, sizeof(buffer), "%c", m_key);
            }
            else if (m_key >= 0xa0 && m_key <= 0xff)
            { // ISO-8859-1 (international keyboards)
                char key[8];
                int kl = fl_utf8encode((unsigned)m_key, key);
                key[kl] = '\0';
                snprintf(buffer, sizeof(buffer), "%s", key);
            }

            bool m_ctrl = false;
            bool m_shift = false;
            bool m_alt = false;
            bool m_meta = false;

            for (int i = 0; i < 45; i++)
            {
                if (mrv::table[i].n == m_key)
                {
                    m_text = _(mrv::table[i].text);
                    break;
                }
            }
            if (m_text.empty())
            {
                m_ctrl = Fl::event_state(FL_CTRL);
                m_shift = Fl::event_state(FL_SHIFT);
                m_alt = Fl::event_state(FL_ALT);
                m_meta = Fl::event_state(FL_META);

                char key = Fl::event_text()[0];
                if (key != '?' && key != '!' && key != '^')
                    m_text = Fl::event_text();
                else
                    m_text = buffer;
            }
            value(m_text.c_str());

            if (hotkey)
            {
                hotkey->uiCtrl->value(m_ctrl);
                hotkey->uiShift->value(m_shift);
                hotkey->uiAlt->value(m_alt);
                hotkey->uiMeta->value(m_meta);
            }
            return 1;
        }
        return Fl_Input::handle(e);
    }
} // namespace mrv
