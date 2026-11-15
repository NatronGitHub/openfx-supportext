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

// an optional parameter (of type double, in degrees) giving the rotation angle of the rectangle around its center.
// The interact uses it only if its name is given in RectangleInteractParams::angle (see below).
#define kParamRectangleInteractAngle "rectangleAngle"
#define kParamRectangleInteractAngleLabel "Angle"
#define kParamRectangleInteractAngleHint "Rotation angle of the shape around its center, in degrees (counter-clockwise)."

namespace OFX {
/**
 * @brief The names of the parameters used by RectangleInteract.
 * The defaults are the names defined above. The angle parameter is not used by default:
 * set angle to the name of a double parameter (e.g. kParamRectangleInteractAngle) to draw and
 * manipulate a rectangle rotated by that angle (in degrees) around its center.
 **/
struct RectangleInteractParams
{
    const char* enable; // optional boolean parameter, used if it exists
    const char* btmLeft; // Double2D parameter
    const char* size; // Double2D parameter
    const char* interactive; // optional boolean parameter, used if it exists
    const char* angle; // optional double parameter, used if not NULL

    RectangleInteractParams()
        : enable(kParamRectangleInteractEnable)
        , btmLeft(kParamRectangleInteractBtmLeft)
        , size(kParamRectangleInteractSize)
        , interactive(kParamRectangleInteractInteractive)
        , angle(NULL)
    {
    }
};

/**
 * @brief In order to work the plug-in using this interact must have the bottom left and size
 * parameters named in RectangleInteractParams (by default, named after the defines above).
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
                      const RectangleInteractParams& params = RectangleInteractParams())
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
        , _angleDrag(0.)
    {
        if ( _effect->paramExists(params.enable) ) {
            _enable = effect->fetchBooleanParam(params.enable);
        }
        _btmLeft = effect->fetchDouble2DParam(params.btmLeft);
        _size = effect->fetchDouble2DParam(params.size);
        addParamToSlaveTo(_btmLeft);
        addParamToSlaveTo(_size);
        assert(_btmLeft && _size);
        _interactive = effect->paramExists(params.interactive) ? effect->fetchBooleanParam(params.interactive) : 0;
        if ( effect->paramExists(kParamHiDPI) ) {
            _hiDPI = effect->fetchBooleanParam(kParamHiDPI);
            assert(_hiDPI);
            addParamToSlaveTo(_hiDPI);
        }
        if (params.angle) {
            _angle = effect->fetchDoubleParam(params.angle);
            assert(_angle);
            addParamToSlaveTo(_angle);
        }
        _btmLeftDragPos.x = _btmLeftDragPos.y = 0;
        _sizeDrag.x = _sizeDrag.y = 0;
        _centerDrag.x = _centerDrag.y = 0;
        _interactiveDrag = false;
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
     * @brif These can be overriden to disallow interaction with a point.
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

private:
    void setValue(OfxPointD btmLeft, OfxPointD size, const OfxPointD &pscale);

    // get the current rectangle, the rotation center and the rotation angle (in degrees)
    void getRectangle(OfxTime time, double* x1, double* y1, double* w, double* h, OfxPointD* center, double* angle) const;

private:
    OFX::ImageEffect* _effect;
    OfxPointD _lastMousePos;
    MouseStateEnum _mouseState;
    DrawStateEnum _drawState;
    int _modifierStateCtrl;
    int _modifierStateShift;
    OFX::BooleanParam* _enable;
    OFX::Double2DParam* _btmLeft;
    OFX::Double2DParam* _size;
    OFX::BooleanParam* _interactive;
    OFX::BooleanParam* _hiDPI;
    OFX::DoubleParam* _angle; // optional rotation angle, in degrees
    OfxPointD _btmLeftDragPos;
    OfxPointD _sizeDrag;
    // While dragging, the rectangle (_btmLeftDragPos,_sizeDrag) is expressed in the frame rotated
    // by _angleDrag around _centerDrag (the center of the rectangle when the pen was pressed).
    OfxPointD _centerDrag;
    double _angleDrag;
    bool _interactiveDrag;
};

class RectangleOverlayDescriptor
    : public OFX::DefaultEffectOverlayDescriptor<RectangleOverlayDescriptor, RectangleInteract>
{
};
} // namespace OFX

#endif /* defined(openfx_supportext_ofxsRectangleInteract_h) */
