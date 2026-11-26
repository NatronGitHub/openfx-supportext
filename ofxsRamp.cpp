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
 * OFX generic ramp interact with 2 points
 */

#include "ofxsRamp.h"

#include <cmath>
#include <cfloat> // DBL_MAX

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#define POINT_TOLERANCE 6
#define POINT_SIZE 5

#include "ofxsPositionInteract.h"

// the parameter ranges and default coordinate systems left to Support (DaVinci Resolve clamps params without a range)
#ifndef OFXS_SUPPORT_HAS_SPEC_PARAM_DEFAULTS
#error "SupportExt needs NatronGitHub openfx 96a53ac4 (2026-11-26, openfx #354) or later, whose Support library sets the spec param defaults"
#endif

namespace OFX {
static inline
void
crossProd(const Ofx3DPointD& u,
          const Ofx3DPointD& v,
          Ofx3DPointD* w)
{
    w->x = u.y * v.z - u.z * v.y;
    w->y = u.z * v.x - u.x * v.z;
    w->z = u.x * v.y - u.y * v.x;
}

// round to the closest int, 1/10 int, etc
// this make parameter editing easier
// pscale is args.pixelScale.x / args.renderScale.x;
// pscale10 is the power of 10 below pscale
static inline
double
fround(double val,
       double pscale)
{
    if ( (pscale < FLT_MIN) || (pscale > DBL_MAX) ) {
        // a zero, negative, denormal or infinite pixel scale (buggy host) would give log10(<= 0),
        // or inf*0
        return val;
    }
    double pscale10 = std::pow( 10., std::floor( std::log10(pscale) ) );

    return pscale10 * std::floor(val / pscale10 + 0.5);
}

bool
RampInteractHelper::draw(const DrawArgs &args)
{
    const double time = args.time;

    if ( !_interactOpen->getValueAtTime(time) ) {
        return false;
    }
    RampTypeEnum type = (RampTypeEnum)_type->getValueAtTime(time);
    bool noramp = (type == eRampTypeNone);
    if (noramp) {
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

    OfxRGBColourD color = { 0.8, 0.8, 0.8 };
    _interact->getSuggestedColour(color);
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

    OfxPointD p[2];
    if (_state == eInteractStateIdle) {
        _point0->getValueAtTime(time, p[0].x, p[0].y);
        _point1->getValueAtTime(time, p[1].x, p[1].y);
    } else {
        p[0] = _point0DragPos;
        p[1] = _point1DragPos;
    }

    ///Clamp points to the rod
    OfxRectD rod = _dstClip->getRegionOfDefinition(time);

    // A line is represented by a 3-vector (a,b,c), and its equation is (a,b,c).(x,y,1)=0
    // The intersection of two lines is given by their cross-product: (wx,wy,w) = (a,b,c)x(a',b',c').
    // The line passing through 2 points is obtained by their cross-product: (a,b,c) = (x,y,1)x(x',y',1)
    // The two lines passing through p0 and p1 and orthogonal to p0p1 are:
    // (p1.x - p0.x, p1.y - p0.y, -p0.x*(p1.x-p0.x) - p0.y*(p1.y-p0.y)) passing through p0
    // (p1.x - p0.x, p1.y - p0.y, -p1.x*(p1.x-p0.x) - p1.y*(p1.y-p0.y)) passing through p1
    // the four lines defining the RoD are:
    // (1,0,-x1) [x=x1]
    // (1,0,-x2) [x=x2]
    // (0,1,-y1) [x=y1]
    // (0,1,-y2) [y=y2]
    const Ofx3DPointD linex1 = {1, 0, -rod.x1};
    const Ofx3DPointD linex2 = {1, 0, -rod.x2};
    const Ofx3DPointD liney1 = {0, 1, -rod.y1};
    const Ofx3DPointD liney2 = {0, 1, -rod.y2};
    Ofx3DPointD line[2];
    OfxPointD pline1[2];
    OfxPointD pline2[2];

    // line passing through p0
    line[0].x = p[1].x - p[0].x;
    line[0].y = p[1].y - p[0].y;
    line[0].z = -p[0].x * (p[1].x - p[0].x) - p[0].y * (p[1].y - p[0].y);
    // line passing through p1
    line[1].x = p[1].x - p[0].x;
    line[1].y = p[1].y - p[0].y;
    line[1].z = -p[1].x * (p[1].x - p[0].x) - p[1].y * (p[1].y - p[0].y);
    // for each line...
    for (int i = 0; i < 2; ++i) {
        // compute the intersection with the four lines
        Ofx3DPointD interx1, interx2, intery1, intery2;

        crossProd(line[i], linex1, &interx1);
        crossProd(line[i], linex2, &interx2);
        crossProd(line[i], liney1, &intery1);
        crossProd(line[i], liney2, &intery2);
        if ( (interx1.z != 0.) && (interx2.z != 0.) ) {
            // initialize pline1 to the intersection with x=x1, pline2 with x=x2
            pline1[i].x = interx1.x / interx1.z;
            pline1[i].y = interx1.y / interx1.z;
            pline2[i].x = interx2.x / interx2.z;
            pline2[i].y = interx2.y / interx2.z;
            if ( ( (pline1[i].y > rod.y2) && (pline2[i].y > rod.y2) ) ||
                 ( ( pline1[i].y < rod.y1) && ( pline2[i].y < rod.y1) ) ) {
                // line doesn't intersect rectangle, don't draw it.
                pline1[i].x = p[i].x;
                pline1[i].y = p[i].y;
                pline2[i].x = p[i].x;
                pline2[i].y = p[i].y;
            } else if (pline1[i].y < pline2[i].y) {
                // y is an increasing function of x, test the two other endpoints
                if ( (intery1.z != 0.) && (intery1.x / intery1.z > pline1[i].x) ) {
                    pline1[i].x = intery1.x / intery1.z;
                    pline1[i].y = intery1.y / intery1.z;
                }
                if ( (intery2.z != 0.) && (intery2.x / intery2.z < pline2[i].x) ) {
                    pline2[i].x = intery2.x / intery2.z;
                    pline2[i].y = intery2.y / intery2.z;
                }
            } else {
                // y is an decreasing function of x, test the two other endpoints
                if ( (intery2.z != 0.) && (intery2.x / intery2.z > pline1[i].x) ) {
                    pline1[i].x = intery2.x / intery2.z;
                    pline1[i].y = intery2.y / intery2.z;
                }
                if ( (intery1.z != 0.) && (intery1.x / intery1.z < pline2[i].x) ) {
                    pline2[i].x = intery1.x / intery1.z;
                    pline2[i].y = intery1.y / intery1.z;
                }
            }
        } else if ( (intery1.z == 0.) || (intery2.z == 0.) ) {
            // degenerate line (point0 == point1: line[i] is 0): don't draw it (the divisions
            // below would give 0/0)
            pline1[i].x = p[i].x;
            pline1[i].y = p[i].y;
            pline2[i].x = p[i].x;
            pline2[i].y = p[i].y;
        } else {
            // initialize pline1 to the intersection with y=y1, pline2 with y=y2
            pline1[i].x = intery1.x / intery1.z;
            pline1[i].y = intery1.y / intery1.z;
            pline2[i].x = intery2.x / intery2.z;
            pline2[i].y = intery2.y / intery2.z;
            if ( ( (pline1[i].x > rod.x2) && (pline2[i].x > rod.x2) ) ||
                 ( ( pline1[i].x < rod.x1) && ( pline2[i].x < rod.x1) ) ) {
                // line doesn't intersect rectangle, don't draw it.
                pline1[i].x = p[i].x;
                pline1[i].y = p[i].y;
                pline2[i].x = p[i].x;
                pline2[i].y = p[i].y;
            }
        }
    }

    //glPushAttrib(GL_ALL_ATTRIB_BITS); // caller is responsible for protecting attribs

    d.glEnable(GL_LINE_SMOOTH);
    d.glDisable(GL_POINT_SMOOTH);
    d.glEnable(GL_BLEND);
    d.glHint(GL_LINE_SMOOTH_HINT, GL_DONT_CARE);
    d.glLineWidth(static_cast<GLfloat>(1.5 * screenPixelRatio));
    d.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    d.glLineStipple(2, 0xAAAA);

    d.glPointSize(static_cast<GLfloat>(POINT_SIZE * screenPixelRatio));

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

        
        for (int i = 0; i < 2; ++i) {
            d.glDisable(GL_LINE_STIPPLE);
            d.glEnable(GL_POINT_SMOOTH);
            const double darken = 0.5;
            d.glColor3f(static_cast<GLfloat>(color.r * l * darken), static_cast<GLfloat>(color.g * l * darken), static_cast<GLfloat>(color.b * l * darken));
            drawPointTrajectory(d, i ==0 ? _point0 : _point1);

            d.glEnable(GL_LINE_STIPPLE);
            d.glDisable(GL_POINT_SMOOTH);
            bool dragging = _state == (i == 0 ? eInteractStateDraggingPoint0 : eInteractStateDraggingPoint1);
            d.glBegin(GL_POINTS);
            if (dragging) {
                d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            } else {
                d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
            }
            d.glVertex2d(p[i].x, p[i].y);
            d.glEnd();

            d.glBegin(GL_LINES);
            d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
            d.glVertex2d(pline1[i].x, pline1[i].y);
            d.glVertex2d(pline2[i].x, pline2[i].y);
            d.glEnd();

            // the label is POINT_SIZE screen pixels above the point, in canonical units (the screen
            // pixel ratio is for framebuffer sizes such as glPointSize)
            d.bitmapString(p[i].x, p[i].y + POINT_SIZE * args.pixelScale.y, i == 0 ? kParamRampPoint0Label : kParamRampPoint1Label, font);
        }
    }

    //glPopAttrib();

    return true;
} // RampInteractHelper::draw

static bool
isNearby(const OfxPointD& p,
         double x,
         double y,
         double tolerance,
         const OfxPointD& pscale)
{
    return std::fabs(p.x - x) <= tolerance * pscale.x &&  std::fabs(p.y - y) <= tolerance * pscale.y;
}

bool
RampInteractHelper::penMotion(const PenArgs &args)
{
    const double time = args.time;

    if ( !_interactOpen->getValueAtTime(time) ) {
        // the overlay is hidden: give the cursor back if we had set it
        _cursor.release(_interact);

        return false;
    }
    RampTypeEnum type = (RampTypeEnum)_type->getValueAtTime(time);
    bool noramp = (type == eRampTypeNone);
    if (noramp) {
        _cursor.release(_interact);

        return false;
    }

    const OfxPointD &pscale = args.pixelScale;
    OfxPointD p0, p1;
    if (_state != eInteractStateIdle) {
        p0 = _point0DragPos;
        p1 = _point1DragPos;
    } else {
        _point0->getValueAtTime(time, p0.x, p0.y);
        _point1->getValueAtTime(time, p1.x, p1.y);
    }
    bool valuesChanged = false;
    OfxPointD delta;
    delta.x = args.penPosition.x - _lastMousePos.x;
    delta.y = args.penPosition.y - _lastMousePos.y;

    if (_state == eInteractStateDraggingPoint0) {
        _point0DragPos.x += delta.x;
        _point0DragPos.y += delta.y;
        valuesChanged = true;
    } else if (_state == eInteractStateDraggingPoint1) {
        _point1DragPos.x += delta.x;
        _point1DragPos.y += delta.y;
        valuesChanged = true;
    }

    if ( (_state != eInteractStateIdle) && _interactiveDrag && valuesChanged ) {
        if (_state == eInteractStateDraggingPoint0) {
            _point0->setValue( fround(_point0DragPos.x, pscale.x), fround(_point0DragPos.y, pscale.y) );
        } else if (_state == eInteractStateDraggingPoint1) {
            _point1->setValue( fround(_point1DragPos.x, pscale.x), fround(_point1DragPos.y, pscale.y) );
        }
    }

    if (valuesChanged) {
        _interact->requestRedraw();
    }

    _lastMousePos = args.penPosition;

    // report the cursor; the host reads it whether or not the event is
    // trapped, so the return value is unchanged
    if ( (_state != eInteractStateIdle) ||
         isNearby(args.penPosition, p0.x, p0.y, POINT_TOLERANCE, pscale) ||
         isNearby(args.penPosition, p1.x, p1.y, POINT_TOLERANCE, pscale) ) {
        _cursor.set(_interact, kOfxsCursorMove);
    } else {
        _cursor.release(_interact);
    }

    return valuesChanged;
} // RampInteractHelper::penMotion

bool
RampInteractHelper::penDown(const PenArgs &args)
{
    const double time = args.time;

    if ( !_interactOpen->getValueAtTime(time) ) {
        return false;
    }
    RampTypeEnum type = (RampTypeEnum)_type->getValueAtTime(time);
    bool noramp = (type == eRampTypeNone);
    if (noramp) {
        return false;
    }

    const OfxPointD &pscale = args.pixelScale;
    OfxPointD p0, p1;
    if (_state != eInteractStateIdle) {
        p0 = _point0DragPos;
        p1 = _point1DragPos;
    } else {
        _point0->getValueAtTime(time, p0.x, p0.y);
        _point1->getValueAtTime(time, p1.x, p1.y);
        _interactive->getValueAtTime(time, _interactiveDrag);
    }

    bool didSomething = false;

    if ( isNearby(args.penPosition, p0.x, p0.y, POINT_TOLERANCE, pscale) ) {
        _state = eInteractStateDraggingPoint0;
        didSomething = true;
    } else if ( isNearby(args.penPosition, p1.x, p1.y, POINT_TOLERANCE, pscale) ) {
        _state = eInteractStateDraggingPoint1;
        didSomething = true;
    } else {
        _state = eInteractStateIdle;
    }

    _point0DragPos = p0;
    _point1DragPos = p1;
    _lastMousePos = args.penPosition;

    if (didSomething) {
        _interact->requestRedraw();
    }

    return didSomething;
}

bool
RampInteractHelper::penUp(const PenArgs &args)
{
    const double time = args.time;

    if ( !_interactOpen->getValueAtTime(time) ) {
        return false;
    }
    RampTypeEnum type = (RampTypeEnum)_type->getValueAtTime(time);
    bool noramp = (type == eRampTypeNone);
    if (noramp) {
        return false;
    }

    bool didSomething = false;
    const OfxPointD &pscale = args.pixelScale;
    // leave the drag state first, so that a setValue that throws doesn't leave the overlay dragging
    const InteractState state = _state;
    _state = eInteractStateIdle;

    if ( !_interactiveDrag && (state != eInteractStateIdle) ) {
        // round newx/y to the closest int, 1/10 int, etc: this makes parameter editing easier
        if (state == eInteractStateDraggingPoint0) {
            _point0->setValue( fround(_point0DragPos.x, pscale.x), fround(_point0DragPos.y, pscale.y) );
            didSomething = true;
        } else if (state == eInteractStateDraggingPoint1) {
            _point1->setValue( fround(_point1DragPos.x, pscale.x), fround(_point1DragPos.y, pscale.y) );
            didSomething = true;
        }
    } else if (state != eInteractStateIdle) {
        _interact->requestRedraw();
    }

    return didSomething;
}

/** @brief Called when the interact is loses input focus */
void
RampInteractHelper::loseFocus(const FocusArgs & /*args*/)
{
    _interactiveDrag = false;
    _state = eInteractStateIdle;
    // the host restores its own cursor
    _cursor.release(_interact);
}

void
ofxsRampDescribeParamType(ImageEffectDescriptor &desc,
                          PageParamDescriptor *page,
                          GroupParamDescriptor *group,
                          RampTypeEnum defaultType,
                          bool oldParams)
{
    // type
    {
        ChoiceParamDescriptor* param = desc.defineChoiceParam(oldParams ? kParamRampTypeOld : kParamRampType);
        param->setLabelAndHint(kParamRampTypeLabel);
        assert(param->getNOptions() == eRampTypeLinear);
        param->appendOption(kParamRampTypeOptionLinear);
        assert(param->getNOptions() == eRampTypePLinear);
        param->appendOption(kParamRampTypeOptionPLinear);
        assert(param->getNOptions() == eRampTypeEaseIn);
        param->appendOption(kParamRampTypeOptionEaseIn);
        assert(param->getNOptions() == eRampTypeEaseOut);
        param->appendOption(kParamRampTypeOptionEaseOut);
        assert(param->getNOptions() == eRampTypeSmooth);
        param->appendOption(kParamRampTypeOptionSmooth);
        assert(param->getNOptions() == eRampTypeNone);
        param->appendOption(kParamRampTypeOptionNone);
        param->setDefault(defaultType);
        param->setAnimates(false);
        if (group) {
            param->setParent(*group);
        }
        if (page) {
            page->addChild(*param);
        }
    }
} // ofxsRampDescribeParamType

void
ofxsRampDescribeParamPoint0(ImageEffectDescriptor &desc,
                            PageParamDescriptor *page,
                            GroupParamDescriptor *group,
                            bool oldParams)
{
    // point0
    {
        Double2DParamDescriptor* param = desc.defineDouble2DParam(oldParams ? kParamRampPoint0Old : kParamRampPoint0);
        param->setLabel(kParamRampPoint0Label);
        param->setDoubleType(eDoubleTypeXYAbsolute);
        param->setDefault(100., 100.);
        param->setDisplayRange(-10000, -10000, 10000, 10000); // Resolve requires display range or values are clamped to (-1,1)
        //param->setUseHostNativeOverlayHandle(true);
        if (group) {
            param->setParent(*group);
        }
        if (page) {
            page->addChild(*param);
        }
    }
} // ofxsRampDescribeParamPoint0

void
ofxsRampDescribeParamColor0(ImageEffectDescriptor &desc,
                            PageParamDescriptor *page,
                            GroupParamDescriptor *group,
                            bool oldParams)
{
    // color0
    {
        RGBAParamDescriptor* param = desc.defineRGBAParam(oldParams ? kParamRampColor0Old : kParamRampColor0);
        param->setLabel(kParamRampColor0Label);
        param->setDefault(0, 0, 0, 0);
        if (group) {
            param->setParent(*group);
        }
        if (page) {
            page->addChild(*param);
        }
    }
} // ofxsRampDescribeParamColor0

void
ofxsRampDescribeParamPoint1(ImageEffectDescriptor &desc,
                            PageParamDescriptor *page,
                            GroupParamDescriptor *group,
                            bool oldParams)
{
    // point1
    {
        Double2DParamDescriptor* param = desc.defineDouble2DParam(oldParams ? kParamRampPoint1Old : kParamRampPoint1);
        param->setLabel(kParamRampPoint1Label);
        param->setDoubleType(eDoubleTypeXYAbsolute);
        param->setDefault(100., 200.);
        param->setDisplayRange(-10000, -10000, 10000, 10000); // Resolve requires display range or values are clamped to (-1,1)
        //param->setUseHostNativeOverlayHandle(true);
        if (group) {
            param->setParent(*group);
        }
        if (page) {
            page->addChild(*param);
        }
    }
} // ofxsRampDescribeParamPoint1

void
ofxsRampDescribeParamColor1(ImageEffectDescriptor &desc,
                            PageParamDescriptor *page,
                            GroupParamDescriptor *group,
                            bool oldParams)
{
    // color1
    {
        RGBAParamDescriptor* param = desc.defineRGBAParam(oldParams ? kParamRampColor1Old : kParamRampColor1);
        param->setLabel(kParamRampColor1Label);
        param->setDefault(1., 1., 1., 1. );
        if (group) {
            param->setParent(*group);
        }
        if (page) {
            page->addChild(*param);
        }
    }
} // ofxsRampDescribeParamColor1

void
ofxsRampDescribeParamsInteract(ImageEffectDescriptor &desc,
                               PageParamDescriptor *page,
                               GroupParamDescriptor *group,
                               bool isOpen,
                               bool oldParams)
{
    // interactOpen
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamRampInteractOpen);
        param->setLabelAndHint(kParamRampInteractOpenLabel);
        param->setDefault(isOpen); // open by default
        param->setIsSecretAndDisabled(true); // secret by default, but this can be changed for specific hosts
        param->setAnimates(false);
        if (group) {
            param->setParent(*group);
        }
        if (page) {
            page->addChild(*param);
        }
    }


    // interactive
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(oldParams ? kParamRampInteractiveOld : kParamRampInteractive);
        param->setLabelAndHint(kParamRampInteractiveLabel);
        param->setEvaluateOnChange(false);
        if (group) {
            param->setParent(*group);
        }
        if (page) {
            page->addChild(*param);
        }
    }

    // HiDPI
    hiDPIDescribeParams(desc, group, page);
} // ofxsRampDescribeParamsInteract

void
ofxsRampDescribeParams(ImageEffectDescriptor &desc,
                       PageParamDescriptor *page,
                       GroupParamDescriptor *group,
                       RampTypeEnum defaultType,
                       bool isOpen,
                       bool oldParams)
{
    ofxsRampDescribeParamType(desc, page, group, defaultType, oldParams);
    ofxsRampDescribeParamPoint0(desc, page, group, oldParams);
    ofxsRampDescribeParamColor0(desc, page, group, oldParams);
    ofxsRampDescribeParamPoint1(desc, page, group, oldParams);
    ofxsRampDescribeParamColor1(desc, page, group, oldParams);
    ofxsRampDescribeParamsInteract(desc, page, group, isOpen, oldParams);
} // ofxsRampDescribeParams
} // namespace OFX
