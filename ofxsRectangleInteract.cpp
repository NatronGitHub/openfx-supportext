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
 * OFX generic rectangle interact with 4 corner points + center point and 4 mid-points.
 * You can use it to define any rectangle in an image resizable by the user.
 */

#include "ofxsRectangleInteract.h"
#include "ofxsOverlayDraw.h"
#include <cmath>
#include <cfloat> // FLT_MIN


#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#define POINT_SIZE 5
#define POINT_TOLERANCE 6
#define CROSS_SIZE 7
#define HANDLE_SIZE 6

using namespace OFX;

using OFX::RectangleInteract;

static bool
isNearby(const OfxPointD & p,
         double x,
         double y,
         double tolerance,
         const OfxPointD & pscale)
{
    return std::fabs(p.x - x) <= tolerance * pscale.x &&  std::fabs(p.y - y) <= tolerance * pscale.y;
}

// round to the closest int, 1/10 int, etc
// this make parameter editing easier
// pscale is args.pixelScale.x / args.renderScale.x;
// pscale10 is the power of 10 below pscale
static double
fround(double val,
       double pscale)
{
    if (pscale < FLT_MIN) {
        // a zero, negative or denormal pixel scale (buggy host) would give log10(<= 0)
        return val;
    }
    double pscale10 = std::pow( 10., std::floor( std::log10(pscale) ) );

    return pscale10 * std::floor(val / pscale10 + 0.5);
}

// round a size like fround, but keep the 1-unit minimum that penMotion enforces
// (when zoomed out, fround(1, 10) would give 0)
static double
froundSize(double val,
           double pscale)
{
    double r = fround(val, pscale);

    return (r < 1.) ? 1. : r;
}

// rotate point p by the angle (given by its cosine and sine) around center c
static inline OfxPointD
rotatePoint(const OfxPointD & p,
            const OfxPointD & c,
            double cosa,
            double sina)
{
    OfxPointD ret;
    double dx = p.x - c.x;
    double dy = p.y - c.y;

    ret.x = c.x + cosa * dx - sina * dy;
    ret.y = c.y + sina * dx + cosa * dy;

    return ret;
}

// get the pen position in the frame of the (unrotated) rectangle
static inline OfxPointD
penPositionInRectangleFrame(const OfxPointD & penPosition,
                            const OfxPointD & center,
                            double angle)
{
    if (angle == 0.) {
        return penPosition;
    }
    double a = -angle * OFXS_PI / 180.;

    return rotatePoint(penPosition, center, std::cos(a), std::sin(a));
}

// is the pen near the handle (x,y)? The handle position is given in the frame of the (unrotated)
// rectangle, and is rotated into canonical coordinates, where the tolerance is defined.
static inline bool
isHandleNearby(const OfxPointD & penPosition,
               double x,
               double y,
               const OfxPointD & center,
               double angle,
               const OfxPointD & pscale)
{
    if (angle != 0.) {
        OfxPointD p = { x, y };
        double a = angle * OFXS_PI / 180.;
        p = rotatePoint(p, center, std::cos(a), std::sin(a));
        x = p.x;
        y = p.y;
    }

    return isNearby(penPosition, x, y, POINT_TOLERANCE, pscale);
}

void
RectangleInteract::getRectangle(OfxTime time,
                                double* x1,
                                double* y1,
                                double* w,
                                double* h,
                                OfxPointD* center,
                                double* angle) const
{
    if (_mouseState != eMouseStateIdle) {
        *x1 = _btmLeftDragPos.x;
        *y1 = _btmLeftDragPos.y;
        *w = _sizeDrag.x;
        *h = _sizeDrag.y;
        // while dragging, the rotation center is the center at the time the pen was pressed
        *center = _centerDrag;
        *angle = _angleDrag;
    } else {
        _btmLeft->getValueAtTime(time, *x1, *y1);
        _size->getValueAtTime(time, *w, *h);
        center->x = *x1 + *w / 2;
        center->y = *y1 + *h / 2;
        *angle = _angle ? _angle->getValueAtTime(time) : 0.;
    }
}

static void
drawPoint(OverlayDraw& d,
          const OfxRGBColourD &color,
          bool draw,
          double x,
          double y,
          RectangleInteract::DrawStateEnum id,
          RectangleInteract::DrawStateEnum ds,
          bool keepAR,
          int l)
{
    if (draw) {
        if (ds == id) {
            if (keepAR) {
                d.glColor3f(1.f * l, 0.f * l, 0.f * l);
            } else {
                d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            }
        } else {
            d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
        }
        d.glVertex2d(x, y);
    }
}

bool
RectangleInteract::draw(const DrawArgs &args)
{
    if ( _btmLeft->getIsSecret() || _size->getIsSecret() ||
         !_btmLeft->getIsEnable() || !_size->getIsEnable() ||
         ( _enable && !_enable->getValueAtTime(args.time) ) ) {
        return false;
    }

    // all drawing goes through d: OpenGL, or the DrawSuite for V2 overlays
    OverlayDraw d(args, _drawMode);
    if ( !d.canDraw() ) {
        return false; // registered as V2 but no draw context: OpenGL is not allowed
    }

    bool hiDPI = _hiDPI ? _hiDPI->getValue() : false;
    double screenPixelRatio = hiDPI ? 2 : 1;
#ifdef OFX_EXTENSIONS_NATRON
    screenPixelRatio *= OverlayDraw::hostScreenPixelRatio(args); // 1 if the host does not set it
    //hiDPI |= args.screenPixelRatio > 1;
#endif
    screenPixelRatio = d.overlayScreenPixelRatio(screenPixelRatio); // DrawSuite: 1 if <= 0
    //TextRenderer::Font font = hiDPI ? TextRenderer::FONT_TIMES_ROMAN_24 : TextRenderer::FONT_HELVETICA_12;

    OfxRGBColourD color = { 0.8, 0.8, 0.8 };
    getSuggestedColour(color);
    const OfxPointD& pscale = args.pixelScale;
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

    double x1, y1, w, h;
    OfxPointD center;
    double angle;
    getRectangle(args.time, &x1, &y1, &w, &h, &center, &angle);
    double x2 = x1 + w;
    double y2 = y1 + h;
    double xc = x1 + w / 2;
    double yc = y1 + h / 2;
    const bool keepAR = shiftDown();
    const bool centered = ctrlDown();

    //glPushAttrib(GL_ALL_ATTRIB_BITS); // caller is responsible for protecting attribs
    aboutToCheckInteractivity(args.time);

    d.glDisable(GL_LINE_STIPPLE);
    d.glEnable(GL_LINE_SMOOTH);
    d.glDisable(GL_POINT_SMOOTH);
    d.glEnable(GL_BLEND);
    d.glHint(GL_LINE_SMOOTH_HINT, GL_DONT_CARE);
    d.glLineWidth(static_cast<GLfloat>(1.5 * screenPixelRatio));
    d.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw everything twice
    // l = 0: shadow
    // l = 1: drawing
    for (int l = 0; l < 2; ++l) {
        // shadow (uses GL_PROJECTION)
        d.glMatrixMode(GL_PROJECTION);
        int direction = (l == 0) ? 1 : -1;
        // translate (1,-1) pixels
        d.glTranslated(direction * shadow.x, -direction * shadow.y, 0);
        d.glMatrixMode(GL_MODELVIEW); // Modelview should be used on Nuke

        if (angle != 0.) {
            // rotate the rectangle around its center (in canonical coordinates)
            d.glPushMatrix();
            d.glTranslated(center.x, center.y, 0);
            d.glRotated(angle, 0, 0, 1);
            d.glTranslated(-center.x, -center.y, 0);
        }

        d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );

        d.glBegin(GL_LINE_LOOP);
        d.glVertex2d(x1, y1);
        d.glVertex2d(x1, y2);
        d.glVertex2d(x2, y2);
        d.glVertex2d(x2, y1);
        d.glEnd();

        d.glPointSize(static_cast<GLfloat>(POINT_SIZE * screenPixelRatio));
        d.glBegin(GL_POINTS);
        drawPoint(d, color, allowBtmLeftInteraction(),  x1, y1, eDrawStateHoveringBtmLeft,  _drawState, keepAR, l);
        drawPoint(d, color, allowMidLeftInteraction(),  x1, yc, eDrawStateHoveringMidLeft,  _drawState, false,  l);
        drawPoint(d, color, allowTopLeftInteraction(),  x1, y2, eDrawStateHoveringTopLeft,  _drawState, keepAR, l);
        drawPoint(d, color, allowBtmMidInteraction(),   xc, y1, eDrawStateHoveringBtmMid,   _drawState, false,  l);
        drawPoint(d, color, allowCenterInteraction(),   xc, yc, eDrawStateHoveringCenter,   _drawState, false,  l);
        drawPoint(d, color, allowTopMidInteraction(),   xc, y2, eDrawStateHoveringTopMid,   _drawState, false,  l);
        drawPoint(d, color, allowBtmRightInteraction(), x2, y1, eDrawStateHoveringBtmRight, _drawState, keepAR, l);
        drawPoint(d, color, allowMidRightInteraction(), x2, yc, eDrawStateHoveringMidRight, _drawState, false,  l);
        drawPoint(d, color, allowTopRightInteraction(), x2, y2, eDrawStateHoveringTopRight, _drawState, keepAR, l);
        d.glEnd();
        d.glPointSize(1);

        ///draw center cross hair
        d.glBegin(GL_LINES);
        if ( (_drawState == eDrawStateHoveringCenter) || ( centered && (_drawState != eDrawStateInactive) ) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else if ( !allowCenterInteraction() ) {
            d.glColor3f( (float)(color.r / 2) * l, (float)(color.g / 2) * l, (float)(color.b / 2) * l );
        } else {
            d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
        }
        d.glVertex2d(xc - CROSS_SIZE * pscale.x, yc);
        d.glVertex2d(xc + CROSS_SIZE * pscale.x, yc);
        d.glVertex2d(xc, yc - CROSS_SIZE * pscale.y);
        d.glVertex2d(xc, yc + CROSS_SIZE * pscale.y);
        d.glEnd();

        if (angle != 0.) {
            d.glPopMatrix();
        }
    }
    //glPopAttrib();

    return true;
} // draw

void
RectangleInteract::updateCursor(double angle,
                                const OfxPointD& pscale)
{
    const char* name = NULL;
    // direction of the handle from the rectangle center, in the rectangle frame
    // (corners use the diagonals, as the unrotated cursors do)
    double dx = 0., dy = 0.;

    switch (_drawState) {
    case eDrawStateInactive:
        break;
    case eDrawStateHoveringCenter:
        name = kOfxsCursorMove;
        break;
    case eDrawStateHoveringTopLeft:
    case eDrawStateHoveringBtmRight:
        name = kOfxsCursorNWSEResize;
        dx = -1.; dy = 1.;
        break;
    case eDrawStateHoveringTopRight:
    case eDrawStateHoveringBtmLeft:
        name = kOfxsCursorNESWResize;
        dx = 1.; dy = 1.;
        break;
    case eDrawStateHoveringTopMid:
    case eDrawStateHoveringBtmMid:
        name = kOfxsCursorNSResize;
        dy = 1.;
        break;
    case eDrawStateHoveringMidLeft:
    case eDrawStateHoveringMidRight:
        name = kOfxsCursorEWResize;
        dx = 1.;
        break;
    }
    if (!name) {
        // not over a handle: give the cursor back to the host
        _cursor.release(this);

        return;
    }
    if ( (angle != 0.) && ( (dx != 0.) || (dy != 0.) ) ) {
        // the rectangle is rotated (counter-clockwise, in canonical coordinates):
        // use the resize cursor along the rotated handle direction
        double a = angle * OFXS_PI / 180.;
        double cosa = std::cos(a);
        double sina = std::sin(a);
        name = InteractCursor::resizeCursorForDirection(cosa * dx - sina * dy, sina * dx + cosa * dy, pscale);
    }
    _cursor.set(this, name);
}

bool
RectangleInteract::penMotion(const PenArgs &args)
{
    if ( _btmLeft->getIsSecret() || _size->getIsSecret() ||
         !_btmLeft->getIsEnable() || !_size->getIsEnable() ||
         ( _enable && !_enable->getValueAtTime(args.time) ) ) {
        // the interact is disabled: give the cursor back if we had set it
        releaseCursor();

        return false;
    }

    const OfxPointD& pscale = args.pixelScale;
    double x1, y1, w, h;
    OfxPointD center;
    double angle;
    getRectangle(args.time, &x1, &y1, &w, &h, &center, &angle);
    double x2 = x1 + w;
    double y2 = y1 + h;
    double xc = x1 + w / 2;
    double yc = y1 + h / 2;
    bool valuesChanged = false;
    // dragging is done in the frame of the unrotated rectangle
    const OfxPointD penPosition = penPositionInRectangleFrame(args.penPosition, center, angle);
    OfxPointD delta;
    delta.x = penPosition.x - _lastMousePos.x;
    delta.y = penPosition.y - _lastMousePos.y;

    aboutToCheckInteractivity(args.time);
    DrawStateEnum newState = _drawState;
    // test center first
    if ( isHandleNearby(args.penPosition, xc, yc, center, angle, pscale)  && allowCenterInteraction() ) {
        newState = eDrawStateHoveringCenter;
    } else if ( isHandleNearby(args.penPosition, x1, y1, center, angle, pscale) && allowBtmLeftInteraction() ) {
        newState = eDrawStateHoveringBtmLeft;
    } else if ( isHandleNearby(args.penPosition, x2, y1, center, angle, pscale) && allowBtmRightInteraction() ) {
        newState = eDrawStateHoveringBtmRight;
    } else if ( isHandleNearby(args.penPosition, x1, y2, center, angle, pscale)  && allowTopLeftInteraction() ) {
        newState = eDrawStateHoveringTopLeft;
    } else if ( isHandleNearby(args.penPosition, x2, y2, center, angle, pscale) && allowTopRightInteraction() ) {
        newState = eDrawStateHoveringTopRight;
    } else if ( isHandleNearby(args.penPosition, xc, y1, center, angle, pscale)  && allowBtmMidInteraction() ) {
        newState = eDrawStateHoveringBtmMid;
    } else if ( isHandleNearby(args.penPosition, xc, y2, center, angle, pscale) && allowTopMidInteraction() ) {
        newState = eDrawStateHoveringTopMid;
    } else if ( isHandleNearby(args.penPosition, x1, yc, center, angle, pscale)  && allowMidLeftInteraction() ) {
        newState = eDrawStateHoveringMidLeft;
    } else if ( isHandleNearby(args.penPosition, x2, yc, center, angle, pscale) && allowMidRightInteraction() ) {
        newState = eDrawStateHoveringMidRight;
    } else {
        newState = eDrawStateInactive;
    }
    bool redraw = _drawState != newState;
    _drawState = newState;

    const bool keepAR = shiftDown();
    const bool centered = ctrlDown();
    if ( keepAR && (_sizeDrag.x > 0.) && (_sizeDrag.y > 0.) &&
         ( ( _mouseState == eMouseStateDraggingTopLeft) ||
           ( _mouseState == eMouseStateDraggingTopRight) ||
           ( _mouseState == eMouseStateDraggingBtmLeft) ||
           ( _mouseState == eMouseStateDraggingBtmRight) ) ) {
        double r2 = _sizeDrag.x * _sizeDrag.x + _sizeDrag.y * _sizeDrag.y;
        if ( (_mouseState == eMouseStateDraggingTopRight) ||
             ( _mouseState == eMouseStateDraggingBtmLeft) ) {
            double dotprod = (delta.x * _sizeDrag.y + delta.y * _sizeDrag.x) / r2;
            delta.x = _sizeDrag.x * dotprod;
            delta.y = _sizeDrag.y * dotprod;
        } else {
            double dotprod = (delta.x * _sizeDrag.y - delta.y * _sizeDrag.x) / r2;
            delta.x = _sizeDrag.x * dotprod;
            delta.y = -_sizeDrag.y * dotprod;
        }
    }
    if (_mouseState == eMouseStateDraggingBtmLeft) {
        _drawState = eDrawStateHoveringBtmLeft;
        OfxPointD topRight;
        topRight.x = _btmLeftDragPos.x + _sizeDrag.x;
        topRight.y = _btmLeftDragPos.y + _sizeDrag.y;
        _btmLeftDragPos.x += delta.x;
        _btmLeftDragPos.y += delta.y;
        _sizeDrag.x = topRight.x - _btmLeftDragPos.x;
        _sizeDrag.y = topRight.y - _btmLeftDragPos.y;
        if (centered) {
            _sizeDrag.x -= delta.x;
            _sizeDrag.y -= delta.y;
        }
        valuesChanged = true;
    } else if (_mouseState == eMouseStateDraggingTopLeft) {
        _drawState = eDrawStateHoveringTopLeft;
        OfxPointD btmRight;
        btmRight.x = _btmLeftDragPos.x + _sizeDrag.x;
        btmRight.y = _btmLeftDragPos.y;
        _btmLeftDragPos.x += delta.x;
        _sizeDrag.y += delta.y;
        _sizeDrag.x = btmRight.x - _btmLeftDragPos.x;
        if (centered) {
            _sizeDrag.x -= delta.x;
            _sizeDrag.y += delta.y;
            _btmLeftDragPos.y -= delta.y;
        }
        valuesChanged = true;
    } else if (_mouseState == eMouseStateDraggingTopRight) {
        _drawState = eDrawStateHoveringTopRight;
        _sizeDrag.x += delta.x;
        _sizeDrag.y += delta.y;
        if (centered) {
            _sizeDrag.x += delta.x;
            _btmLeftDragPos.x -= delta.x;
            _sizeDrag.y += delta.y;
            _btmLeftDragPos.y -= delta.y;
        }
        valuesChanged = true;
    } else if (_mouseState == eMouseStateDraggingBtmRight) {
        _drawState = eDrawStateHoveringBtmRight;
        OfxPointD topLeft;
        topLeft.x = _btmLeftDragPos.x;
        topLeft.y = _btmLeftDragPos.y + _sizeDrag.y;
        _sizeDrag.x += delta.x;
        _btmLeftDragPos.y += delta.y;
        _sizeDrag.y = topLeft.y - _btmLeftDragPos.y;
        if (centered) {
            _sizeDrag.x += delta.x;
            _btmLeftDragPos.x -= delta.x;
            _sizeDrag.y -= delta.y;
        }
        valuesChanged = true;
    } else if (_mouseState == eMouseStateDraggingTopMid) {
        _drawState = eDrawStateHoveringTopMid;
        _sizeDrag.y += delta.y;
        if (centered) {
            _sizeDrag.y += delta.y;
            _btmLeftDragPos.y -= delta.y;
        }
        valuesChanged = true;
    } else if (_mouseState == eMouseStateDraggingMidRight) {
        _drawState = eDrawStateHoveringMidRight;
        _sizeDrag.x += delta.x;
        if (centered) {
            _sizeDrag.x += delta.x;
            _btmLeftDragPos.x -= delta.x;
        }
        valuesChanged = true;
    } else if (_mouseState == eMouseStateDraggingBtmMid) {
        _drawState = eDrawStateHoveringBtmMid;
        double top = _btmLeftDragPos.y + _sizeDrag.y;
        _btmLeftDragPos.y += delta.y;
        _sizeDrag.y = top - _btmLeftDragPos.y;
        if (centered) {
            _sizeDrag.y -= delta.y;
        }
        valuesChanged = true;
    } else if (_mouseState == eMouseStateDraggingMidLeft) {
        _drawState = eDrawStateHoveringMidLeft;
        double right = _btmLeftDragPos.x + _sizeDrag.x;
        _btmLeftDragPos.x += delta.x;
        _sizeDrag.x = right - _btmLeftDragPos.x;
        if (centered) {
            _sizeDrag.x -= delta.x;
        }
        valuesChanged = true;
    } else if (_mouseState == eMouseStateDraggingCenter) {
        _drawState = eDrawStateHoveringCenter;
        _btmLeftDragPos.x += delta.x;
        _btmLeftDragPos.y += delta.y;
        valuesChanged = true;
    }


    // Normalize the dragged rectangle. Only while dragging: on a hover,
    // _sizeDrag is not the current rectangle (it is (0,0) before the first
    // drag), and normalizing it would mark the event as handled.
    if (_mouseState != eMouseStateIdle) {
        //if size is negative shift bottom left
        if (_sizeDrag.x < 0) {
            if (_mouseState == eMouseStateDraggingBtmLeft) {
                _mouseState = eMouseStateDraggingBtmRight;
            } else if (_mouseState == eMouseStateDraggingMidLeft) {
                _mouseState = eMouseStateDraggingMidRight;
            } else if (_mouseState == eMouseStateDraggingTopLeft) {
                _mouseState = eMouseStateDraggingTopRight;
            } else if (_mouseState == eMouseStateDraggingBtmRight) {
                _mouseState = eMouseStateDraggingBtmLeft;
            } else if (_mouseState == eMouseStateDraggingMidRight) {
                _mouseState = eMouseStateDraggingMidLeft;
            } else if (_mouseState == eMouseStateDraggingTopRight) {
                _mouseState = eMouseStateDraggingTopLeft;
            }

            _btmLeftDragPos.x += _sizeDrag.x;
            _sizeDrag.x = -_sizeDrag.x;
            valuesChanged = true;
        }
        if (_sizeDrag.y < 0) {
            if (_mouseState == eMouseStateDraggingTopLeft) {
                _mouseState = eMouseStateDraggingBtmLeft;
            } else if (_mouseState == eMouseStateDraggingTopMid) {
                _mouseState = eMouseStateDraggingBtmMid;
            } else if (_mouseState == eMouseStateDraggingTopRight) {
                _mouseState = eMouseStateDraggingBtmRight;
            } else if (_mouseState == eMouseStateDraggingBtmLeft) {
                _mouseState = eMouseStateDraggingTopLeft;
            } else if (_mouseState == eMouseStateDraggingBtmMid) {
                _mouseState = eMouseStateDraggingTopMid;
            } else if (_mouseState == eMouseStateDraggingBtmRight) {
                _mouseState = eMouseStateDraggingTopRight;
            }

            _btmLeftDragPos.y += _sizeDrag.y;
            _sizeDrag.y = -_sizeDrag.y;
            valuesChanged = true;
        }

        ///forbid 0 pixels wide crop rectangles
        if (_sizeDrag.x < 1) {
            _sizeDrag.x = 1;
            valuesChanged = true;
        }
        if (_sizeDrag.y < 1) {
            _sizeDrag.y = 1;
            valuesChanged = true;
        }
    }

    if ( (_mouseState != eMouseStateIdle) && _interactiveDrag && valuesChanged ) {
        setValue(_btmLeftDragPos, _sizeDrag, args.pixelScale);
        // no need to redraw overlay since it is slave to the paramaters
    } else if (redraw || valuesChanged) {
        requestRedraw();
    }


    _lastMousePos = penPosition;

    // report the cursor for the hovered or dragged handle (during a drag,
    // _drawState is the dragged handle); the host reads it whether or not the
    // event is trapped, so the return value is unchanged
    updateCursor(angle, pscale);

    return valuesChanged;
} // penMotion

bool
RectangleInteract::penDown(const PenArgs &args)
{
    if ( _btmLeft->getIsSecret() || _size->getIsSecret() ||
         !_btmLeft->getIsEnable() || !_size->getIsEnable() ||
         ( _enable && !_enable->getValueAtTime(args.time) ) ) {
        return false;
    }

    const OfxPointD& pscale = args.pixelScale;
    double x1, y1, w, h;
    OfxPointD center;
    double angle;
    getRectangle(args.time, &x1, &y1, &w, &h, &center, &angle);
    if (_mouseState == eMouseStateIdle) {
        if ( _interactive && _interactive->getIsEnable() ) {
            _interactive->getValueAtTime(args.time, _interactiveDrag);
        } else {
            _interactiveDrag = false;
        }
    }
    double x2 = x1 + w;
    double y2 = y1 + h;
    double xc = x1 + w / 2;
    double yc = y1 + h / 2;
    bool didSomething = false;
    // dragging is done in the frame of the unrotated rectangle
    const OfxPointD penPosition = penPositionInRectangleFrame(args.penPosition, center, angle);

    aboutToCheckInteractivity(args.time);

    // test center first
    MouseStateEnum newState = _mouseState;
    if ( isHandleNearby(args.penPosition, xc, yc, center, angle, pscale)  && allowCenterInteraction() ) {
        newState = eMouseStateDraggingCenter;
    } else if ( isHandleNearby(args.penPosition, x1, y1, center, angle, pscale) && allowBtmLeftInteraction() ) {
        newState = eMouseStateDraggingBtmLeft;
    } else if ( isHandleNearby(args.penPosition, x2, y1, center, angle, pscale) && allowBtmRightInteraction() ) {
        newState = eMouseStateDraggingBtmRight;
    } else if ( isHandleNearby(args.penPosition, x1, y2, center, angle, pscale)  && allowTopLeftInteraction() ) {
        newState = eMouseStateDraggingTopLeft;
    } else if ( isHandleNearby(args.penPosition, x2, y2, center, angle, pscale) && allowTopRightInteraction() ) {
        newState = eMouseStateDraggingTopRight;
    } else if ( isHandleNearby(args.penPosition, xc, y1, center, angle, pscale)  && allowBtmMidInteraction() ) {
        newState = eMouseStateDraggingBtmMid;
    } else if ( isHandleNearby(args.penPosition, xc, y2, center, angle, pscale) && allowTopMidInteraction() ) {
        newState = eMouseStateDraggingTopMid;
    } else if ( isHandleNearby(args.penPosition, x1, yc, center, angle, pscale)  && allowMidLeftInteraction() ) {
        newState = eMouseStateDraggingMidLeft;
    } else if ( isHandleNearby(args.penPosition, x2, yc, center, angle, pscale) && allowMidRightInteraction() ) {
        newState = eMouseStateDraggingMidRight;
    } else {
        newState = eMouseStateIdle;
    }
    didSomething = _mouseState != newState;
    _mouseState = newState;

    _btmLeftDragPos.x = x1;
    _btmLeftDragPos.y = y1;
    _sizeDrag.x = w;
    _sizeDrag.y = h;
    _centerDrag = center;
    _angleDrag = angle;
    _lastMousePos = penPosition;
    if (didSomething) {
        // the pen is over the pressed handle: _drawState is otherwise only set by
        // motions, and may be stale if penDown was not preceded by a hover (tablets)
        switch (_mouseState) {
        case eMouseStateIdle:
            break;
        case eMouseStateDraggingTopLeft:
            _drawState = eDrawStateHoveringTopLeft;
            break;
        case eMouseStateDraggingTopRight:
            _drawState = eDrawStateHoveringTopRight;
            break;
        case eMouseStateDraggingBtmLeft:
            _drawState = eDrawStateHoveringBtmLeft;
            break;
        case eMouseStateDraggingBtmRight:
            _drawState = eDrawStateHoveringBtmRight;
            break;
        case eMouseStateDraggingCenter:
            _drawState = eDrawStateHoveringCenter;
            break;
        case eMouseStateDraggingTopMid:
            _drawState = eDrawStateHoveringTopMid;
            break;
        case eMouseStateDraggingMidRight:
            _drawState = eDrawStateHoveringMidRight;
            break;
        case eMouseStateDraggingBtmMid:
            _drawState = eDrawStateHoveringBtmMid;
            break;
        case eMouseStateDraggingMidLeft:
            _drawState = eDrawStateHoveringMidLeft;
            break;
        }
        updateCursor(angle, pscale);
        requestRedraw();
    }

    return didSomething;
} // penDown

bool
RectangleInteract::penUp(const PenArgs &args)
{
    if ( _btmLeft->getIsSecret() || _size->getIsSecret() ||
         !_btmLeft->getIsEnable() || !_size->getIsEnable() ||
         ( _enable && !_enable->getValueAtTime(args.time) ) ) {
        // the interact was disabled during the drag (if any): abandon it
        abandonDrag();

        return false;
    }

    bool didSmthing = false;

    if ( !_interactiveDrag && (_mouseState != eMouseStateIdle) ) {
        // no need to redraw overlay since it is slave to the paramaters
        setValue(_btmLeftDragPos, _sizeDrag, args.pixelScale);
        didSmthing = true;
    } else if (_mouseState != eMouseStateIdle) {
        requestRedraw();
    }
    _mouseState = eMouseStateIdle;

    return didSmthing;
} // penUp

bool
RectangleInteract::ctrlDown() const
{
#ifdef OFXS_HAVE_KEY_MODIFIERS
    return (getKeyModifiers() & kNatronOfxKeyModifierControl) != 0;
#else
    return _modifierStateCtrl > 0;
#endif
}

bool
RectangleInteract::shiftDown() const
{
#ifdef OFXS_HAVE_KEY_MODIFIERS
    return (getKeyModifiers() & kNatronOfxKeyModifierShift) != 0;
#else
    return _modifierStateShift > 0;
#endif
}

// keyDown just updates the modifier state
bool
RectangleInteract::keyDown(const KeyArgs &args)
{
    if ( _btmLeft->getIsSecret() || _size->getIsSecret() ||
         !_btmLeft->getIsEnable() || !_size->getIsEnable() ||
         ( _enable && !_enable->getValueAtTime(args.time) ) ) {
        return false;
    }

    // Note that on the Mac:
    // cmd/apple/cloverleaf is kOfxKey_Control_L
    // ctrl is kOfxKey_Meta_L
    // alt/option is kOfxKey_Alt_L
    bool mustRedraw = false;

    // the two control keys may be pressed consecutively, be aware about this
    if ( (args.keySymbol == kOfxKey_Control_L) || (args.keySymbol == kOfxKey_Control_R) ) {
        mustRedraw = _modifierStateCtrl == 0;
        ++_modifierStateCtrl;
    }
    if ( (args.keySymbol == kOfxKey_Shift_L) || (args.keySymbol == kOfxKey_Shift_R) ) {
        mustRedraw = _modifierStateShift == 0;
        ++_modifierStateShift;
    }
#ifdef OFXS_HAVE_KEY_MODIFIERS
    // the drawn state comes from Support's modifiers, which the counters may not follow (a
    // modifier pressed before the overlay got the focus): redraw on every modifier event
    if ( (args.keySymbol == kOfxKey_Control_L) || (args.keySymbol == kOfxKey_Control_R) ||
         (args.keySymbol == kOfxKey_Shift_L) || (args.keySymbol == kOfxKey_Shift_R) ) {
        mustRedraw = true;
    }
#endif
    if (mustRedraw) {
        requestRedraw();
    }
    //std::cout << std::hex << args.keySymbol << std::endl;

    // modifiers are not "caught"
    return false;
}

// keyUp just updates the modifier state
bool
RectangleInteract::keyUp(const KeyArgs &args)
{
    if ( _btmLeft->getIsSecret() || _size->getIsSecret() ||
         !_btmLeft->getIsEnable() || !_size->getIsEnable() ||
         ( _enable && !_enable->getValueAtTime(args.time) ) ) {
        return false;
    }

    bool mustRedraw = false;

    if ( (args.keySymbol == kOfxKey_Control_L) || (args.keySymbol == kOfxKey_Control_R) ) {
        // we may have missed a keypress
        if (_modifierStateCtrl > 0) {
            --_modifierStateCtrl;
            mustRedraw = _modifierStateCtrl == 0;
        }
    }
    if ( (args.keySymbol == kOfxKey_Shift_L) || (args.keySymbol == kOfxKey_Shift_R) ) {
        if (_modifierStateShift > 0) {
            --_modifierStateShift;
            mustRedraw = _modifierStateShift == 0;
        }
    }
#ifdef OFXS_HAVE_KEY_MODIFIERS
    // the drawn state comes from Support's modifiers, which the counters may not follow (a
    // modifier pressed before the overlay got the focus): redraw on every modifier event
    if ( (args.keySymbol == kOfxKey_Control_L) || (args.keySymbol == kOfxKey_Control_R) ||
         (args.keySymbol == kOfxKey_Shift_L) || (args.keySymbol == kOfxKey_Shift_R) ) {
        mustRedraw = true;
    }
#endif
    if (mustRedraw) {
        requestRedraw();
    }

    // modifiers are not "caught"
    return false;
}

/** @brief Called when the interact is loses input focus */
void
RectangleInteract::loseFocus(const FocusArgs & /*args*/)
{
    // reset the modifiers state
    _modifierStateCtrl = 0;
    _modifierStateShift = 0;
    _interactiveDrag = false;
    // a drag is abandoned if the focus is lost (the host may never send the
    // pen up), as in TransformInteractHelper and TrackerRegionInteract
    _mouseState = eMouseStateIdle;
    _drawState = eDrawStateInactive;
    // the host restores its own cursor
    _cursor.release(this);
}

OfxPointD
RectangleInteract::getBtmLeft(OfxTime time) const
{
    OfxPointD ret;

    _btmLeft->getValueAtTime(time, ret.x, ret.y);

    return ret;
}

void
RectangleInteract::setValue(OfxPointD btmLeft,
                            OfxPointD size,
                            const OfxPointD &pscale)
{
    bool setBtmLeft = false;
    bool setSize = false;
    if (_angleDrag != 0.) {
        // Rotated: btmLeft is not an edge position on screen, so rounding it would move the
        // side that must stay fixed. Round the size instead, in the rectangle frame, and move
        // btmLeft so that the fixed side (or the center, with Ctrl) stays in place.
        const bool centered = ctrlDown();
        int sideX = 0; // -1: the left side moves, +1: the right side moves, 0: width unchanged
        int sideY = 0; // -1: the bottom side moves, +1: the top side moves, 0: height unchanged
        switch (_mouseState) {
        case eMouseStateDraggingTopLeft:
            sideX = -1; sideY = +1;
            break;
        case eMouseStateDraggingTopRight:
            sideX = +1; sideY = +1;
            break;
        case eMouseStateDraggingBtmLeft:
            sideX = -1; sideY = -1;
            break;
        case eMouseStateDraggingBtmRight:
            sideX = +1; sideY = -1;
            break;
        case eMouseStateDraggingTopMid:
            sideY = +1;
            break;
        case eMouseStateDraggingBtmMid:
            sideY = -1;
            break;
        case eMouseStateDraggingMidRight:
            sideX = +1;
            break;
        case eMouseStateDraggingMidLeft:
            sideX = -1;
            break;
        case eMouseStateDraggingCenter:
        case eMouseStateIdle:
            break;
        }
        if (sideX != 0) {
            double w = froundSize(size.x, pscale.x);
            btmLeft.x -= centered ? (w - size.x) / 2 : (sideX < 0 ? w - size.x : 0.);
            size.x = w;
        }
        if (sideY != 0) {
            double h = froundSize(size.y, pscale.y);
            btmLeft.y -= centered ? (h - size.y) / 2 : (sideY < 0 ? h - size.y : 0.);
            size.y = h;
        }
        // The rectangle was edited in the frame rotated around _centerDrag, but the
        // rendered shape is rotated around its own center: translate it so that it stays
        // at the same place, i.e. btmLeft += (I - R) * (_centerDrag - newCenter)
        OfxPointD d;
        d.x = _centerDrag.x - (btmLeft.x + size.x / 2);
        d.y = _centerDrag.y - (btmLeft.y + size.y / 2);
        double a = _angleDrag * OFXS_PI / 180.;
        double cosa = std::cos(a);
        double sina = std::sin(a);
        btmLeft.x += d.x - (cosa * d.x - sina * d.y);
        btmLeft.y += d.y - (sina * d.x + cosa * d.y);
        setBtmLeft = setSize = true;
    } else {
        // round newx/y to the closest int, 1/10 int, etc
        // this make parameter editing easier
        switch (_mouseState) {
        case eMouseStateIdle:
            break;
        case eMouseStateDraggingTopLeft:
            btmLeft.x = fround(btmLeft.x, pscale.x);
            size.x = froundSize(size.x, pscale.x);
            size.y = froundSize(size.y, pscale.y);
            setBtmLeft = setSize = true;
            break;
        case eMouseStateDraggingTopRight:
            size.x = froundSize(size.x, pscale.x);
            size.y = froundSize(size.y, pscale.y);
            setSize = true;
            break;
        case eMouseStateDraggingBtmLeft:
            btmLeft.x = fround(btmLeft.x, pscale.x);
            btmLeft.y = fround(btmLeft.y, pscale.y);
            size.x = froundSize(size.x, pscale.x);
            size.y = froundSize(size.y, pscale.y);
            setBtmLeft = setSize = true;
            break;
        case eMouseStateDraggingBtmRight:
            size.x = froundSize(size.x, pscale.x);
            size.y = froundSize(size.y, pscale.y);
            btmLeft.y = fround(btmLeft.y, pscale.y);
            setBtmLeft = setSize = true;
            break;
        case eMouseStateDraggingCenter:
            btmLeft.x = fround(btmLeft.x, pscale.x);
            btmLeft.y = fround(btmLeft.y, pscale.y);
            setBtmLeft = true;
            break;
        case eMouseStateDraggingTopMid:
            size.y = froundSize(size.y, pscale.y);
            setSize = true;
            break;
        case eMouseStateDraggingMidRight:
            size.x = froundSize(size.x, pscale.x);
            setSize = true;
            break;
        case eMouseStateDraggingBtmMid:
            btmLeft.y = fround(btmLeft.y, pscale.y);
            setBtmLeft = true;
            break;
        case eMouseStateDraggingMidLeft:
            btmLeft.x = fround(btmLeft.x, pscale.x);
            setBtmLeft = true;
            break;
        }
    }
    ImageEffect::EditBlock eb(*_effect, "setRectangle", setBtmLeft + setSize > 1);
    _btmLeft->setValue(btmLeft.x, btmLeft.y);
    _size->setValue(size.x, size.y);
} // penDown
