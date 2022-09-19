#pragma once


// mrViewer includes
#include <mrvGL/mrvTimelineViewport.h>

namespace mrv
{

    //
    // This class implements a viewport using OpenGL
    //
    class GLViewport : public TimelineViewport
    {
        TLRENDER_NON_COPYABLE(GLViewport);

    public:
        GLViewport( int X, int Y, int W, int H, const char* L = 0 );

        ~GLViewport();

        //! Virual draw method
        virtual void draw() override;

        void setContext(
            const std::weak_ptr<system::Context>& context);


    protected:
        void initializeGL();

        void _drawHUD();

        virtual
        void _readPixel( imaging::Color4f& rgba ) const noexcept override;

    private:
        void _drawText( const std::vector<std::shared_ptr<imaging::Glyph> >&,
                        math::Vector2i&,
                        const int16_t lineHeight,
                        const imaging::Color4f&);

    private:
        struct GLPrivate;
        std::unique_ptr<GLPrivate> _gl;
    };
}
