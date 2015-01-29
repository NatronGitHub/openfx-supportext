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
          bool keepAR,
          int l)
{
    if (draw) {
        if (ds == id) {
            if (keepAR) {
                glColor3f(1. * l, 0. * l, 0. * l);
            } else {
                glColor3f(0. * l, 1. * l, 0. * l);
            }
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
    if (_mouseState != eMouseStateIdle) {
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

    const bool keepAR = _modifierStateShift > 0;
    const bool centered = _modifierStateCtrl > 0;

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_MODELVIEW); // Modelview should be used on Nuke

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
        drawPoint(allowBtmLeftInteraction(),  x1, y1, eDrawStateHoveringBtmLeft,  _drawState, keepAR, l);
        drawPoint(allowMidLeftInteraction(),  x1, yc, eDrawStateHoveringMidLeft,  _drawState, false,  l);
        drawPoint(allowTopLeftInteraction(),  x1, y2, eDrawStateHoveringTopLeft,  _drawState, keepAR, l);
        drawPoint(allowBtmMidInteraction(),   xc, y1, eDrawStateHoveringBtmMid,   _drawState, false,  l);
        drawPoint(allowCenterInteraction(),   xc, yc, eDrawStateHoveringCenter,   _drawState, false,  l);
        drawPoint(allowTopMidInteraction(),   xc, y2, eDrawStateHoveringTopMid,   _drawState, false,  l);
        drawPoint(allowBtmRightInteraction(), x2, y1, eDrawStateHoveringBtmRight, _drawState, keepAR, l);
        drawPoint(allowMidRightInteraction(), x2, yc, eDrawStateHoveringMidRight, _drawState, false,  l);
        drawPoint(allowTopRightInteraction(), x2, y2, eDrawStateHoveringTopRight, _drawState, keepAR, l);
        glEnd();
        glPointSize(1);

        ///draw center cross hair
        glBegin(GL_LINES);
        if (_drawState == eDrawStateHoveringCenter || (centered && _drawState != eDrawStateInactive)) {
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
    if (_mouseState != eMouseStateIdle) {
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

    bool lastStateWasHovered = _drawState != eDrawStateInactive;


    aboutToCheckInteractivity(args.time);
    // test center first
    if ( isNearby(args.penPosition, xc, yc, POINT_TOLERANCE, pscale)  && allowCenterInteraction() ) {
        _drawState = eDrawStateHoveringCenter;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x1, y1, POINT_TOLERANCE, pscale) && allowBtmLeftInteraction() ) {
        _drawState = eDrawStateHoveringBtmLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x2, y1, POINT_TOLERANCE, pscale) && allowBtmRightInteraction() ) {
        _drawState = eDrawStateHoveringBtmRight;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x1, y2, POINT_TOLERANCE, pscale)  && allowTopLeftInteraction() ) {
        _drawState = eDrawStateHoveringTopLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x2, y2, POINT_TOLERANCE, pscale) && allowTopRightInteraction() ) {
        _drawState = eDrawStateHoveringTopRight;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xc, y1, POINT_TOLERANCE, pscale)  && allowBtmMidInteraction() ) {
        _drawState = eDrawStateHoveringBtmMid;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xc, y2, POINT_TOLERANCE, pscale) && allowTopMidInteraction() ) {
        _drawState = eDrawStateHoveringTopMid;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x1, yc, POINT_TOLERANCE, pscale)  && allowMidLeftInteraction() ) {
        _drawState = eDrawStateHoveringMidLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x2, yc, POINT_TOLERANCE, pscale) && allowMidRightInteraction() ) {
        _drawState = eDrawStateHoveringMidRight;
        didSomething = true;
    } else {
        _drawState = eDrawStateInactive;
    }

    const bool keepAR = _modifierStateShift > 0;
    const bool centered = _modifierStateCtrl > 0;
    if (keepAR && (_sizeDrag.x > 0.) && (_sizeDrag.y > 0.) &&
        (_mouseState == eMouseStateDraggingTopLeft  ||
         _mouseState == eMouseStateDraggingTopRight ||
         _mouseState == eMouseStateDraggingBtmLeft  ||
         _mouseState == eMouseStateDraggingBtmRight)) {
        double r2 = _sizeDrag.x*_sizeDrag.x + _sizeDrag.y*_sizeDrag.y;
        if (_mouseState == eMouseStateDraggingTopRight ||
            _mouseState == eMouseStateDraggingBtmLeft) {
            double dotprod = (delta.x * _sizeDrag.y + delta.y * _sizeDrag.x)/r2;
            delta.x = _sizeDrag.x * dotprod;
            delta.y = _sizeDrag.y * dotprod;
        } else  {
            double dotprod = (delta.x * _sizeDrag.y - delta.y * _sizeDrag.x)/r2;
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
        didSomething = true;
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
        didSomething = true;
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
        didSomething = true;
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
        didSomething = true;
    } else if (_mouseState == eMouseStateDraggingTopMid) {
        _drawState = eDrawStateHoveringTopMid;
        _sizeDrag.y += delta.y;
        if (centered) {
            _sizeDrag.y += delta.y;
            _btmLeftDragPos.y -= delta.y;
        }
        didSomething = true;
    } else if (_mouseState == eMouseStateDraggingMidRight) {
        _drawState = eDrawStateHoveringMidRight;
        _sizeDrag.x += delta.x;
        if (centered) {
            _sizeDrag.x += delta.x;
            _btmLeftDragPos.x -= delta.x;
        }
        didSomething = true;
    } else if (_mouseState == eMouseStateDraggingBtmMid) {
        _drawState = eDrawStateHoveringBtmMid;
        double top = _btmLeftDragPos.y + _sizeDrag.y;
        _btmLeftDragPos.y += delta.y;
        _sizeDrag.y = top - _btmLeftDragPos.y;
        if (centered) {
            _sizeDrag.y -= delta.y;
        }
        didSomething = true;
    } else if (_mouseState == eMouseStateDraggingMidLeft) {
        _drawState = eDrawStateHoveringMidLeft;
        double right = _btmLeftDragPos.x + _sizeDrag.x;
        _btmLeftDragPos.x += delta.x;
        _sizeDrag.x = right - _btmLeftDragPos.x;
        if (centered) {
            _sizeDrag.x -= delta.x;
        }
        didSomething = true;
    } else if (_mouseState == eMouseStateDraggingCenter) {
        _drawState = eDrawStateHoveringCenter;
        _btmLeftDragPos.x += delta.x;
        _btmLeftDragPos.y += delta.y;
        didSomething = true;
    }


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
    if (_mouseState != eMouseStateIdle) {
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
        _mouseState = eMouseStateDraggingCenter;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x1, y1, POINT_TOLERANCE, pscale) && allowBtmLeftInteraction() ) {
        _mouseState = eMouseStateDraggingBtmLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x2, y1, POINT_TOLERANCE, pscale) && allowBtmRightInteraction() ) {
        _mouseState = eMouseStateDraggingBtmRight;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x1, y2, POINT_TOLERANCE, pscale)  && allowTopLeftInteraction() ) {
        _mouseState = eMouseStateDraggingTopLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x2, y2, POINT_TOLERANCE, pscale) && allowTopRightInteraction() ) {
        _mouseState = eMouseStateDraggingTopRight;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xc, y1, POINT_TOLERANCE, pscale)  && allowBtmMidInteraction() ) {
        _mouseState = eMouseStateDraggingBtmMid;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xc, y2, POINT_TOLERANCE, pscale) && allowTopMidInteraction() ) {
        _mouseState = eMouseStateDraggingTopMid;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x1, yc, POINT_TOLERANCE, pscale)  && allowMidLeftInteraction() ) {
        _mouseState = eMouseStateDraggingMidLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, x2, yc, POINT_TOLERANCE, pscale) && allowMidRightInteraction() ) {
        _mouseState = eMouseStateDraggingMidRight;
        didSomething = true;
    } else {
        _mouseState = eMouseStateIdle;
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

    if (_mouseState != eMouseStateIdle) {
        // round newx/y to the closest int, 1/10 int, etc
        // this make parameter editing easier
        OfxPointD pscale;
        pscale.x = args.pixelScale.x / args.renderScale.x;
        pscale.y = args.pixelScale.y / args.renderScale.y;
        OfxPointD btmLeft = _btmLeftDragPos;
        OfxPointD size = _sizeDrag;

        switch (_mouseState) {
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
        _effect->beginEditBlock("setRectangle");
        _btmLeft->setValue(btmLeft.x, btmLeft.y);
        _size->setValue(size.x, size.y);
        _effect->endEditBlock();

        didSmthing = true;
    }
    _mouseState = eMouseStateIdle;

    return didSmthing;
} // penUp

// keyDown just updates the modifier state
bool
RectangleInteract::keyDown(const OFX::KeyArgs &args)
{
    // Note that on the Mac:
    // cmd/apple/cloverleaf is kOfxKey_Control_L
    // ctrl is kOfxKey_Meta_L
    // alt/option is kOfxKey_Alt_L
    bool mustRedraw = false;

    // the two control keys may be pressed consecutively, be aware about this
    if (args.keySymbol == kOfxKey_Control_L || args.keySymbol == kOfxKey_Control_R) {
        mustRedraw = _modifierStateCtrl == 0;
        ++_modifierStateCtrl;
    }
    if (args.keySymbol == kOfxKey_Shift_L || args.keySymbol == kOfxKey_Shift_R) {
        mustRedraw = _modifierStateShift == 0;
        ++_modifierStateShift;
    }
    if (mustRedraw) {
        _effect->redrawOverlays();
    }
    //std::cout << std::hex << args.keySymbol << std::endl;
    // modifiers are not "caught"
    return false;
}

// keyUp just updates the modifier state
bool
RectangleInteract::keyUp(const OFX::KeyArgs &args)
{
    bool mustRedraw = false;

    if (args.keySymbol == kOfxKey_Control_L || args.keySymbol == kOfxKey_Control_R) {
        // we may have missed a keypress
        if (_modifierStateCtrl > 0) {
            --_modifierStateCtrl;
            mustRedraw = _modifierStateCtrl == 0;
        }
    }
    if (args.keySymbol == kOfxKey_Shift_L || args.keySymbol == kOfxKey_Shift_R) {
        if (_modifierStateShift > 0) {
            --_modifierStateShift;
            mustRedraw = _modifierStateShift == 0;
        }
    }
    if (mustRedraw) {
        _effect->redrawOverlays();
    }
    // modifiers are not "caught"
    return false;
}

/** @brief Called when the interact is loses input focus */
void
RectangleInteract::loseFocus(const OFX::FocusArgs &/*args*/)
{
    // reset the modifiers state
    _modifierStateCtrl = 0;
    _modifierStateShift = 0;
}

OfxPointD
RectangleInteract::getBtmLeft(OfxTime time) const
{
    OfxPointD ret;

    _btmLeft->getValueAtTime(time, ret.x, ret.y);

    return ret;
}

