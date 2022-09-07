#include "mrvFl/mrvTimelineSlider.h"

#include <tlCore/Math.h>
#include <tlCore/StringFormat.h>

#include "mrvCore/mrvPreferences.h"

#include <FL/fl_draw.H>

#include "mrvFl/mrvTimelinePlayer.h"
#include "mrvFl/mrvHotkey.h"

#include "mrViewer.h"


namespace mrv
{

    Timecode::Display TimelineSlider::_display = Timecode::kFrames;

    struct TimelineSlider::Private
    {
        std::weak_ptr<system::Context> context;
        // mrv::TimelineThumbnailProvider* thumbnailProvider = nullptr;
        // std::map<otime::RationalTime, QImage> thumbnailImages;
        imaging::ColorConfig colorConfig;
        mrv::TimelinePlayer* timelinePlayer = nullptr;
        mrv::TimeUnits units = mrv::TimeUnits::Timecode;
        mrv::TimeObject* timeObject = nullptr;
        bool thumbnails = true;
        int64_t thumbnailRequestId = 0;
        bool stopOnScrub = true;
        ViewerUI*  ui          = nullptr;
    };


    TimelineSlider::TimelineSlider( int x, int y, int w, int h,
                                    char* l ) :
        Slider( x, y, w, h, l ),
        _p( new Private )
    {
        type( TICK_ABOVE );
        slider_type( kNORMAL );
        Slider::minimum( 1 );
        Slider::maximum( 50 );
    }

    TimelineSlider::~TimelineSlider()
    {
    }

    void TimelineSlider::main( ViewerUI* m )
    {
        _p->ui = m;
    }

    ViewerUI* TimelineSlider::main() const
    {
        return _p->ui;
    }

    void TimelineSlider::setContext(
        const std::shared_ptr<system::Context>& context )
    {
        _p->context = context;
    }


    int TimelineSlider::handle( int e )
    {
        TLRENDER_P();

        if ( e == FL_ENTER ) {
            window()->cursor( FL_CURSOR_DEFAULT );
            return 1;
        }
        else if ( e == FL_DRAG || e == FL_PUSH )
        {
            window()->cursor( FL_CURSOR_DEFAULT );
        }
        else if ( e == FL_LEAVE )
        {
            // Fl::remove_timeout( (Fl_Timeout_Handler)showwin, this );
            // if (win) win->hide();
        }
        else if ( e == FL_KEYDOWN )
        {
            unsigned int rawkey = Fl::event_key();
            int ok = p.ui->uiView->handle( e );
            if ( ok ) return ok;
        }
        Fl_Boxtype bx = box();
        box( FL_FLAT_BOX );
        int ok = Slider::handle( e );
        box( bx );
        return ok;
        // if ( r != 0 ) return r;
        // return uiMain->uiView->handle( e );
    }

    //! Set the timeline player.
    void TimelineSlider::setTimelinePlayer(mrv::TimelinePlayer* t)
    {
        TLRENDER_P();
        p.timelinePlayer = t;

        const auto& globalStartTime = t->globalStartTime();
        const auto& duration = t->duration();
        const double start = globalStartTime.value();

        Slider::minimum( start );
        Slider::maximum( start + duration.value() );
        value( start );

    }

    void TimelineSlider::draw()
    {
        TLRENDER_P();
        // @todo: handle drawing of cache lines
        double v = _timeToPos( p.timelinePlayer->currentTime() );
        value( v );
        Slider::draw();
    }


    otime::RationalTime TimelineSlider::_posToTime(int value) const
    {
        TLRENDER_P();
        otime::RationalTime out = time::invalidTime;
        if (p.timelinePlayer)
        {
            const auto& globalStartTime = p.timelinePlayer->globalStartTime();
            const auto& duration = p.timelinePlayer->duration();
            out = otime::RationalTime(
                floor(math::clamp(static_cast<double>(value), 0.0, maximum()) /
                      static_cast<double>(maximum()) * (duration.value() - 1) +
                      globalStartTime.value()),
                duration.rate());
        }
        return out;
    }

    double TimelineSlider::_timeToPos(const otime::RationalTime& value) const
    {
        TLRENDER_P();
        double out = 0;
        if (p.timelinePlayer)
        {
            out = value.value();
        }
        return out;
    }

} // namespace mrv
