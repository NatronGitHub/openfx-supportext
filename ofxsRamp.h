/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of openfx-supportext <https://github.com/devernay/openfx-supportext>,
 * Copyright (C) 2015 INRIA
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

#ifndef openfx_supportext_ofxsRamp_h
#define openfx_supportext_ofxsRamp_h

#include <ofxsInteract.h>
#include <ofxsImageEffect.h>
#include "ofxsMacros.h"
#include "ofxsOGLTextRenderer.h"

#define kParamRampPoint0 "point0"
#define kParamRampPoint0Label "Point 0"

#define kParamRampColor0 "color0"
#define kParamRampColor0Label "Color 0"

#define kParamRampPoint1 "point1"
#define kParamRampPoint1Label "Point 1"

#define kParamRampColor1 "color1"
#define kParamRampColor1Label "Color 1"

#define kParamRampType "type"
#define kParamRampTypeLabel "Type"
#define kParamRampTypeHint "The type of interpolation used to generate the ramp"
#define kParamRampTypeOptionLinear "Linear"
#define kParamRampTypeOptionLinearHint "Linear ramp."
#define kParamRampTypeOptionPLinear "PLinear"
#define kParamRampTypeOptionPLinearHint "Perceptually linear ramp in Rec.709."
#define kParamRampTypeOptionEaseIn "Ease-in"
#define kParamRampTypeOptionEaseInHint "Catmull-Rom spline, smooth start, linear end (a.k.a. smooth0)."
#define kParamRampTypeOptionEaseOut "Ease-out"
#define kParamRampTypeOptionEaseOutHint "Catmull-Rom spline, linear start, smooth end (a.k.a. smooth1)."
#define kParamRampTypeOptionSmooth "Smooth"
#define kParamRampTypeOptionSmoothHint "Traditional smoothstep ramp."
#define kParamRampTypeOptionNone "None"
#define kParamRampTypeOptionNoneHint "No color gradient."

#define kParamRampInteractive "interactive"
#define kParamRampInteractiveLabel "Interactive Update"
#define kParamRampInteractiveHint "If checked, update the parameter values during interaction with the image viewer, else update the values when pen is released."


namespace OFX {
enum RampTypeEnum
{
    eRampTypeLinear = 0,
    eRampTypePLinear,
    eRampTypeEaseIn,
    eRampTypeEaseOut,
    eRampTypeSmooth,
    eRampTypeNone
};

class RampInteract : public OFX::OverlayInteract
{
    enum InteractState
    {
        eInteractStateIdle = 0,
        eInteractStateDraggingPoint0,
        eInteractStateDraggingPoint1
    };
    
    Double2DParam* _point0;
    Double2DParam* _point1;
    ChoiceParam* _type;
    BooleanParam* _interactive;
    OfxPointD _point0DragPos,_point1DragPos;
    bool _interactiveDrag;
    OfxPointD _lastMousePos;
    InteractState _state;
    RampPlugin* _effect;
    
public:
    RampInteract(OfxInteractHandle handle, OFX::ImageEffect* effect)
    : OFX::OverlayInteract(handle)
    , _point0(0)
    , _point1(0)
    , _type(0)
    , _interactive(0)
    , _point0DragPos()
    , _point1DragPos()
    , _interactiveDrag(false)
    , _lastMousePos()
    , _state(eInteractStateIdle)
    , _effect(0)
    {
        _point0 = effect->fetchDouble2DParam(kParamRampPoint0);
        _point1 = effect->fetchDouble2DParam(kParamRampPoint1);
        _type = effect->fetchChoiceParam(kParamRampType);
        _interactive = effect->fetchBooleanParam(kParamRampInteractive);
        assert(_point0 && _point1 && _type && _interactive);
        _effect = dynamic_cast<RampPlugin*>(effect);
        assert(_effect);
    }
    
    /** @brief the function called to draw in the interact */
    virtual bool draw(const DrawArgs &args) OVERRIDE FINAL;
    
    /** @brief the function called to handle pen motion in the interact
     
     returns true if the interact trapped the action in some sense. This will block the action being passed to
     any other interact that may share the viewer.
     */
    virtual bool penMotion(const PenArgs &args) OVERRIDE FINAL;
    
    /** @brief the function called to handle pen down events in the interact
     
     returns true if the interact trapped the action in some sense. This will block the action being passed to
     any other interact that may share the viewer.
     */
    virtual bool penDown(const PenArgs &args) OVERRIDE FINAL;
    
    /** @brief the function called to handle pen up events in the interact
     
     returns true if the interact trapped the action in some sense. This will block the action being passed to
     any other interact that may share the viewer.
     */
    virtual bool penUp(const PenArgs &args) OVERRIDE FINAL;

    virtual void loseFocus(const FocusArgs &args) OVERRIDE FINAL;

};

class RampOverlayDescriptor : public DefaultEffectOverlayDescriptor<RampOverlayDescriptor, RampInteract> {};

} // namespace OFX

#endif /* defined(openfx_supportext_ofxsRamp_h) */
