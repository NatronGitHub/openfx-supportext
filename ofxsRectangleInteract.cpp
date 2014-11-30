/*
   OFX generic rectangle interact with 4 corner points + center point and 4 mid-points.
   You can use it to define any rectangle in an image resizable by the user.

   Copyright (C) 2014 INRIA

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

   Redistributions in binary form must reproduce the above copyright notice, this
   list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   Neither the name of the {organization} nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
   ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   INRIA
   Domaine de Voluceau
   Rocquencourt - B.P. 105
   78153 Le Chesnay Cedex - France
 */

#include "ofxsRectangleInteract.h"
#include <cmath>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#define POINT_SIZE 5
#define POINT_TOLERANCE 6
#define CROSS_SIZE 7
#define HANDLE_SIZE 6

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
    double pscale10 = std::pow( 10.,std::floor( std::log10(pscale) ) );

    return pscale10 * std::floor(val / pscale10 + 0.5);
}

static void
drawPoint(bool draw,
          double x,
          double y,
          RectangleInteract::DrawStateEnum id,
          RectangleInteract::DrawStateEnum ds,
          int l)
{
    if (draw) {
        if (ds == id) {
            glColor3f(0. * l, 1. * l, 0. * l);
        } else {
            glColor3f(0.8 * l, 0.8 * l, 0.8 * l);
        }
        glVertex2d(x, y);
    }
}

bool
RectangleInteract::draw(const OFX::DrawArgs &args)
{
    OfxPointD pscale;

    pscale.x = args.pixelScale.x / args.renderScale.x;
    pscale.y = args.pixelScale.y / args.renderScale.y;

    double x1, y1, w, h;
    if (_ms != eMouseStateIdle) {
        x1 = _btmLeftDragPos.x;
        y1 = _btmLeftDragPos.y;
        w = _sizeDrag.x;
        h = _sizeDrag.y;
    } else {
        _btmLeft->getValueAtTime(args.time, x1, y1);
        _size->getValueAtTime(args.time, w, h);
    }
    double x2 = x1 + w;
    double y2 = y1 + h;
    double xc = x1 + w / 2;
    double yc = y1 + h / 2;

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);

    //glDisable(GL_LINE_STIPPLE);
    glEnable(GL_LINE_SMOOTH);
    //glEnable(GL_POINT_SMOOTH);
    glEnable(GL_BLEND);
    glHint(GL_LINE_SMOOTH_HINT,GL_DONT_CARE);
    glLineWidth(1.5);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    // Draw everything twice
    // l = 0: shadow
    // l = 1: drawing
    for (int l = 0; l < 2; ++l) {
        if (l == 0) {
            // translate (1,-1) pixels
            glTranslated(pscale.x, -pscale.y, 0);
        }
        glColor3f(0.8 * l, 0.8 * l, 0.8 * l);

        glBegin(GL_LINE_LOOP);
        glVertex2d(x1, y1);
        glVertex2d(x1, y2);
        glVertex2d(x2, y2);
        glVertex2d(x2, y1);
        glEnd();

        glPointSize(POINT_SIZE);
        glBegin(GL_POINTS);
        drawPoint(allowBtmLeftInteraction(),  x1, y1, eDrawStateHoveringBtmLeft,  _ds, l);
        drawPoint(allowMidLeftInteraction(),  x1, yc, eDrawStateHoveringMidLeft,  _ds, l);
        drawPoint(allowTopLeftInteraction(),  x1, y2, eDrawStateHoveringTopLeft,  _ds, l);
        drawPoint(allowBtmMidInteraction(),   xc, y1, eDrawStateHoveringBtmMid,   _ds, l);
        drawPoint(allowCenterInteraction(),   xc, yc, eDrawStateHoveringCenter,   _ds, l);
        drawPoint(allowTopMidInteraction(),   xc, y2, eDrawStateHoveringTopMid,   _ds, l);
        drawPoint(allowBtmRightInteraction(), x2, y1, eDrawStateHoveringBtmRight, _ds, l);
        drawPoint(allowMidRightInteraction(), x2, yc, eDrawStateHoveringMidRight, _ds, l);
        drawPoint(allowTopRightInteraction(), x2, y2, eDrawStateHoveringTopRight, _ds, l);
        glEnd();
        glPointSize(1);

        ///draw center cross hair
        glBegin(GL_LINES);
        if (_ds == eDrawStateHoveringCenter) {
            glColor3f(0. * l, 1. * l, 0. * l);
        } else if ( !allowCenterInteraction() ) {
            glColor3f(0.5 * l, 0.5 * l, 0.5 * l);
        } else {
            glColor3f(0.8 * l, 0.8 * l, 0.8 * l);
        }
        glVertex2d(xc - CROSS_SIZE * pscale.x, yc);
        glVertex2d(xc + CROSS_SIZE * pscale.x, yc);
        glVertex2d(xc, yc - CROSS_SIZE * pscale.y);
        glVertex2d(xc, yc + CROSS_SIZE * pscale.y);
        glEnd();
        if (l == 0) {
            // translate (-1,1) pixels
            glTranslated(-pscale.x, pscale.y, 0);
        }
    }
    glPopAttrib();

    return true;
} // draw

bool
RectangleInteract::penMotion(const OFX::PenArgs &args)
{
    OfxPointD pscale;

    pscale.x = args.pixelScale.x / args.renderScale.x;
    pscale.y = args.pixelScale.y / args.renderScale.y;

    double x1, y1, w, h;
    if (_ms != eMouseStateIdle) {
        x1 = _btmLeftDragPos.x;
        y1 = _btmLeftDragPos.y;
        w = _sizeDrag.x;
        h = _sizeDrag.y;
    } else {
        _btmLeft->getValueAtTime(args.time, x1, y1);
        _size->getValueAtTime(args.time, w, h);
    }
    double x2 = x1 + w;
    double y2 = y1 + h;
    double xc = x1 + w / 2;
    double yc = y1 + h / 2;
    bool didSomething = false;
    OfxPointD delta;
    delta.x = args.penPosition.x - _lastMousePos.x;
    delta.y = args.penPosition.y - _lastMousePos.y;

    bool lastStateWasHovered = _ds != eDrawStateInactive;


    aboutToCheckInteractivity(args.time);
    // test center first
    if ( isNearby(args.penPosition, xc, yc, POINT_TOLERANCE, pscale)  && allowCenterInteraction() ) {
        _ds = eDrawStateHoveringCenter;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x1, y1, POINT_TOLERANCE, pscale) && allowBtmLeftInteraction() ) {
        _ds = eDrawStateHoveringBtmLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x2, y1, POINT_TOLERANCE, pscale) && allowBtmRightInteraction() ) {
        _ds = eDrawStateHoveringBtmRight;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x1, y2, POINT_TOLERANCE, pscale)  && allowTopLeftInteraction() ) {
        _ds = eDrawStateHoveringTopLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x2, y2, POINT_TOLERANCE, pscale) && allowTopRightInteraction() ) {
        _ds = eDrawStateHoveringTopRight;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xc, y1, POINT_TOLERANCE, pscale)  && allowBtmMidInteraction() ) {
        _ds = eDrawStateHoveringBtmMid;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xc, y2, POINT_TOLERANCE, pscale) && allowTopMidInteraction() ) {
        _ds = eDrawStateHoveringTopMid;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x1, yc, POINT_TOLERANCE, pscale)  && allowMidLeftInteraction() ) {
        _ds = eDrawStateHoveringMidLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x2, yc, POINT_TOLERANCE, pscale) && allowMidRightInteraction() ) {
        _ds = eDrawStateHoveringMidRight;
        didSomething = true;
    } else {
        _ds = eDrawStateInactive;
    }

    if (_ms == eMouseStateDraggingBtmLeft) {
        OfxPointD topRight;
        topRight.x = _btmLeftDragPos.x + _sizeDrag.x;
        topRight.y = _btmLeftDragPos.y + _sizeDrag.y;
        _btmLeftDragPos.x += delta.x;
        _btmLeftDragPos.y += delta.y;
        _sizeDrag.x = topRight.x - _btmLeftDragPos.x;
        _sizeDrag.y = topRight.y - _btmLeftDragPos.y;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingTopLeft) {
        OfxPointD btmRight;
        btmRight.x = _btmLeftDragPos.x + _sizeDrag.x;
        btmRight.y = _btmLeftDragPos.y;
        _btmLeftDragPos.x += delta.x;
        _sizeDrag.y += delta.y;
        _sizeDrag.x = btmRight.x - _btmLeftDragPos.x;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingTopRight) {
        _sizeDrag.x += delta.x;
        _sizeDrag.y += delta.y;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingBtmRight) {
        OfxPointD topLeft;
        topLeft.x = _btmLeftDragPos.x;
        topLeft.y = _btmLeftDragPos.y + _sizeDrag.y;
        _sizeDrag.x += delta.x;
        _btmLeftDragPos.y += delta.y;
        _sizeDrag.y = topLeft.y - _btmLeftDragPos.y;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingTopMid) {
        _sizeDrag.y += delta.y;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingMidRight) {
        _sizeDrag.x += delta.x;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingBtmMid) {
        double top = _btmLeftDragPos.y + _sizeDrag.y;
        _btmLeftDragPos.y += delta.y;
        _sizeDrag.y = top - _btmLeftDragPos.y;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingMidLeft) {
        double right = _btmLeftDragPos.x + _sizeDrag.x;
        _btmLeftDragPos.x += delta.x;
        _sizeDrag.x = right - _btmLeftDragPos.x;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingCenter) {
        _btmLeftDragPos.x += delta.x;
        _btmLeftDragPos.y += delta.y;
        didSomething = true;
    }


    //if size is negative shift bottom left
    if (_sizeDrag.x < 0) {
        if (_ms == eMouseStateDraggingBtmLeft) {
            _ms = eMouseStateDraggingBtmRight;
        } else if (_ms == eMouseStateDraggingMidLeft) {
            _ms = eMouseStateDraggingMidRight;
        } else if (_ms == eMouseStateDraggingTopLeft) {
            _ms = eMouseStateDraggingTopRight;
        } else if (_ms == eMouseStateDraggingBtmRight) {
            _ms = eMouseStateDraggingBtmLeft;
        } else if (_ms == eMouseStateDraggingMidRight) {
            _ms = eMouseStateDraggingMidLeft;
        } else if (_ms == eMouseStateDraggingTopRight) {
            _ms = eMouseStateDraggingTopLeft;
        }

        _btmLeftDragPos.x += _sizeDrag.x;
        _sizeDrag.x = -_sizeDrag.x;
    }
    if (_sizeDrag.y < 0) {
        if (_ms == eMouseStateDraggingTopLeft) {
            _ms = eMouseStateDraggingBtmLeft;
        } else if (_ms == eMouseStateDraggingTopMid) {
            _ms = eMouseStateDraggingBtmMid;
        } else if (_ms == eMouseStateDraggingTopRight) {
            _ms = eMouseStateDraggingBtmRight;
        } else if (_ms == eMouseStateDraggingBtmLeft) {
            _ms = eMouseStateDraggingTopLeft;
        } else if (_ms == eMouseStateDraggingBtmMid) {
            _ms = eMouseStateDraggingTopMid;
        } else if (_ms == eMouseStateDraggingBtmRight) {
            _ms = eMouseStateDraggingTopRight;
        }

        _btmLeftDragPos.y += _sizeDrag.y;
        _sizeDrag.y = -_sizeDrag.y;
    }

    ///forbid 0 pixels wide crop rectangles
    if (_sizeDrag.x < 1) {
        _sizeDrag.x = 1;
    }
    if (_sizeDrag.y < 1) {
        _sizeDrag.y = 1;
    }

    ///repaint if we toggled off a hovered handle
    if (lastStateWasHovered && !didSomething) {
        didSomething = true;
    }

    _lastMousePos = args.penPosition;

    return didSomething;
} // penMotion

bool
RectangleInteract::penDown(const OFX::PenArgs &args)
{
    OfxPointD pscale;

    pscale.x = args.pixelScale.x / args.renderScale.x;
    pscale.y = args.pixelScale.y / args.renderScale.y;

    double x1, y1, w, h;
    if (_ms != eMouseStateIdle) {
        x1 = _btmLeftDragPos.x;
        y1 = _btmLeftDragPos.y;
        w = _sizeDrag.x;
        h = _sizeDrag.y;
    } else {
        _btmLeft->getValueAtTime(args.time, x1, y1);
        _size->getValueAtTime(args.time, w, h);
    }
    double x2 = x1 + w;
    double y2 = y1 + h;
    double xc = x1 + w / 2;
    double yc = y1 + h / 2;
    bool didSomething = false;

    aboutToCheckInteractivity(args.time);

    // test center first
    if ( isNearby(args.penPosition, xc, yc, POINT_TOLERANCE, pscale)  && allowCenterInteraction() ) {
        _ms = eMouseStateDraggingCenter;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x1, y1, POINT_TOLERANCE, pscale) && allowBtmLeftInteraction() ) {
        _ms = eMouseStateDraggingBtmLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x2, y1, POINT_TOLERANCE, pscale) && allowBtmRightInteraction() ) {
        _ms = eMouseStateDraggingBtmRight;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x1, y2, POINT_TOLERANCE, pscale)  && allowTopLeftInteraction() ) {
        _ms = eMouseStateDraggingTopLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x2, y2, POINT_TOLERANCE, pscale) && allowTopRightInteraction() ) {
        _ms = eMouseStateDraggingTopRight;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xc, y1, POINT_TOLERANCE, pscale)  && allowBtmMidInteraction() ) {
        _ms = eMouseStateDraggingBtmMid;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xc, y2, POINT_TOLERANCE, pscale) && allowTopMidInteraction() ) {
        _ms = eMouseStateDraggingTopMid;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x1, yc, POINT_TOLERANCE, pscale)  && allowMidLeftInteraction() ) {
        _ms = eMouseStateDraggingMidLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x2, yc, POINT_TOLERANCE, pscale) && allowMidRightInteraction() ) {
        _ms = eMouseStateDraggingMidRight;
        didSomething = true;
    } else {
        _ms = eMouseStateIdle;
    }

    _btmLeftDragPos.x = x1;
    _btmLeftDragPos.y = y1;
    _sizeDrag.x = w;
    _sizeDrag.y = h;
    _lastMousePos = args.penPosition;

    return didSomething;
} // penDown

bool
RectangleInteract::penUp(const OFX::PenArgs &args)
{
    bool didSmthing = false;

    if (_ms != eMouseStateIdle) {
        // round newx/y to the closest int, 1/10 int, etc
        // this make parameter editing easier
        OfxPointD pscale;
        pscale.x = args.pixelScale.x / args.renderScale.x;
        pscale.y = args.pixelScale.y / args.renderScale.y;
        OfxPointD btmLeft = _btmLeftDragPos;
        OfxPointD size = _sizeDrag;

        switch (_ms) {
        case eMouseStateIdle:
            break;
        case eMouseStateDraggingTopLeft:
            btmLeft.x = fround(btmLeft.x, pscale.x);
            size.x = fround(size.x, pscale.x);
            size.y = fround(size.y, pscale.y);
            break;
        case eMouseStateDraggingTopRight:
            size.x = fround(size.x, pscale.x);
            size.y = fround(size.y, pscale.y);
            break;
        case eMouseStateDraggingBtmLeft:
            btmLeft.x = fround(btmLeft.x, pscale.x);
            btmLeft.y = fround(btmLeft.y, pscale.y);
            size.x = fround(size.x, pscale.x);
            size.y = fround(size.y, pscale.y);
            break;
        case eMouseStateDraggingBtmRight:
            size.x = fround(size.x, pscale.x);
            size.y = fround(size.y, pscale.y);
            btmLeft.y = fround(btmLeft.y, pscale.y);
            break;
        case eMouseStateDraggingCenter:
            btmLeft.x = fround(btmLeft.x, pscale.x);
            btmLeft.y = fround(btmLeft.y, pscale.y);
            break;
        case eMouseStateDraggingTopMid:
            size.y = fround(size.y, pscale.y);
            break;
        case eMouseStateDraggingMidRight:
            size.x = fround(size.x, pscale.x);
            break;
        case eMouseStateDraggingBtmMid:
            btmLeft.y = fround(btmLeft.y, pscale.y);
            break;
        case eMouseStateDraggingMidLeft:
            btmLeft.x = fround(btmLeft.x, pscale.x);
            break;
        }
        _btmLeft->setValue(btmLeft.x, btmLeft.y);
        _size->setValue(size.x, size.y);
        didSmthing = true;
    }
    _ms = eMouseStateIdle;

    return didSmthing;
} // penUp

OfxPointD
RectangleInteract::getBtmLeft(OfxTime time) const
{
    OfxPointD ret;

    _btmLeft->getValueAtTime(time, ret.x, ret.y);

    return ret;
}

