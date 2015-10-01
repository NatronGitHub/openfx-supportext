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
 * OFX TransformInteract.
 */

#ifndef openfx_supportext_ofxsTransformInteract_h
#define openfx_supportext_ofxsTransformInteract_h

#include <cmath>

#include "ofxsImageEffect.h"
#include "ofxsMacros.h"

// NON-GENERIC (Transform DirBlur and GodRays only)
#define kParamTransformTranslate "translate"
#define kParamTransformTranslateLabel "Translate"
#define kParamTransformRotate "rotate"
#define kParamTransformRotateLabel "Rotate"
#define kParamTransformScale "scale"
#define kParamTransformScaleLabel "Scale"
#define kParamTransformScaleUniform "uniform"
#define kParamTransformScaleUniformLabel "Uniform"
#define kParamTransformScaleUniformHint "Use the X scale for both directions"
#define kParamTransformSkewX "skewX"
#define kParamTransformSkewXLabel "Skew X"
#define kParamTransformSkewY "skewY"
#define kParamTransformSkewYLabel "Skew Y"
#define kParamTransformSkewOrder "skewOrder"
#define kParamTransformSkewOrderLabel "Skew Order"
#define kParamTransformCenter "center"
#define kParamTransformCenterLabel "Center"
#define kParamTransformResetCenter "resetCenter"
#define kParamTransformResetCenterLabel "Reset Center"
#define kParamTransformResetCenterHint "Reset the position of the center to the center of the input region of definition"
#define kParamTransformInteractive "interactive"
#define kParamTransformInteractiveLabel "Interactive Update"
#define kParamTransformInteractiveHint "If checked, update the parameter values during interaction with the image viewer, else update the values when pen is released."

namespace OFX {

inline void ofxsTransformGetScale(const OfxPointD &scaleParam, bool scaleUniform, OfxPointD* scale)
{
    const double SCALE_MIN = 0.0001;
    scale->x = scaleParam.x;
    if (std::fabs(scale->x) < SCALE_MIN) {
        scale->x = (scale->x >= 0) ? SCALE_MIN : -SCALE_MIN;
    }
    if (scaleUniform) {
        scale->y = scaleParam.x;
    } else {
        scale->y = scaleParam.y;
    }
    if (std::fabs(scale->y) < SCALE_MIN) {
        scale->y = (scale->y >= 0) ? SCALE_MIN : -SCALE_MIN;
    }
}

class TransformInteract : public OFX::OverlayInteract
{
protected:
    enum DrawStateEnum {
        eInActive = 0, //< nothing happening
        eCircleHovered, //< the scale circle is hovered
        eLeftPointHovered, //< the left point of the circle is hovered
        eRightPointHovered, //< the right point of the circle is hovered
        eBottomPointHovered, //< the bottom point of the circle is hovered
        eTopPointHovered, //< the top point of the circle is hovered
        eCenterPointHovered, //< the center point of the circle is hovered
        eRotationBarHovered, //< the rotation bar is hovered
        eSkewXBarHoverered, //< the skew bar is hovered
        eSkewYBarHoverered //< the skew bar is hovered
    };
    
    enum MouseStateEnum {
        eReleased = 0,
        eDraggingCircle,
        eDraggingLeftPoint,
        eDraggingRightPoint,
        eDraggingTopPoint,
        eDraggingBottomPoint,
        eDraggingTranslation,
        eDraggingCenter,
        eDraggingRotationBar,
        eDraggingSkewXBar,
        eDraggingSkewYBar
    };
    
    enum OrientationEnum
    {
        eOrientationAllDirections = 0,
        eOrientationNotSet,
        eOrientationHorizontal,
        eOrientationVertical
    };
    
    DrawStateEnum _drawState;
    MouseStateEnum _mouseState;
    int _modifierStateCtrl;
    int _modifierStateShift;
    OrientationEnum _orientation;
    ImageEffect* _plugin;
    OfxPointD _lastMousePos;

    OfxPointD _centerDrag;
    OfxPointD _translateDrag;
    OfxPointD _scaleParamDrag;
    bool _scaleUniformDrag;
    double _rotateDrag;
    double _skewXDrag;
    double _skewYDrag;
    int _skewOrderDrag;
    bool _invertedDrag;
    bool _interactiveDrag;

public:
    TransformInteract(OfxInteractHandle handle, OFX::ImageEffect* effect);

    // overridden functions from OFX::Interact to do things
    virtual bool draw(const OFX::DrawArgs &args) OVERRIDE FINAL;
    virtual bool penMotion(const OFX::PenArgs &args) OVERRIDE FINAL;
    virtual bool penDown(const OFX::PenArgs &args) OVERRIDE FINAL;
    virtual bool penUp(const OFX::PenArgs &args) OVERRIDE FINAL;
    virtual bool keyDown(const OFX::KeyArgs &args) OVERRIDE FINAL;
    virtual bool keyUp(const OFX::KeyArgs &args) OVERRIDE FINAL;

    /** @brief Called when the interact is loses input focus */
    virtual void loseFocus(const FocusArgs &args) OVERRIDE FINAL;

private:
    // NON-GENERIC
    OFX::Double2DParam* _translate;
    OFX::DoubleParam* _rotate;
    OFX::Double2DParam* _scale;
    OFX::BooleanParam* _scaleUniform;
    OFX::DoubleParam* _skewX;
    OFX::DoubleParam* _skewY;
    OFX::ChoiceParam* _skewOrder;
    OFX::Double2DParam* _center;
    OFX::BooleanParam* _invert;
    OFX::BooleanParam* _interactive;
};

class TransformOverlayDescriptor : public DefaultEffectOverlayDescriptor<TransformOverlayDescriptor, TransformInteract> {};

}
#endif /* defined(openfx_supportext_ofxsTransformInteract_h) */
