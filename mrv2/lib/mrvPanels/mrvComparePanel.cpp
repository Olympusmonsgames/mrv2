// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#include <string>
#include <vector>
#include <map>

#include <FL/Fl_Button.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_RGB_Image.H>

#include "mrvCore/mrvHome.h"

#include "mrvWidgets/mrvHorSlider.h"
#include "mrvWidgets/mrvFunctional.h"
#include "mrvWidgets/mrvClipButton.h"
#include "mrvWidgets/mrvButton.h"
#include "mrvWidgets/mrvCollapsibleGroup.h"

#include "mrvEdit/mrvEditUtil.h"

#include "mrvPanels/mrvPanelsAux.h"
#include "mrvPanels/mrvComparePanel.h"
#include "mrvPanels/mrvPanelsCallbacks.h"

#include "mrvGL/mrvThumbnailCreator.h"

#include "mrvApp/mrvSettingsObject.h"
#include "mrvApp/mrvFilesModel.h"
#include "mrvApp/App.h"

#include "mrViewer.h"

namespace mrv
{

    typedef std::map< ClipButton*, int64_t > WidgetIds;
    typedef std::map< ClipButton*, size_t > WidgetIndices;

    struct ComparePanel::Private
    {
        std::weak_ptr<system::Context> context;
        mrv::ThumbnailCreator* thumbnailCreator;
        std::map< size_t, ClipButton* > map;
        WidgetIds ids;
        WidgetIndices indices;
        std::vector< Fl_Button* > buttons;

        std::shared_ptr<
            observer::ListObserver<std::shared_ptr<FilesModelItem> > >
            filesObserver;

        std::shared_ptr<observer::ListObserver<int> > bIndexesObserver;
        std::shared_ptr<observer::ListObserver<int> > layerObserver;

        std::shared_ptr<observer::ValueObserver<timeline::CompareOptions> >
            compareOptionsObserver;
    };

    struct ThumbnailData
    {
        ClipButton* widget;
    };

    void compareThumbnail_cb(
        const int64_t id,
        const std::vector< std::pair<otime::RationalTime, Fl_RGB_Image*> >&
            thumbnails,
        void* opaque)
    {
        ThumbnailData* data = static_cast< ThumbnailData* >(opaque);
        ClipButton* w = data->widget;
        if (comparePanel)
            comparePanel->compareThumbnail(id, thumbnails, w);
        delete data;
    }

    void ComparePanel::compareThumbnail(
        const int64_t id,
        const std::vector< std::pair<otime::RationalTime, Fl_RGB_Image*> >&
            thumbnails,
        ClipButton* w)
    {
        WidgetIds::const_iterator it = _r->ids.find(w);
        if (it == _r->ids.end())
            return;

        if (it->second == id)
        {
            for (const auto& i : thumbnails)
            {
                Fl_Image* img = w->image();
                w->image(i.second);
                delete img;
                w->redraw();
            }
        }
        else
        {
            for (const auto& i : thumbnails)
            {
                delete i.second;
            }
        }
    }

    ComparePanel::ComparePanel(ViewerUI* ui) :
        _r(new Private),
        PanelWidget(ui)
    {
        _r->context = ui->app->getContext();

        add_group("Compare");

        Fl_SVG_Image* svg = load_svg("Compare.svg");
        g->image(svg);

        g->callback(
            [](Fl_Widget* w, void* d)
            {
                ViewerUI* ui = static_cast< ViewerUI* >(d);
                delete comparePanel;
                comparePanel = nullptr;
                ui->uiMain->fill_menu(ui->uiMenuBar);
            },
            ui);

        _r->filesObserver =
            observer::ListObserver<std::shared_ptr<FilesModelItem> >::create(
                ui->app->filesModel()->observeFiles(),
                [this](
                    const std::vector< std::shared_ptr<FilesModelItem> >& value)
                { refresh(); });

        _r->bIndexesObserver = observer::ListObserver<int>::create(
            ui->app->filesModel()->observeBIndexes(),
            [this](const std::vector<int>& value) { redraw(); });

        _r->layerObserver = observer::ListObserver<int>::create(
            ui->app->filesModel()->observeLayers(),
            [this](const std::vector<int>& value) { redraw(); });

        _r->compareOptionsObserver =
            observer::ValueObserver<timeline::CompareOptions>::create(
                ui->app->filesModel()->observeCompareOptions(),
                [this](const timeline::CompareOptions& value)
                { setCompareOptions(value); });
    }

    ComparePanel::~ComparePanel()
    {
        cancel_thumbnails();
        clear_controls();
    }

    void ComparePanel::clear_controls()
    {
        for (const auto& i : _r->map)
        {
            ClipButton* b = i.second;
            delete b->image();
            b->image(nullptr);
            g->remove(b);
            delete b;
        }

        // Clear buttons' SVG images
        for (const auto& b : _r->buttons)
        {
            delete b->image();
            b->image(nullptr);
        }

        _r->buttons.clear();
        _r->map.clear();
        _r->indices.clear();
    }

    void ComparePanel::cancel_thumbnails()
    {
        for (const auto& it : _r->ids)
        {
            _r->thumbnailCreator->cancelRequests(it.second);
        }

        _r->ids.clear();
    }

    void ComparePanel::add_controls()
    {
        TLRENDER_P();

        _r->thumbnailCreator = p.ui->uiTimeline->thumbnailCreator();

        auto settingsObject = p.ui->app->settingsObject();
        const std::string& prefix = tab_prefix();

        g->clear();
        g->begin();
        const auto& model = p.ui->app->filesModel();
        const auto& files = model->observeFiles();
        size_t numFiles = files->getSize();
        auto Bindices = model->observeBIndexes()->get();
        auto Aindex = model->observeAIndex()->get();

        const auto player = p.ui->uiView->getTimelinePlayer();

        otio::RationalTime time = otio::RationalTime(0.0, 1.0);
        if (player)
            time = player->currentTime();

        const image::Size size(128, 64);

        file::Path lastPath;

        for (size_t i = 0; i < numFiles; ++i)
        {
            const auto& media = files->getItem(i);
            const auto& path = media->path;

            const bool isEDL = isTemporaryEDL(path);

            // When we refresh the .otio for EDL, we get two clips with the
            // same name, we avoid displaying both with this check.
            if (path == lastPath && isEDL)
                continue;
            lastPath = path;

            const std::string dir = path.getDirectory();
            const std::string base = path.getBaseName();
            const std::string extension = path.getExtension();
            const std::string file = base + path.getNumber() + extension;
            const std::string fullfile = dir + file;

            auto bW = new Widget<ClipButton>(
                g->x(), g->y() + 20 + i * 68, g->w(), 68);
            ClipButton* b = bW;
            b->tooltip(_("Select one or more B images."));
            _r->indices[b] = i;

            uint16_t layerId = media->videoLayer;
            time = media->currentTime;
            if (Aindex == i)
            {
                layerId = p.ui->uiColorChannel->value();
                if (player)
                    time = player->currentTime();
            }

            for (auto Bindex : Bindices)
            {
                if (Bindex == i)
                {
                    b->value(1);
                    if (player)
                        time = player->currentTime();
                    break;
                }
            }
            bW->callback(
                [=](auto b)
                {
                    WidgetIndices::const_iterator it = _r->indices.find(b);
                    if (it == _r->indices.end())
                        return;
                    int index = (*it).second;
                    const auto& model = p.ui->app->filesModel();
                    const auto bIndexes = model->observeBIndexes()->get();
                    const auto i =
                        std::find(bIndexes.begin(), bIndexes.end(), index);
                    model->setB(index, i == bIndexes.end());
                });

            _r->map[i] = b;

            const std::string& layer = getLayerName(layerId, p.ui);
            std::string text = dir + "\n" + file + layer;
            b->copy_label(text.c_str());

            if (auto context = _r->context.lock())
            {
                ThumbnailData* data = new ThumbnailData;
                data->widget = b;

                WidgetIds::const_iterator it = _r->ids.find(b);
                if (it != _r->ids.end())
                {
                    _r->thumbnailCreator->cancelRequests(it->second);
                    _r->ids.erase(it);
                }

                try
                {
                    auto timeline = timeline::Timeline::create(path, context);
                    auto timeRange = timeline->getTimeRange();

                    if (time::isValid(timeRange))
                    {
                        auto startTime = timeRange.start_time();
                        auto endTime = timeRange.end_time_inclusive();

                        if (time < startTime)
                            time = startTime;
                        else if (time > endTime)
                            time = endTime;
                    }

                    _r->thumbnailCreator->initThread();
                    int64_t id = _r->thumbnailCreator->request(
                        fullfile, time, size, compareThumbnail_cb, (void*)data,
                        layerId);
                    _r->ids[b] = id;
                }
                catch (const std::exception&)
                {
                }
            }
        }

        Fl_Pack* bg =
            new Fl_Pack(g->x(), g->y() + 20 + numFiles * 64, g->w(), 30);
        bg->type(Fl_Pack::HORIZONTAL);
        bg->begin();

        Fl_Button* b;
        auto bW = new Widget< Button >(g->x(), 90, 30, 30);
        b = bW;
        Fl_SVG_Image* svg = load_svg("CompareA.svg");
        b->image(svg);
        _r->buttons.push_back(b);
        b->tooltip(_("Compare A"));
        bW->callback(
            [=](auto w)
            {
                auto o = model->observeCompareOptions()->get();
                o.mode = timeline::CompareMode::A;
                model->setCompareOptions(o);
            });

        bW = new Widget< Button >(g->x(), 90, 30, 30);
        b = bW;
        svg = load_svg("CompareB.svg");
        b->image(svg);
        _r->buttons.push_back(b);
        b->tooltip(_("Compare B"));

        bW->callback(
            [=](auto w)
            {
                auto o = model->observeCompareOptions()->get();
                o.mode = timeline::CompareMode::B;
                model->setCompareOptions(o);
            });

        bW = new Widget< Button >(g->x(), 90, 30, 30);
        b = bW;
        svg = load_svg("CompareWipe.svg");
        b->image(svg);
        _r->buttons.push_back(b);
        b->tooltip(
#ifdef __APPLE__
            _("Wipe between the A and B files\n\n"
              "Use the Option key + left mouse button to move the wipe in "
              "X or "
              "in Y")
#else
            _("Wipe between the A and B files\n\n"
              "Use the Alt key + left mouse button to move the wipe in X "
              "or in "
              "Y.")
#endif
        );

        bW->callback(
            [=](auto w)
            {
                auto o = model->observeCompareOptions()->get();
                o.mode = timeline::CompareMode::Wipe;
                model->setCompareOptions(o);
            });

        bW = new Widget< Button >(g->x(), 90, 30, 30);
        b = bW;
        svg = load_svg("CompareOverlay.svg");
        b->image(svg);
        _r->buttons.push_back(b);
        b->tooltip(_("Overlay the A and B files with optional transparencyy"));

        bW->callback(
            [=](auto w)
            {
                auto o = model->observeCompareOptions()->get();
                o.mode = timeline::CompareMode::Overlay;
                model->setCompareOptions(o);
            });

        bW = new Widget< Button >(g->x(), 90, 30, 30);
        b = bW;
        svg = load_svg("CompareDifference.svg");
        b->image(svg);
        _r->buttons.push_back(b);
        b->tooltip(_("Difference the A and B files"));

        bW->callback(
            [=](auto w)
            {
                auto o = model->observeCompareOptions()->get();
                o.mode = timeline::CompareMode::Difference;
                model->setCompareOptions(o);
            });

        bW = new Widget< Button >(g->x(), 90, 30, 30);
        b = bW;
        svg = load_svg("CompareHorizontal.svg");
        b->image(svg);
        _r->buttons.push_back(b);
        b->tooltip(_("Compare the A and B files side by side"));

        bW->callback(
            [=](auto w)
            {
                auto o = model->observeCompareOptions()->get();
                o.mode = timeline::CompareMode::Horizontal;
                model->setCompareOptions(o);
            });

        bW = new Widget< Button >(g->x(), 90, 30, 30);
        b = bW;
        svg = load_svg("CompareVertical.svg");
        b->image(svg);
        _r->buttons.push_back(b);
        b->tooltip(_("Show the A file above the B file"));

        bW->callback(
            [=](auto w)
            {
                auto o = model->observeCompareOptions()->get();
                o.mode = timeline::CompareMode::Vertical;
                model->setCompareOptions(o);
            });

        bW = new Widget< Button >(g->x(), 90, 30, 30);
        b = bW;
        svg = load_svg("CompareTile.svg");
        b->image(svg);
        _r->buttons.push_back(b);
        b->tooltip(_("Tile the A and B files"));

        bW->callback(
            [=](auto w)
            {
                auto o = model->observeCompareOptions()->get();
                o.mode = timeline::CompareMode::Tile;
                model->setCompareOptions(o);
            });

        bW = new Widget< Button >(g->x() + 120, 90, 30, 30);
        b = bW;
        svg = load_svg("Prev.svg");
        b->image(svg);
        _r->buttons.push_back(b);
        b->tooltip(_("Previous filename"));
        bW->callback(
            [=](auto w)
            {
                if (p.ui->app->filesModel()->observeFiles()->getSize() > 0)
                    p.ui->app->filesModel()->prevB();
            });

        bW = new Widget< Button >(g->x() + 150, 90, 30, 30);
        b = bW;
        svg = load_svg("Next.svg");
        b->image(svg);
        _r->buttons.push_back(b);
        b->tooltip(_("Next filename"));
        bW->callback(
            [=](auto w)
            {
                if (p.ui->app->filesModel()->observeFiles()->getSize() > 0)
                    p.ui->app->filesModel()->nextB();
            });

        bg->end();

        HorSlider* s;
        CollapsibleGroup* cg =
            new CollapsibleGroup(g->x(), 20, g->w(), 20, _("Wipe"));
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

                const std::string& prefix = comparePanel->tab_prefix();
                const std::string key = prefix + "Wipe";

                App* app = App::ui->app;
                auto settingsObject = app->settingsObject();
                settingsObject->setValue(key, static_cast<int>(cg->is_open()));

                comparePanel->refresh();
            },
            cg);

        cg->begin();

        auto sV = new Widget< HorSlider >(g->x(), 90, g->w(), 20, "X");
        s = wipeX = sV;
        s->tooltip(
#ifdef __APPLE__
            _("Use the Option key + left mouse button to move the wipe in "
              "X.")
#else
            _("Use the Alt key + left mouse button to move the wipe in X.")
#endif
        );
        s->range(0.f, 1.0f);
        s->step(0.01F);
        s->default_value(0.5f);
        auto o = model->observeCompareOptions()->get();
        s->value(o.wipeCenter.x);
        sV->callback(
            [=](auto w)
            {
                auto o = model->observeCompareOptions()->get();
                o.wipeCenter.x = w->value();
                model->setCompareOptions(o);
            });

        sV = new Widget< HorSlider >(g->x(), 110, g->w(), 20, "Y");
        s = wipeY = sV;
        s->tooltip(
#ifdef __APPLE__
            _("Use the Option key + left mouse button to move the wipe in "
              "Y.")
#else
            _("Use the Alt key + left mouse button to move the wipe in Y.")
#endif
        );
        s->range(0.f, 1.0f);
        s->step(0.01F);
        s->default_value(0.5f);
        s->value(o.wipeCenter.y);
        sV->callback(
            [=](auto w)
            {
                auto o = model->observeCompareOptions()->get();
                o.wipeCenter.y = w->value();
                model->setCompareOptions(o);
            });

        sV = new Widget< HorSlider >(g->x(), 130, g->w(), 20, _("Rotation"));
        s = wipeRotation = sV;
        s->tooltip(_("Wipe Rotation.  Use Shift + left mouse button along X to "
                     "rotate wipe."));
        s->range(0.f, 360.0f);
        s->default_value(0.0f);
        s->value(o.wipeRotation);
        sV->callback(
            [=](auto w)
            {
                auto o = model->observeCompareOptions()->get();
                o.wipeRotation = w->value();
                model->setCompareOptions(o);
            });

        cg->end();

        std::string key = prefix + "Wipe";
        std_any value = settingsObject->value(key);
        int open = std_any_empty(value) ? 1 : std_any_cast<int>(value);
        if (!open)
            cg->close();

        cg = new CollapsibleGroup(g->x(), 20, g->w(), 20, _("Overlay"));
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

                const std::string& prefix = comparePanel->tab_prefix();
                const std::string key = prefix + "Overlay";

                App* app = App::ui->app;
                auto settingsObject = app->settingsObject();
                settingsObject->setValue(key, static_cast<int>(cg->is_open()));

                comparePanel->refresh();
            },
            cg);

        cg->begin();

        sV = new Widget< HorSlider >(g->x(), 90, g->w(), 20, _("Overlay"));
        s = overlay = sV;
        s->range(0.f, 1.0f);
        s->step(0.01F);
        s->default_value(0.5f);
        s->value(o.overlay);
        s->tooltip(
#ifdef __APPLE__
            _("Use the Option key + left mouse button to change transparency.")
#else
            _("Use the Alt key + left mouse button to change transparency.")
#endif
        );
        sV->callback(
            [=](auto w)
            {
                auto o = model->observeCompareOptions()->get();
                o.overlay = w->value();
                model->setCompareOptions(o);
            });

        cg->end();

        key = prefix + "Overlay";
        value = settingsObject->value(key);
        open = std_any_empty(value) ? 1 : std_any_cast<int>(value);
        if (!open)
            cg->close();

        g->end();
    }

    void ComparePanel::redraw()
    {
        TLRENDER_P();

        const auto player = p.ui->uiView->getTimelinePlayer();
        if (!player)
            return;
        otio::RationalTime time = player->currentTime();

        const image::Size size(128, 64);

        const auto& model = p.ui->app->filesModel();
        const auto& files = model->observeFiles();
        size_t numFiles = files->getSize();

        auto Aindex = model->observeAIndex()->get();
        auto Bindices = model->observeBIndexes()->get();
        auto o = model->observeCompareOptions()->get();

        for (int i = 0; i < numFiles; ++i)
        {
            const auto& media = files->getItem(i);
            const auto& path = media->path;

            const std::string& dir = path.getDirectory();
            const std::string file =
                path.getBaseName() + path.getNumber() + path.getExtension();
            const std::string fullfile = dir + file;

            auto m = _r->map.find(i);
            ClipButton* b = (*m).second;

            uint16_t layerId = media->videoLayer;
            bool found = false;
            if (Aindex == i)
            {
                b->value(0);
                found = true;
                layerId = p.ui->uiColorChannel->value();
                time = player->currentTime();
            }

            for (auto Bindex : Bindices)
            {
                if (Bindex == i)
                {
                    found = true;
                    b->value(1);
                    time = player->currentTime();
                    break;
                }
            }
            if (!found)
            {
                b->value(0);
                time = media->currentTime;
            }
            b->redraw();

            const std::string& layer = getLayerName(layerId, p.ui);
            std::string text = dir + "\n" + file + layer;
            b->copy_label(text.c_str());

            if (auto context = _r->context.lock())
            {
                ThumbnailData* data = new ThumbnailData;
                data->widget = b;

                WidgetIds::const_iterator it = _r->ids.find(b);
                if (it != _r->ids.end())
                {
                    _r->thumbnailCreator->cancelRequests(it->second);
                    _r->ids.erase(it);
                }

                try
                {
                    auto timeline = timeline::Timeline::create(path, context);
                    auto timeRange = timeline->getTimeRange();

                    if (time::isValid(timeRange))
                    {
                        auto startTime = timeRange.start_time();
                        auto endTime = timeRange.end_time_inclusive();

                        if (time < startTime)
                            time = startTime;
                        else if (time > endTime)
                            time = endTime;
                    }

                    _r->thumbnailCreator->initThread();
                    int64_t id = _r->thumbnailCreator->request(
                        fullfile, time, size, compareThumbnail_cb, (void*)data,
                        layerId);
                    _r->ids[b] = id;
                }
                catch (const std::exception&)
                {
                }
            }
        }
    }

    void ComparePanel::setCompareOptions(const timeline::CompareOptions& value)
    {
        wipeX->value(value.wipeCenter.x);
        wipeY->value(value.wipeCenter.y);
        wipeRotation->value(value.wipeRotation);
        overlay->value(value.overlay);
    }

    void ComparePanel::refresh()
    {
        cancel_thumbnails();
        clear_controls();
        add_controls();
        end_group();
    }

} // namespace mrv
