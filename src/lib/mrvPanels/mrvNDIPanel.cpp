// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#if defined(TLRENDER_NDI)

#    include <chrono>
#    include <regex>
#    include <thread>
#    include <mutex>
#    include <atomic>

#    include <tlCore/StringFormat.h>
#    include <tlCore/NDIOptions.h>

#    include <tlDevice/OutputData.h>

#    include <FL/Fl_Choice.H>
#    include <FL/Fl_Check_Button.H>
#    include <FL/Fl_Toggle_Button.H>

#    include <Processing.NDI.Lib.h>

#    include "mrvCore/mrvHome.h"
#    include "mrvCore/mrvFile.h"
#    include "mrvCore/mrvMemory.h"

#    include "mrvWidgets/mrvButton.h"
#    include "mrvWidgets/mrvFunctional.h"
#    include "mrvWidgets/mrvHorSlider.h"
#    include "mrvWidgets/mrvInput.h"
#    include "mrvWidgets/mrvCollapsibleGroup.h"

#    include "mrvPanels/mrvPanelsCallbacks.h"
#    include "mrvPanels/mrvNDIPanel.h"

#    include "mrvFl/mrvIO.h"

#    include "mrvApp/mrvSettingsObject.h"

#    include "mrViewer.h"

namespace
{
    const char* kModule = "ndi";
}

namespace mrv
{
    namespace panel
    {

        struct NDIPanel::Private
        {

            PopupMenu* sourceMenu = nullptr;
            PopupMenu* inputAudioMenu = nullptr;
            PopupMenu* inputBestFormatMenu = nullptr;

            PopupMenu* outputAudioMenu = nullptr;
            PopupMenu* outputBestFormatMenu = nullptr;

            uint32_t no_sources = 0;
            const NDIlib_source_t* p_sources = NULL;

            static std::string lastStream;

            std::shared_ptr<observer::ValueObserver<timeline::PlayerCacheInfo> >
                cacheInfoObserver;

            struct PlayThread
            {
                std::atomic<bool> found = false;
                std::thread thread;
            };
            PlayThread play;

            struct FindThread
            {
                NDIlib_find_instance_t NDI = nullptr;
                std::atomic<bool> running = false;
                std::atomic<bool> awake = false;
                std::thread thread;
            };
            FindThread find;

            std::atomic<bool> running = false;
        };

        std::string NDIPanel::Private::lastStream;

        void NDIPanel::refresh_sources_cb(void* v)
        {
            NDIPanel* self = (NDIPanel*)v;
            self->refresh_sources();
        }

        void NDIPanel::refresh_sources()
        {
            MRV2_R();

            PopupMenu* m = r.sourceMenu;

            if (m->popped() || !r.find.running)
            {
                r.find.awake = false;
                return;
            }

            std::string sourceName;
            bool changed = false;
            const Fl_Menu_Item* item = nullptr;

            // Empty menu returns 0, while all others return +1.
            size_t numSources = r.no_sources;

            // We substract 2: 1 for FLTK quirk and one for "None".
            int size = m->size() - 2;
            if (size < 0)
                size = 0;

            sourceName = r.lastStream;

            if (numSources != size)
            {
                changed = true;
            }
            else
            {
                // child(0) is "None".
                for (int i = 0; i < numSources; ++i)
                {
                    item = m->child(i + 1);
                    if (!item->label() ||
                        !strcmp(item->label(), r.p_sources[i].p_ndi_name))
                    {
                        changed = true;
                        break;
                    }
                }
            }

            if (!changed)
            {
                r.find.awake = false;
                return;
            }

            m->clear();
            m->add(_("None"));
            int selected = 0;
            for (int i = 0; i < r.no_sources; ++i)
            {
                const std::string ndiName = r.p_sources[i].p_ndi_name;
                m->add(ndiName.c_str());
                if (sourceName == ndiName)
                {
                    selected = i + 1;
                }
            }
            m->menu_end();
            if (selected >= 0 && selected < m->size())
                m->value(selected);

            r.find.awake = false;
        }

        NDIPanel::NDIPanel(ViewerUI* ui) :
            _r(new Private),
            PanelWidget(ui)
        {
            MRV2_R();

            add_group("NDI");

            Fl_SVG_Image* svg = load_svg("NDI.svg");
            g->bind_image(svg);

            r.find.NDI = NDIlib_find_create_v2();

            // Run for one minute
            r.find.thread = std::thread(
                [this]
                {
                    MRV2_R();

                    r.find.running = true;
                    while (r.find.running)
                    {
                        using namespace std::chrono;
                        for (const auto start = high_resolution_clock::now();
                             high_resolution_clock::now() - start <
                             seconds(10);)
                        {
                            // Wait up till 1 second to check for new sources to
                            // be added or removed
                            if (!NDIlib_find_wait_for_sources(
                                    r.find.NDI, 1000 /* milliseconds */))
                            {
                                break;
                            }
                        }

                        if (!r.sourceMenu)
                            continue;

                        r.no_sources = std::numeric_limits<uint32_t>::max();
                        while (r.no_sources ==
                                   std::numeric_limits<uint32_t>::max() &&
                               r.find.running)
                        {
                            // Get the updated list of sources
                            r.p_sources = NDIlib_find_get_current_sources(
                                r.find.NDI, &r.no_sources);
                        }

                        if (!r.find.awake)
                        {
                            Fl::awake(
                                (Fl_Awake_Handler)refresh_sources_cb, this);
                            r.find.awake = true;
                        }
                    }
                    r.no_sources = 0;
                });

            g->callback(
                [](Fl_Widget* w, void* d)
                {
                    ViewerUI* ui = static_cast< ViewerUI* >(d);
                    delete ndiPanel;
                    ndiPanel = nullptr;
                    ui->uiMain->fill_menu(ui->uiMenuBar);
                },
                ui);
        }

        NDIPanel::~NDIPanel()
        {
            MRV2_R();

            r.find.awake = true;
            r.find.running = false;

            if (r.find.thread.joinable())
                r.find.thread.join();

            if (r.find.NDI)
            {
                NDIlib_find_destroy(r.find.NDI);
                r.find.NDI = nullptr;
            }
        }

        void NDIPanel::add_controls()
        {
            TLRENDER_P();
            MRV2_R();

            SettingsObject* settings = App::app->settings();
            const std::string& prefix = tab_prefix();

            HorSlider* s;
            Fl_Group *bg, *bg2;
            std_any value;
            int open;

            int Y = g->y();

            auto cg = new CollapsibleGroup(g->x(), Y, g->w(), 20, _("Input"));
            cg->spacing(2);
            auto b = cg->button();
            b->labelsize(14);
            b->size(b->w(), 18);
            b->callback(
                [](Fl_Widget* w, void* d)
                {
                    CollapsibleGroup* cg = static_cast<CollapsibleGroup*>(d);
                    if (cg->is_open())
                        cg->close();
                    else
                        cg->open();

                    const std::string& prefix = ndiPanel->tab_prefix();
                    const std::string key = prefix + "NDI Input";

                    auto settings = App::app->settings();
                    settings->setValue(key, static_cast<int>(cg->is_open()));
                },
                cg);

            cg->begin();

            Fl_Box* title = new Fl_Box(
                g->x() + 10, Y, g->w() - 20, 20, _("NDI Connection"));
            title->align(FL_ALIGN_CENTER);
            title->labelsize(12);

            auto mW = new Widget< PopupMenu >(
                g->x() + 10, Y, g->w() - 20, 20, _("None"));
            PopupMenu* m = r.sourceMenu = mW;
            m->disable_submenus();
            m->labelsize(12);
            m->align(FL_ALIGN_CENTER | FL_ALIGN_CLIP);

            mW->callback(
                [=](auto o)
                {
                    if (o->size() < 3)
                        return;
                    const Fl_Menu_Item* item = o->mvalue();
                    if (!item)
                        return;
                    _open_ndi(item);
                });

            mW = new Widget< PopupMenu >(
                g->x() + 10, Y, g->w() - 20, 20, _("Fast Format"));
            m = r.inputBestFormatMenu = mW;
            m->disable_submenus();
            m->labelsize(12);
            m->align(FL_ALIGN_CENTER | FL_ALIGN_CLIP);
            m->add(_("Fast Format"));
            m->add(_("Best Format"));
            m->value(0);

            mW = new Widget< PopupMenu >(
                g->x() + 10, Y, g->w() - 20, 20, _("With Audio"));
            m = r.inputAudioMenu = mW;
            m->disable_submenus();
            m->labelsize(12);
            m->align(FL_ALIGN_CENTER | FL_ALIGN_CLIP);
            m->add(_("With Audio"));
            m->add(_("Without Audio"));
            m->value(0);

            r.find.awake = false;

            cg->end();

            std::string key = prefix + "NDI Input";
            value = settings->getValue<std::any>(key);
            open = std_any_empty(value) ? 1 : std_any_cast<int>(value);
            if (!open)
            {
                cg->close();
            }

            cg = new CollapsibleGroup(g->x(), Y, g->w(), 20, _("Output"));
            cg->spacing(2);
            b = cg->button();
            b->labelsize(14);
            b->size(b->w(), 18);
            b->callback(
                [](Fl_Widget* w, void* d)
                {
                    CollapsibleGroup* cg = static_cast<CollapsibleGroup*>(d);
                    if (cg->is_open())
                        cg->close();
                    else
                        cg->open();

                    const std::string& prefix = ndiPanel->tab_prefix();
                    const std::string key = prefix + "NDI Output";

                    auto settings = App::app->settings();
                    settings->setValue(key, static_cast<int>(cg->is_open()));
                },
                cg);

            cg->begin();

            Fl_Toggle_Button* stream_b = new Fl_Toggle_Button(
                g->x() + 10, Y, g->w() - 20, 20, _("Start streaming"));
            stream_b->align(FL_ALIGN_CENTER);
            stream_b->labelsize(12);
            stream_b->callback(
                [](Fl_Widget* w, void* d)
                {
                    Fl_Toggle_Button* b = static_cast<Fl_Toggle_Button*>(w);
                    if (!App::ui->uiView->getTimelinePlayer())
                    {
                        b->value(0);
                        return;
                    }
                    if (b->value())
                    {
                        device::DeviceConfig config;
                        config.deviceIndex = 0;
                        config.displayModeIndex = 0;
                        // config.pixelType = device::PixelType::_8BitYUV; // OK
                        // config.pixelType = device::PixelType::_8BitRGBX; //
                        // OK config.pixelType = device::PixelType::_8BitBGRA;
                        // // OK config.pixelType =
                        // device::PixelType::_8BitBGRX; // OK config.pixelType
                        // = device::PixelType::_8BitUYVA; // BAD (black)
                        // config.pixelType = device::PixelType::_8BitI420; //
                        // BAD (black) config.pixelType =
                        // device::PixelType::_10BitRGB;  // Unsupported
                        // config.pixelType = device::PixelType::_10BitRGBX; //
                        // Unsupported config.pixelType =
                        // device::PixelType::_10BitRGBXLE; // Unsupported
                        // config.pixelType = device::PixelType::_10BitYUV; //
                        // Unsupported config.pixelType =
                        // device::PixelType::_12BitRGB;    // Unsupported
                        // config.pixelType = device::PixelType::_12BitRGBLE; //
                        // Unsupported
                        config.pixelType =
                            device::PixelType::_16BitP216; // BAD (need sws)
                        // config.pixelType = device::PixelType::_16BitPA16; //
                        // BAD (need sws)
                        App::app->beginNDIOutputStream(config);
                        b->copy_label(_("Stop streaming"));
                    }
                    else
                    {
                        App::app->endNDIOutputStream();
                        b->copy_label(_("Start streaming"));
                    }
                },
                stream_b);

            Fl_Group* ig = new Fl_Group(g->x() + 10, Y, g->w() - 20, 20);
            Input* mI = new Input(g->x() + 80, Y, g->w() - 90, 20, _("Name"));
            mI->value("mrv2 HDR");
            ig->end();

            mW = new Widget< PopupMenu >(
                g->x() + 10, Y, g->w() - 20, 20, _("Fast Format"));
            m = r.outputBestFormatMenu = mW;
            m->disable_submenus();
            m->labelsize(12);
            m->align(FL_ALIGN_CENTER | FL_ALIGN_CLIP);
            m->add(_("Fast Format"));
            m->add(_("Best Format"));
            m->value(0);

            mW = new Widget< PopupMenu >(
                g->x() + 10, Y, g->w() - 20, 20, _("With Audio"));
            m = r.outputAudioMenu = mW;
            m->disable_submenus();
            m->labelsize(12);
            m->align(FL_ALIGN_CENTER | FL_ALIGN_CLIP);
            m->add(_("With Audio"));
            m->add(_("Without Audio"));
            m->value(0);

            r.find.awake = false;

            cg->end();

            key = prefix + "NDI Output";
            value = settings->getValue<std::any>(key);
            open = std_any_empty(value) ? 1 : std_any_cast<int>(value);
            if (!open)
            {
                cg->close();
            }
        }

        void NDIPanel::_open_ndi(const Fl_Menu_Item* item)
        {
            TLRENDER_P();
            MRV2_R();

            // Get the NDI name from the menu item
            const std::string sourceName = item->label();

            if (r.lastStream == sourceName)
                return;

            if (!r.lastStream.empty())
                LOG_STATUS("Close stream " << r.lastStream);

            auto model = p.ui->app->filesModel();
            if (model)
            {
                auto aItem = model->observeA()->get();
                if (aItem && file::isTemporaryNDI(aItem->path))
                    model->close();
            }

            // Windows has weird items called REMOTE CONNECTION.
            // We don't allow selecting them.
            const std::regex pattern(
                "remote connection", std::regex_constants::icase);
            if (std::regex_search(sourceName, pattern))
                return;

            r.lastStream = sourceName;

            if (sourceName == _("None"))
                return;

            // Create an ndi file
            std::string ndiFile = file::NDI(p.ui);

            ndi::Options options;
            options.sourceName = sourceName;
            options.bestFormat = r.inputBestFormatMenu->value();
            options.noAudio = r.inputAudioMenu->value();

            nlohmann::json j;
            j = options;

            std::ofstream s(ndiFile);
            s << j << std::endl;
            s.close();

            LOG_STATUS("Opened stream " << sourceName);

            open_file_cb(ndiFile, p.ui);

            auto player = p.ui->uiView->getTimelinePlayer();
            if (!player)
                return;

            int seconds = 4;
            r.play.found = !options.noAudio;
            player->stop();

            if (!options.noAudio)
            {
                r.cacheInfoObserver =
                    observer::ValueObserver<timeline::PlayerCacheInfo>::create(
                        player->player()->observeCacheInfo(),
                        [this, player,
                         options](const timeline::PlayerCacheInfo& value)
                        {
                            MRV2_R();
                            auto startTime = player->currentTime();
                            auto endTime =
                                startTime + options.audioBufferSize.rescaled_to(
                                                startTime.rate());

                            for (const auto& t : value.audioFrames)
                            {
                                if (t.start_time() <= startTime &&
                                    t.end_time_exclusive() >= endTime)
                                {
                                    r.play.found = true;
                                    r.cacheInfoObserver.reset();
                                    break;
                                }
                            }
                        },
                        observer::CallbackAction::Suppress);
            }

            r.play.thread = std::thread(
                [this, player, seconds]
                {
                    MRV2_R();
                    auto start = std::chrono::steady_clock::now();
                    while (!r.play.found &&
                           std::chrono::steady_clock::now() - start <=
                               std::chrono::seconds(seconds))
                    {
                    }
                    player->forward();
                });

            r.play.thread.detach();
        }

    } // namespace panel

} // namespace mrv

#endif // TLRENDER_NDI
