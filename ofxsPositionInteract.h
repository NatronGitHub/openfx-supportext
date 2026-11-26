/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of openfx-supportext <https://github.com/NatronGitHub/openfx-supportext>,
 * (C) 2018-2021 The Natron Developers
 * (C) 2013-2018 INRIA
 *
 * openfx-supportext is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * openfx-supportext is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with openfx-supportext.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

/*
 * OFX generic position interact.
 */

#ifndef openfx_supportext_ofxsPositionInteract_h
#define openfx_supportext_ofxsPositionInteract_h

#include <cmath>
#include <cassert>
#include <cfloat> // FLT_MIN
#include <string>
#include <vector>

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <ofxsInteract.h>
#include <ofxsImageEffect.h>
#include "ofxsInteractCursor.h"
#include "ofxsOGLTextRenderer.h"
#include "ofxsOGLHiDPI.h"
#include "ofxsOverlayDraw.h"
#include "ofxsMacros.h"

#define kDrawPointTrajectoryMaxPoints 1000
#define kDrawPointTrajectoryMinStepsPerKey 8

namespace OFX {

// Before calling this function: make sure the OpenGL props are correctly set.
// Try to draw one point per frame, but no more than kDrawPointTrajectoryMaxPoints
// in total, and at least kDrawPointTrajectoryMinStepsPerKey between two keys.
// DRAW is OverlayDraw, or OverlayDrawGLDirect for plain OpenGL.
template <class DRAW>
inline void
drawPointTrajectoryT(DRAW& d, Double2DParam* p)
{
    int numKeys = p->getNumKeys();

    if (numKeys > 0) {
        int maxStepsPerKey = int(kDrawPointTrajectoryMaxPoints / numKeys);
        d.glBegin(GL_POINTS);
        for (int i=0; i < numKeys; ++i) {
            double time = p->getKeyTime(i);
            OfxPointD pt;
            p->getValueAtTime(time, pt.x, pt.y);
            d.glVertex2d(pt.x, pt.y);

        }
        d.glEnd();
        d.glBegin(GL_LINE_STRIP);
        double time = p->getKeyTime(0);
        for (int i = 1; i < numKeys; ++i) {
            double timeNext = p->getKeyTime(i);
            int steps = (std::max)(kDrawPointTrajectoryMinStepsPerKey, (std::min)(int(timeNext - time), maxStepsPerKey));
            for (int j = (i == 1 ? 0 : 1); j <= steps; ++j) {
                double timeStep = time + j * (timeNext - time) / steps;

                OfxPointD pt;
                p->getValueAtTime(timeStep, pt.x, pt.y);
                d.glVertex2d(pt.x, pt.y);
            }
            time = timeNext;
        }
        d.glEnd();
    }
}


/// forwards to OpenGL, for drawPointTrajectory(Double2DParam*)
struct OverlayDrawGLDirect
{
    void glBegin(GLenum mode) { ::glBegin(mode); }
    void glVertex2d(GLdouble x, GLdouble y) { ::glVertex2d(x, y); }
    void glEnd() { ::glEnd(); }
};

// draw with OpenGL
inline void
drawPointTrajectory(Double2DParam* p)
{
    OverlayDrawGLDirect d;

    drawPointTrajectoryT(d, p);
}

// draw with OpenGL or the DrawSuite
inline void
drawPointTrajectory(OverlayDraw& d, Double2DParam* p)
{
    drawPointTrajectoryT(d, p);
}


/// One point handle of a PointsInteract.
struct PointsInteractPoint
{
    explicit PointsInteractPoint(const std::string& paramName)
        : name(paramName)
        , label()
        , colour()
        , hostColour(false)
        , size(5.)
        , updateWhileDragging(false)
        , interactiveName()
    {
        colour.r = colour.g = colour.b = 0.8;
    }

    std::string name; ///< the Double2D param, an eDoubleTypeXYAbsolute position
    std::string label; ///< drawn above the point; none if empty
    OfxRGBColourD colour; ///< its colour (green while the pen hovers or drags it)
    bool hostColour; ///< use the host's suggested overlay colour instead of colour, if the host has one
    double size; ///< GL point size in screen pixels; the pen picks it within size + 1 pixels in x and y
    bool updateWhileDragging; ///< set the param at each pen motion (the host renders during the drag), else at pen-up only
    std::string interactiveName; ///< if not empty, a Boolean param read at pen down instead of updateWhileDragging
};

/// Point handles configured at run time: each point is a Double2D param, drawn as a GL point
/// with a one-pixel shadow on OpenGL (no shadow with the DrawSuite), and dragged with the pen.
/*
   A point is shown, and can be picked, if its param is not secret, is enabled, the host draws no
   native handle for it (kOfxParamPropUseHostOverlayHandle), and isPointShown() is true. The
   first point in the list is drawn on top, and picked first.
   A drag sets the point's value with setPointValue(): at each pen motion if it updates while
   dragging, and at pen up. The drag state is reset before setPointValue() is called, so a
   host error can't leave the point following the pen. loseFocus abandons the drag (the points
   that update while dragging keep their last value).

   Subclass it to give the points at run time, and register its V2 descriptor:

   class MyPointsInteract : public OFX::PointsInteract
   {
   public:
       MyPointsInteract(OfxInteractHandle handle, OFX::ImageEffect* effect)
           : OFX::PointsInteract(handle, effect, myPoints(effect)) {}
   private:
       static std::vector<OFX::PointsInteractPoint> myPoints(OFX::ImageEffect* effect);
       // optional: a run-time condition, and the plug-in's bookkeeping (see below)
       bool isPointShown(int i, double time) const OVERRIDE;
       void setPointValue(int i, double time, const OfxPointD& p, bool firstChange, bool dragEnd) OVERRIDE;
   };
   ...
   OFX::ofxsSetOverlayInteractDescriptorV2(desc, new OFX::PointsOverlayDescriptorV2<MyPointsInteract>);
 */
class PointsInteract
    : public OFX::OverlayInteract
{
public:
    PointsInteract(OfxInteractHandle handle,
                   OFX::ImageEffect* effect,
                   const std::vector<PointsInteractPoint>& points)
        : OFX::OverlayInteract(handle)
        , _points()
        , _hiDPI(NULL)
        , _hovered(-1)
        , _dragged(-1)
        , _dragUpdates(false)
        , _dragChanged(false)
        , _drawMode( OFX::ofxsOverlayDrawModeForNewInteract() )
    {
        _points.reserve( points.size() );
        for (std::vector<PointsInteractPoint>::const_iterator it = points.begin(); it != points.end(); ++it) {
            Handle h(*it);
            h.param = effect->fetchDouble2DParam(it->name);
            assert(h.param);
            addParamToSlaveTo(h.param);
            if ( !it->interactiveName.empty() ) {
                h.interactive = effect->fetchBooleanParam(it->interactiveName);
                assert(h.interactive);
            }
            h.hostHandle = h.param->getHostHasNativeOverlayHandle();
            _points.push_back(h);
        }
        if ( effect->paramExists(kParamHiDPI) ) {
            _hiDPI = effect->fetchBooleanParam(kParamHiDPI);
            assert(_hiDPI);
            addParamToSlaveTo(_hiDPI);
        }
        _dragPosition.x = _dragPosition.y = 0;
    }

protected:
    int pointCount() const { return (int)_points.size(); }

    OFX::Double2DParam* pointParam(int i) const { return _points[i].param; }

    /// A run-time condition for showing point i, tested after its param's (see above).
    virtual bool isPointShown(int /*i*/,
                              double /*time*/) const
    {
        return true;
    }

    /// Set point i to p (already rounded to the pixel) during a drag. firstChange: the first
    /// value set by this drag; dragEnd: pen up. Override it to add the plug-in's bookkeeping to
    /// the first and the last change, in an edit block opened and closed here around the
    /// value change (one undo step per drag; never an edit block over the whole drag, which
    /// spans several actions). The drag state is already reset when dragEnd is true.
    virtual void setPointValue(int i,
                               double /*time*/,
                               const OfxPointD& p,
                               bool /*firstChange*/,
                               bool /*dragEnd*/)
    {
        _points[i].param->setValue(p.x, p.y);
    }

private:
    // overridden functions from OFX::Interact to do things
    virtual bool draw(const OFX::DrawArgs &args) OVERRIDE FINAL;
    virtual bool penMotion(const OFX::PenArgs &args) OVERRIDE FINAL;
    virtual bool penDown(const OFX::PenArgs &args) OVERRIDE FINAL;
    virtual bool penUp(const OFX::PenArgs &args) OVERRIDE FINAL;
    virtual void loseFocus(const FocusArgs &args) OVERRIDE FINAL;

    struct Handle
    {
        explicit Handle(const PointsInteractPoint& p) : config(p), param(NULL), interactive(NULL), hostHandle(false) {}

        PointsInteractPoint config;
        OFX::Double2DParam* param;
        OFX::BooleanParam* interactive;
        bool hostHandle; // the host draws its own handle
    };

    bool isShown(int i,
                 double time) const
    {
        const Handle& h = _points[i];

        return !h.hostHandle && !h.param->getIsSecret() && h.param->getIsEnable() && isPointShown(i, time);
    }

    OfxPointD position(int i,
                       double time) const
    {
        OfxPointD pos;

        if (i == _dragged) {
            pos = _dragPosition;
        } else {
            _points[i].param->getValueAtTime(time, pos.x, pos.y);
        }

        return pos;
    }

    // the first shown point within size + 1 screen pixels of the pen in x and y, -1 if none
    int pick(const OFX::PenArgs &args) const;

    // round to the closest int, 1/10 int, etc
    // this make parameter editing easier
    // pscale is args.pixelScale.x / args.renderScale.x;
    // pscale10 is the power of 10 below pscale
    static double fround(double val,
                         double pscale)
    {
        if ( (pscale < FLT_MIN) || (pscale > DBL_MAX) ) {
            // a zero, negative, denormal or infinite pixel scale (buggy host) would give
            // log10(<= 0), or inf*0
            return val;
        }
        double pscale10 = std::pow( 10., std::floor( std::log10(pscale) ) );

        return pscale10 * std::floor(val / pscale10 + 0.5);
    }

    static OfxPointD fround(const OfxPointD& p,
                            const OfxPointD& pscale)
    {
        OfxPointD r;

        r.x = fround(p.x, pscale.x);
        r.y = fround(p.y, pscale.y);

        return r;
    }

    std::vector<Handle> _points;
    OFX::BooleanParam* _hiDPI;
    int _hovered; // the point under the pen, -1 if none
    int _dragged; // the point being dragged, -1 if none
    OfxPointD _dragPosition; // where it is drawn: the pen position of the last motion
    bool _dragUpdates; // it is set at each pen motion
    bool _dragChanged; // its value was set during this drag
    OFX::InteractCursor _cursor;
    OFX::OverlayDrawModeEnum _drawMode; // set by the descriptor that created the interact, see ofxsOverlayDraw.h
};

inline bool
PointsInteract::draw(const OFX::DrawArgs &args)
{
    std::vector<int> shown;
    for (int i = 0; i < pointCount(); ++i) {
        if ( isShown(i, args.time) ) {
            shown.push_back(i);
        }
    }
    if ( shown.empty() ) {
        return false;
    }

    // all drawing goes through d: OpenGL, or the DrawSuite for V2 overlays
    OverlayDraw d(args, _drawMode);
    if ( !d.canDraw() ) {
        return false; // nothing may be drawn in this action (see OverlayDraw::canDraw())
    }

    bool hiDPI = _hiDPI ? _hiDPI->getValue() : false;
    double screenPixelRatio = hiDPI ? 2 : 1;
#ifdef OFX_EXTENSIONS_NATRON
    screenPixelRatio *= OverlayDraw::hostScreenPixelRatio(args); // 1 if the host does not set it
    hiDPI |= args.screenPixelRatio > 1;
#endif
    screenPixelRatio = d.overlayScreenPixelRatio(screenPixelRatio); // DrawSuite: 1 if <= 0
    TextRenderer::Font font = hiDPI ? TextRenderer::FONT_TIMES_ROMAN_24 : TextRenderer::FONT_HELVETICA_12;

    // the colour and position of each shown point
    std::vector<OfxRGBColourD> colours( shown.size() );
    std::vector<OfxPointD> positions( shown.size() );
    for (size_t k = 0; k < shown.size(); ++k) {
        const PointsInteractPoint& p = _points[shown[k]].config;
        colours[k] = p.colour;
        if (p.hostColour) {
            getSuggestedColour(colours[k]);
        }
        positions[k] = position(shown[k], args.time);
    }
    GLdouble projection[16];
    d.glGetDoublev( GL_PROJECTION_MATRIX, projection);
    GLint viewport[4];
    d.glGetIntegerv(GL_VIEWPORT, viewport);
    OfxPointD shadow; // how much to translate GL_PROJECTION to get exactly one pixel on screen
    // one pixel in the projection; 0 for a degenerate viewport or projection (inf, then
    // inf - inf = NaN in the shadow translations)
    {
        const double denx = projection[0] * viewport[2];
        const double deny = projection[5] * viewport[3];
        shadow.x = (denx != 0.) ? 2. / denx : 0.;
        shadow.y = (deny != 0.) ? 2. / deny : 0.;
    }

    //glPushAttrib(GL_ALL_ATTRIB_BITS); // caller is responsible for protecting attribs
    d.glEnable(GL_LINE_SMOOTH);
    d.glEnable(GL_BLEND);
    d.glHint(GL_LINE_SMOOTH_HINT, GL_DONT_CARE);
    d.glLineWidth(static_cast<GLfloat>(1.5 * screenPixelRatio));
    d.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw everything twice
    // l = 0: shadow
    // l = 1: drawing
    // with the DrawSuite, no shadow (the host's job): only the drawing pass
    for (int l = d.usesDrawSuite() ? 1 : 0; l < 2; ++l) {
        // shadow (uses GL_PROJECTION)
        d.glMatrixMode(GL_PROJECTION);
        int direction = (l == 0) ? 1 : -1;
        // translate (1,-1) pixels
        if ( !d.usesDrawSuite() ) {
            d.glTranslated(direction * shadow.x, -direction * shadow.y, 0);
        }
        d.glMatrixMode(GL_MODELVIEW); // Modelview should be used on Nuke

        // the last point first, so that the first one is on top
        for (size_t k = shown.size(); k-- > 0;) {
            const int i = shown[k];
            const PointsInteractPoint& p = _points[i].config;
            const OfxRGBColourD& color = colours[k];
            const OfxPointD& pos = positions[k];
            const bool active = (i == _dragged) || (_dragged < 0 && i == _hovered);

            d.glPointSize(static_cast<GLfloat>( (float)p.size * screenPixelRatio));
            d.glEnable(GL_POINT_SMOOTH);
            const double darken = 0.5;
            d.glColor3f(static_cast<GLfloat>(color.r * l * darken), static_cast<GLfloat>(color.g * l * darken), static_cast<GLfloat>(color.b * l * darken));
            drawPointTrajectory(d, _points[i].param);

            d.glDisable(GL_POINT_SMOOTH);
            if (active) {
                d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            } else {
                d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
            }
            d.glBegin(GL_POINTS);
            d.glVertex2d(pos.x, pos.y);
            d.glEnd();
            if ( !p.label.empty() ) {
                // the label is p.size screen pixels above the point, in canonical units
                // (kOfxInteractPropPixelScale: "The size of a real screen pixel under the interact's
                // canonical projection"; the screen pixel ratio is for framebuffer sizes such as
                // glPointSize, so it is not applied here, as for the handles)
                d.bitmapString( pos.x, pos.y + p.size * args.pixelScale.y, p.label.c_str(), font );
            }
        }
    }

    //glPopAttrib();

    return true;
} // draw

inline int
PointsInteract::pick(const OFX::PenArgs &args) const
{
    const OfxPointD& pscale = args.pixelScale;
    // pen position is in canonical coords
    const OfxPointD &penPos = args.penPosition;

    for (int i = 0; i < pointCount(); ++i) {
        if ( !isShown(i, args.time) ) {
            continue;
        }
        const OfxPointD pos = position(i, args.time);
        const double tolerance = _points[i].config.size + 1.;
        if ( ( std::fabs(penPos.x - pos.x) <= tolerance * pscale.x) &&
             ( std::fabs(penPos.y - pos.y) <= tolerance * pscale.y) ) {
            return i;
        }
    }

    return -1;
}

// overridden functions from OFX::Interact to do things
inline bool
PointsInteract::penMotion(const OFX::PenArgs &args)
{
    if ( (_dragged >= 0) && !isShown(_dragged, args.time) ) {
        // hidden or disabled while dragged: abandon the drag
        _dragged = -1;
        requestRedraw();
    }
    if (_dragged >= 0) {
        _dragPosition = args.penPosition;
        if (_dragUpdates) {
            setPointValue(_dragged, args.time, fround(_dragPosition, args.pixelScale), !_dragChanged, false);
            _dragChanged = true;
        }
        requestRedraw();
        _cursor.set(this, kOfxsCursorMove);

        return true;
    }

    // are we on a point: become 'poised'
    const int hovered = pick(args);
    if (hovered != _hovered) {
        _hovered = hovered;
        requestRedraw();
    }

    // report the cursor; the host reads it whether or not the event is
    // trapped, so the return value is unchanged
    if (_hovered < 0) {
        _cursor.release(this);
    } else {
        _cursor.set(this, kOfxsCursorMove);
    }

    return false;
} // >::penMotion

inline bool
PointsInteract::penDown(const OFX::PenArgs &args)
{
    penMotion(args);
    if ( (_dragged >= 0) || (_hovered < 0) ) {
        return false;
    }
    const Handle& h = _points[_hovered];
    bool updates = h.config.updateWhileDragging;
    if (h.interactive) {
        h.interactive->getValueAtTime(args.time, updates);
    }
    _dragged = _hovered;
    _dragPosition = args.penPosition;
    _dragUpdates = updates;
    _dragChanged = false;

    return true;
}

inline bool
PointsInteract::penUp(const OFX::PenArgs &args)
{
    if (_dragged < 0) {
        return false;
    }
    const int i = _dragged;
    const bool firstChange = !_dragChanged;
    // with update while dragging, as the last pen motion would: the pen-up position;
    // otherwise the position of the last pen motion, where the point is drawn
    const OfxPointD pos = fround(_dragUpdates ? args.penPosition : _dragPosition, args.pixelScale);
    // leave the drag state first, so that a host error doesn't leave the overlay
    // dragging; penMotion below then only refreshes the hover state
    _dragged = -1;
    if ( !isShown(i, args.time) ) {
        requestRedraw();

        return false;
    }
    setPointValue(i, args.time, pos, firstChange, true);
    penMotion(args);
    requestRedraw();

    return true;
}

/** @brief Called when the interact is loses input focus */
inline void
PointsInteract::loseFocus(const OFX::FocusArgs & /*args*/)
{
    if ( (_dragged >= 0) || (_hovered >= 0) ) {
        _dragged = -1;
        _hovered = -1;
        requestRedraw();
    }
    // the host restores its own cursor
    _cursor.release(this);
}

/// V2 descriptor of a PointsInteract subclass INSTANCE: draws with the DrawSuite when the host
/// provides it, else with OpenGL. Register it with
/// ofxsSetOverlayInteractDescriptorV2(desc, new PointsOverlayDescriptorV2<INSTANCE>)
template <class INSTANCE>
class PointsOverlayDescriptorV2
    : public OFX::DefaultEffectOverlayDescriptorV2<PointsOverlayDescriptorV2<INSTANCE>, INSTANCE>
{
};

/// template for a generic position interact: a PointsInteract with one point.
/*
   The ParamName class must define two static functions (using const char* directly
   as template parameter is not reliable):
   - name() returns the OFX name of the position parameter;
   - interactiveName() returns the name of a boolean "Interactive Update" parameter,
     or NULL if there is none.
   namespace {
   struct MyPositionInteractParam {
     static const char *name() { return kMyName; }
     static const char *interactiveName() { return kParamInteractive; } // or NULL
   };
   }

   // the describe() function should include the declaration of the interact.
   // Prefer V2, which draws with the DrawSuite when the host has one and always keeps an
   // OpenGL (V1) entry:
   OFX::ofxsSetOverlayInteractDescriptorV2(desc, new PositionOverlayDescriptorV2<MyPositionInteractParam>);
   // or, OpenGL only:
   desc.setOverlayInteractDescriptor(new PositionOverlayDescriptor<MyPositionInteractParam>);

   // The position param should be defined in describeInContext() as follows:
   Double2DParamDescriptor* position = desc.defineDouble2DParam(kMyName);
   position->setLabel(kMyLabel, kMyLabel, kMyLabel);
   position->setHint(kMyHint);
   position->setDoubleType(eDoubleTypeXYAbsolute);
   position->setDefaultCoordinateSystem(eCoordinatesNormalised);
   position->setDefault(0.5, 0.5);
   if (page) {
       page->addChild(*position);
   }
 */
template<typename ParamName>
class PositionInteract
    : public PointsInteract
{
public:
    PositionInteract(OfxInteractHandle handle,
                     OFX::ImageEffect* effect)
        : PointsInteract( handle, effect, points() )
    {
    }

private:
    // the point: labelled with its param name, in the host's colour
    static std::vector<PointsInteractPoint> points()
    {
        PointsInteractPoint p( ParamName::name() );

        p.label = ParamName::name();
        p.hostColour = true;
        if ( ParamName::interactiveName() ) {
            p.interactiveName = ParamName::interactiveName();
        }

        return std::vector<PointsInteractPoint>(1, p);
    }
};

template <typename ParamName>
class PositionOverlayDescriptor
    : public OFX::DefaultEffectOverlayDescriptor<PositionOverlayDescriptor<ParamName>, PositionInteract<ParamName> >
{
};

/// Draws with the DrawSuite when the host provides it; register with
/// ofxsSetOverlayInteractDescriptorV2(desc, new PositionOverlayDescriptorV2<ParamName>)
template <typename ParamName>
class PositionOverlayDescriptorV2
    : public OFX::DefaultEffectOverlayDescriptorV2<PositionOverlayDescriptorV2<ParamName>, PositionInteract<ParamName> >
{
};
} // namespace OFX

#endif /* defined(openfx_supportext_ofxsPositionInteract_h) */
