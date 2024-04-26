// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#include <cinttypes>

#include <tlCore/FontSystem.h>
#include <tlCore/StringFormat.h>

#include <tlGL/OffscreenBuffer.h>
#include <tlTimelineGL/RenderPrivate.h>
#include <tlGL/Init.h>
#include <tlGL/Util.h>

// mrViewer includes
#include "mrViewer.h"

#include "mrvCore/mrvColorSpaces.h"
#include "mrvCore/mrvLocale.h"
#include "mrvCore/mrvSequence.h"
#include "mrvCore/mrvI8N.h"

#include "mrvWidgets/mrvMultilineInput.h"

#include "mrvFl/mrvIO.h"
#include "mrvFl/mrvTimelinePlayer.h"

#include "mrvGL/mrvGLViewportPrivate.h"
#include "mrvGL/mrvGLDefines.h"
#include "mrvGL/mrvGLErrors.h"
#include "mrvGL/mrvGLUtil.h"
#include "mrvGL/mrvGLShaders.h"
#include "mrvGL/mrvGLShape.h"
#include "mrvGL/mrvTimelineViewport.h"
#include "mrvGL/mrvTimelineViewportPrivate.h"
#include "mrvGL/mrvGLViewport.h"

#include "mrvPanels/mrvPanelsCallbacks.h"

#include "mrvNetwork/mrvDummyClient.h"

#include "mrvApp/mrvSettingsObject.h"

#include <FL/platform.H>
#undef None

namespace
{
    const char* kModule = "view";
}

namespace mrv
{
    using namespace tl;

    Viewport::Viewport(int X, int Y, int W, int H, const char* L) :
        TimelineViewport(X, Y, W, H, L),
        _gl(new GLPrivate)
    {
        int stereo = 0;
        int fl_double = FL_DOUBLE;
#ifdef __APPLE__
        fl_double = 0;
#endif

        mode(FL_RGB | fl_double | FL_ALPHA | FL_STENCIL | FL_OPENGL3 | stereo);
    }

    Viewport::~Viewport() {}

    void Viewport::setContext(const std::weak_ptr<system::Context>& context)
    {
        _gl->context = context;
    }

    //! Refresh window by clearing the associated resources.
    void Viewport::refresh()
    {
        TLRENDER_P();
        MRV2_GL();
        if (gl.render)
            glDeleteBuffers(2, gl.pboIds);
        gl.render.reset();
        gl.outline.reset();
        gl.lines.reset();
#ifdef USE_ONE_PIXEL_LINES
        gl.outline.reset();
#endif
        gl.buffer.reset();
        gl.annotation.reset();
        gl.shader.reset();
        gl.stereoShader.reset();
        gl.annotationShader.reset();
        gl.vbo.reset();
        gl.vao.reset();
        p.fontSystem.reset();
        gl.index = 0;
        gl.nextIndex = 1;
    }

    void Viewport::_initializeGLResources()
    {
        TLRENDER_P();
        MRV2_GL();

        if (auto context = gl.context.lock())
        {

            gl.render = timeline_gl::Render::create(context);

            glGenBuffers(2, gl.pboIds);

            p.fontSystem = image::FontSystem::create(context);

#ifdef USE_ONE_PIXEL_LINES
            gl.outline = std::make_shared<opengl::Outline>();
#endif

            gl.lines = std::make_shared<opengl::Lines>();

            try
            {
                const std::string& vertexSource = timeline_gl::vertexSource();
                gl.shader =
                    gl::Shader::create(vertexSource, textureFragmentSource());
                gl.stereoShader =
                    gl::Shader::create(vertexSource, stereoFragmentSource());
                gl.annotationShader = gl::Shader::create(
                    vertexSource, annotationFragmentSource());
            }
            catch (const std::exception& e)
            {
                LOG_ERROR(e.what());
            }
        }
    }

    void Viewport::_initializeGL()
    {
        MRV2_GL();
        gl::initGLAD();

#ifdef MRV2_DEBUG_GL
        if (!gl.init_debug)
        {
            gl.init_debug = true;
#    ifndef __APPLE__
            // Apple's OpenGL 4.1 does not support glDebugMessageCallback.
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(glDebugOutput, nullptr);
            glDebugMessageControl(
                static_cast<GLenum>(GL_DONT_CARE),
                static_cast<GLenum>(GL_DONT_CARE),
                static_cast<GLenum>(GL_DONT_CARE), 0, nullptr, GL_TRUE);
#    endif
        }
#endif

        refresh();

        _initializeGLResources();
    }

    int Viewport::handle(int event)
    {
        switch (event)
        {
        case FL_HIDE:
            refresh();
            valid(0);
            break;
        default:
            break;
        }

        return TimelineViewport::handle(event);
    }

    void Viewport::draw()
    {
        TLRENDER_P();
        MRV2_GL();

        make_current(); // needed to work with GLFW

        if (!valid())
        {
            _initializeGL();

            if (p.ui->uiPrefs->uiPrefsOpenGLVsync->value() ==
                MonitorVSync::kVSyncNone)
                swap_interval(0);
            else
                swap_interval(1);

            valid(1);
        }

#ifdef DEBUG_SPEED
        auto start_time = std::chrono::high_resolution_clock::now();
#endif

        const auto& viewportSize = getViewportSize();
        const auto& renderSize = getRenderSize();
        bool transparent =
            p.backgroundOptions.type == timeline::Background::Transparent;

        try
        {
            if (renderSize.isValid())
            {
                gl::OffscreenBufferOptions offscreenBufferOptions;
                offscreenBufferOptions.colorType = image::PixelType::RGBA_F32;
                if (!p.displayOptions.empty())
                {
                    offscreenBufferOptions.colorFilters =
                        p.displayOptions[0].imageFilters;
                }
                offscreenBufferOptions.depth = gl::OffscreenDepth::_24;
                offscreenBufferOptions.stencil = gl::OffscreenStencil::_8;
                if (gl::doCreate(gl.buffer, renderSize, offscreenBufferOptions))
                {
                    gl.buffer = gl::OffscreenBuffer::create(
                        renderSize, offscreenBufferOptions);
                    unsigned dataSize =
                        renderSize.w * renderSize.h * 4 * sizeof(GLfloat);
                    glBindBuffer(GL_PIXEL_PACK_BUFFER, gl.pboIds[0]);
                    glBufferData(
                        GL_PIXEL_PACK_BUFFER, dataSize, 0, GL_STREAM_READ);
                    glBindBuffer(GL_PIXEL_PACK_BUFFER, gl.pboIds[1]);
                    glBufferData(
                        GL_PIXEL_PACK_BUFFER, dataSize, 0, GL_STREAM_READ);
                    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
                }

                if (can_do(FL_STEREO))
                {
                    if (gl::doCreate(
                            gl.stereoBuffer, renderSize,
                            offscreenBufferOptions))
                    {
                        gl.stereoBuffer = gl::OffscreenBuffer::create(
                            renderSize, offscreenBufferOptions);
                    }
                }
            }
            else
            {
                gl.buffer.reset();
                gl.stereoBuffer.reset();
            }

            if (gl.buffer && gl.render)
            {
                if (p.stereo3DOptions.output == Stereo3DOutput::OpenGL &&
                    p.stereo3DOptions.input == Stereo3DInput::Image &&
                    p.videoData.size() > 1)
                {
                    _drawStereoOpenGL();
                }
                else
                {
                    gl::OffscreenBufferBinding binding(gl.buffer);

                    locale::SetAndRestore saved;
                    timeline::RenderOptions renderOptions;
                    renderOptions.offscreenColorType =
                        image::PixelType::RGBA_F32;

                    gl.render->begin(renderSize, renderOptions);
                    gl.render->setOCIOOptions(p.ocioOptions);
                    gl.render->setLUTOptions(p.lutOptions);
                    if (p.missingFrame &&
                        p.missingFrameType != MissingFrameType::kBlackFrame)
                    {
                        _drawMissingFrame(renderSize);
                    }
                    else
                    {
                        if (p.stereo3DOptions.input == Stereo3DInput::Image &&
                            p.videoData.size() > 1)
                        {
                            _drawStereo3D();
                        }
                        else
                        {
                            gl.render->drawVideo(
                                p.videoData,
                                timeline::getBoxes(
                                    p.compareOptions.mode, p.videoData),
                                p.imageOptions, p.displayOptions,
                                p.compareOptions, p.backgroundOptions);
                        }
                    }
                    _drawOverlays(renderSize);
                    gl.render->end();
                }
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(e.what());
            gl.buffer.reset();
            gl.stereoBuffer.reset();
        }

        float r = 0.F, g = 0.F, b = 0.F, a = 0.F;

        if (!p.presentation)
        {
            uint8_t ur, ug, ub;
            Fl::get_color(p.ui->uiPrefs->uiPrefsViewBG->color(), ur, ug, ub);
            r = ur / 255.0f;
            g = ug / 255.0f;
            b = ub / 255.0f;

#ifdef FLTK_USE_WAYLAND
            if (fl_wl_display())
            {
                p.ui->uiViewGroup->color(fl_rgb_color(ur, ug, ub));
                p.ui->uiViewGroup->redraw();
            }
#endif
        }
        else
        {
            auto time = std::chrono::high_resolution_clock::now();
            auto elapsedTime =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    time - p.presentationTime)
                    .count();
            if (elapsedTime >= 3000)
            {
                window()->cursor(FL_CURSOR_NONE);
            }
        }

        glDrawBuffer(GL_BACK_LEFT);

        glViewport(0, 0, GLsizei(viewportSize.w), GLsizei(viewportSize.h));
        glClearStencil(0);
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        if (gl.buffer && gl.shader)
        {
            math::Matrix4x4f mvp;

            if (!p.ui->uiPrefs->uiPrefsBlitViewports->value() ||
                p.environmentMapOptions.type != EnvironmentMapOptions::kNone)
            {
                if (p.environmentMapOptions.type !=
                    EnvironmentMapOptions::kNone)
                {
                    mvp = _createEnvironmentMap();
                }
                else
                {
                    mvp = _createTexturedRectangle();
                }

                if (p.imageOptions[0].alphaBlend == timeline::AlphaBlend::None)
                {
                    glDisable(GL_BLEND);
                }

                gl.shader->bind();
                gl.shader->setUniform("transform.mvp", mvp);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, gl.buffer->getColorID());

                if (gl.vao && gl.vbo)
                {
                    gl.vao->bind();
                    gl.vao->draw(GL_TRIANGLES, 0, gl.vbo->getSize());
                }

                if (p.stereo3DOptions.output == Stereo3DOutput::OpenGL &&
                    p.stereo3DOptions.input == Stereo3DInput::Image)
                {
                    gl.shader->bind();
                    gl.shader->setUniform("transform.mvp", mvp);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, gl.stereoBuffer->getColorID());

                    if (gl.vao && gl.vbo)
                    {
                        glDrawBuffer(GL_BACK_RIGHT);
                        gl.vao->bind();
                        gl.vao->draw(GL_TRIANGLES, 0, gl.vbo->getSize());
                    }
                }

                if (p.imageOptions[0].alphaBlend == timeline::AlphaBlend::None)
                {
                    glEnable(GL_BLEND);
                }
            }
            else
            {
                mvp = _projectionMatrix();

                const GLint viewportX = p.viewPos.x;
                const GLint viewportY = p.viewPos.y;
                const GLsizei sizeW = renderSize.w * p.viewZoom;
                const GLsizei sizeH = renderSize.h * p.viewZoom;
                if (sizeW > 0 && sizeH > 0)
                {
                    glViewport(viewportX, viewportY, sizeW, sizeH);

                    glBindFramebuffer(GL_READ_FRAMEBUFFER, gl.buffer->getID());
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // 0 is screen

                    // Blit the offscreen buffer contents to the viewport
                    GLenum filter = GL_NEAREST;
                    if (!p.displayOptions.empty())
                    {
                        const auto& filters = p.displayOptions[0].imageFilters;
                        if (p.viewZoom < 1.0f &&
                            filters.minify == timeline::ImageFilter::Linear)
                            filter = GL_LINEAR;
                        else if (
                            p.viewZoom > 1.0f &&
                            filters.magnify == timeline::ImageFilter::Linear)
                            filter = GL_LINEAR;
                    }
                    glBlitFramebuffer(
                        0, 0, renderSize.w, renderSize.h, viewportX, viewportY,
                        sizeW + viewportX, sizeH + viewportY,
                        GL_COLOR_BUFFER_BIT, filter);

                    if (p.stereo3DOptions.output == Stereo3DOutput::OpenGL &&
                        p.stereo3DOptions.input == Stereo3DInput::Image)
                    {
                        glBindFramebuffer(
                            GL_READ_FRAMEBUFFER, gl.stereoBuffer->getID());
                        glBindFramebuffer(
                            GL_DRAW_FRAMEBUFFER, 0); // 0 is screen
                        glDrawBuffer(GL_BACK_RIGHT);
                        glBlitFramebuffer(
                            0, 0, renderSize.w, renderSize.h, viewportX,
                            viewportY, sizeW + viewportX, sizeH + viewportY,
                            GL_COLOR_BUFFER_BIT, filter);
                    }
                }
            }

            math::Box2i selection = p.colorAreaInfo.box = p.selection;
            if (selection.max.x >= 0)
            {
                // Check min < max
                if (selection.min.x > selection.max.x)
                {
                    int tmp = selection.max.x;
                    selection.max.x = selection.min.x;
                    selection.min.x = tmp;
                }
                if (selection.min.y > selection.max.y)
                {
                    int tmp = selection.max.y;
                    selection.max.y = selection.min.y;
                    selection.min.y = tmp;
                }
                // Copy it again in case it changed
                p.colorAreaInfo.box = selection;

                _mapBuffer();
            }
            else
            {
                p.image = nullptr;
            }
            if (panel::colorAreaPanel)
            {
                _calculateColorArea(p.colorAreaInfo);
                panel::colorAreaPanel->update(p.colorAreaInfo);
            }
            if (panel::histogramPanel)
            {
                panel::histogramPanel->update(p.colorAreaInfo);
            }
            if (panel::vectorscopePanel)
            {
                panel::vectorscopePanel->update(p.colorAreaInfo);
            }

            // Update the pixel bar from here only if we are playing a movie
            // and one that is not 1 frames long.
            bool update = !_shouldUpdatePixelBar();
            if (update)
                updatePixelBar();

            _unmapBuffer();

            update = _isPlaybackStopped() || _isSingleFrame();
            if (update)
                updatePixelBar();

            gl::OffscreenBufferOptions offscreenBufferOptions;
            offscreenBufferOptions.colorType = image::PixelType::RGBA_U8;
            if (!p.displayOptions.empty())
            {
                offscreenBufferOptions.colorFilters =
                    p.displayOptions[0].imageFilters;
            }
            offscreenBufferOptions.depth = gl::OffscreenDepth::None;
            offscreenBufferOptions.stencil = gl::OffscreenStencil::None;
            if (gl::doCreate(
                    gl.annotation, viewportSize, offscreenBufferOptions))
            {
                gl.annotation = gl::OffscreenBuffer::create(
                    viewportSize, offscreenBufferOptions);
            }

            if (p.selection.max.x >= 0)
            {
                Fl_Color c = p.ui->uiPrefs->uiPrefsViewSelection->color();
                uint8_t r, g, b;
                Fl::get_color(c, r, g, b);

                const image::Color4f color(r / 255.F, g / 255.F, b / 255.F);

                math::Box2i selection = p.selection;
                if (selection.min == selection.max)
                {
                    selection.max.x++;
                    selection.max.y++;
                }
                _drawRectangleOutline(selection, color, mvp);
            }

            if (p.showAnnotations && gl.annotation)
            {
                _drawAnnotations(mvp);
            }

            if (p.dataWindow)
                _drawDataWindow();
            if (p.displayWindow)
                _drawDisplayWindow();

            if (p.safeAreas)
                _drawSafeAreas();

            _drawCursor(mvp);

            if (p.hudActive && p.hud != HudDisplay::kNone)
                _drawHUD();

            if (!p.helpText.empty())
                _drawHelpText();
        }

        MultilineInput* w = getMultilineInput();
        if (w)
        {
            std_any value;
            int font_size = p.ui->app->settings()->getValue<int>(kFontSize);
            double pixels_unit = pixels_per_unit();
            double pct = renderSize.h / 1024.F;
            double fontSize = font_size * pct * p.viewZoom / pixels_unit;
            w->textsize(fontSize);
            math::Vector2i pos(w->pos.x, w->pos.y);
            w->Fl_Widget::position(pos.x, pos.y);
        }

#ifdef USE_OPENGL2
        Fl_Gl_Window::draw_begin(); // Set up 1:1 projection
        Fl_Window::draw();          // Draw FLTK children
        glViewport(0, 0, viewportSize.w, viewportSize.h);
        if (p.showAnnotations)
            _drawGL2TextShapes();
        Fl_Gl_Window::draw_end(); // Restore GL state
#else
#    ifndef NO_GL_WINDOW_CHILDREN
        Fl_Gl_Window::draw();
#    endif
#endif

#ifdef DEBUG_SPEED
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end_time - start_time;
        std::cout << "GL::draw() duration " << diff.count() << std::endl;
#endif
    }

    void Viewport::_calculateColorAreaFullValues(area::Info& info) noexcept
    {
        TLRENDER_P();
        MRV2_GL();

        PixelToolBarClass* c = p.ui->uiPixelWindow;
        BrightnessType brightness_type = (BrightnessType)c->uiLType->value();
        int hsv_colorspace = c->uiBColorType->value() + 1;

        const int maxX = info.box.max.x;
        const int maxY = info.box.max.y;
        const auto& renderSize = gl.buffer->getSize();

        for (int Y = info.box.y(); Y <= maxY; ++Y)
        {
            for (int X = info.box.x(); X <= maxX; ++X)
            {
                image::Color4f rgba, hsv;
                rgba.b = p.image[(X + Y * renderSize.w) * 4];
                rgba.g = p.image[(X + Y * renderSize.w) * 4 + 1];
                rgba.r = p.image[(X + Y * renderSize.w) * 4 + 2];
                rgba.a = p.image[(X + Y * renderSize.w) * 4 + 3];

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

    void Viewport::_calculateColorArea(area::Info& info)
    {
        TLRENDER_P();
        MRV2_GL();

        if (!p.image || !gl.buffer)
            return;

        info.rgba.max.r = std::numeric_limits<float>::min();
        info.rgba.max.g = std::numeric_limits<float>::min();
        info.rgba.max.b = std::numeric_limits<float>::min();
        info.rgba.max.a = std::numeric_limits<float>::min();

        info.rgba.min.r = std::numeric_limits<float>::max();
        info.rgba.min.g = std::numeric_limits<float>::max();
        info.rgba.min.b = std::numeric_limits<float>::max();
        info.rgba.min.a = std::numeric_limits<float>::max();

        info.rgba.mean.r = info.rgba.mean.g = info.rgba.mean.b =
            info.rgba.mean.a = 0.F;

        info.hsv.max.r = std::numeric_limits<float>::min();
        info.hsv.max.g = std::numeric_limits<float>::min();
        info.hsv.max.b = std::numeric_limits<float>::min();
        info.hsv.max.a = std::numeric_limits<float>::min();

        info.hsv.min.r = std::numeric_limits<float>::max();
        info.hsv.min.g = std::numeric_limits<float>::max();
        info.hsv.min.b = std::numeric_limits<float>::max();
        info.hsv.min.a = std::numeric_limits<float>::max();

        info.hsv.mean.r = info.hsv.mean.g = info.hsv.mean.b = info.hsv.mean.a =
            0.F;

        if (p.ui->uiPixelWindow->uiPixelValue->value() == PixelValue::kFull)
            _calculateColorAreaFullValues(info);
        else
            _calculateColorAreaRawValues(info);
    }

    void Viewport::_mapBuffer() const noexcept
    {
        MRV2_GL();
        TLRENDER_P();

        if (p.ui->uiPixelWindow->uiPixelValue->value() == PixelValue::kFull)
        {

            // For faster access, we muse use BGRA.
            constexpr GLenum format = GL_BGRA;
            constexpr GLenum type = GL_FLOAT;

            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);

            gl::OffscreenBufferBinding binding(gl.buffer);
            const auto& renderSize = gl.buffer->getSize();

            // bool update = _shouldUpdatePixelBar();
            bool stopped = _isPlaybackStopped();
            bool single_frame = _isSingleFrame();

            // set the target framebuffer to read
            // "index" is used to read pixels from framebuffer to a PBO
            // "nextIndex" is used to update pixels in the other PBO
            gl.index = (gl.index + 1) % 2;
            gl.nextIndex = (gl.index + 1) % 2;

            // If we are a single frame, we do a normal ReadPixels of front
            // buffer.

            if (single_frame)
            {
                _unmapBuffer();
                _mallocBuffer();
                if (!p.image)
                    return;
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glReadBuffer(GL_FRONT);
                glReadPixels(
                    0, 0, renderSize.w, renderSize.h, format, type, p.image);
                return;
            }
            else
            {

                // read pixels from framebuffer to PBO
                // glReadPixels() should return immediately.
                glBindBuffer(GL_PIXEL_PACK_BUFFER, gl.pboIds[gl.index]);

                glReadPixels(0, 0, renderSize.w, renderSize.h, format, type, 0);

                // map the PBO to process its data by CPU
                glBindBuffer(GL_PIXEL_PACK_BUFFER, gl.pboIds[gl.nextIndex]);

                // We are stopped, read the first PBO.
                if (stopped)
                {
                    glBindBuffer(GL_PIXEL_PACK_BUFFER, gl.pboIds[gl.index]);
                }
            }

            if (p.rawImage)
            {
                free(p.image);
                p.image = nullptr;
            }

            p.image = (float*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
            p.rawImage = false;
        }
        else
        {
            TimelineViewport::_mapBuffer();
        }
    }

    void Viewport::_unmapBuffer() const noexcept
    {
        TLRENDER_P();

        if (p.image)
        {
            if (!p.rawImage)
            {
                glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
                p.image = nullptr;
                p.rawImage = true;
            }
        }

        // back to conventional pixel operation
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }

    void Viewport::_readPixel(image::Color4f& rgba) const noexcept
    {

        TLRENDER_P();
        MRV2_GL();

        math::Vector2i pos = _getRaster();

        if (p.ui->uiPixelWindow->uiPixelValue->value() != PixelValue::kFull)
        {
            if (_isEnvironmentMap())
                return;

            rgba.r = rgba.g = rgba.b = rgba.a = 0.f;

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

                        if (layer.transition == timeline::Transition::Dissolve)
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
            }
        }
        else
        {
            // This is needed as the FL_MOVE of fltk wouuld get called
            // before the draw routine
            if (!gl.buffer || !valid())
            {
                return;
            }

            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);

            // We use ReadPixels when the movie is stopped or has only a
            // a single frame.
            bool update = _shouldUpdatePixelBar();

            if (_isEnvironmentMap())
            {
                update = true;
            }

            const GLenum type = GL_FLOAT;

            if (update)
            {
                _unmapBuffer();
                if (_isEnvironmentMap())
                {
                    pos = _getFocus();
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glReadBuffer(GL_FRONT);
                    glReadPixels(pos.x, pos.y, 1, 1, GL_RGBA, type, &rgba);
                    return;
                }
                else
                {
                    Viewport* self = const_cast<Viewport*>(this);
                    self->make_current();
                    gl::OffscreenBufferBinding binding(gl.buffer);
                    glReadPixels(pos.x, pos.y, 1, 1, GL_RGBA, type, &rgba);
                    return;
                }
            }

            if (!p.image)
                _mapBuffer();

            if (p.image)
            {
                const auto& renderSize = gl.buffer->getSize();
                rgba.b = p.image[(pos.x + pos.y * renderSize.w) * 4];
                rgba.g = p.image[(pos.x + pos.y * renderSize.w) * 4 + 1];
                rgba.r = p.image[(pos.x + pos.y * renderSize.w) * 4 + 2];
                rgba.a = p.image[(pos.x + pos.y * renderSize.w) * 4 + 3];
            }
        }

        _unmapBuffer();
    }

    void Viewport::_pushAnnotationShape(const std::string& command) const
    {
        // We should not update tcp client when not needed
        if (dynamic_cast< DummyClient* >(tcp))
            return;

        bool send = _p->ui->uiPrefs->SendAnnotations->value();
        if (!send)
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
        Message value;
        if (dynamic_cast< GLArrowShape* >(shape))
        {
            auto s = dynamic_cast< GLArrowShape* >(shape);
            value = *s;
        }
        else if (dynamic_cast< GLRectangleShape* >(shape))
        {
            auto s = dynamic_cast< GLRectangleShape* >(shape);
            value = *s;
        }
        else if (dynamic_cast< GLCircleShape* >(shape))
        {
            auto s = dynamic_cast< GLCircleShape* >(shape);
            value = *s;
        }
#ifdef USE_OPENGL2
        else if (dynamic_cast< GL2TextShape* >(shape))
        {
            auto s = dynamic_cast< GL2TextShape* >(shape);
            value = *s;
        }
#else
        else if (dynamic_cast< GLTextShape* >(shape))
        {
            auto s = dynamic_cast< GLTextShape* >(shape);
            value = *s;
        }
#endif
        else if (dynamic_cast< draw::NoteShape* >(shape))
        {
            auto s = dynamic_cast< draw::NoteShape* >(shape);
            value = *s;
        }
        else if (dynamic_cast< GLErasePathShape* >(shape))
        {
            auto s = dynamic_cast< GLErasePathShape* >(shape);
            value = *s;
        }
        else if (dynamic_cast< GLPathShape* >(shape))
        {
            auto s = dynamic_cast< GLPathShape* >(shape);
            value = *s;
        }
        else
        {
            throw std::runtime_error("Unknown shape");
        }
        Message msg;
        msg["command"] = command;
        msg["value"] = value;
        tcp->pushMessage(msg);
    }

} // namespace mrv
