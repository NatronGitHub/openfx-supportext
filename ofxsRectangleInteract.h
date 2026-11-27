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

#ifndef openfx_supportext_ofxsRectangleInteract_h
#define openfx_supportext_ofxsRectangleInteract_h

#include <ofxsInteract.h>
#include <ofxsImageEffect.h>
#include "ofxsMacros.h"
#include "ofxsOGLHiDPI.h"
#include "ofxsInteractCursor.h"
#include "ofxsOverlayDraw.h"

// a secret parameter used to enable the interact, if it exists
#define kParamRectangleInteractEnable "rectangleInteractEnable"

#define kParamRectangleInteractBtmLeft "bottomLeft"
#define kParamRectangleInteractBtmLeftLabel "Bottom Left"
#define kParamRectangleInteractBtmLeftHint "Coordinates of the bottom left corner of the rectangle"

#define kParamRectangleInteractSize "size"
#define kParamRectangleInteractSizeLabel "Size"
#define kParamRectangleInteractSizeHint "Width and height of the rectangle"
#define kParamRectangleInteractSizeDim1 "w"
#define kParamRectangleInteractSizeDim2 "h"

#define kParamRectangleInteractInteractive "interactive"
#define kParamRectangleInteractInteractiveLabel "Interactive Update"
#define kParamRectangleInteractInteractiveHint "If checked, update the parameter values during interaction with the image viewer, else update the values when pen is released."

// An optional parameter (of type double, in degrees) giving the rotation angle of the rectangle around its center.
// It is not detected by name: the effect must pass its name to the RectangleInteract constructor.
#define kParamRectangleInteractAngle "rectangleAngle"
#define kParamRectangleInteractAngleLabel "Angle"

namespace OFX {
/**
 * @brief The plug-in using this interact must have the kParamRectangleInteractBtmLeft and
 * kParamRectangleInteractSize parameters. kParamRectangleInteractInteractive,
 * kParamRectangleInteractEnable and kParamHiDPI are optional, and the optional angle
 * parameter is passed by name to the constructor.
 *
 **/
class RectangleInteract
    : public OFX::OverlayInteract
{
public:
    enum MouseStateEnum
    {
        eMouseStateIdle = 0,
        eMouseStateDraggingTopLeft,
        eMouseStateDraggingTopRight,
        eMouseStateDraggingBtmLeft,
        eMouseStateDraggingBtmRight,
        eMouseStateDraggingCenter,
        eMouseStateDraggingTopMid,
        eMouseStateDraggingMidRight,
        eMouseStateDraggingBtmMid,
        eMouseStateDraggingMidLeft
    };

    enum DrawStateEnum
    {
        eDrawStateInactive = 0,
        eDrawStateHoveringTopLeft,
        eDrawStateHoveringTopRight,
        eDrawStateHoveringBtmLeft,
        eDrawStateHoveringBtmRight,
        eDrawStateHoveringCenter,
        eDrawStateHoveringTopMid,
        eDrawStateHoveringMidRight,
        eDrawStateHoveringBtmMid,
        eDrawStateHoveringMidLeft
    };

public:

    RectangleInteract(OfxInteractHandle handle,
                      OFX::ImageEffect* effect,
                      const char* angleParamName = NULL) // name of the optional rotation angle parameter (see kParamRectangleInteractAngle)
        : OFX::OverlayInteract(handle)
        , _effect(effect)
        , _lastMousePos()
        , _mouseState(eMouseStateIdle)
        , _drawState(eDrawStateInactive)
        , _modifierStateCtrl(0)
        , _modifierStateShift(0)
        , _enable(NULL)
        , _btmLeft(NULL)
        , _size(NULL)
        , _interactive(NULL)
        , _hiDPI(NULL)
        , _angle(NULL)
        , _drawMode( OFX::ofxsOverlayDrawModeForNewInteract() )
        , _angleDrag(0.)
    {
        if ( _effect->paramExists(kParamRectangleInteractEnable) ) {
            _enable = effect->fetchBooleanParam(kParamRectangleInteractEnable);
        }
        _btmLeft = effect->fetchDouble2DParam(kParamRectangleInteractBtmLeft);
        _size = effect->fetchDouble2DParam(kParamRectangleInteractSize);
        addParamToSlaveTo(_btmLeft);
        addParamToSlaveTo(_size);
        assert(_btmLeft && _size);
        _interactive = effect->paramExists(kParamRectangleInteractInteractive) ? effect->fetchBooleanParam(kParamRectangleInteractInteractive) : 0;
        if ( effect->paramExists(kParamHiDPI) ) {
            _hiDPI = effect->fetchBooleanParam(kParamHiDPI);
            assert(_hiDPI);
            addParamToSlaveTo(_hiDPI);
        }
        if (angleParamName) {
            _angle = effect->fetchDoubleParam(angleParamName);
            assert(_angle);
            addParamToSlaveTo(_angle);
        }
        _btmLeftDragPos.x = _btmLeftDragPos.y = 0;
        _sizeDrag.x = _sizeDrag.y = 0;
        _centerDrag.x = _centerDrag.y = 0;
        _interactiveDrag = false;
        _dragMoved = false;
    }

    // overridden functions from OFX::Interact to do things
    virtual bool draw(const OFX::DrawArgs &args) OVERRIDE;
    virtual bool penMotion(const OFX::PenArgs &args) OVERRIDE;
    virtual bool penDown(const OFX::PenArgs &args) OVERRIDE;
    virtual bool penUp(const OFX::PenArgs &args) OVERRIDE;
    virtual bool keyDown(const OFX::KeyArgs &args) OVERRIDE;
    virtual bool keyUp(const OFX::KeyArgs & args) OVERRIDE;
    virtual void loseFocus(const FocusArgs &args) OVERRIDE;

protected:


    /**
     * @brief This method returns the bottom left point. The base implementation just returns the value
     * of the _btmLeft parameter at the given time.
     * One could override this function to  do more complex stuff based on other parameters state like the Crop plug-in does.
     **/
    virtual OfxPointD getBtmLeft(OfxTime time) const;

    /**
     * @brief This is called right before any call to allowXXX is made.
     * This way you can query values of a parameter and store it away without having to do this
     * at every allowXXX call.
     **/
    virtual void aboutToCheckInteractivity(OfxTime /*time*/)
    {
    }

    /**
     * @brief These can be overridden to disallow interaction with a point.
     **/
    virtual bool allowTopLeftInteraction() const
    {
        return true;
    }

    virtual bool allowTopRightInteraction() const
    {
        return true;
    }

    virtual bool allowBtmRightInteraction() const
    {
        return true;
    }

    virtual bool allowBtmLeftInteraction() const
    {
        return true;
    }

    virtual bool allowTopMidInteraction() const
    {
        return true;
    }

    virtual bool allowMidRightInteraction() const
    {
        return true;
    }

    virtual bool allowBtmMidInteraction() const
    {
        return true;
    }

    virtual bool allowMidLeftInteraction() const
    {
        return true;
    }

    virtual bool allowCenterInteraction() const
    {
        return true;
    }

    /**
     * @brief Give the cursor back to the host if this interact had set one.
     * Subclasses that skip RectangleInteract::penMotion() (e.g. when their
     * rectangle is disabled) should call it on that path, so that a handle
     * cursor does not stay displayed.
     **/
    void releaseCursor()
    {
        _cursor.release(this);
    }

    /**
     * @brief Abandon a drag in progress, without changing the parameters, and
     * clear the hover state and the cursor. Called when the interact stops
     * handling pen events (disabled, or a subclass condition such as
     * GeneratorInteract's extent): a later hover must not act as a drag.
     **/
    void abandonDrag()
    {
        _mouseState = eMouseStateIdle;
        _drawState = eDrawStateInactive;
        _interactiveDrag = false;
        releaseCursor();
    }

private:
    void setValue(OfxPointD btmLeft, OfxPointD size, const OfxPointD &pscale);

    // get the current rectangle, the rotation center and the rotation angle (in degrees)
    void getRectangle(OfxTime time, double* x1, double* y1, double* w, double* h, OfxPointD* center, double* angle) const;

    /// Report the cursor for the hovered or dragged handle (the host reads it
    /// after each pen or key action, trapped or not). angle is the rotation of
    /// the rectangle in degrees: resize cursors follow the rotated handles.
    void updateCursor(double angle, const OfxPointD& pscale);

private:
    OFX::ImageEffect* _effect;
    OfxPointD _lastMousePos;
    MouseStateEnum _mouseState;
    DrawStateEnum _drawState;
    // counted from keyDown/keyUp: the state without OFX_EXTENSIONS_NATRON (OFXS_HAVE_KEY_MODIFIERS)
    int _modifierStateCtrl;
    int _modifierStateShift;
    bool ctrlDown() const;
    bool shiftDown() const;
    OFX::BooleanParam* _enable;
    OFX::Double2DParam* _btmLeft;
    OFX::Double2DParam* _size;
    OFX::BooleanParam* _interactive;
    OFX::BooleanParam* _hiDPI;
    OFX::DoubleParam* _angle; // optional rotation angle, in degrees
    OFX::InteractCursor _cursor;
    OFX::OverlayDrawModeEnum _drawMode; // set by the descriptor that created the interact, see ofxsOverlayDraw.h
    OfxPointD _btmLeftDragPos;
    OfxPointD _sizeDrag;
    // While dragging, the rectangle (_btmLeftDragPos,_sizeDrag) is expressed in the frame rotated
    // by _angleDrag around _centerDrag (the center of the rectangle when the pen was pressed).
    OfxPointD _centerDrag;
    double _angleDrag;
    bool _interactiveDrag;
    bool _dragMoved; // a pen motion of this drag changed the rectangle: penUp sets the params
};

class RectangleOverlayDescriptor
    : public OFX::DefaultEffectOverlayDescriptor<RectangleOverlayDescriptor, RectangleInteract>
{
};

/// Draws with the DrawSuite when the host provides it; register with
/// ofxsSetOverlayInteractDescriptorV2(desc, new RectangleOverlayDescriptorV2)
class RectangleOverlayDescriptorV2
    : public OFX::DefaultEffectOverlayDescriptorV2<RectangleOverlayDescriptorV2, RectangleInteract>
{
};
} // namespace OFX

#endif /* defined(openfx_supportext_ofxsRectangleInteract_h) */
