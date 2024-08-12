// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#include <string>
#include <sstream>

#include <tlIO/System.h>

#include <tlCore/String.h>
#include <tlCore/StringFormat.h>
#include <tlCore/Time.h>

#include <tlGL/Init.h>
#include <tlGL/Util.h>
#include <tlGL/GLFWWindow.h>

#include <tlTimelineGL/Render.h>

#include "mrvCore/mrvLocale.h"
#include "mrvCore/mrvMath.h"
#include "mrvCore/mrvUtil.h"

#include "mrvWidgets/mrvProgressReport.h"

#include "mrvGL/mrvGLErrors.h"

#include "mrvNetwork/mrvTCP.h"

#include "mrvFl/mrvSaveOptions.h"
#include "mrvFl/mrvIO.h"

#include "mrvUI/mrvDesktop.h"

#include "mrvApp/mrvSettingsObject.h"

#include "mrViewer.h"

namespace
{
    const char* kModule = "save";
}

namespace
{
}

namespace mrv
{

    void
    save_movie(const std::string& file, const ViewerUI* ui, SaveOptions options)
    {
        std::string msg;
        Viewport* view = ui->uiView;
        bool presentation = view->getPresentationMode();
        bool hud = view->getHudActive();

        auto player = view->getTimelinePlayer();
        if (!player)
            return; // should never happen

        file::Path path(file);

        if (file::isTemporaryEDL(path))
        {
            LOG_ERROR(_("Cannot save an NDI stream"));
            return;
        }

        // Stop the playback
        player->stop();

        // Time range.
        auto timeRange = player->inOutRange();
        auto speed = player->speed();
        auto startTime = timeRange.start_time();
        auto endTime = timeRange.end_time_inclusive();
        auto currentTime = startTime;

        auto mute = player->isMuted();
        player->setMute(true);
        
        auto context = ui->app->getContext();

        // Get I/O cache and store its size.
        auto ioSystem = context->getSystem<io::System>();
        auto cache = ioSystem->getCache();

        size_t oldCacheSize = cache->getMax();

        const std::string& directory = path.getDirectory();
        const std::string& baseName = path.getBaseName();
        const std::string& number = path.getNumber();
        const std::string& extension = path.getExtension();

        std::string newFile = directory + baseName + number + extension;

        try
        {

            tl::io::Options ioOptions;

#ifdef TLRENDER_FFMPEG
            ioOptions["FFmpeg/WriteProfile"] = getLabel(options.ffmpegProfile);
            ioOptions["FFmpeg/AudioCodec"] = getLabel(options.ffmpegAudioCodec);
            ioOptions["FFmpeg/ThreadCount"] =
                string::Format("{0}").arg(ffmpeg::threadCount);

            // If we are not saving a movie, take speed from the player's
            // current speed.
            {
                const auto& model = ui->app->filesModel();
                const auto& Aitem = model->observeA()->get();
                const auto& extension = Aitem->path.getExtension();
                if (!file::isMovie(extension))
                {
                    ioOptions["FFmpeg/Speed"] =
                        string::Format("{0}").arg(speed);
                }
            }

            // If we have a preset, send it over.
            if (!options.ffmpegPreset.empty())
            {
                ioOptions["FFmpeg/PresetFile"] = options.ffmpegPreset;
            }

            ioOptions["FFmpeg/PixelFormat"] = options.ffmpegPixelFormat;
            ioOptions["FFmpeg/HardwareEncode"] =
                string::Format("{0}").arg(options.ffmpegHardwareEncode);
            if (options.ffmpegOverride)
            {
                ioOptions["FFmpeg/ColorRange"] = options.ffmpegColorRange;
                ioOptions["FFmpeg/ColorSpace"] = options.ffmpegColorSpace;
                ioOptions["FFmpeg/ColorPrimaries"] =
                    options.ffmpegColorPrimaries;
                ioOptions["FFmpeg/ColorTRC"] = options.ffmpegColorTRC;
            }
#endif

#ifdef TLRENDER_EXR
            ioOptions["OpenEXR/Compression"] = getLabel(options.exrCompression);
            ioOptions["OpenEXR/PixelType"] = getLabel(options.exrPixelType);
            {
                std::stringstream s;
                s << options.zipCompressionLevel;
                ioOptions["OpenEXR/ZipCompressionLevel"] = s.str();
            }
            {
                std::stringstream s;
                s << options.dwaCompressionLevel;
                ioOptions["OpenEXR/DWACompressionLevel"] = s.str();
            }

            {
                std::stringstream s;
                s << speed;
                ioOptions["OpenEXR/Speed"] = s.str();
                LOG_INFO("OpenEXR Speed=" << speed);
            }
#endif

            auto model = ui->app->filesModel();
            auto Aitem = model->observeA()->get();
            std::string inputFile = Aitem->path.get();

            // Make I/O cache be 1Gb to deal with long movies fine.
            size_t bytes = memory::gigabyte;
            cache->setMax(bytes);

            auto context = ui->app->getContext();
            auto timeline = player->timeline();

            auto startTimeOpt = timeline->getTimeline()->global_start_time();
            if (startTime.value() > 0.0 || startTimeOpt.has_value())
            {
                std::stringstream s;
                std::string timecode = startTime.to_timecode();
                if (timecode.empty() && startTimeOpt.has_value())
                {
                    timecode = startTimeOpt.value().to_timecode();
                }
                if (!timecode.empty())
                {
                    s << timecode;
                    ioOptions["timecode"] = s.str();
                }
            }

            const bool savingMovie = file::isMovie(extension);
            const bool savingAudio = file::isAudio(extension);
            if (savingMovie)
            {
                msg = string::Format(_("Saving movie to {0}.")).arg(newFile);
            }
            else if (savingAudio)
            {
                msg = string::Format(_("Saving audio to {0}.")).arg(newFile);
            }
            else
            {
                msg = string::Format(_("Saving pictures to {0}.")).arg(newFile);
            }
            LOG_INFO(msg);

            // Render information.
            const auto& info = player->ioInfo();

            auto videoTime = info.videoTime;

            const bool hasVideo = !info.video.empty();

            if (player->timeRange() != timeRange ||
                info.videoTime.start_time() != timeRange.start_time())
            {
                double videoRate = info.videoTime.duration().rate();
                videoTime = otime::TimeRange(
                    timeRange.start_time().rescaled_to(videoRate),
                    timeRange.duration().rescaled_to(videoRate));
            }

            auto audioTime = time::invalidTimeRange;
            const double sampleRate = info.audio.sampleRate;
            bool hasAudio = info.audio.isValid();
            if (hasAudio)
            {
                audioTime = info.audioTime;
                if (player->timeRange() != timeRange ||
                    audioTime.start_time() !=
                        timeRange.start_time().rescaled_to(sampleRate))
                {
                    audioTime = otime::TimeRange(
                        timeRange.start_time().rescaled_to(sampleRate),
                        timeRange.duration().rescaled_to(sampleRate));
                }
            }


#ifdef TLRENDER_FFMPEG
            const std::string& profile = getLabel(options.ffmpegProfile);

            std::string newExtension = extension;
            if (profile == "VP9")
            {
                if (!string::compare(
                        extension, ".mp4", string::Compare::CaseInsensitive) &&
                    !string::compare(
                        extension, ".webm", string::Compare::CaseInsensitive) &&
                    !string::compare(
                        extension, ".mkv", string::Compare::CaseInsensitive))
                {
                    LOG_WARNING(
                        _("VP9 profile needs a .mp4, .mkv or .webm movie "
                          "extension.  Changing it to .mp4"));
                    newExtension = ".mp4";
                }
            }
            else if (profile == "AV1")
            {
                if (!string::compare(
                        extension, ".mp4", string::Compare::CaseInsensitive) &&
                    !string::compare(
                        extension, ".mkv", string::Compare::CaseInsensitive))
                {
                    LOG_WARNING(_("AV1 profile needs a .mp4 or .mkv movie "
                                  "extension.  Changing it to .mp4"));
                    newExtension = ".mp4";
                }
            }
            else if (profile == "Cineform")
            {
                if (!string::compare(
                        extension, ".mkv", string::Compare::CaseInsensitive))
                {
                    LOG_WARNING(_("GoPro Cineform profile needs a .mkv movie "
                                  "extension.  Changing it to .mkv"));
                    newExtension = ".mkv";
                }
            }
            else if (profile == "HAP")
            {
                if (!string::compare(
                        extension, ".mov", string::Compare::CaseInsensitive))
                {
                    LOG_WARNING(
                        _("HAP profile needs a .mov extension.  Changing "
                          "it to .mov"));
                    newExtension = ".mov";
                }
            }

            newFile = directory + baseName + number + newExtension;

            if (newFile != file)
            {
                if (fs::exists(newFile))
                    throw(string::Format(_("New file {0} already exist!  "
                                           "Cannot overwrite it."))
                              .arg(newFile));
            }

            path = file::Path(newFile);
#endif

            bool saveEXR = string::compare(
                extension, ".exr", string::Compare::CaseInsensitive);
            bool saveHDR = string::compare(
                extension, ".hdr", string::Compare::CaseInsensitive);

            if (time::compareExact(videoTime, time::invalidTimeRange))
                videoTime = audioTime;


            const size_t endAudioSampleCount =
                endTime.rescaled_to(sampleRate).value();
            const size_t maxAudioSampleCount =
                timeRange.duration().rescaled_to(sampleRate).value();

            const std::string& originalFile = player->path().get();
            if (originalFile == file)
            {
                throw std::runtime_error(
                    string::Format("{0}: Saving over same file being played!")
                        .arg(file));
            }

            gl::OffscreenBufferOptions offscreenBufferOptions;
            std::shared_ptr<timeline_gl::Render> render;
            image::Size renderSize;
            int layerId = ui->uiColorChannel->value();


            const SaveResolution resolution = options.resolution;
            if (hasVideo)
            {
                renderSize = info.video[layerId].size;
                auto rotation = ui->uiView->getRotation();
                if (options.annotations && rotationSign(rotation) != 0)
                {
                    size_t tmp = renderSize.w;
                    renderSize.w = renderSize.h;
                    renderSize.h = tmp;

                    msg = tl::string::Format(_("Rotated image info: {0}"))
                              .arg(renderSize);
                    LOG_INFO(msg);
                }
                if (resolution == SaveResolution::kHalfSize)
                {
                    renderSize.w /= 2;
                    renderSize.h /= 2;
                    msg = tl::string::Format(_("Scaled image info: {0}"))
                              .arg(renderSize);
                    LOG_INFO(msg);
                }
                else if (resolution == SaveResolution::kQuarterSize)
                {
                    renderSize.w /= 4;
                    renderSize.h /= 4;
                    msg = tl::string::Format(_("Scaled image info: {0}"))
                              .arg(renderSize);
                    LOG_INFO(msg);
                }
            }


            bool interactive = ui->uiView->visible_r();
            std::shared_ptr<gl::GLFWWindow> window;
            if (!interactive)
            {
                // Create the window.
                window = gl::GLFWWindow::create(
                    "bake",
                    math::Size2i(1, 1),
                    context,
                    static_cast<int>(gl::GLFWWindowOptions::MakeCurrent));
            }
            
            // Create the renderer.
            render = timeline_gl::Render::create(context);

            offscreenBufferOptions.colorType = image::PixelType::RGBA_F32;

            // Create the writer.
            auto writerPlugin = ioSystem->getPlugin(path);

            if (!writerPlugin)
            {
                throw std::runtime_error(
                    string::Format(_("{0}: Cannot open writer plugin."))
                        .arg(file));
            }


            int X = 0, Y = 0;

            io::Info ioInfo;
            image::Info outputInfo;

            outputInfo.size = renderSize;
            std::shared_ptr<image::Image> outputImage;

            outputInfo.pixelType = info.video[layerId].pixelType;

            if (hasVideo)
            {

                msg = tl::string::Format(_("Image info: {0} {1}"))
                          .arg(outputInfo.size)
                          .arg(outputInfo.pixelType);
                LOG_INFO(msg);

                if (options.annotations)
                {
                    view->setActionMode(ActionMode::kScrub);
                    view->setPresentationMode(true);
                    view->redraw();
                    // flush is needed
                    Fl::flush();
                    view->flush();
                    Fl::check();
                    const auto& viewportSize = view->getViewportSize();
                    math::Size2i outputSize;
                    if (viewportSize.w >= renderSize.w &&
                        viewportSize.h >= renderSize.h)
                    {
                        view->setFrameView(false);
                        if (resolution == SaveResolution::kHalfSize)
                            view->setViewZoom(0.5);
                        else if (resolution == SaveResolution::kQuarterSize)
                            view->setViewZoom(0.25);
                        else
                            view->setViewZoom(1.0);
                        view->centerView();
                        view->redraw();
                        // flush is needed
                        Fl::flush();

                        outputInfo.size = renderSize;
                    }
                    else
                    {
                        LOG_WARNING(_("Image too big for options.annotations.  "
                                      "Will scale to the viewport size."));

                        view->frameView();

                        double viewportRatio =
                            viewportSize.w /
                            static_cast<double>(viewportSize.h);
                        double imageRatio =
                            renderSize.w / static_cast<double>(renderSize.h);

                        if (imageRatio < viewportRatio)
                        {
                            double factor = viewportSize.h /
                                            static_cast<double>(renderSize.h);
                            outputInfo.size.w =
                                std::round(renderSize.w * factor);
                            outputInfo.size.h = viewportSize.h;
                        }
                        else
                        {
                            double factor = viewportSize.w /
                                            static_cast<double>(renderSize.w);
                            outputInfo.size.h =
                                std::round(renderSize.h * factor);
                            outputInfo.size.w = viewportSize.w;
                        }
                    }

                    X = (viewportSize.w - outputInfo.size.w) / 2;
                    Y = (viewportSize.h - outputInfo.size.h) / 2;

                    msg = tl::string::Format(_("Viewport Size: {0} - "
                                               "X={1}, Y={2}"))
                              .arg(viewportSize)
                              .arg(X)
                              .arg(Y);
                    LOG_INFO(msg);
                }

                outputInfo = writerPlugin->getWriteInfo(outputInfo);
                if (image::PixelType::None == outputInfo.pixelType)
                {
                    outputInfo.pixelType = image::PixelType::RGB_U8;
                    offscreenBufferOptions.colorType = image::PixelType::RGB_U8;
#ifdef TLRENDER_EXR
                    if (saveEXR)
                    {
                        offscreenBufferOptions.colorType =
                            image::PixelType::RGB_F32;
                    }
#endif
                    msg = tl::string::Format(
                              _("Writer plugin did not get output info.  "
                                "Defaulting to {0}"))
                              .arg(offscreenBufferOptions.colorType);
                    LOG_INFO(msg);
                }

#ifdef TLRENDER_EXR
                if (saveEXR)
                {
                    outputInfo.pixelType = options.exrPixelType;
                }
#endif
                if (saveHDR)
                {
                    outputInfo.pixelType = image::PixelType::RGB_F32;
                    offscreenBufferOptions.colorType =
                        image::PixelType::RGB_F32;
                }


                msg = tl::string::Format(_("Output info: {0} {1}"))
                          .arg(outputInfo.size)
                          .arg(outputInfo.pixelType);
                LOG_INFO(msg);

                outputImage = image::Image::create(outputInfo);
                ioInfo.videoTime = videoTime;
                ioInfo.video.push_back(outputInfo);

#ifdef TLRENDER_FFMPEG
                auto entries = tl::ffmpeg::getProfileLabels();
                std::string profileName = entries[(int)options.ffmpegProfile];

                msg = tl::string::Format(
                          _("Using profile {0}, pixel format {1}."))
                          .arg(profileName)
                          .arg(options.ffmpegPixelFormat);
                LOG_INFO(msg);
                if (!options.ffmpegPreset.empty())
                {
                    msg = tl::string::Format(_("Using preset {0}."))
                              .arg(options.ffmpegPreset);
                    LOG_INFO(msg);
                }
#endif
            }

            if (hasAudio)
            {
                ioInfo.audio = info.audio;
                ioInfo.audioTime = audioTime;
            }

            auto writer = writerPlugin->write(path, ioInfo, ioOptions);
            if (!writer)
            {
                throw std::runtime_error(
                    string::Format("{0}: Cannot open").arg(file));
            }

            int64_t startFrame = startTime.to_frames();
            int64_t endFrame = endTime.to_frames();

            char title[1024];

#ifdef TLRENDER_FFMPEG
            if (hasVideo && savingMovie)
            {
                if (static_cast<ffmpeg::AudioCodec>(options.ffmpegAudioCodec) ==
                        ffmpeg::AudioCodec::None ||
                    !hasAudio)
                    snprintf(
                        title, 1024,
                        _("Saving Movie without Audio %" PRId64 " - %" PRId64),
                        startFrame, endFrame);
                else
                    snprintf(
                        title, 1024,
                        _("Saving Movie with Audio %" PRId64 " - %" PRId64),
                        startFrame, endFrame);
            }
            else if (hasAudio && savingAudio)
            {
                snprintf(
                    title, 1024, _("Saving Audio %" PRId64 " - %" PRId64),
                    startFrame, endFrame);
            }
            else
#endif
                if (hasVideo && !savingMovie && !savingAudio)
            {
                snprintf(
                    title, 1024,
                    _("Saving Pictures without Audio %" PRId64 " - %" PRId64),
                    startFrame, endFrame);
                hasAudio = false;
            }
            else
            {
                LOG_ERROR(
                    _("Audio only in timeline, but not trying to save audio."));
                return;
            }

            ProgressReport progress(ui->uiMain, startFrame, endFrame, title);
            if (interactive)
                progress.show();

            bool running = true;

            // Don't send any tcp updates
            tcp->lock();

            GLenum format = gl::getReadPixelsFormat(outputInfo.pixelType);
            GLenum type = gl::getReadPixelsType(outputInfo.pixelType);
            if (hasVideo)
            {
                if (GL_NONE == format || GL_NONE == type)
                {
                    throw std::runtime_error(
                        string::Format(_("{0}: Invalid OpenGL format and type"))
                            .arg(file));
                }
            }

            msg = tl::string::Format(_("OpenGL info: {0}"))
                      .arg(offscreenBufferOptions.colorType);
            LOG_INFO(msg);

            player->start();

            // Turn off hud so it does not get captured by glReadPixels.
            view->setHudActive(false);

            math::Size2i offscreenBufferSize(renderSize.w, renderSize.h);
            std::shared_ptr<gl::OffscreenBuffer> buffer;

            if (hasVideo)
            {
                if (interactive)
                {
                    view->make_current();
                    gl::initGLAD();
                }
                
                buffer = gl::OffscreenBuffer::create(
                    offscreenBufferSize, offscreenBufferOptions);
            }

            size_t totalSamples = 0;
            size_t currentSampleCount =
                startTime.rescaled_to(sampleRate).value();

            while (running)
            {
                context->tick();

                // If progress window is closed, exit loop.
                if (interactive)
                {
                    if(!progress.tick())
                        break;
                }
                else
                {
                    msg = string::Format(_("Saving... {0}")).arg(currentTime);
                    LOG_INFO(msg);
                }

                if (hasAudio)
                {
                    const double seconds = currentTime.to_seconds();
                    const auto audioData =
                        timeline->getAudio(seconds).future.get();
                    if (!audioData.layers.empty())
                    {
                        bool skip = false;
                        otime::TimeRange range;

                        if (hasVideo)
                            range = otime::TimeRange(
                                currentTime,
                                otime::RationalTime(1.0, currentTime.rate()));
                        else
                            range = otime::TimeRange(
                                currentTime,
                                otime::RationalTime(
                                    currentTime.rate(), currentTime.rate()));
                        auto audio = audioData.layers[0].audio;

                        // \todo mix audio layers (or have a function in
                        // timeline to do it).
                        const auto currentAudioTime =
                            currentTime.rescaled_to(sampleRate);

                        // timeline->getAudio() returns one second of audio.
                        // Clamp to end of the timeRange/inOutRange.
                        if (currentAudioTime.value() >= currentSampleCount)
                        {
                            const size_t sampleCount = audio->getSampleCount();
                            if (currentSampleCount + sampleCount >=
                                endAudioSampleCount)
                            {
                                int64_t clampedSamples =
                                    maxAudioSampleCount - totalSamples;
                                if (clampedSamples > 0)
                                {
                                    clampedSamples = std::min(
                                        static_cast<size_t>(clampedSamples),
                                        sampleCount);
                                    auto tmp = audio::Audio::create(
                                        audio->getInfo(), clampedSamples);
                                    memcpy(
                                        tmp->getData(), audio->getData(),
                                        tmp->getByteCount());
                                    audio = tmp;
                                }
                                else
                                {
                                    skip = true;
                                }
                            }
                        }
                        else
                        {
                            skip = true;
                        }

                        if (audioTime.contains(currentAudioTime))
                        {
                            if (!skip)
                            {
                                writer->writeAudio(range, audio);

                                const size_t sampleCount =
                                    audio->getSampleCount();
                                currentSampleCount += sampleCount;
                                totalSamples += sampleCount;
                            }
                        }
                    }
                }

                if (hasVideo)
                {
                    if (options.annotations)
                    {
                        view->redraw();
                        view->flush();

                        GLenum imageBuffer = GL_FRONT;

                        // @note: Wayland does not work like Windows, macOS or
                        //        X11.  The compositor does not immediately
                        //        swap buffers when calling view->flush().
                        if (desktop::Wayland())
                        {
                            imageBuffer = GL_BACK;
                        }

                        glReadBuffer(imageBuffer);

                        glReadBuffer(GL_FRONT);
                        glReadPixels(
                            X, Y, outputInfo.size.w, outputInfo.size.h, format,
                            type, outputImage->getData());
                    }
                    else
                    {
                        // Get the videoData
                        const auto videoData =
                            timeline->getVideo(currentTime).future.get();
                        if (videoData.layers.empty() ||
                            !videoData.layers[0].image)
                        {
                            std::string err =
                                string::Format(
                                    _("Empty video data at time {0}."))
                                    .arg(currentTime);
                            LOG_ERROR(err);
                        }

                        // This refreshes the view window
                        if (interactive)
                        {
                            view->make_current();
                            view->currentVideoCallback({videoData});
                            view->flush();
                        }
                        
                        // back to conventional pixel operation
                        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
                        // CHECK_GL;
                        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

                        if (interactive)
                            gl::initGLAD();

                        // Render the video to an offscreen buffer.
                        gl::OffscreenBufferBinding binding(buffer);
                        {
                            locale::SetAndRestore saved;
                            render->begin(offscreenBufferSize);
                            render->setOCIOOptions(view->getOCIOOptions());
                            render->setLUTOptions(view->lutOptions());
                            render->drawVideo(
                                {videoData},
                                {math::Box2i(0, 0, renderSize.w, renderSize.h)},
                                {timeline::ImageOptions()},
                                {timeline::DisplayOptions()},
                                timeline::CompareOptions(),
                                ui->uiView->getBackgroundOptions());
                            render->end();
                        }

                        glPixelStorei(
                            GL_PACK_ALIGNMENT, outputInfo.layout.alignment);
#if defined(TLRENDER_API_GL_4_1)
                        glPixelStorei(
                            GL_PACK_SWAP_BYTES,
                            outputInfo.layout.endian != memory::getEndian());
#endif // TLRENDER_API_GL_4_1

                        glReadPixels(
                            0, 0, outputInfo.size.w, outputInfo.size.h, format,
                            type, outputImage->getData());
                    }

                    if (videoTime.contains(currentTime))
                        writer->writeVideo(currentTime, outputImage);
                }

                if (hasVideo)
                    currentTime += otime::RationalTime(1, currentTime.rate());
                else
                    currentTime += otime::RationalTime(
                        currentTime.rate(), currentTime.rate());

                if (currentTime > endTime)
                {
                    running = false;
                }
                else
                {
                    // We need to use frameNext instead of seeking as large
                    // movies can lag behind the seek
                    // When saving video and not options.annotations, we cannot
                    // use seek as it corrupts the timeline.
                    if (options.annotations && hasVideo)
                        player->frameNext();
                    else if (!hasVideo)
                        player->seek(currentTime);
                }
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(e.what());
        }

        view->setFrameView(ui->uiPrefs->uiPrefsAutoFitImage->value());
        view->setHudActive(hud);
        view->setPresentationMode(presentation);
        player->seek(currentTime);
        player->setMute(mute);
        ui->uiTimeline->valid(0); // needed
        ui->uiTimeline->redraw();
        tcp->unlock();

        auto settings = ui->app->settings();
        if (file::isReadable(newFile))
        {
            settings->addRecentFile(path.get());
            ui->uiMain->fill_menu(ui->uiMenuBar);
        }

        cache->setMax(oldCacheSize);
    }

} // namespace mrv
