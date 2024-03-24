// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#include <memory>
#include <cmath>
#include <algorithm>

#include <tlCore/Matrix.h>

#include "mrViewer.h"

#include "mrvPanels/mrvAnnotationsPanel.h"
#include "mrvPanels/mrvPanelsCallbacks.h"

#include "mrvGL/mrvTimelineViewport.h"
#include "mrvGL/mrvTimelineViewportPrivate.h"

#include "mrvFl/mrvCallbacks.h"
#include "mrvFl/mrvOCIO.h"
#include "mrvFl/mrvTimelinePlayer.h"

#include "mrvCore/mrvUtil.h"
#include "mrvCore/mrvMath.h"
#include "mrvCore/mrvHotkey.h"
#include "mrvCore/mrvColorSpaces.h"

#include "mrvWidgets/mrvHorSlider.h"
#include "mrvWidgets/mrvMultilineInput.h"

#include "mrvNetwork/mrvTCP.h"
#include "mrvNetwork/mrvDummyClient.h"

#include "mrvApp/mrvSettingsObject.h"

#include "mrvFl/mrvIO.h"

#include <FL/platform.H>
#include <FL/Fl.H>

namespace
{
    const char* kModule = "view";
    const float kHelpTimeout = 0.1F;
    const float kHelpTextFade = 1.5F; // 1.5 Seconds
} // namespace

namespace
{
    int normalizeAngle0to360(float angle)
    {
        int out = static_cast<int>(std::fmod(angle, 360.0f));
        if (out < 0)
        {
            out += 360;
        }
        return out;
    }
} // namespace

namespace mrv
{
    using namespace tl;

    timeline::BackgroundOptions TimelineViewport::Private::backgroundOptions;
    EnvironmentMapOptions TimelineViewport::Private::environmentMapOptions;
    math::Box2i TimelineViewport::Private::selection =
        math::Box2i(0, 0, -1, -1);
    image::Size TimelineViewport::Private::videoSize;
    ActionMode TimelineViewport::Private::actionMode = ActionMode::kScrub;
    timeline::Playback TimelineViewport::Private::playbackMode =
        timeline::Playback::Stop;
    float TimelineViewport::Private::masking = 0.F;
    otio::RationalTime TimelineViewport::Private::lastTime;
    uint64_t TimelineViewport::Private::skippedFrames = 0;
    float TimelineViewport::Private::rotation = 0.F;
    bool TimelineViewport::Private::resizeWindow = true;
    bool TimelineViewport::Private::safeAreas = false;
    bool TimelineViewport::Private::dataWindow = false;
    bool TimelineViewport::Private::displayWindow = false;
    bool TimelineViewport::Private::ignoreDisplayWindow = false;
    std::string TimelineViewport::Private::helpText;
    float TimelineViewport::Private::helpTextFade;
    bool TimelineViewport::Private::hudActive = true;
    HudDisplay TimelineViewport::Private::hud = HudDisplay::kNone;
    std::map<std::string, std::string, string::CaseInsensitiveCompare>
        TimelineViewport::Private::tagData;

    static void drawTimeoutText_cb(TimelineViewport* view)
    {
        view->clearHelpText();
    }

    TimelineViewport::TimelineViewport(
        int X, int Y, int W, int H, const char* L) :
        Fl_SuperClass(X, Y, W, H, L),
        _p(new Private)
    {
    }

    TimelineViewport::TimelineViewport(int W, int H, const char* L) :
        Fl_SuperClass(W, H, L),
        _p(new Private)
    {
    }

    TimelineViewport::~TimelineViewport()
    {
        _unmapBuffer();
    }

    void TimelineViewport::main(ViewerUI* m) noexcept
    {
        TLRENDER_P();
        p.ui = m;
    }

    //
    const std::vector<tl::timeline::VideoData>&
    TimelineViewport::getVideoData() const noexcept
    {
        return _p->videoData;
    }

    void TimelineViewport::clearHelpText()
    {
        TLRENDER_P();
        p.helpTextFade -= kHelpTimeout;
        if (mrv::is_equal(p.helpTextFade, 0.F))
        {
            Fl::remove_timeout((Fl_Timeout_Handler)drawTimeoutText_cb, this);
            p.helpText.clear();
        }
        else
        {
            Fl::repeat_timeout(
                kHelpTimeout, (Fl_Timeout_Handler)drawTimeoutText_cb, this);
        }
        redrawWindows();
    }

    void TimelineViewport::setHelpText(const std::string& text)
    {
        TLRENDER_P();
        if (text == p.helpText)
            return;

        p.helpText = text;
        p.helpTextFade = kHelpTextFade;

        Fl::remove_timeout((Fl_Timeout_Handler)drawTimeoutText_cb, this);
        Fl::add_timeout(
            kHelpTimeout, (Fl_Timeout_Handler)drawTimeoutText_cb, this);

        redrawWindows();
    }

    void TimelineViewport::undo()
    {
        TLRENDER_P();

        if (!p.player)
            return;

        p.player->undoAnnotation();

        tcp->pushMessage("undo", 0);
        redrawWindows();

        updateUndoRedoButtons();
    }

    void TimelineViewport::redo()
    {
        TLRENDER_P();

        if (!p.player)
            return;

        p.player->redoAnnotation();
        tcp->pushMessage("redo", 0);
        redrawWindows();

        updateUndoRedoButtons();
    }

    void TimelineViewport::setActionMode(const ActionMode& mode) noexcept
    {
        TLRENDER_P();

        if (mode == p.actionMode)
            return;

        //! Turn off all buttons
        p.ui->uiScrub->value(0);
        p.ui->uiSelection->value(0);
        p.ui->uiDraw->value(0);
        p.ui->uiErase->value(0);
        p.ui->uiCircle->value(0);
        p.ui->uiRectangle->value(0);
        p.ui->uiArrow->value(0);
        p.ui->uiText->value(0);

        if (mode != kSelection)
        {
            math::Box2i area;
            area.max.x = -1; // disable area selection.
            setSelectionArea(area);
        }

        if (p.actionMode == kText && mode != kText)
        {
            acceptMultilineInput();
        }

        p.actionMode = mode;

        switch (mode)
        {
        default:
        case kScrub:
            p.ui->uiScrub->value(1);
            p.ui->uiStatus->copy_label(_("Scrub"));
            break;
        case kSelection:
            p.ui->uiSelection->value(1);
            p.ui->uiStatus->copy_label(_("Selection"));
            break;
        case kDraw:
            p.ui->uiDraw->value(1);
            p.ui->uiStatus->copy_label(_("Draw"));
            break;
        case kErase:
            p.ui->uiErase->value(1);
            p.ui->uiStatus->copy_label(_("Erase"));
            break;
        case kCircle:
            p.ui->uiCircle->value(1);
            p.ui->uiStatus->copy_label(_("Circle"));
            break;
        case kRectangle:
            p.ui->uiRectangle->value(1);
            p.ui->uiStatus->copy_label(_("Rectangle"));
            break;
        case kArrow:
            p.ui->uiArrow->value(1);
            p.ui->uiStatus->copy_label(_("Arrow"));
            break;
        case kText:
            p.ui->uiText->value(1);
            p.ui->uiStatus->copy_label(_("Text"));
            break;
        case kRotate:
            p.ui->uiStatus->copy_label(_("Rotate"));
            break;
        case kEditTrim:
            p.ui->uiStatus->copy_label(_("Trim"));
            break;
        case kEditSlip:
            p.ui->uiStatus->copy_label(_("Slip"));
            break;
        case kEditSlide:
            p.ui->uiStatus->copy_label(_("Slide"));
            break;
        case kEditRipple:
            p.ui->uiStatus->copy_label(_("Ripple"));
            break;
        case kEditRoll:
            p.ui->uiStatus->copy_label(_("Roll"));
            break;
        }

        _updateCursor();

        // We refresh the window to clear the OpenGL drawing cursor
        redraw();
    }

    void TimelineViewport::cursor(Fl_Cursor x) const noexcept
    {
        window()->cursor(x);
    }

    void TimelineViewport::_scrub(float dx) noexcept
    {
        TLRENDER_P();

        const auto player = p.player;
        if (file::isTemporaryNDI(player->path()))
            return;

        const auto& t = player->currentTime();
        const auto& time = t + otime::RationalTime(dx, t.rate());
        if (!player->isMuted() && p.ui->uiPrefs->uiPrefsScrubAutoPlay->value())
        {
            if (dx > 0)
            {
                player->setPlayback(timeline::Playback::Forward);
            }
            else
            {
                player->setPlayback(timeline::Playback::Reverse);
            }
        }
        player->seek(time);
    }

    void TimelineViewport::scrub() noexcept
    {
        TLRENDER_P();

        if (!p.player)
            return;

        const int X = Fl::event_x() * pixels_per_unit();
        const float scale = p.ui->uiPrefs->uiPrefsScrubbingSensitivity->value();
        float dx = (X - p.mousePress.x) / scale;

        if (std::abs(dx) >= 1.0F)
        {
            _scrub(dx);
            p.mousePress.x = X;
        }
    }

    void TimelineViewport::resize(int X, int Y, int W, int H)
    {
        Fl_SuperClass::resize(X, Y, W, H);
        if (hasFrameView())
        {
            _frameView();
        }
    }

    void TimelineViewport::updatePlaybackButtons() const noexcept
    {
        TLRENDER_P();

        p.frameTimes.clear();

        TimelineClass* c = p.ui->uiTimeWindow;

        if (!p.player)
        {
            c->uiPlayForwards->color(FL_BACKGROUND_COLOR);
            c->uiPlayBackwards->color(FL_BACKGROUND_COLOR);
            c->uiStop->color(FL_BACKGROUND_COLOR);
            c->uiPlayForwards->redraw();
            c->uiPlayBackwards->redraw();
            c->uiStop->redraw();
            return;
        }

        bool isNDI = file::isTemporaryNDI(p.player->path());
        if (isNDI)
        {
            c->uiFrame->deactivate();
            c->uiStartFrame->deactivate();
            c->uiEndFrame->deactivate();
            c->uiStartButton->deactivate();
            c->uiEndButton->deactivate();

            c->uiLoopMode->deactivate();

            c->uiPlayEnd->deactivate();
            c->uiPlayStart->deactivate();
            c->uiPlayBackwards->deactivate();
            c->uiStepForwards->deactivate();
            c->uiStepBackwards->deactivate();

            c->uiFPS->deactivate();
            c->fpsDefaults->deactivate();
        }
        else
        {
            c->uiFrame->activate();

            c->uiStartFrame->activate();
            c->uiEndFrame->activate();
            c->uiStartButton->activate();
            c->uiEndButton->activate();

            c->uiLoopMode->activate();

            c->uiPlayEnd->activate();
            c->uiPlayStart->activate();
            c->uiPlayBackwards->activate();
            c->uiStepForwards->activate();
            c->uiStepBackwards->activate();

            c->uiFPS->activate();
            c->fpsDefaults->activate();
        }

        c->uiPlayForwards->color(FL_BACKGROUND_COLOR);
        c->uiPlayBackwards->color(FL_BACKGROUND_COLOR);
        c->uiStop->color(FL_BACKGROUND_COLOR);
        Fl_Color color = FL_YELLOW;
        switch (p.player->playback())
        {
        case timeline::Playback::Forward:
            p.startTime = std::chrono::high_resolution_clock::now();
            c->uiPlayForwards->color(color);
            break;
        case timeline::Playback::Reverse:
            p.startTime = std::chrono::high_resolution_clock::now();
            c->uiPlayBackwards->color(color);
            break;
        default:
        case timeline::Playback::Stop:
            _updateCursor();
            c->uiStop->color(color);
            redrawWindows(); // to refresh FPS if set
            break;
        }

        c->uiPlayForwards->redraw();
        c->uiPlayBackwards->redraw();
        c->uiStop->redraw();
    }

    void TimelineViewport::startFrame() noexcept
    {
        TLRENDER_P();

        p.skippedFrames = 0;

        if (!p.player)
            return;

        p.player->start();
        updatePlaybackButtons();
    }

    void TimelineViewport::framePrev() noexcept
    {
        TLRENDER_P();
        p.skippedFrames = 0;

        if (!p.player)
            return;

        p.player->framePrev();
        updatePlaybackButtons();
    }

    void TimelineViewport::frameNext() noexcept
    {
        TLRENDER_P();
        p.skippedFrames = 0;

        if (!p.player)
            return;

        p.player->frameNext();
        updatePlaybackButtons();
    }

    void TimelineViewport::endFrame() noexcept
    {
        TLRENDER_P();
        p.skippedFrames = 0;

        if (!p.player)
            return;

        p.player->end();
        updatePlaybackButtons();
    }

    void TimelineViewport::playBackwards() noexcept
    {
        TLRENDER_P();
        p.skippedFrames = 0;

        if (!p.player)
            return;

        p.player->setPlayback(timeline::Playback::Reverse);
        togglePixelBar();
        updatePlaybackButtons();
        p.ui->uiMain->fill_menu(p.ui->uiMenuBar);
    }

    void TimelineViewport::stop() noexcept
    {
        TLRENDER_P();
        p.skippedFrames = 0;
        if (!p.player)
            return;

        p.player->setPlayback(timeline::Playback::Stop);

        togglePixelBar();
        updatePlaybackButtons();
        p.ui->uiMain->fill_menu(p.ui->uiMenuBar);
    }

    void TimelineViewport::playForwards() noexcept
    {
        TLRENDER_P();
        p.skippedFrames = 0;

        if (!p.player)
            return;

        p.player->setPlayback(timeline::Playback::Forward);

        togglePixelBar();
        updatePlaybackButtons();
        p.ui->uiMain->fill_menu(p.ui->uiMenuBar);
    }

    void TimelineViewport::togglePixelBar() const noexcept
    {
        TLRENDER_P();

        if (!p.player || !p.ui->uiPrefs->uiPrefsAutoHidePixelBar->value() ||
            !p.ui->uiPrefs->uiPrefsPixelToolbar->value() || p.presentation)
            return;

        auto playback = p.player->playback();
        if (playback == timeline::Playback::Stop)
        {
            if (!p.ui->uiPixelBar->visible())
                toggle_pixel_bar(nullptr, p.ui);
        }
        else
        {
            if (p.ui->uiPixelBar->visible())
                toggle_pixel_bar(nullptr, p.ui);
        }
    }

    void TimelineViewport::togglePlayback() noexcept
    {
        TLRENDER_P();
        p.skippedFrames = 0;

        if (!p.player)
            return;

        p.player->togglePlayback();

        togglePixelBar();
        updatePlaybackButtons();
        p.ui->uiMain->fill_menu(p.ui->uiMenuBar);
    }

    const area::Info& TimelineViewport::getColorAreaInfo() noexcept
    {
        return _p->colorAreaInfo;
    }

    const timeline::OCIOOptions& TimelineViewport::getOCIOOptions() noexcept
    {
        return _p->ocioOptions;
    }

    const timeline::BackgroundOptions&
    TimelineViewport::getBackgroundOptions() const noexcept
    {
        return _p->backgroundOptions;
    }

    void TimelineViewport::setBackgroundOptions(
        const timeline::BackgroundOptions& value)
    {
        TLRENDER_P();
        if (value == p.backgroundOptions)
            return;
        Message msg;
        msg["command"] = "setBackgroundOptions";
        msg["value"] = value;
        tcp->pushMessage(msg);
        p.backgroundOptions = value;
        redrawWindows();
    }

    void TimelineViewport::setOCIOOptions(
        const timeline::OCIOOptions& value) noexcept
    {
        TLRENDER_P();
        if (value == p.ocioOptions)
            return;

        std::string input, view;

        input = value.input;
        if (input.empty())
            input = _("None");

        view = value.view;
        if (view.empty())
            return;

        p.ocioOptions = value;

        ocio::setOcioIcs(input);
        ocio::setOcioView(ocio::ocioDisplayViewShortened(value.display, view));

        if (panel::colorPanel)
        {
            panel::colorPanel->refresh();
        }

        if (p.ui->uiSecondary && p.ui->uiSecondary->viewport())
        {
            Viewport* view = p.ui->uiSecondary->viewport();
            view->setOCIOOptions(value);
        }
        p.ui->uiTimeline->setOCIOOptions(value);
        p.ui->uiTimeline->redraw(); // to refresh thumbnail

        Message msg;
        msg["command"] = "setOCIOOptions";
        msg["value"] = value;
        tcp->pushMessage(msg);

        redrawWindows();
    }

    timeline::LUTOptions& TimelineViewport::lutOptions() noexcept
    {
        return _p->lutOptions;
    }

    void
    TimelineViewport::setLUTOptions(const timeline::LUTOptions& value) noexcept
    {
        TLRENDER_P();
        if (value == p.lutOptions)
            return;
        p.lutOptions = value;
        redraw();
    }

    void TimelineViewport::setImageOptions(
        const std::vector<timeline::ImageOptions>& value) noexcept
    {
        TLRENDER_P();
        if (value == p.imageOptions)
            return;
        p.imageOptions = value;
        redraw();
    }

    void TimelineViewport::setDisplayOptions(
        const std::vector<timeline::DisplayOptions>& value) noexcept
    {
        TLRENDER_P();
        if (value == p.displayOptions)
            return;
        p.displayOptions = value;
        redraw();
    }

    void TimelineViewport::setCompareOptions(
        const timeline::CompareOptions& value) noexcept
    {
        TLRENDER_P();
        if (value == p.compareOptions)
            return;

        p.compareOptions = value;

        redraw();
    }

    void
    TimelineViewport::setStereo3DOptions(const Stereo3DOptions& value) noexcept
    {
        TLRENDER_P();
        if (value == p.stereo3DOptions)
            return;
        p.stereo3DOptions = value;
        redraw();
    }

    void TimelineViewport::setTimelinePlayer(
        TimelinePlayer* player, const bool primary) noexcept
    {
        TLRENDER_P();

        if (p.player == player)
            return;

        p.player = player;

        updateVideoLayers();

        p.videoData.clear();

        if (player)
        {
            if (primary)
                player->setTimelineViewport(this);
            else
                player->setSecondaryViewport(this);

            p.videoData = player->currentVideo();
            p.switchClip = true;
        }


        refreshWindows(); // needed We need to refresh, as the new
                          // video data may have different sizes.
    }

    mrv::TimelinePlayer* TimelineViewport::getTimelinePlayer() const noexcept
    {
        return _p->player;
    }

    const math::Vector2i& TimelineViewport::viewPos() const noexcept
    {
        return _p->viewPos;
    }

    float TimelineViewport::viewZoom() const noexcept
    {
        return _p->viewZoom;
    }

    void TimelineViewport::setFrameView(bool active) noexcept
    {
        _p->frameView = active;
    }

    bool TimelineViewport::hasFrameView() const noexcept
    {
        return _p->frameView;
    }

    bool TimelineViewport::getSafeAreas() const noexcept
    {
        return _p->safeAreas;
    }

    bool TimelineViewport::getDataWindow() const noexcept
    {
        return _p->dataWindow;
    }

    bool TimelineViewport::getDisplayWindow() const noexcept
    {
        return _p->displayWindow;
    }

    bool TimelineViewport::getIgnoreDisplayWindow() const noexcept
    {
        return _p->ignoreDisplayWindow;
    }

    void TimelineViewport::setSafeAreas(bool value) noexcept
    {
        if (value == _p->safeAreas)
            return;
        _p->safeAreas = value;
        redrawWindows();
    }

    void TimelineViewport::setDataWindow(bool value) noexcept
    {
        if (value == _p->dataWindow)
            return;
        _p->dataWindow = value;
        redrawWindows();
    }

    void TimelineViewport::setDisplayWindow(bool value) noexcept
    {
        if (value == _p->displayWindow)
            return;
        _p->displayWindow = value;
        redrawWindows();
    }

    void TimelineViewport::setIgnoreDisplayWindow(bool value) noexcept
    {
        if (value == _p->ignoreDisplayWindow)
            return;
        _p->ignoreDisplayWindow = value;
        redrawWindows();
    }

    //! Return the crop masking
    float TimelineViewport::getMask() const noexcept
    {
        return _p->masking;
    }

    //! Set the crop masking
    void TimelineViewport::setMask(float value) noexcept
    {
        if (value == _p->masking)
            return;
        _p->masking = value;
        redrawWindows();
    }

    bool TimelineViewport::getHudActive() const
    {
        return _p->hudActive;
    }

    void TimelineViewport::setHudActive(const bool active)
    {
        _p->hudActive = active;
        redrawWindows();
    }

    void TimelineViewport::setHudDisplay(const HudDisplay hud)
    {
        _p->hud = hud;
        redrawWindows();
    }

    void TimelineViewport::toggleHudDisplay(const HudDisplay value)
    {
        _p->hud = static_cast<HudDisplay>(
            static_cast<int>(_p->hud) ^ static_cast<int>(value));
        redrawWindows();
    }

    HudDisplay TimelineViewport::getHudDisplay() const noexcept
    {
        return _p->hud;
    }

    void TimelineViewport::_updateCursor() const noexcept
    {
        TLRENDER_P();

        if (p.actionMode == ActionMode::kScrub ||
            p.actionMode == ActionMode::kSelection ||
            p.actionMode == ActionMode::kRotate)
            cursor(FL_CURSOR_CROSS);
        // else if ( p.actionMode == ActionMode::kRotate )
        //     cursor( FL_CURSOR_MOVE );
        else if (p.actionMode == ActionMode::kText)
            cursor(FL_CURSOR_INSERT);
        else
        {
            // Only hide the cursor if we are on the view widgets.
            auto primary = p.ui->uiView;
            Viewport* secondary = nullptr;
            if (_p->ui->uiSecondary && _p->ui->uiSecondary->window()->visible())
            {
                secondary = _p->ui->uiSecondary->viewport();
            }

            Fl_Widget* widget = Fl::belowmouse();
            if (widget != primary && (secondary && widget != secondary))
                return;
            if (p.player)
                cursor(FL_CURSOR_NONE);
        }
    }

    void TimelineViewport::setViewPosAndZoom(
        const math::Vector2i& pos, float zoom) noexcept
    {
        TLRENDER_P();
        if (pos == p.viewPos && zoom == p.viewZoom)
            return;
        p.viewPos = pos;
        p.viewZoom = zoom;

        _updateZoom();
        redraw();

        bool send = p.ui->uiPrefs->SendPanAndZoom->value();
        if (send)
        {
            nlohmann::json viewPos = p.viewPos;
            nlohmann::json viewport = getViewportSize();
            Message msg;
            msg["command"] = "viewPosAndZoom";
            msg["viewPos"] = viewPos;
            msg["zoom"] = p.viewZoom;
            msg["viewport"] = viewport;
            tcp->pushMessage(msg);
        }

        auto m = getMultilineInput();
        if (!m)
            return;

        redraw();
    }

    void TimelineViewport::setViewZoom(
        float zoom, const math::Vector2i& focus) noexcept
    {
        TLRENDER_P();
        math::Vector2i pos;
        pos.x = focus.x + (p.viewPos.x - focus.x) * (zoom / p.viewZoom);
        pos.y = focus.y + (p.viewPos.y - focus.y) * (zoom / p.viewZoom);
        setViewPosAndZoom(pos, zoom);
    }

    void TimelineViewport::frameView() noexcept
    {
        TLRENDER_P();
        _frameView();
        _refresh();
        _updateZoom();
        updateCoords();
    }

    void TimelineViewport::viewZoom1To1() noexcept
    {
        TLRENDER_P();
        const auto viewportSize = _getViewportCenter();
        const auto renderSize = getRenderSize();
        const math::Vector2i c(renderSize.w / 2, renderSize.h / 2);
        p.viewPos.x = viewportSize.x - c.x;
        p.viewPos.y = viewportSize.y - c.y;
        setViewPosAndZoom(p.viewPos, 1.F);
    }

    void TimelineViewport::currentVideoCallback(
        const std::vector<timeline::VideoData>& values,
        const TimelinePlayer* sender) noexcept
    {
        TLRENDER_P();
        p.videoData = values;

        if (p.resizeWindow)
        {
            p.resizeWindow = p.switchClip = false;
            resizeWindow();
        }
        else if (p.frameView && p.switchClip)
        {
            _frameView();
            p.switchClip = false;
        }

        _getTags();
        int layerId = sender->videoLayer();
        p.missingFrame = false;
        if (p.missingFrameType != MissingFrameType::kBlackFrame &&
            !values[0].layers.empty())
        {
            const auto& image = values[0].layers[0].image;
            const auto& imageB = values[0].layers[0].imageB;
            if ((!image || !image->isValid()) &&
                (!imageB || !imageB->isValid()))
            {
                p.missingFrame = true;
                if (sender->playback() != timeline::Playback::Forward)
                {
                    io::Options ioOptions;
                    {
                        std::stringstream s;
                        s << layerId;
                        ioOptions["Layer"] = s.str();
                    }
                    const auto& timeline = sender->timeline();
                    const auto& inOutRange = sender->inOutRange();
                    auto currentTime = values[0].time;
                    // Seek until we find a previous frame or reach
                    // the beginning of the inOutRange.
                    while (1)
                    {
                        currentTime -=
                            otio::RationalTime(1, currentTime.rate());
                        const auto& videoData =
                            timeline->getVideo(currentTime, ioOptions)
                                .future.get();
                        if (videoData.layers.empty())
                            continue;
                        const auto& image = videoData.layers[0].image;
                        if (image && image->isValid())
                        {
                            p.lastVideoData = videoData;
                            break;
                        }
                        if (currentTime <= inOutRange.start_time())
                            break;
                    }
                }
            }
            else
            {
                if (sender->playback() == timeline::Playback::Forward)
                {
                    p.lastVideoData = values[0];
                }
            }
        }

        // Refresh media info panel if there's data window present
        if (panel::imageInfoPanel)
        {
            bool refresh = false;

            // If timeline is stopped or has a single frame,
            // refresh the media info panel.
            if (sender->playback() == timeline::Playback::Stop ||
                sender->timeRange().duration().value() == 1.0)
                refresh = true;

            // If timeline has a Data Window (it is an OpenEXR)
            // we also refresh the media info panel.
            auto i = p.tagData.find("Data Window");
            if (i != p.tagData.end())
                refresh = true;

            if (refresh)
            {
                panel::imageInfoPanel->refresh();
            }
        }

        if (p.selection.max.x != -1)
        {
            if (!values[0].layers.empty())
            {
                const auto& image = values[0].layers[0].image;
                if (image && image->isValid())
                {
                    const auto& videoSize = image->getSize();
                    if (p.videoSize != videoSize)
                    {
                        math::Box2i area;
                        area.max.x = -1;
                        setSelectionArea(area);
                        p.videoSize = videoSize;
                    }
                }
            }
        }
        if (p.ui->uiBottomBar->visible())
        {
            TimelineClass* c = p.ui->uiTimeWindow;
            c->uiFrame->setTime(values[0].time);
            p.ui->uiTimeline->redraw();
        }

        redraw();
    }

    bool TimelineViewport::_isPlaybackStopped() const noexcept
    {
        TLRENDER_P();
        bool stopped = false;
        if (p.player)
        {
            const auto player = p.player;
            stopped = (player->playback() == timeline::Playback::Stop) ||
                      player->isStepping();
        }
        return stopped;
    }

    bool TimelineViewport::_isSingleFrame() const noexcept
    {
        TLRENDER_P();

        bool single = false;
        if (p.player)
        {
            // However, if the movie is a single frame long, we need to
            // update it
            if (p.player->inOutRange().duration().to_frames() == 1)
                single = true;
        }
        return single;
    }

    bool TimelineViewport::_shouldUpdatePixelBar() const noexcept
    {
        TLRENDER_P();
        // Don't update the pixel bar here if we are playing the movie,
        // as we will update it in the draw() routine.
        bool update = _isPlaybackStopped() | _isSingleFrame();
        return update;
    }

    void TimelineViewport::cacheChangedCallback() const noexcept
    {
        if (!_p->ui->uiBottomBar->visible())
            return;

        // This checks whether playback is stopped and if so redraws timeline
        bool update = _shouldUpdatePixelBar();
        if (update)
        {
            _p->ui->uiTimeline->redraw();

            if (getHudActive() && (getHudDisplay() & HudDisplay::kCache))
            {
                redrawWindows();
            }
        }
    }

    math::Size2i TimelineViewport::getViewportSize() const noexcept
    {
        TimelineViewport* t = const_cast< TimelineViewport* >(this);
        return math::Size2i(t->pixel_w(), t->pixel_h());
    }

    math::Size2i TimelineViewport::getRenderSize() const noexcept
    {
        TLRENDER_P();
        return timeline::getRenderSize(p.compareOptions.mode, p.videoData);
    }

    float TimelineViewport::getRotation() const noexcept
    {
        return _p->rotation;
    }

    void TimelineViewport::setRotation(float x) noexcept
    {
        TLRENDER_P();

        if (x == p.rotation)
            return;

        p.rotation = x;

        if (hasFrameView())
        {
            _frameView();
        }

        redrawWindows();
        updatePixelBar();
        updateCoords();
    }

    math::Vector2i TimelineViewport::_getViewportCenter() const noexcept
    {
        const auto viewportSize = getViewportSize();
        return math::Vector2i(viewportSize.w / 2, viewportSize.h / 2);
    }

    void TimelineViewport::centerView() noexcept
    {
        TLRENDER_P();
        const auto viewportSize = getViewportSize();
        const auto renderSize = getRenderSize();
        const math::Vector2i c(renderSize.w / 2, renderSize.h / 2);
        math::Vector2i pos;
        pos.x = viewportSize.w / 2.F - c.x * p.viewZoom;
        pos.y = viewportSize.h / 2.F - c.y * p.viewZoom;
        setViewPosAndZoom(pos, p.viewZoom);
        p.mousePos = _getFocus();
        _refresh();
        updateCoords();
    }

    bool TimelineViewport::_isEnvironmentMap() const noexcept
    {
        bool isEnvironment =
            _p->environmentMapOptions.type != EnvironmentMapOptions::kNone;
        return isEnvironment;
    }

    void TimelineViewport::_frameView() noexcept
    {
        TLRENDER_P();
        if (p.environmentMapOptions.type != EnvironmentMapOptions::kNone)
        {
            return;
        }
        const auto viewportSize = getViewportSize();
        const auto renderSize = getRenderSize();
        const float rotation =
            normalizeAngle0to360(p.rotation + p.videoRotation);

        float zoom = 1.0;

        if (rotation == 90.F || rotation == 270.F)
        {
            if (renderSize.w > 0)
            {
                zoom = viewportSize.h / static_cast<float>(renderSize.w);
                if (renderSize.h > 0 && zoom * renderSize.h > viewportSize.w)
                {
                    zoom = viewportSize.w / static_cast<float>(renderSize.h);
                }
            }
        }
        else
        {
            if (renderSize.h > 0)
            {
                zoom = viewportSize.h / static_cast<float>(renderSize.h);
                if (renderSize.w > 0 && zoom * renderSize.w > viewportSize.w)
                {
                    zoom = viewportSize.w / static_cast<float>(renderSize.w);
                }
            }
        }

        const math::Vector2i c(renderSize.w / 2, renderSize.h / 2);
        const math::Vector2i viewPos(
            viewportSize.w / 2.F - c.x * zoom,
            viewportSize.h / 2.F - c.y * zoom);
        setViewPosAndZoom(viewPos, zoom);

        p.mousePos = _getFocus();
        redraw();
    }

    void TimelineViewport::resizeWindow() noexcept
    {
        TLRENDER_P();
        auto renderSize = getRenderSize();

        if (!renderSize.isValid())
            return;

        Fl_Double_Window* mw = p.ui->uiMain;
        int screen = mw->screen_num();
        float scale = Fl::screen_scale(screen);

        int W = renderSize.w;
        int H = renderSize.h;

        int minx, miny, maxW, maxH, posX, posY;
        Fl::screen_work_area(minx, miny, maxW, maxH, screen);

        PreferencesUI* uiPrefs = p.ui->uiPrefs;
        if (uiPrefs->uiWindowFixedPosition->value())
        {
            posX = (int)uiPrefs->uiWindowXPosition->value();
            posY = (int)uiPrefs->uiWindowYPosition->value();
        }
        else
        {
            posX = mw->x();
            posY = mw->y();
        }

        int decW = mw->decorated_w();
        int decH = mw->decorated_h();

        int dW = decW - mw->w();
        int dH = decH - mw->h();

        maxW -= dW;
        maxH -= dH;
        posX += dW / 2;
#ifdef _WIN32
        miny += dH - dW / 2;
#endif

        // Take into account the different UI bars
        if (p.ui->uiMenuGroup->visible())
            H += p.ui->uiMenuGroup->h();

        if (p.ui->uiTopBar->visible())
            H += p.ui->uiTopBar->h();

        if (p.ui->uiPixelBar->visible())
            H += p.ui->uiPixelBar->h();

        if (p.ui->uiBottomBar->visible())
        {
            H += p.ui->uiBottomBar->h();
        }

        if (p.ui->uiStatusGroup->visible())
            H += p.ui->uiStatusGroup->h();

        if (p.ui->uiToolsGroup->visible())
            W += p.ui->uiToolsGroup->w();

        if (p.ui->uiDockGroup->visible())
            W += p.ui->uiDockGroup->w();

        bool alwaysFrameView = (bool)uiPrefs->uiPrefsAutoFitImage->value();
        p.frameView = alwaysFrameView;

        if (uiPrefs->uiWindowFixedSize->value())
        {
            W = (int)uiPrefs->uiWindowXSize->value();
            H = (int)uiPrefs->uiWindowYSize->value();
        }

        maxW = (int)(maxW / scale);
        if (W < 690)
        {
            p.frameView = true;
            W = 690;
        }
        else if (W > maxW)
        {
            p.frameView = true;
            W = maxW;
        }

        maxH = (int)(maxH / scale);
        if (H < 602)
        {
            p.frameView = true;
            H = 602;
        }
        else if (H > maxH)
        {
            p.frameView = true;
            H = maxH;
        }

        if (W == renderSize.w)
        {
            p.frameView = true;
        }

        if (p.ui->uiBottomBar->visible())
        {
            int timelineSize = calculate_edit_viewport_size(p.ui);
            H += timelineSize;
            if (H > maxH)
                H = maxH;
        }

        if (posX + W > maxW)
            posX = minx;
        if (posY + W > maxH)
            posY = miny;

        mw->resize(posX, posY, W, H);

        p.ui->uiRegion->layout();

        set_edit_mode_cb(editMode, p.ui);
    }

    math::Vector2i TimelineViewport::_getFocus(int X, int Y) const noexcept
    {
        TimelineViewport* self = const_cast< TimelineViewport* >(this);
        math::Vector2i pos;
        const float devicePixelRatio = self->pixels_per_unit();
        pos.x = X * devicePixelRatio;
        pos.y = (h() - 1 - Y) * devicePixelRatio;
        return pos;
    }

    math::Vector2i TimelineViewport::_getFocus() const noexcept
    {
        return _getFocus(_p->event_x, _p->event_y);
    }

    math::Vector2f TimelineViewport::_getRasterf(int X, int Y) const noexcept
    {
        const auto& pm = _pixelMatrix();
        math::Vector3f pos(X, Y, 1.F);
        pos = pm * pos;
        return math::Vector2f(pos.x, pos.y);
    }

    math::Vector2f TimelineViewport::_getRasterf() const noexcept
    {
        return _getRasterf(_p->mousePos.x, _p->mousePos.y);
    }

    math::Vector2i TimelineViewport::_getRaster() const noexcept
    {
        const auto& pos = _getRasterf(_p->mousePos.x, _p->mousePos.y);
        return math::Vector2i(pos.x, pos.y);
    }

    void TimelineViewport::_updateZoom() const noexcept
    {
        TLRENDER_P();
        char label[12];
        if (p.viewZoom >= 1.0f)
            snprintf(label, 12, "x%.2g", p.viewZoom);
        else
            snprintf(label, 12, "1/%.3g", 1.0f / p.viewZoom);
        PixelToolBarClass* c = _p->ui->uiPixelWindow;
        c->uiZoom->copy_label(label);
    }

    void TimelineViewport::updateCoords() const noexcept
    {
        TLRENDER_P();
        char buf[40];

        math::Vector2i pos;
        if (p.environmentMapOptions.type == EnvironmentMapOptions::kNone)
        {
            p.mousePos = _getFocus();
            pos = _getRaster();

            // If we have Mirror X or Mirror Y on, flip the coordinates
            const auto o = p.ui->app->displayOptions();
            TimelineViewport* self = const_cast< TimelineViewport* >(this);
            const float devicePixelRatio = self->pixels_per_unit();
            const auto& renderSize = getRenderSize();
            if (o.mirror.x)
                pos.x = (renderSize.w - 1 - pos.x) * devicePixelRatio;
            if (o.mirror.y)
                pos.y = (renderSize.h - 1 - pos.y) * devicePixelRatio;
        }
        else
        {
            pos.x = Fl::event_x();
            pos.y = Fl::event_x();
        }

        snprintf(buf, 40, "%5d, %5d", pos.x, pos.y);
        PixelToolBarClass* c = _p->ui->uiPixelWindow;
        c->uiCoord->value(buf);
    }

    //! Set the Annotation previous ghost frames.
    void TimelineViewport::setGhostPrevious(int x)
    {
        _p->ghostPrevious = x;
    }

    //! Set the Annotation previous ghost frames.
    void TimelineViewport::setGhostNext(int x)
    {
        _p->ghostNext = x;
    }

    //! Set the Annotation previous ghost frames.
    void TimelineViewport::setMissingFrameType(MissingFrameType x)
    {
        _p->missingFrameType = x;
    }

    // Cannot be const image::Color4f& rgba, as we clamp values
    void TimelineViewport::_updatePixelBar(image::Color4f& rgba) const noexcept
    {
        TLRENDER_P();

        PixelToolBarClass* c = _p->ui->uiPixelWindow;
        char buf[24];
        switch (c->uiAColorType->value())
        {
        case kRGBA_Float:
            c->uiPixelR->value(float_printf(buf, rgba.r));
            c->uiPixelG->value(float_printf(buf, rgba.g));
            c->uiPixelB->value(float_printf(buf, rgba.b));
            c->uiPixelA->value(float_printf(buf, rgba.a));
            break;
        case kRGBA_Hex:
            c->uiPixelR->value(hex_printf(buf, rgba.r));
            c->uiPixelG->value(hex_printf(buf, rgba.g));
            c->uiPixelB->value(hex_printf(buf, rgba.b));
            c->uiPixelA->value(hex_printf(buf, rgba.a));
            break;
        case kRGBA_Decimal:
            c->uiPixelR->value(dec_printf(buf, rgba.r));
            c->uiPixelG->value(dec_printf(buf, rgba.g));
            c->uiPixelB->value(dec_printf(buf, rgba.b));
            c->uiPixelA->value(dec_printf(buf, rgba.a));
            break;
        }

        if (rgba.r > 1.0f)
            rgba.r = 1.0f;
        else if (rgba.r < 0.0f)
            rgba.r = 0.0f;
        if (rgba.g > 1.0f)
            rgba.g = 1.0f;
        else if (rgba.g < 0.0f)
            rgba.g = 0.0f;
        if (rgba.b > 1.0f)
            rgba.b = 1.0f;
        else if (rgba.b < 0.0f)
            rgba.b = 0.0f;

        uint8_t col[3];
        col[0] = uint8_t(rgba.r * 255.f);
        col[1] = uint8_t(rgba.g * 255.f);
        col[2] = uint8_t(rgba.b * 255.f);

        Fl_Color fltk_color(fl_rgb_color(col[0], col[1], col[2]));

        // In fltk color lookup? (0 != Fl_BLACK)
        if (fltk_color == 0)
            c->uiPixelView->color(FL_BLACK);
        else
            c->uiPixelView->color(fltk_color);
        c->uiPixelView->redraw();

        image::Color4f hsv;

        int cspace = c->uiBColorType->value() + 1;

        switch (cspace)
        {
        case color::kHSV:
            hsv = color::rgb::to_hsv(rgba);
            break;
        case color::kHSL:
            hsv = color::rgb::to_hsl(rgba);
            break;
#ifdef TLRENDER_EXR
        case color::kCIE_XYZ:
            hsv = color::rgb::to_xyz(rgba);
            break;
        case color::kCIE_xyY:
            hsv = color::rgb::to_xyY(rgba);
            break;
        case color::kCIE_Lab:
            hsv = color::rgb::to_lab(rgba);
            break;
        case color::kCIE_Luv:
            hsv = color::rgb::to_luv(rgba);
            break;
#endif
        case color::kYUV:
            hsv = color::rgb::to_yuv(rgba);
            break;
        case color::kYDbDr:
            hsv = color::rgb::to_YDbDr(rgba);
            break;
        case color::kYIQ:
            hsv = color::rgb::to_yiq(rgba);
            break;
        case color::kITU_601:
            hsv = color::rgb::to_ITU601(rgba);
            break;
        case color::kITU_709:
            hsv = color::rgb::to_ITU709(rgba);
            break;
        case color::kRGB:
        default:
            hsv = rgba;
            break;
        }

        c->uiPixelH->value(float_printf(buf, hsv.r));
        c->uiPixelS->value(float_printf(buf, hsv.g));
        c->uiPixelV->value(float_printf(buf, hsv.b));

        mrv::BrightnessType brightness_type =
            (mrv::BrightnessType)c->uiLType->value();
        hsv.a = calculate_brightness(rgba, brightness_type);

        c->uiPixelL->value(float_printf(buf, hsv.a));
    }

    void TimelineViewport::updatePixelBar() const noexcept
    {
        TLRENDER_P();
        const Fl_Widget* belowmouse = Fl::belowmouse();
        if (!p.ui->uiPixelBar->visible() || !visible_r() || belowmouse != this)
            return;

        const math::Size2i& r = getRenderSize();

        p.mousePos = _getFocus();

        constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
        image::Color4f rgba(NaN, NaN, NaN, NaN);
        bool inside = true;
        const auto& pos = _getRaster();
        if (p.environmentMapOptions.type == EnvironmentMapOptions::kNone &&
            (pos.x < 0 || pos.x >= r.w || pos.y < 0 || pos.y >= r.h))
            inside = false;

        if (inside)
        {
            _readPixel(rgba);
        }

        _updatePixelBar(rgba);
    }

    EnvironmentMapOptions
    TimelineViewport::getEnvironmentMapOptions() const noexcept
    {
        return _p->environmentMapOptions;
    }

    void TimelineViewport::setEnvironmentMapOptions(
        const EnvironmentMapOptions& value) noexcept
    {
        TLRENDER_P();
        if (p.environmentMapOptions == value)
            return;
        bool refresh = p.environmentMapOptions.type != value.type;
        p.environmentMapOptions = value;

        bool send = p.ui->uiPrefs->SendPanAndZoom->value();
        if (send)
        {
            Message opts = value;
            Message msg;
            msg["command"] = "setEnvironmentMapOptions";
            msg["value"] = opts;
            tcp->pushMessage(msg);
        }

        if (panel::environmentMapPanel)
        {
            panel::environmentMapPanel->setEnvironmentMapOptions(
                p.environmentMapOptions);
        }

        if (refresh)
            refreshWindows();
        else
            redrawWindows();
    }

    void TimelineViewport::refreshWindows()
    {
        _p->ui->uiView->valid(0);
        _p->ui->uiView->redraw();
        if (_hasSecondaryViewport())
        {
            Viewport* view = _p->ui->uiSecondary->viewport();
            view->valid(0);
            view->redraw();
        }
    }

    void TimelineViewport::updateOCIOOptions() noexcept
    {
        TLRENDER_P();
        int inputIndex = p.ui->uiICS->value();
        std::string input = p.ui->uiICS->label();
        if (inputIndex < 0)
            input = "";

        timeline::OCIOOptions o;
        if (!input.empty() && input != _("None"))
            o.enabled = true;
        else
            o.enabled = false;
        o.fileName = p.ui->uiPrefs->uiPrefsOCIOConfig->value();
        o.input = input;

        PopupMenu* menu = p.ui->OCIOView;

        int viewIndex = menu->value();
        const Fl_Menu_Item* w = nullptr;
        if (viewIndex >= 0)
            w = menu->mvalue();
        const char* lbl;
        if (!w)
        {
            lbl = menu->label();
            for (int i = 0; i < menu->children(); ++i)
            {
                const Fl_Menu_Item* c = menu->child(i);
                if ((c->flags & FL_SUBMENU) || !c->label())
                    continue;
                if (strcmp(lbl, c->label()) == 0)
                {
                    w = menu->child(i);
                    break;
                }
            }
        }
        else
            lbl = w->label();
        const Fl_Menu_Item* t = nullptr;
        const Fl_Menu_Item* c = nullptr;
        if (menu->children() > 0)
            c = menu->child(0);
        for (; c && w && c != w; ++c)
        {
            if (c && (c->flags & FL_SUBMENU))
                t = c;
        }
        if (t && t->label())
        {
            o.display = t->label();
            if (lbl && strcmp(lbl, t->label()) != 0)
            {
                o.view = lbl;
            }

            const std::string& fullname = o.display + " / " + o.view;
            menu->copy_label(fullname.c_str());
        }
        else
        {
            if (!c || !c->label())
                return;
            std::string view = c->label();
            size_t pos = view.find('(');
            std::string display = view.substr(pos + 1, view.size());
            o.view = view.substr(0, pos - 1);
            pos = display.find(')');
            o.display = display.substr(0, pos);

            menu->copy_label(o.view.c_str());
        }

        PopupMenu* lookMenu = p.ui->OCIOLook;

        int lookIndex = lookMenu->value();
        if (lookIndex >= 0 && lookIndex < lookMenu->children())
        {
            w = lookMenu->child(lookIndex);
            std::string look = w->label();
            if (look == _("None"))
                look = "";
            o.look = look;
        }
        setOCIOOptions(o);
        redrawWindows();
    }

    inline float calculate_fstop(float exposure) noexcept
    {
        float base = 3.0f; // for exposure 0 = f/8

        float seq1, seq2;

        float e = exposure * 0.5f;
        float v = (float)base + (float)int(-e);

        float f = fmod(fabs(exposure), 2.0f);
        if (exposure >= 0)
        {
            seq1 = 1.0f * powf(2.0f, v);     // 8
            seq2 = 1.4f * powf(2.0f, v - 1); // 5.6
        }
        else
        {
            seq1 = 1.0f * powf(2.0f, v); // 8
            seq2 = 1.4f * powf(2.0f, v); // 11
        }

        float fstop = seq1 * (1 - f) + f * seq2;
        return fstop;
    }

    void
    TimelineViewport::_pushColorMessage(const std::string& command, float value)
    {
        bool send = _p->ui->uiPrefs->SendColor->value();
        if (send)
        {
            tcp->pushMessage(command, value);
        }
    }

    void TimelineViewport::updateDisplayOptions() noexcept
    {
        TLRENDER_P();

        if (p.displayOptions.empty())
        {
            p.ui->uiGain->value(1.0f);
            p.ui->uiGainInput->value(1.0f);
            p.ui->uiGamma->value(1.0f);
            p.ui->uiGammaInput->value(1.0f);
            _pushColorMessage("gain", 1.0f);
            _pushColorMessage("gamma", 1.0f);
            return;
        }

        timeline::DisplayOptions d;
        d = p.displayOptions[0];

        // Get these from the toggle menus

        // We toggle R,G,B,A channels from hotkeys

        float gamma = p.ui->uiGamma->value();
        _pushColorMessage("gamma", gamma);
        if (gamma != d.levels.gamma)
        {
            d.levels.gamma = gamma;
            d.levels.enabled = true;
            redraw();
        }

        d.exrDisplay.enabled = false;
        if (d.exrDisplay.exposure < 0.001F)
            d.exrDisplay.exposure = d.color.brightness.x;

        float gain = p.ui->uiGain->value();
        _pushColorMessage("gain", gain);
        d.color.brightness.x = d.exrDisplay.exposure * gain;
        d.color.brightness.y = d.exrDisplay.exposure * gain;
        d.color.brightness.z = d.exrDisplay.exposure * gain;

        if (!mrv::is_equal(gain, 1.F))
        {
            d.color.enabled = true;

            float exposure = (logf(gain) / logf(2.0f));
            float fstop = calculate_fstop(exposure);
            char buf[8];
            snprintf(buf, 8, "f/%1.1f", fstop);
            p.ui->uiFStop->copy_label(buf);
            p.ui->uiFStop->labelcolor(0xFF800000);
        }
        else
        {
            p.ui->uiFStop->copy_label("f/8");
            p.ui->uiFStop->labelcolor(p.ui->uiGain->labelcolor());
        }

        //  @todo.    ask darby why image filters are both in display
        //            options and in imageoptions
        const Fl_Menu_Item* item =
            p.ui->uiMenuBar->find_item(_("Render/Minify Filter/Linear"));
        timeline::ImageFilter min_filter = timeline::ImageFilter::Nearest;
        if (item && item->value())
            min_filter = timeline::ImageFilter::Linear;

        item = p.ui->uiMenuBar->find_item(_("Render/Magnify Filter/Linear"));
        timeline::ImageFilter mag_filter = timeline::ImageFilter::Nearest;
        if (item && item->value())
            mag_filter = timeline::ImageFilter::Linear;

        d.imageFilters.minify = min_filter;
        d.imageFilters.magnify = mag_filter;

        _updateDisplayOptions(d);
    }

    void TimelineViewport::updateVideoLayers(int idx) noexcept
    {
        TLRENDER_P();

        const auto& player = getTimelinePlayer();
        if (!player)
            return;

        const auto& info = player->player()->getIOInfo();

        const auto& videos = info.video;

        p.ui->uiColorChannel->clear();

        std::string name;
        size_t pos;
        for (const auto& video : videos)
        {
            name = video.name;

            if (name == "B,G,R" || name == "R,G,B" || name == "Default")
                name = _("Color");
            else if (name == "A,B,G,R")
                name = "R,G,B,A";

            p.ui->uiColorChannel->add(name.c_str());
        }

        p.ui->uiColorChannel->menu_end();

        if (p.ui->uiColorChannel->children() == 0)
        {
            p.ui->uiColorChannel->copy_label(_("(no image)"));
        }
        else
        {
            const Fl_Menu_Item* item = p.ui->uiColorChannel->child(idx);
            p.ui->uiColorChannel->copy_label(item->label());
        }
    }

    // This function is needed to force the repositioning of the window/view
    // before querying, for example, the mouse coordinates.
    void TimelineViewport::_refresh() noexcept
    {
        redraw();
        Fl::flush(); // force the redraw
    }

    bool TimelineViewport::getPresentationMode() const noexcept
    {
        return _p->presentation;
    }

    bool TimelineViewport::_hasSecondaryViewport() const noexcept
    {
        TLRENDER_P();
        bool secondary = false;
        if (p.ui->uiSecondary && p.ui->uiSecondary->window()->visible())
            secondary = true;
        return secondary;
    }

    // Change the main window to fullscreen without modifying the
    // internal variables p.fullScreen nor p.presentation.
    void TimelineViewport::_setFullScreen(bool active) noexcept
    {
        TLRENDER_P();
        MainWindow* w;
        TimelineViewport* view;

        bool secondary = _hasSecondaryViewport();
        if (secondary)
        {
            w = p.ui->uiSecondary->window();
            view = p.ui->uiSecondary->viewport();
        }
        else
        {
            w = p.ui->uiMain;
            view = p.ui->uiView;
        }

        if (!active)
        {
            if (w->fullscreen_active())
            {
                w->fullscreen_off();
            }
        }
        else
        {
            if (!w->fullscreen_active())
            {
                w->fullscreen();
                view->take_focus();

                if (!secondary)
                {
                    // Fullscreen does not update immediately, so we need
                    // to force a resize.
                    int X, Y, W, H;
                    Fl::screen_xywh(X, Y, W, H);
                    w->resize(0, 0, W, H);

                    // When fullscreen happens, the tool group bar also resizes
                    // on width, so we need to bring it back to its originazl
                    // size.
                    p.ui->uiRegion->layout();
                    p.ui->uiViewGroup->layout();
                    p.ui->uiViewGroup->redraw();
                }
            }
        }

        w->fill_menu(p.ui->uiMenuBar);
    }

    //! Set or unset the window to full screen and hide/show all bars
    void TimelineViewport::setPresentationMode(bool active) noexcept
    {
        TLRENDER_P();

        if (p.presentation == active)
            return;

        if (!active)
        {
            if (!p.fullScreen)
                _setFullScreen(false);
            if (p.ui->uiView == this)
                Fl::add_timeout(
                    0.01, (Fl_Timeout_Handler)restore_ui_state, p.ui);
            p.presentation = false;
            if (p.ui->uiPrefs->uiPrefsOpenGLVsync->value() ==
                MonitorVSync::kVSyncPresentationOnly)
            {
                swap_interval(0);
            }
            _updateCursor();
        }
        else
        {
            save_ui_state(p.ui);
            if (!_hasSecondaryViewport())
            {
                hide_ui_state(p.ui);
            }
            _setFullScreen(active);
            p.presentation = true;
            p.presentationTime = std::chrono::high_resolution_clock::now();
            if (p.ui->uiPrefs->uiPrefsOpenGLVsync->value() ==
                MonitorVSync::kVSyncPresentationOnly)
            {
                swap_interval(1);
            }
        }
    }

    bool TimelineViewport::getFullScreenMode() const noexcept
    {
        return _p->fullScreen;
    }

    //! Set or unset the window to full screen but don't hide any bars
    void TimelineViewport::setFullScreenMode(bool active) noexcept
    {
        TLRENDER_P();

        MainWindow* w = p.ui->uiMain;
        if (!active)
        {
            if (!p.presentation)
                _setFullScreen(false);
            if (p.fullScreen || p.presentation)
            {
#ifdef __APPLE__
                restore_ui_state(p.ui);
#else
                Fl::add_timeout(
                    0.01, (Fl_Timeout_Handler)restore_ui_state, p.ui);
#endif
            }
            p.fullScreen = false;
            p.presentation = false;
        }
        else
        {
            if (p.presentation)
                restore_ui_state(p.ui);
            save_ui_state(p.ui);
            _setFullScreen(true);
            if (!p.presentation)
                Fl::add_timeout(
                    0.0, (Fl_Timeout_Handler)restore_ui_state, p.ui);
            p.presentation = false;
            p.fullScreen = true;
        }
        w->fill_menu(p.ui->uiMenuBar);
    }

    void TimelineViewport::_updateDisplayOptions(
        const timeline::DisplayOptions& d) noexcept
    {
        TLRENDER_P();
        for (auto& display : p.displayOptions)
        {
            display = d;
        }

        const TimelinePlayer* player = getTimelinePlayer();
        if (!player)
            return;

        const auto& info = player->player()->getIOInfo();

        const auto& videos = info.video;
        if (videos.empty())
            return;

        int layer = p.ui->uiColorChannel->value();
        if (layer < 0)
            layer = 0;

        std::string name = videos[layer].name;
        if (name == "B,G,R" || name == "R,G,B" || name == "Default")
            name = "Color";
        else if (name == "A,B,G,R")
            name = "R,G,B,A";

        switch (d.channels)
        {
        case timeline::Channels::Red:
            name += " (R)";
            break;
        case timeline::Channels::Green:
            name += " (G)";
            break;
        case timeline::Channels::Blue:
            name += " (B)";
            break;
        case timeline::Channels::Alpha:
            name += " (A)";
            break;
        case timeline::Channels::Color:
        default:
            break;
        }

        p.ui->uiColorChannel->copy_label(name.c_str());
        p.ui->uiColorChannel->redraw();
        redraw();
    }

    void TimelineViewport::hsv_to_info(
        const image::Color4f& hsv, area::Info& info) const noexcept
    {
        info.hsv.mean.r += hsv.r;
        info.hsv.mean.g += hsv.g;
        info.hsv.mean.b += hsv.b;
        info.hsv.mean.a += hsv.a;

        if (hsv.r < info.hsv.min.r)
            info.hsv.min.r = hsv.r;
        if (hsv.g < info.hsv.min.g)
            info.hsv.min.g = hsv.g;
        if (hsv.b < info.hsv.min.b)
            info.hsv.min.b = hsv.b;
        if (hsv.a < info.hsv.min.a)
            info.hsv.min.a = hsv.a;

        if (hsv.r > info.hsv.max.r)
            info.hsv.max.r = hsv.r;
        if (hsv.g > info.hsv.max.g)
            info.hsv.max.g = hsv.g;
        if (hsv.b > info.hsv.max.b)
            info.hsv.max.b = hsv.b;
        if (hsv.a > info.hsv.max.a)
            info.hsv.max.a = hsv.a;
    }

    image::Color4f TimelineViewport::rgba_to_hsv(
        int hsv_colorspace, image::Color4f& rgba) const noexcept
    {
        if (rgba.r < 0.F)
            rgba.r = 0.F;
        else if (rgba.r > 1.F)
            rgba.r = 1.F;
        if (rgba.g < 0.F)
            rgba.g = 0.F;
        else if (rgba.g > 1.F)
            rgba.g = 1.F;
        if (rgba.b < 0.F)
            rgba.b = 0.F;
        else if (rgba.b > 1.F)
            rgba.b = 1.F;

        image::Color4f hsv;

        switch (hsv_colorspace)
        {
        case color::kHSV:
            hsv = color::rgb::to_hsv(rgba);
            break;
        case color::kHSL:
            hsv = color::rgb::to_hsl(rgba);
            break;
#ifdef TLRENDER_HSV
        case color::kCIE_XYZ:
            hsv = color::rgb::to_xyz(rgba);
            break;
        case color::kCIE_xyY:
            hsv = color::rgb::to_xyY(rgba);
            break;
        case color::kCIE_Lab:
            hsv = color::rgb::to_lab(rgba);
            break;
        case color::kCIE_Luv:
            hsv = color::rgb::to_luv(rgba);
            break;
#endif
        case color::kYUV:
            hsv = color::rgb::to_yuv(rgba);
            break;
        case color::kYDbDr:
            hsv = color::rgb::to_YDbDr(rgba);
            break;
        case color::kYIQ:
            hsv = color::rgb::to_yiq(rgba);
            break;
        case color::kITU_601:
            hsv = color::rgb::to_ITU601(rgba);
            break;
        case color::kITU_709:
            hsv = color::rgb::to_ITU709(rgba);
            break;
        case color::kRGB:
        default:
            hsv = rgba;
            break;
        }
        return hsv;
    }

    void TimelineViewport::_getPixelValue(
        image::Color4f& rgba, const std::shared_ptr<image::Image>& image,
        const math::Vector2i& pos) const noexcept
    {
        TLRENDER_P();
        image::PixelType type = image->getPixelType();
        uint8_t channels = image::getChannelCount(type);
        uint8_t depth = image::getBitDepth(type) / 8;
        const auto& info = image->getInfo();
        auto pixelAspectRatio = info.size.pixelAspectRatio;
        image::VideoLevels videoLevels = info.videoLevels;
        const math::Vector4f& yuvCoefficients =
            getYUVCoefficients(info.yuvCoefficients);
        auto size = image->getSize();
        const uint8_t* data = image->getData();
        int X = pos.x / pixelAspectRatio;
        int Y = size.h - pos.y - 1;
        if (p.displayOptions[0].mirror.x)
            X = size.w - X - 1;
        if (p.displayOptions[0].mirror.y)
            Y = size.h - Y - 1;

        // Do some sanity check just in case
        if (X < 0 || Y < 0 || X >= size.w || Y >= size.h)
            return;

        size_t offset = (Y * size.w + X);

        switch (type)
        {
        case image::PixelType::YUV_420P_U8:
        case image::PixelType::YUV_422P_U8:
        case image::PixelType::YUV_444P_U8:
            break;
        case image::PixelType::YUV_420P_U16:
        case image::PixelType::YUV_422P_U16:
        case image::PixelType::YUV_444P_U16:
            break;
        default:
            offset *= channels * depth;
            break;
        }

        rgba.a = 1.0f;
        switch (type)
        {
        case image::PixelType::L_U8:
            rgba.r = data[offset] / 255.0f;
            rgba.g = data[offset] / 255.0f;
            rgba.b = data[offset] / 255.0f;
            break;
        case image::PixelType::LA_U8:
            rgba.r = data[offset] / 255.0f;
            rgba.g = data[offset] / 255.0f;
            rgba.b = data[offset] / 255.0f;
            rgba.a = data[offset + 1] / 255.0f;
            break;
        case image::PixelType::L_U16:
        {
            uint16_t* f = (uint16_t*)(&data[offset]);
            rgba.r = f[0] / 65535.0f;
            rgba.g = f[0] / 65535.0f;
            rgba.b = f[0] / 65535.0f;
            break;
        }
        case image::PixelType::LA_U16:
        {
            uint16_t* f = (uint16_t*)(&data[offset]);
            rgba.r = f[0] / 65535.0f;
            rgba.g = f[0] / 65535.0f;
            rgba.b = f[0] / 65535.0f;
            rgba.a = f[1] / 65535.0f;
            break;
        }
        case image::PixelType::L_U32:
        {
            uint32_t* f = (uint32_t*)(&data[offset]);
            constexpr float max =
                static_cast<float>(std::numeric_limits<uint32_t>::max());
            rgba.r = f[0] / max;
            rgba.g = f[0] / max;
            rgba.b = f[0] / max;
            break;
        }
        case image::PixelType::LA_U32:
        {
            uint32_t* f = (uint32_t*)(&data[offset]);
            constexpr float max =
                static_cast<float>(std::numeric_limits<uint32_t>::max());
            rgba.r = f[0] / max;
            rgba.g = f[0] / max;
            rgba.b = f[0] / max;
            rgba.a = f[1] / max;
            break;
        }
        case image::PixelType::L_F16:
        {
            half* f = (half*)(&data[offset]);
            rgba.r = f[0];
            rgba.g = f[0];
            rgba.b = f[0];
            break;
        }
        case image::PixelType::LA_F16:
        {
            half* f = (half*)(&data[offset]);
            rgba.r = f[0];
            rgba.g = f[0];
            rgba.b = f[0];
            rgba.a = f[1];
            break;
        }
        case image::PixelType::RGB_U8:
            rgba.r = data[offset] / 255.0f;
            rgba.g = data[offset + 1] / 255.0f;
            rgba.b = data[offset + 2] / 255.0f;
            break;
        case image::PixelType::RGB_U10:
        {
            image::U10* f = (image::U10*)(&data[offset]);
            constexpr float max =
                static_cast<float>(std::numeric_limits<uint32_t>::max());
            rgba.r = f->r / max;
            rgba.g = f->g / max;
            rgba.b = f->b / max;
            break;
        }
        case image::PixelType::RGBA_U8:
            rgba.r = data[offset] / 255.0f;
            rgba.g = data[offset + 1] / 255.0f;
            rgba.b = data[offset + 2] / 255.0f;
            rgba.a = data[offset + 3] / 255.0f;
            break;
        case image::PixelType::RGB_U16:
        {
            uint16_t* f = (uint16_t*)(&data[offset]);
            rgba.r = f[0] / 65535.0f;
            rgba.g = f[1] / 65535.0f;
            rgba.b = f[2] / 65535.0f;
            break;
        }
        case image::PixelType::RGBA_U16:
        {
            uint16_t* f = (uint16_t*)(&data[offset]);
            rgba.r = f[0] / 65535.0f;
            rgba.g = f[1] / 65535.0f;
            rgba.b = f[2] / 65535.0f;
            rgba.a = f[3] / 65535.0f;
            break;
        }
        case image::PixelType::RGB_U32:
        {
            uint32_t* f = (uint32_t*)(&data[offset]);
            constexpr float max =
                static_cast<float>(std::numeric_limits<uint32_t>::max());
            rgba.r = f[0] / max;
            rgba.g = f[1] / max;
            rgba.b = f[2] / max;
            break;
        }
        case image::PixelType::RGBA_U32:
        {
            uint32_t* f = (uint32_t*)(&data[offset]);
            constexpr float max =
                static_cast<float>(std::numeric_limits<uint32_t>::max());
            rgba.r = f[0] / max;
            rgba.g = f[1] / max;
            rgba.b = f[2] / max;
            rgba.a = f[3] / max;
            break;
        }
        case image::PixelType::RGB_F16:
        {
            half* f = (half*)(&data[offset]);
            rgba.r = f[0];
            rgba.g = f[1];
            rgba.b = f[2];
            break;
        }
        case image::PixelType::RGBA_F16:
        {
            half* f = (half*)(&data[offset]);
            rgba.r = f[0];
            rgba.g = f[1];
            rgba.b = f[2];
            rgba.a = f[3];
            break;
        }
        case image::PixelType::RGB_F32:
        {
            float* f = (float*)(&data[offset]);
            rgba.r = f[0];
            rgba.g = f[1];
            rgba.b = f[2];
            break;
        }
        case image::PixelType::RGBA_F32:
        {
            float* f = (float*)(&data[offset]);
            rgba.r = f[0];
            rgba.g = f[1];
            rgba.b = f[2];
            rgba.a = f[3];
            break;
        }
        case image::PixelType::YUV_420P_U8:
        {
            size_t Ysize = size.w * size.h;
            size_t w2 = (size.w + 1) / 2;
            size_t h2 = (size.h + 1) / 2;
            size_t Usize = w2 * h2;
            size_t offset2 = (Y / 2) * w2 + X / 2;
            rgba.r = data[offset] / 255.0f;
            rgba.g = data[Ysize + offset2] / 255.0f;
            rgba.b = data[Ysize + Usize + offset2] / 255.0f;
            color::checkLevels(rgba, videoLevels);
            rgba = color::YPbPr::to_rgb(rgba, yuvCoefficients);
            break;
        }
        case image::PixelType::YUV_422P_U8:
        {
            size_t Ysize = size.w * size.h;
            size_t w2 = (size.w + 1) / 2;
            size_t Usize = w2 * size.h;
            size_t offset2 = Y * w2 + X / 2;
            rgba.r = data[offset] / 255.0f;
            rgba.g = data[Ysize + offset2] / 255.0f;
            rgba.b = data[Ysize + Usize + offset2] / 255.0f;
            color::checkLevels(rgba, videoLevels);
            rgba = color::YPbPr::to_rgb(rgba, yuvCoefficients);
            break;
        }
        case image::PixelType::YUV_444P_U8:
        {
            size_t Ysize = size.w * size.h;
            rgba.r = data[offset] / 255.0f;
            rgba.g = data[Ysize + offset] / 255.0f;
            rgba.b = data[Ysize * 2 + offset] / 255.0f;
            color::checkLevels(rgba, videoLevels);
            rgba = color::YPbPr::to_rgb(rgba, yuvCoefficients);
            break;
        }
        case image::PixelType::YUV_420P_U16: // Works
        {
            uint16_t* f = (uint16_t*)data;

            size_t Ysize = size.w * size.h;
            size_t w2 = (size.w + 1) / 2;
            size_t h2 = (size.h + 1) / 2;
            size_t Usize = w2 * h2;
            size_t offset2 = (Y / 2) * w2 + X / 2;

            rgba.r = f[offset] / 65535.0f;
            rgba.g = f[Ysize + offset2] / 65535.0f;
            rgba.b = f[Ysize + Usize + offset2] / 65535.0f;
            color::checkLevels(rgba, videoLevels);
            rgba = color::YPbPr::to_rgb(rgba, yuvCoefficients);
            break;
        }
        case image::PixelType::YUV_422P_U16:
        {
            uint16_t* f = (uint16_t*)data;

            size_t Ysize = size.w * size.h;
            size_t w2 = (size.w + 1) / 2;
            size_t Usize = w2 * size.h;
            size_t offset2 = Y * w2 + X / 2;

            rgba.r = f[offset] / 65535.0f;
            rgba.g = f[Ysize + offset2] / 65535.0f;
            rgba.b = f[Ysize + Usize + offset2] / 65535.0f;
            color::checkLevels(rgba, videoLevels);
            rgba = color::YPbPr::to_rgb(rgba, yuvCoefficients);
            break;
        }
        case image::PixelType::YUV_444P_U16: // Works
        {
            uint16_t* f = (uint16_t*)data;
            size_t Ysize = size.w * size.h;
            rgba.r = f[offset] / 65535.0f;
            rgba.g = f[Ysize + offset] / 65535.0f;
            rgba.b = f[Ysize * 2 + offset] / 65535.0f;
            color::checkLevels(rgba, videoLevels);
            rgba = color::YPbPr::to_rgb(rgba, yuvCoefficients);
            break;
        }
        default:
            break;
        }
    }

    void TimelineViewport::_calculateColorAreaRawValues(
        area::Info& info) const noexcept
    {
        TLRENDER_P();

        PixelToolBarClass* c = p.ui->uiPixelWindow;
        BrightnessType brightness_type = (BrightnessType)c->uiLType->value();
        int hsv_colorspace = c->uiBColorType->value() + 1;

        int maxX = info.box.max.x;
        int maxY = info.box.max.y;

        for (int Y = info.box.y(); Y <= maxY; ++Y)
        {
            for (int X = info.box.x(); X <= maxX; ++X)
            {
                image::Color4f rgba, hsv;
                rgba.r = rgba.g = rgba.b = rgba.a = 0.f;

                math::Vector2i pos(X, Y);
                for (const auto& video : p.videoData)
                {
                    for (const auto& layer : video.layers)
                    {
                        const auto& image = layer.image;
                        if (!image->isValid())
                            continue;

                        image::Color4f pixel, pixelB;

                        _getPixelValue(pixel, image, pos);

                        const auto& imageB = layer.image;
                        if (imageB->isValid())
                        {
                            _getPixelValue(pixelB, imageB, pos);

                            if (layer.transition ==
                                timeline::Transition::Dissolve)
                            {
                                float f2 = layer.transitionValue;
                                float f = 1.0 - f2;
                                pixel.r = pixel.r * f + pixelB.r * f2;
                                pixel.g = pixel.g * f + pixelB.g * f2;
                                pixel.b = pixel.b * f + pixelB.b * f2;
                                pixel.a = pixel.a * f + pixelB.a * f2;
                            }
                        }
                        rgba.r += pixel.r;
                        rgba.g += pixel.g;
                        rgba.b += pixel.b;
                        rgba.a += pixel.a;
                    }

                    info.rgba.mean.r += rgba.r;
                    info.rgba.mean.g += rgba.g;
                    info.rgba.mean.b += rgba.b;
                    info.rgba.mean.a += rgba.a;

                    if (rgba.r < info.rgba.min.r)
                        info.rgba.min.r = rgba.r;
                    if (rgba.g < info.rgba.min.g)
                        info.rgba.min.g = rgba.g;
                    if (rgba.b < info.rgba.min.b)
                        info.rgba.min.b = rgba.b;
                    if (rgba.a < info.rgba.min.a)
                        info.rgba.min.a = rgba.a;

                    if (rgba.r > info.rgba.max.r)
                        info.rgba.max.r = rgba.r;
                    if (rgba.g > info.rgba.max.g)
                        info.rgba.max.g = rgba.g;
                    if (rgba.b > info.rgba.max.b)
                        info.rgba.max.b = rgba.b;
                    if (rgba.a > info.rgba.max.a)
                        info.rgba.max.a = rgba.a;

                    hsv = rgba_to_hsv(hsv_colorspace, rgba);
                    hsv.a = calculate_brightness(rgba, brightness_type);
                    hsv_to_info(hsv, info);
                }
            }
        }

        int num = info.box.w() * info.box.h();
        info.rgba.mean.r /= num;
        info.rgba.mean.g /= num;
        info.rgba.mean.b /= num;
        info.rgba.mean.a /= num;

        info.rgba.diff.r = info.rgba.max.r - info.rgba.min.r;
        info.rgba.diff.g = info.rgba.max.g - info.rgba.min.g;
        info.rgba.diff.b = info.rgba.max.b - info.rgba.min.b;
        info.rgba.diff.a = info.rgba.max.a - info.rgba.min.a;

        info.hsv.mean.r /= num;
        info.hsv.mean.g /= num;
        info.hsv.mean.b /= num;
        info.hsv.mean.a /= num;

        info.hsv.diff.r = info.hsv.max.r - info.hsv.min.r;
        info.hsv.diff.g = info.hsv.max.g - info.hsv.min.g;
        info.hsv.diff.b = info.hsv.max.b - info.hsv.min.b;
        info.hsv.diff.a = info.hsv.max.a - info.hsv.min.a;
    }

    void TimelineViewport::_mallocBuffer() const noexcept
    {
        TLRENDER_P();

        p.rawImage = true;
        const math::Size2i& renderSize = getRenderSize();
        unsigned dataSize = renderSize.w * renderSize.h * 4 * sizeof(float);

        if (dataSize != p.rawImageSize || !p.image)
        {
            free(p.image);
            p.image = (float*)malloc(dataSize);
            p.rawImageSize = dataSize;
        }
    }

    void TimelineViewport::_mapBuffer() const noexcept
    {
        TLRENDER_P();

        _mallocBuffer();
        if (!p.image)
            return;

        const math::Size2i& renderSize = getRenderSize();
        unsigned maxY = renderSize.h;
        unsigned maxX = renderSize.w;
        for (int Y = 0; Y < maxY; ++Y)
        {
            for (int X = 0; X < maxX; ++X)
            {
                image::Color4f& rgba =
                    (image::Color4f&)p.image[(X + maxX * Y) * 4];
                rgba.r = rgba.g = rgba.b = rgba.a = 0.f;

                math::Vector2i pos(X, Y);
                for (const auto& video : p.videoData)
                {
                    for (const auto& layer : video.layers)
                    {
                        const auto& image = layer.image;
                        if (!image->isValid())
                            continue;

                        image::Color4f pixel, pixelB;

                        _getPixelValue(pixel, image, pos);

                        const auto& imageB = layer.image;
                        if (imageB->isValid())
                        {
                            _getPixelValue(pixelB, imageB, pos);

                            if (layer.transition ==
                                timeline::Transition::Dissolve)
                            {
                                float f2 = layer.transitionValue;
                                float f = 1.0 - f2;
                                pixel.r = pixel.r * f + pixelB.r * f2;
                                pixel.g = pixel.g * f + pixelB.g * f2;
                                pixel.b = pixel.b * f + pixelB.b * f2;
                                pixel.a = pixel.a * f + pixelB.a * f2;
                            }
                        }
                        rgba.r += pixel.r;
                        rgba.g += pixel.g;
                        rgba.b += pixel.b;
                        rgba.a += pixel.a;
                    }
                    float tmp = rgba.r;
                    rgba.r = rgba.b;
                    rgba.b = tmp;
                }
            }
        }
    }

    void TimelineViewport::_unmapBuffer() const noexcept
    {
        TLRENDER_P();
        if (p.rawImage)
        {
            free(p.image);
            p.image = nullptr;
            p.rawImage = true;
        }
    }

    const image::Color4f* TimelineViewport::image() const
    {
        return (image::Color4f*)(_p->image);
    }

    void TimelineViewport::_addAnnotationShapePoint() const
    {
        // We should not update tcp client when not needed
        if (dynamic_cast< DummyClient* >(tcp))
            return;

        const auto player = getTimelinePlayer();
        if (!player)
            return;

        auto annotation = player->getAnnotation();
        if (!annotation)
            return;
        auto shape = annotation->lastShape().get();
        if (!shape)
            return;

        auto path = dynamic_cast< draw::PathShape* >(shape);
        if (path == nullptr)
            return;

        const draw::Point& pnt = path->pts.back();

        nlohmann::json json = pnt;

        Message msg;
        msg["command"] = "Add Shape Point";
        msg["value"] = json;

        tcp->pushMessage(msg);
    }

    void TimelineViewport::_updateAnnotationShape() const
    {
        _pushAnnotationShape("Update Shape");
    }

    // This routine is used for text shapes only.
    void TimelineViewport::_endAnnotationShape() const
    {
        TLRENDER_P();
        updateUndoRedoButtons();
        _pushAnnotationShape("End Shape");
    }

    void TimelineViewport::_createAnnotationShape(const bool laser) const
    {
        TLRENDER_P();
        if (!laser)
            updateUndoRedoButtons();
        _pushAnnotationShape("Create Shape");
    }

    //! Set selection area.
    void TimelineViewport::setSelectionArea(const math::Box2i& area) noexcept
    {
        TLRENDER_P();
        if (p.selection == area)
            return;

        p.selection = area;
        redrawWindows();

        bool send = p.ui->uiPrefs->SendColor->value();
        if (send)
        {
            Message msg;
            Message selection = p.selection;
            msg["command"] = "Selection Area";
            msg["value"] = selection;
            tcp->pushMessage(msg);
        }
    }

    bool TimelineViewport::getShowAnnotations() const noexcept
    {
        TLRENDER_P();

        return p.showAnnotations;
    }

    void TimelineViewport::setShowAnnotations(const bool value) noexcept
    {
        TLRENDER_P();

        if (value == p.showAnnotations)
            return;
        p.showAnnotations = value;
        redrawWindows();

        bool send = p.ui->uiPrefs->SendAnnotations->value();
        if (send)
        {
            Message msg;
            msg["command"] = "Show Annotations";
            msg["value"] = value;
            tcp->pushMessage(msg);
        }
    }

    void TimelineViewport::updateUndoRedoButtons() const noexcept
    {
        TLRENDER_P();

        bool hasUndo = false, hasRedo = false;
        auto player = getTimelinePlayer();
        if (player)
        {
            hasUndo |= player->hasUndo();
            hasRedo |= player->hasRedo();
        }

        if (hasUndo)
            p.ui->uiUndoDraw->activate();
        else
            p.ui->uiUndoDraw->deactivate();

        if (hasRedo)
            p.ui->uiRedoDraw->activate();
        else
            p.ui->uiRedoDraw->deactivate();
    }

    float TimelineViewport::_getPenSize() const noexcept
    {
        const float kPenSizeMultiplier = 800.F;
        const auto settings = _p->ui->app->settings();
        std_any value;
        float pen_size = static_cast<float>(settings->getValue<int>(kPenSize));
        auto renderSize = getRenderSize();
        pen_size *= renderSize.h / kPenSizeMultiplier;
        if (pen_size < 1.0F)
            pen_size = 1.0F;
        return pen_size;
    }

    float TimelineViewport::_getZoomSpeedValue() const noexcept
    {
        int idx = _p->ui->uiPrefs->uiPrefsZoomSpeed->value();
        const float speedValues[] = {0.1F, 0.25F, 0.5F};
        return speedValues[idx];
    }

    void TimelineViewport::_getTags() noexcept
    {
        TLRENDER_P();

        p.tagData.clear();

        if (!p.player)
            return;

        char buf[1024];

        const auto& player = p.player->player();
        const auto& info = player->getIOInfo();
        for (const auto& tag : info.tags)
        {
            const std::string& key = tag.first;
            const std::string rendererKey = "Renderer ";
            if (key.compare(0, rendererKey.size(), rendererKey) == 0)
                continue;
            p.tagData[key] = tag.second;
        }

        if (!p.videoData.empty() && !p.videoData[0].layers.empty() &&
            p.videoData[0].layers[0].image)
        {
            const auto& tags = p.videoData[0].layers[0].image->getTags();
            for (const auto& tag : tags)
            {
                p.tagData[tag.first] = tag.second;
            }
        }

        // If we have a Video Rotation Metadata, extract its value from it.
        auto i = p.tagData.find("Video Rotation");
        float videoRotation = 0.F;
        if (i != p.tagData.end())
        {
            std::stringstream s(i->second);
            s >> videoRotation;
        }
        _setVideoRotation(videoRotation);
    }

    void TimelineViewport::_setVideoRotation(float value) noexcept
    {
        TLRENDER_P();

        if (p.videoRotation == value)
            return;

        p.videoRotation = value;

        if (hasFrameView())
            _frameView();
    }

    math::Matrix4x4f TimelineViewport::_projectionMatrix() const noexcept
    {
        TLRENDER_P();

        const auto& renderSize = getRenderSize();
        const auto renderAspect = renderSize.getAspect();
        const auto& viewportSize = getViewportSize();
        const auto viewportAspect = viewportSize.getAspect();

        image::Size transformSize;
        math::Vector2f transformOffset;
        if (viewportAspect > 1.F)
        {
            transformOffset.x = renderSize.w / 2.F;
            transformOffset.y = renderSize.w / renderAspect / 2.F;
        }
        else
        {
            transformOffset.x = renderSize.h * renderAspect / 2.F;
            transformOffset.y = renderSize.h / 2.F;
        }

        const math::Matrix4x4f& vm =
            math::translate(math::Vector3f(p.viewPos.x, p.viewPos.y, 0.F)) *
            math::scale(math::Vector3f(p.viewZoom, p.viewZoom, 1.F));
        const auto& rm = math::rotateZ(p.rotation + p.videoRotation);
        const math::Matrix4x4f& tm = math::translate(
            math::Vector3f(-renderSize.w / 2, -renderSize.h / 2, 0.F));
        const math::Matrix4x4f& to = math::translate(
            math::Vector3f(transformOffset.x, transformOffset.y, 0.F));

        const auto pm = math::ortho(
            0.F, static_cast<float>(viewportSize.w), 0.F,
            static_cast<float>(viewportSize.h), -1.F, 1.F);

        return pm * vm * to * rm * tm;
    }

    math::Matrix4x4f TimelineViewport::_pixelMatrix() const noexcept
    {
        TLRENDER_P();

        const auto& renderSize = getRenderSize();
        const auto renderAspect = renderSize.getAspect();
        const auto& viewportSize = getViewportSize();
        const auto viewportAspect = viewportSize.getAspect();

        image::Size transformSize;
        math::Vector2f transformOffset;
        if (viewportAspect > 1.F)
        {
            transformOffset.x = renderSize.w / 2.F;
            transformOffset.y = renderSize.w / renderAspect / 2.F;
        }
        else
        {
            transformOffset.x = renderSize.h * renderAspect / 2.F;
            transformOffset.y = renderSize.h / 2.F;
        }

        // Create transformation matrices
        math::Matrix4x4f translation =
            math::translate(math::Vector3f(-p.viewPos.x, -p.viewPos.y, 0.F));
        math::Matrix4x4f zoom = math::scale(
            math::Vector3f(1.F / p.viewZoom, 1.F / p.viewZoom, 1.F));
        const auto& rotation = math::rotateZ(-p.rotation - p.videoRotation);

        const math::Matrix4x4f tm = math::translate(
            math::Vector3f(renderSize.w / 2, renderSize.h / 2, 0.F));
        const math::Matrix4x4f to = math::translate(
            math::Vector3f(-transformOffset.x, -transformOffset.y, 0.F));
        // Combined transformation matrix
        const math::Matrix4x4f& vm = tm * rotation * to * zoom * translation;
        return vm;
    }

} // namespace mrv
