// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#include "mrvCore/mrvI8N.h"
#include "mrvCore/mrvHotkey.h"
#include "mrvCore/mrvMath.h"
#include "mrvCore/mrvUtil.h"

#include "mrvFl/mrvCallbacks.h"
#include "mrvFl/mrvVersioning.h"

#include "mrvUI/mrvMenus.h"

#include "mrvWidgets/mrvMainWindow.h"

#include "mrvPanels/mrvPanelsCallbacks.h"

#include "mrvNetwork/mrvDummyClient.h"

#include "mrvApp/mrvSettingsObject.h"
#include "mrvApp/mrvFilesModel.h"
#include "mrvApp/App.h"

#include "mrvPreferencesUI.h"

#include "mrvFl/mrvIO.h"

// The FLTK includes have to come last as they conflict with Windows' includes
#include <FL/platform.H>
#include <FL/fl_utf8.h>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl.H>

#ifdef __linux__
#    undef None // X11 defines None as a macro
#endif

namespace
{
    const char* kModule = "menus";
}

namespace mrv
{
#ifdef MRV2_PYBIND11
    OrderedMap<std::string, py::handle > pythonMenus;
#endif

    float kCrops[] = {0.00f, 1.00f, 1.19f, 1.37f, 1.50f, 1.56f, 1.66f, 1.77f,
                      1.85f, 2.00f, 2.10f, 2.20f, 2.35f, 2.39f, 4.00f};

    void MainWindow::fill_menu(Fl_Menu_* menu)
    {
        Fl_Menu_Item* item = nullptr;
        int mode = 0;
        char buf[256];

        const auto& model = ui->app->filesModel();
        const auto& files = model->observeFiles();
        size_t numFiles = files->getSize();

        menu->clear();

        int idx;

        DBG3;
        menu->add(
            _("File/Open/Movie or Sequence"), kOpenImage.hotkey(),
            (Fl_Callback*)open_cb, ui);

        menu->add(
            _("File/Open/With Separate Audio"), kOpenSeparateAudio.hotkey(),
            (Fl_Callback*)open_separate_audio_cb, ui);

        menu->add(
            _("File/Open/Directory"), kOpenDirectory.hotkey(),
            (Fl_Callback*)open_directory_cb, ui, FL_MENU_DIVIDER);

        menu->add(
            _("File/Open/Session"), kOpenSession.hotkey(),
            (Fl_Callback*)load_session_cb, ui);

        mode = 0;
        if (numFiles == 0)
            mode = FL_MENU_INACTIVE;

        menu->add(
            _("File/Save/Movie or Sequence"), kSaveSequence.hotkey(),
            (Fl_Callback*)save_movie_cb, ui, mode);
        menu->add(
            _("File/Save/Single Frame"), kSaveImage.hotkey(),
            (Fl_Callback*)save_single_frame_cb, ui, mode);
        menu->add(
            _("File/Save/PDF Document"), kSavePDF.hotkey(),
            (Fl_Callback*)save_pdf_cb, ui, FL_MENU_DIVIDER | mode);
        menu->add(
            _("File/Save/Session"), kSaveSession.hotkey(),
            (Fl_Callback*)save_session_cb, ui);
        menu->add(
            _("File/Save/Session As"), kSaveSessionAs.hotkey(),
            (Fl_Callback*)save_session_as_cb, ui);

        menu->add(
            _("File/Close Current"), kCloseCurrent.hotkey(),
            (Fl_Callback*)close_current_cb, ui, mode);

        menu->add(
            _("File/Close All"), kCloseAll.hotkey(), (Fl_Callback*)close_all_cb,
            ui, mode);

        std_any value;
        SettingsObject* settings = ui->app->settingsObject();
        const std::vector< std::string >& recentFiles = settings->recentFiles();

        // Add files to Recent menu quoting the / to avoid splitting the menu
        for (auto file : recentFiles)
        {
            file = commentCharacter(file, '\\');
            file = commentCharacter(file, '/');
            snprintf(buf, 256, _("File/Recent/%s"), file.c_str());
            menu->add(buf, 0, (Fl_Callback*)open_recent_cb, ui);
        }

        menu->add(
            _("File/Quit"), kQuitProgram.hotkey(), (Fl_Callback*)exit_cb, ui);

        idx = menu->add(
            _("Window/Presentation"), kTogglePresentation.hotkey(),
            (Fl_Callback*)toggle_presentation_cb, ui, FL_MENU_TOGGLE);

        const Viewport* view = ui->uiView;
        const Viewport* view2 = nullptr;
        if (ui->uiSecondary && ui->uiSecondary->window()->visible())
            view2 = ui->uiSecondary->viewport();

        item = (Fl_Menu_Item*)&menu->menu()[idx];
        if (view->getPresentationMode() ||
            (view2 && view2->getPresentationMode()))
            item->set();
        else
            item->clear();

        idx = menu->add(
            _("Window/Full Screen"), kFullScreen.hotkey(),
            (Fl_Callback*)toggle_fullscreen_cb, ui, FL_MENU_TOGGLE);
        item = (Fl_Menu_Item*)&menu->menu()[idx];

        if ((view->getFullScreenMode() && !view->getPresentationMode()) ||
            (view2 && view2->getFullScreenMode() &&
             !view2->getPresentationMode()))
            item->set();
        else
            item->clear();

        idx = menu->add(
            _("Window/Float On Top"), kToggleFloatOnTop.hotkey(),
            (Fl_Callback*)toggle_float_on_top_cb, ui,
            FL_MENU_TOGGLE | FL_MENU_DIVIDER);
        item = (Fl_Menu_Item*)&menu->menu()[idx];
        if (ui->uiMain->is_on_top())
            item->set();
        else
            item->clear();

        idx = menu->add(
            _("Window/Secondary"), kToggleSecondary.hotkey(),
            (Fl_Callback*)toggle_secondary_cb, ui, FL_MENU_TOGGLE);
        item = (Fl_Menu_Item*)&menu->menu()[idx];
        if (ui->uiSecondary && ui->uiSecondary->window()->visible())
            item->set();
        else
            item->clear();

        idx = menu->add(
            _("Window/Secondary Float On Top"),
            kToggleSecondaryFloatOnTop.hotkey(),
            (Fl_Callback*)toggle_secondary_float_on_top_cb, ui,
            FL_MENU_TOGGLE | FL_MENU_DIVIDER);
        item = (Fl_Menu_Item*)&menu->menu()[idx];
        if (ui->uiSecondary && ui->uiSecondary->window()->is_on_top())
            item->set();
        else
            item->clear();

        mode = FL_MENU_TOGGLE;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;
        idx = menu->add(
            _("View/Safe Areas"), kSafeAreas.hotkey(),
            (Fl_Callback*)safe_areas_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (ui->uiView->getSafeAreas())
            item->set();

        idx = menu->add(
            _("View/Data Window"), kDataWindow.hotkey(),
            (Fl_Callback*)data_window_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (ui->uiView->getDataWindow())
            item->set();

        idx = menu->add(
            _("View/Display Window"), kDisplayWindow.hotkey(),
            (Fl_Callback*)display_window_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (ui->uiView->getDisplayWindow())
            item->set();

        snprintf(buf, 256, "%s", _("View/Toggle Menu bar"));
        idx = menu->add(
            buf, kToggleMenuBar.hotkey(), (Fl_Callback*)toggle_menu_bar, ui,
            FL_MENU_TOGGLE);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (ui->uiMenuGroup->visible())
            item->set();

        snprintf(buf, 256, "%s", _("View/Toggle Top bar"));
        idx = menu->add(
            buf, kToggleTopBar.hotkey(), (Fl_Callback*)toggle_top_bar, ui,
            FL_MENU_TOGGLE);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (ui->uiTopBar->visible())
            item->set();

        snprintf(buf, 256, "%s", _("View/Toggle Pixel bar"));
        idx = menu->add(
            buf, kTogglePixelBar.hotkey(), (Fl_Callback*)toggle_pixel_bar, ui,
            FL_MENU_TOGGLE);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (ui->uiPixelBar->visible())
            item->set();

        snprintf(buf, 256, "%s", _("View/Toggle Timeline"));
        idx = menu->add(
            buf, kToggleTimeline.hotkey(), (Fl_Callback*)toggle_bottom_bar, ui,
            FL_MENU_TOGGLE);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (ui->uiBottomBar->visible())
            item->set();

        snprintf(buf, 256, "%s", _("View/Toggle Status Bar"));
        idx = menu->add(
            buf, kToggleStatusBar.hotkey(), (Fl_Callback*)toggle_status_bar, ui,
            FL_MENU_TOGGLE);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (ui->uiStatusGroup->visible())
            item->set();

        snprintf(buf, 256, "%s", _("View/Toggle Action Dock"));
        idx = menu->add(
            buf, kToggleToolBar.hotkey(), (Fl_Callback*)toggle_action_tool_bar,
            ui, FL_MENU_TOGGLE);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (ui->uiToolsGroup->visible())
            item->set();

        idx = menu->add(
            _("Panel/One Panel Only"), kToggleOnePanelOnly.hotkey(),
            (Fl_Callback*)toggle_one_panel_only_cb, ui,
            FL_MENU_TOGGLE | FL_MENU_DIVIDER);
        item = (Fl_Menu_Item*)&menu->menu()[idx];
        if (onePanelOnly())
            item->set();
        else
            item->clear();

        std::string menu_panel_root = _("Panel/");
        std::string menu_window_root = _("Window/");
        const WindowCallback* wc = kWindowCallbacks;
        for (; wc->name; ++wc)
        {
            std::string tmp = wc->name;
            std::string menu_root = menu_panel_root;

            mode = FL_MENU_TOGGLE;
            unsigned hotkey = 0;
            if (tmp == "Files")
                hotkey = kToggleReel.hotkey();
            else if (tmp == "Media Information")
                hotkey = kToggleMediaInfo.hotkey();
            else if (tmp == "Color Info")
                hotkey = kToggleColorInfo.hotkey();
            else if (tmp == "Color")
                hotkey = kToggleColorControls.hotkey();
            else if (tmp == "Color Area")
                hotkey = kToggleColorInfo.hotkey();
            else if (tmp == "Compare")
                hotkey = kToggleCompare.hotkey();
            else if (tmp == "Playlist")
                hotkey = kTogglePlaylist.hotkey();
            else if (tmp == "Devices")
                hotkey = kToggleDevices.hotkey();
            else if (tmp == "Settings")
                hotkey = kToggleSettings.hotkey();
            else if (tmp == "Annotations")
                hotkey = kToggleAnnotation.hotkey();
            else if (tmp == "Histogram")
                hotkey = kToggleHistogram.hotkey();
            else if (tmp == "Vectorscope")
                hotkey = kToggleVectorscope.hotkey();
            else if (tmp == "Environment Map")
                hotkey = kToggleEnvironmentMap.hotkey();
            else if (tmp == "Waveform")
                hotkey = kToggleWaveform.hotkey();
            else if (tmp == "Network")
                hotkey = kToggleNetwork.hotkey();
            else if (tmp == "USD")
                hotkey = kToggleUSD.hotkey();
            else if (tmp == "Stereo 3D")
                hotkey = kToggleStereo3D.hotkey();
            else if (tmp == "Python")
                hotkey = kTogglePythonConsole.hotkey();
            else if (tmp == "Logs")
                hotkey = kToggleLogs.hotkey();
            else if (tmp == "Hotkeys")
            {
                menu_root = menu_window_root;
                hotkey = kToggleHotkeys.hotkey();
                mode = 0;
            }
            else if (tmp == "Preferences")
            {
                menu_root = menu_window_root;
                hotkey = kTogglePreferences.hotkey();
                mode = 0;
            }
            else if (tmp == "About")
            {
                continue;
            }
            else
            {
                std::cerr << "Menus: Unknown panel " << tmp << std::endl;
                continue; // Unknown window check
            }

            tmp = _(wc->name);
            std::string menu_name = menu_root + tmp;
            int idx = menu->add(
                menu_name.c_str(), hotkey, (Fl_Callback*)window_cb, ui, mode);
            item = const_cast<Fl_Menu_Item*>(&menu->menu()[idx]);
            if (tmp == _("Files"))
            {
                if (filesPanel)
                    item->set();
                else
                    item->clear();
            }
            else if (tmp == _("Color"))
            {
                if (colorPanel)
                    item->set();
                else
                    item->clear();
            }
            else if (tmp == _("Color Area"))
            {
                if (colorAreaPanel)
                    item->set();
                else
                    item->clear();
            }
            else if (tmp == _("Histogram"))
            {
                if (histogramPanel)
                    item->set();
                else
                    item->clear();
            }
            else if (tmp == _("Vectorscope"))
            {
                if (vectorscopePanel)
                    item->set();
                else
                    item->clear();
            }
            else if (tmp == _("Compare"))
            {
                if (comparePanel)
                    item->set();
                else
                    item->clear();
            }
            else if (tmp == _("Playlist"))
            {
                if (playlistPanel)
                    item->set();
                else
                    item->clear();
            }
            else if (tmp == _("Devices"))
            {
                if (devicesPanel)
                    item->set();
                else
                    item->clear();
            }
            else if (tmp == _("Annotations"))
            {
                if (annotationsPanel)
                    item->set();
                else
                    item->clear();
            }
            else if (tmp == _("Settings"))
            {
                if (settingsPanel)
                    item->set();
                else
                    item->clear();
            }
            else if (tmp == _("Logs"))
            {
                if (logsPanel)
                    item->set();
                else
                    item->clear();
            }
#ifdef MRV2_PYBIND11
            else if (tmp == _("Python"))
            {
                if (pythonPanel)
                    item->set();
                else
                    item->clear();
            }
#endif
            else if (tmp == _("Network"))
            {
#ifdef MRV2_NETWORK
                if (networkPanel)
                    item->set();
                else
                    item->clear();
#endif
            }
            else if (tmp == _("USD"))
            {
#ifdef TLRENDER_USD
                if (usdPanel)
                    item->set();
                else
                    item->clear();
#endif
            }
            else if (tmp == _("Stereo 3D"))
            {
                if (stereo3DPanel)
                    item->set();
                else
                    item->clear();
            }
            else if (tmp == _("Media Information"))
            {
                if (imageInfoPanel)
                    item->set();
                else
                    item->clear();
            }
            else if (tmp == _("Environment Map"))
            {
                if (environmentMapPanel)
                    item->set();
                else
                    item->clear();
            }
            else if (
                tmp == _("Hotkeys") || tmp == _("Preferences") ||
                tmp == _("About"))
            {
                continue;
            }
            else
            {
                LOG_ERROR(_("Unknown menu entry ") << tmp);
            }
        }

        // Make sure to sync panels remotely.
        syncPanels();

        const timeline::DisplayOptions& d = ui->app->displayOptions();
        const timeline::ImageOptions& o = ui->uiView->getImageOptions(-1);
        const bool blackBackground = ui->uiView->getBlackBackground();

        mode = FL_MENU_RADIO;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;
        if (d.channels == timeline::Channels::Color)
            mode |= FL_MENU_VALUE;
        idx = menu->add(
            _("Render/Color Channel"), kColorChannel.hotkey(),
            (Fl_Callback*)toggle_color_channel_cb, ui, mode);

        mode = FL_MENU_RADIO;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;
        if (d.channels == timeline::Channels::Red)
            mode |= FL_MENU_VALUE;
        idx = menu->add(
            _("Render/Red Channel"), kRedChannel.hotkey(),
            (Fl_Callback*)toggle_red_channel_cb, ui, mode);

        mode = FL_MENU_RADIO;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;
        if (d.channels == timeline::Channels::Green)
            mode |= FL_MENU_VALUE;
        idx = menu->add(
            _("Render/Green Channel "), kGreenChannel.hotkey(),
            (Fl_Callback*)toggle_green_channel_cb, ui, mode);

        mode = FL_MENU_RADIO;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;
        if (d.channels == timeline::Channels::Blue)
            mode |= FL_MENU_VALUE;
        idx = menu->add(
            _("Render/Blue Channel"), kBlueChannel.hotkey(),
            (Fl_Callback*)toggle_blue_channel_cb, ui, mode);

        mode = FL_MENU_RADIO;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;
        if (d.channels == timeline::Channels::Alpha)
            mode |= FL_MENU_VALUE;
        idx = menu->add(
            _("Render/Alpha Channel"), kAlphaChannel.hotkey(),
            (Fl_Callback*)toggle_alpha_channel_cb, ui, FL_MENU_DIVIDER | mode);

        mode = FL_MENU_TOGGLE;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;

        if (d.mirror.x)
            mode |= FL_MENU_VALUE;
        idx = menu->add(
            _("Render/Mirror X"), kFlipX.hotkey(), (Fl_Callback*)mirror_x_cb,
            ui, mode);

        mode = FL_MENU_TOGGLE;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;

        if (d.mirror.y)
            mode |= FL_MENU_VALUE;
        idx = menu->add(
            _("Render/Mirror Y"), kFlipY.hotkey(), (Fl_Callback*)mirror_y_cb,
            ui, FL_MENU_DIVIDER | mode);

        mode = FL_MENU_TOGGLE;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;

        idx = menu->add(
            _("Render/Black Background  "), kToggleBlackBackground.hotkey(),
            (Fl_Callback*)toggle_black_background_cb, ui,
            FL_MENU_DIVIDER | mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (blackBackground)
            item->set();

        mode = FL_MENU_RADIO;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;

        idx = menu->add(
            _("Render/Video Levels/From File"), 0,
            (Fl_Callback*)video_levels_from_file_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (o.videoLevels == timeline::InputVideoLevels::FromFile)
            item->set();

        idx = menu->add(
            _("Render/Video Levels/Legal Range"), 0,
            (Fl_Callback*)video_levels_legal_range_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (o.videoLevels == timeline::InputVideoLevels::LegalRange)
            item->set();

        idx = menu->add(
            _("Render/Video Levels/Full Range"), 0,
            (Fl_Callback*)video_levels_full_range_cb, ui,
            FL_MENU_DIVIDER | mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (o.videoLevels == timeline::InputVideoLevels::FullRange)
            item->set();

        mode = FL_MENU_RADIO;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;

        idx = menu->add(
            _("Render/Alpha Blend/None"), 0, (Fl_Callback*)alpha_blend_none_cb,
            ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (o.alphaBlend == timeline::AlphaBlend::None)
            item->set();

        idx = menu->add(
            _("Render/Alpha Blend/Straight"), 0,
            (Fl_Callback*)alpha_blend_straight_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (o.alphaBlend == timeline::AlphaBlend::Straight)
            item->set();

        idx = menu->add(
            _("Render/Alpha Blend/Premultiplied"), 0,
            (Fl_Callback*)alpha_blend_premultiplied_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (o.alphaBlend == timeline::AlphaBlend::Premultiplied)
            item->set();

        mode = FL_MENU_RADIO;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;

        unsigned filtering_linear = 0;
        unsigned filtering_nearest = 0;
        if (d.imageFilters.minify == timeline::ImageFilter::Nearest)
            filtering_linear = kMinifyTextureFiltering.hotkey();
        else
            filtering_nearest = kMinifyTextureFiltering.hotkey();

        idx = menu->add(
            _("Render/Minify Filter/Nearest"), filtering_nearest,
            (Fl_Callback*)minify_nearest_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (d.imageFilters.minify == timeline::ImageFilter::Nearest)
            item->set();

        idx = menu->add(
            _("Render/Minify Filter/Linear"), filtering_linear,
            (Fl_Callback*)minify_linear_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (d.imageFilters.minify == timeline::ImageFilter::Linear)
            item->set();

        filtering_linear = 0;
        filtering_nearest = 0;
        if (d.imageFilters.magnify == timeline::ImageFilter::Nearest)
            filtering_linear = kMagnifyTextureFiltering.hotkey();
        else
            filtering_nearest = kMagnifyTextureFiltering.hotkey();

        idx = menu->add(
            _("Render/Magnify Filter/Nearest"), filtering_nearest,
            (Fl_Callback*)magnify_nearest_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (d.imageFilters.magnify == timeline::ImageFilter::Nearest)
            item->set();

        idx = menu->add(
            _("Render/Magnify Filter/Linear"), filtering_linear,
            (Fl_Callback*)magnify_linear_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (d.imageFilters.magnify == timeline::ImageFilter::Linear)
            item->set();

        timeline::Playback playback = timeline::Playback::Stop;

        auto player = ui->uiView->getTimelinePlayer();
        if (player)
            playback = player->playback();

        mode = FL_MENU_RADIO;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;

        idx = menu->add(
            _("Playback/Stop"), kStop.hotkey(), (Fl_Callback*)stop_cb, ui,
            mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (playback == timeline::Playback::Stop)
            item->set();

        idx = menu->add(
            _("Playback/Forwards"), kPlayFwd.hotkey(),
            (Fl_Callback*)play_forwards_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (playback == timeline::Playback::Forward)
            item->set();

        idx = menu->add(
            _("Playback/Backwards"), kPlayBack.hotkey(),
            (Fl_Callback*)play_backwards_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (playback == timeline::Playback::Reverse)
            item->set();

        mode = 0;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;
        menu->add(
            _("Playback/Toggle Playback"), kPlayDirection.hotkey(),
            (Fl_Callback*)toggle_playback_cb, ui, FL_MENU_DIVIDER | mode);

        // Set In/Out
        TimelineClass* c = ui->uiTimeWindow;

        mode = FL_MENU_TOGGLE;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;

        idx = menu->add(
            _("Playback/Toggle In Point"), kSetInPoint.hotkey(),
            (Fl_Callback*)playback_set_in_point_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (c->uiStartButton->value())
            item->set();
        idx = menu->add(
            _("Playback/Toggle Out Point"), kSetOutPoint.hotkey(),
            (Fl_Callback*)playback_set_out_point_cb, ui,
            FL_MENU_DIVIDER | mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (c->uiEndButton->value())
            item->set();

        // Looping

        timeline::Loop loop = timeline::Loop::Loop;
        if (player)
            loop = player->loop();

        mode = FL_MENU_RADIO;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;

        idx = menu->add(
            _("Playback/Loop Playback"), kPlaybackLoop.hotkey(),
            (Fl_Callback*)playback_loop_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (loop == timeline::Loop::Loop)
            item->set();
        idx = menu->add(
            _("Playback/Playback Once"), kPlaybackOnce.hotkey(),
            (Fl_Callback*)playback_once_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (loop == timeline::Loop::Once)
            item->set();
        idx = menu->add(
            _("Playback/Playback Ping Pong"), kPlaybackPingPong.hotkey(),
            (Fl_Callback*)playback_ping_pong_cb, ui, FL_MENU_DIVIDER | mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (loop == timeline::Loop::PingPong)
            item->set();

        mode = 0;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;

        menu->add(
            _("Playback/Go to/Start"), 0, (Fl_Callback*)start_frame_cb, ui,
            mode);
        menu->add(
            _("Playback/Go to/End"), 0, (Fl_Callback*)end_frame_cb, ui,
            FL_MENU_DIVIDER | mode);

        menu->add(
            _("Playback/Go to/Previous Frame"), kFrameStepBack.hotkey(),
            (Fl_Callback*)previous_frame_cb, ui, mode);
        menu->add(
            _("Playback/Go to/Next Frame"), kFrameStepFwd.hotkey(),
            (Fl_Callback*)next_frame_cb, ui, FL_MENU_DIVIDER | mode);

        if (player)
        {
            mode = 0;
            if (numFiles == 0)
                mode |= FL_MENU_INACTIVE;

            const auto& annotations = player->getAllAnnotations();
            if (!annotations.empty())
            {
                menu->add(
                    _("Playback/Go to/Previous Annotation"),
                    kShapeFrameStepBack.hotkey(),
                    (Fl_Callback*)previous_annotation_cb, ui, mode);
                menu->add(
                    _("Playback/Go to/Next Annotation"),
                    kShapeFrameStepFwd.hotkey(),
                    (Fl_Callback*)next_annotation_cb, ui,
                    FL_MENU_DIVIDER | mode);

                menu->add(
                    _("Playback/Annotation/Clear"), kShapeFrameClear.hotkey(),
                    (Fl_Callback*)annotation_clear_cb, ui);
                menu->add(
                    _("Playback/Annotation/Clear All"),
                    kShapeFrameClearAll.hotkey(),
                    (Fl_Callback*)annotation_clear_all_cb, ui);
            }
        }

        mode = FL_MENU_RADIO;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;

        const char* tmp;
        size_t num = ui->uiPrefs->uiPrefsCropArea->children();
        for (size_t i = 0; i < num; ++i)
        {
            tmp = ui->uiPrefs->uiPrefsCropArea->child(i)->label();
            if (!tmp)
                continue;
            snprintf(buf, 256, _("View/Mask/%s"), tmp);
            idx = menu->add(buf, 0, (Fl_Callback*)masking_cb, ui, mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            float mask = kCrops[i];
            if (mrv::is_equal(mask, ui->uiView->getMask()))
                item->set();
        }

        mode = 0;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;

        snprintf(buf, 256, "%s", _("View/Hud"));
        idx =
            menu->add(buf, kHudToggle.hotkey(), (Fl_Callback*)hud_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (hud)
            item->set();

        const auto& options = ui->uiTimeline->getItemOptions();

        mode = FL_MENU_TOGGLE;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;

        if (ui->uiPrefs->uiPrefsTimelineEditable->value())
        {
            idx = menu->add(
                _("Timeline/Editable"), kToggleTimelineEditable.hotkey(),
                (Fl_Callback*)toggle_timeline_editable_cb, ui, mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            bool editable = ui->uiTimeline->isEditable();
            if (editable)
                item->set();

            idx = menu->add(
                _("Timeline/Edit Associated Clips"),
                kToggleEditAssociatedClips.hotkey(),
                (Fl_Callback*)toggle_timeline_edit_associated_clips_cb, ui,
                mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            if (options.editAssociatedClips)
                item->set();
        }

        mode = FL_MENU_RADIO;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;

        int thumbnails_none = 0;
        int thumbnails_small = 0;
        if (options.thumbnails)
            thumbnails_none = kToggleTimelineThumbnails.hotkey();
        else
            thumbnails_small = kToggleTimelineThumbnails.hotkey();

        idx = menu->add(
            _("Timeline/Thumbnails/None"), thumbnails_none,
            (Fl_Callback*)timeline_thumbnails_none_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (!options.thumbnails)
            item->set();
        idx = menu->add(
            _("Timeline/Thumbnails/Small"), thumbnails_small,
            (Fl_Callback*)timeline_thumbnails_small_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (options.thumbnails && options.thumbnailHeight == 100)
            item->set();
        idx = menu->add(
            _("Timeline/Thumbnails/Medium"), 0,
            (Fl_Callback*)timeline_thumbnails_medium_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (options.thumbnails && options.thumbnailHeight == 200)
            item->set();
        idx = menu->add(
            _("Timeline/Thumbnails/Large"), 0,
            (Fl_Callback*)timeline_thumbnails_large_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (options.thumbnails && options.thumbnailHeight == 300)
            item->set();

        mode = FL_MENU_TOGGLE;
        if (numFiles == 0)
            mode |= FL_MENU_INACTIVE;
        idx = menu->add(
            _("Timeline/Transitions"), kToggleTimelineTransitions.hotkey(),
            (Fl_Callback*)toggle_timeline_transitions_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (options.showTransitions)
            item->set();
        idx = menu->add(
            _("Timeline/Markers"), kToggleTimelineMarkers.hotkey(),
            (Fl_Callback*)toggle_timeline_markers_cb, ui, mode);
        item = (Fl_Menu_Item*)&(menu->menu()[idx]);
        if (options.showMarkers)
            item->set();

        if (numFiles > 0)
        {

            const int aIndex = ui->app->filesModel()->observeAIndex()->get();
            const auto& files = ui->app->filesModel()->observeFiles()->get();
            std::string fileName = files[aIndex]->path.get(-1, false);

            const std::regex& regex = version_regex(ui, false);
            bool has_version = regex_match(fileName, regex);

            if (has_version)
            {
                menu->add(
                    _("Image/Version/First"), kFirstVersionImage.hotkey(),
                    (Fl_Callback*)first_image_version_cb, ui);
                menu->add(
                    _("Image/Version/Last"), kLastVersionImage.hotkey(),
                    (Fl_Callback*)last_image_version_cb, ui, FL_MENU_DIVIDER);
                menu->add(
                    _("Image/Version/Previous"), kPreviousVersionImage.hotkey(),
                    (Fl_Callback*)previous_image_version_cb, ui);
                menu->add(
                    _("Image/Version/Next"), kNextVersionImage.hotkey(),
                    (Fl_Callback*)next_image_version_cb, ui);
            }

            menu->add(
                _("Edit/Frame/Cut"), kEditCutFrame.hotkey(),
                (Fl_Callback*)edit_cut_frame_cb, ui);
            menu->add(
                _("Edit/Frame/Copy"), kEditCopyFrame.hotkey(),
                (Fl_Callback*)edit_copy_frame_cb, ui);
            menu->add(
                _("Edit/Frame/Paste"), kEditPasteFrame.hotkey(),
                (Fl_Callback*)edit_paste_frame_cb, ui);
            menu->add(
                _("Edit/Frame/Insert"), kEditInsertFrame.hotkey(),
                (Fl_Callback*)edit_insert_frame_cb, ui);

            menu->add(
                _("Edit/Slice"), kEditSliceClip.hotkey(),
                (Fl_Callback*)edit_slice_clip_cb, ui);
            menu->add(
                _("Edit/Remove"), kEditRemoveClip.hotkey(),
                (Fl_Callback*)edit_remove_clip_cb, ui);

#if 0
            menu->add(
                "Edit/Dump Undo Queue", 0, (Fl_Callback*)dump_undo_queue_cb,
                ui);

            menu->add(
                _("Edit/Remove With Gap"), kEditRemoveClipWithGap.hotkey(),
                (Fl_Callback*)edit_remove_clip_with_gap_cb, ui);

            menu->add(
                _("Edit/Roll"), kEditRoll.hotkey(), (Fl_Callback*)edit_roll_cb,
                ui);
#endif

            menu->add(
                _("Edit/Undo"), kEditUndo.hotkey(), (Fl_Callback*)edit_undo_cb,
                ui);
            menu->add(
                _("Edit/Redo"), kEditRedo.hotkey(), (Fl_Callback*)edit_redo_cb,
                ui);
        }

        // if ( num > 0 )
        // {
        //     idx = menu->add( _("Subtitle/No Subtitle"), 0,
        //                      (Fl_Callback*)change_subtitle_cb, ui,
        //                      FL_MENU_TOGGLE  );
        //     Fl_Menu_Item* item = (Fl_Menu_Item*) &(menu->menu()[idx]);
        //     if ( image->subtitle_stream() == -1 )
        //         item->set();
        //     for ( unsigned i = 0; i < num; ++i )
        //     {
        //         char buf[256];
        //         snprintf( buf, 256, _("Subtitle/Track #%d - %s"), i,
        //                  image->subtitle_info(i).language.c_str() );

        //         idx = menu->add( buf, 0,
        //                          (Fl_Callback*)change_subtitle_cb, ui,
        //                          FL_MENU_RADIO );
        //         item = (Fl_Menu_Item*) &(menu->menu()[idx]);
        //         if ( image->subtitle_stream() == (int)i )
        //             item->set();
        //     }
        // }

        // if ( dynamic_cast< Fl_Menu_Button* >( menu ) )
        // {
        //     menu->add( _("Pixel/Copy RGBA Values to Clipboard"),
        //                kCopyRGBAValues.hotkey(),
        //                (Fl_Callback*)copy_pixel_rgba_cb, (void*)ui->uiView);
        // }

        if (dynamic_cast< DummyClient* >(tcp) == nullptr)
        {
            mode = FL_MENU_TOGGLE;

            idx = menu->add(
                _("Sync/Send/Media"), 0, (Fl_Callback*)toggle_sync_send_cb, ui,
                mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            if (ui->uiPrefs->SendMedia->value())
                item->set();
            else
                item->clear();

            if (numFiles == 0)
                mode |= FL_MENU_INACTIVE;
            idx = menu->add(
                _("Sync/Send/UI"), 0, (Fl_Callback*)toggle_sync_send_cb, ui,
                mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            if (ui->uiPrefs->SendUI->value())
                item->set();
            else
                item->clear();
            idx = menu->add(
                _("Sync/Send/Pan And Zoom"), 0,
                (Fl_Callback*)toggle_sync_send_cb, ui, mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            if (ui->uiPrefs->SendPanAndZoom->value())
                item->set();
            else
                item->clear();
            idx = menu->add(
                _("Sync/Send/Color"), 0, (Fl_Callback*)toggle_sync_send_cb, ui,
                mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            if (ui->uiPrefs->SendColor->value())
                item->set();
            else
                item->clear();
            idx = menu->add(
                _("Sync/Send/Timeline"), 0, (Fl_Callback*)toggle_sync_send_cb,
                ui, mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            if (ui->uiPrefs->SendTimeline->value())
                item->set();
            else
                item->clear();
            idx = menu->add(
                _("Sync/Send/Annotations"), 0,
                (Fl_Callback*)toggle_sync_send_cb, ui, mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            if (ui->uiPrefs->SendAnnotations->value())
                item->set();
            else
                item->clear();
            idx = menu->add(
                _("Sync/Send/Audio"), 0, (Fl_Callback*)toggle_sync_send_cb, ui,
                mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            if (ui->uiPrefs->SendAudio->value())
                item->set();
            else
                item->clear();

            /// ACCEPT
            mode = FL_MENU_TOGGLE;

            idx = menu->add(
                _("Sync/Accept/Media"), 0, (Fl_Callback*)toggle_sync_receive_cb,
                ui, mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            if (ui->uiPrefs->ReceiveMedia->value())
                item->set();
            else
                item->clear();

            if (numFiles == 0)
                mode |= FL_MENU_INACTIVE;
            idx = menu->add(
                _("Sync/Accept/UI"), 0, (Fl_Callback*)toggle_sync_receive_cb,
                ui, mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            if (ui->uiPrefs->ReceiveUI->value())
                item->set();
            else
                item->clear();
            idx = menu->add(
                _("Sync/Accept/Pan And Zoom"), 0,
                (Fl_Callback*)toggle_sync_receive_cb, ui, mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            if (ui->uiPrefs->ReceivePanAndZoom->value())
                item->set();
            else
                item->clear();
            idx = menu->add(
                _("Sync/Accept/Color"), 0, (Fl_Callback*)toggle_sync_receive_cb,
                ui, mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            if (ui->uiPrefs->ReceiveColor->value())
                item->set();
            else
                item->clear();
            idx = menu->add(
                _("Sync/Accept/Timeline"), 0,
                (Fl_Callback*)toggle_sync_receive_cb, ui, mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            if (ui->uiPrefs->ReceiveTimeline->value())
                item->set();
            else
                item->clear();
            idx = menu->add(
                _("Sync/Accept/Annotations"), 0,
                (Fl_Callback*)toggle_sync_receive_cb, ui, mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            if (ui->uiPrefs->ReceiveAnnotations->value())
                item->set();
            else
                item->clear();
            idx = menu->add(
                _("Sync/Accept/Audio"), 0, (Fl_Callback*)toggle_sync_receive_cb,
                ui, mode);
            item = (Fl_Menu_Item*)&(menu->menu()[idx]);
            if (ui->uiPrefs->ReceiveAudio->value())
                item->set();
            else
                item->clear();
        }

#ifdef MRV2_PYBIND11
        for (const auto& entry : pythonMenus)
        {
            menu->add(
                entry.c_str(), 0, (Fl_Callback*)run_python_method_cb,
                (void*)&pythonMenus.at(entry));
        }
#endif

        menu->add(
            _("Help/Documentation"), 0, (Fl_Callback*)help_documentation_cb,
            ui);
        menu->add(
            _("Help/About"), kToggleAbout.hotkey(), (Fl_Callback*)window_cb,
            ui);

        menu->menu_end();

#ifdef __APPLE__
        Fl_Sys_Menu_Bar* smenubar = dynamic_cast< Fl_Sys_Menu_Bar* >(menu);
        if (smenubar)
        {
            Fl_Mac_App_Menu::about = _("About mrv2");
            Fl_Mac_App_Menu::print = "";
            Fl_Mac_App_Menu::hide = _("Hide mrv2");
            Fl_Mac_App_Menu::hide_others = _("Hide Others");
            Fl_Mac_App_Menu::services = _("Services");
            Fl_Mac_App_Menu::quit = _("Quit mrv2");

            Fl_Sys_Menu_Bar::about((Fl_Callback*)about_cb, ui);

            smenubar->update();
        }
#endif

        menu->redraw();
        DBG3;
    }

} // namespace mrv
