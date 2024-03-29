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
 * OFX TransformInteract.
 */

#ifndef openfx_supportext_ofxsTransformInteract_h
#define openfx_supportext_ofxsTransformInteract_h

#include <cmath>

#include "ofxsImageEffect.h"
#include "ofxsMacros.h"

#define kParamTransformTranslate "transformTranslate"
#define kParamTransformTranslateLabel "Translate"
#define kParamTransformTranslateHint "Translation along the x and y axes in pixels. Can also be adjusted by clicking and dragging the center handle in the Viewer."
#define kParamTransformRotate "transformRotate"
#define kParamTransformRotateLabel "Rotate"
#define kParamTransformRotateHint "Rotation angle in degrees around the Center. Can also be adjusted by clicking and dragging the rotation bar in the Viewer."
#define kParamTransformScale "transformScale"
#define kParamTransformScaleLabel "Scale"
#define kParamTransformScaleHint "Scale factor along the x and y axes. Can also be adjusted by clicking and dragging the outer circle or the diameter handles in the Viewer."
#define kParamTransformScaleUniform "transformScaleUniform"
#define kParamTransformScaleUniformLabel "Uniform"
#define kParamTransformScaleUniformHint "Use the X scale for both directions"
#define kParamTransformSkewX "transformSkewX"
#define kParamTransformSkewXLabel "Skew X"
#define kParamTransformSkewXHint "Skew along the x axis. Can also be adjusted by clicking and dragging the skew bar in the Viewer."
#define kParamTransformSkewY "transformSkewY"
#define kParamTransformSkewYLabel "Skew Y"
#define kParamTransformSkewYHint "Skew along the y axis."
#define kParamTransformSkewOrder "transformSkewOrder"
#define kParamTransformSkewOrderLabel "Skew Order"
#define kParamTransformSkewOrderHint "The order in which skew transforms are applied: X then Y, or Y then X."
#define kParamTransformAmount "transformAmount"
#define kParamTransformAmountLabel "Amount"
#define kParamTransformAmountHint "Amount of transform to apply. 0 means the transform is identity, 1 means to apply the full transform."
#define kParamTransformCenter "transformCenter"
#define kParamTransformCenterLabel "Center"
#define kParamTransformCenterHint "Center of rotation and scale."
#define kParamTransformCenterChanged "transformCenterChanged"
#define kParamTransformResetCenter "transformResetCenter"
#define kParamTransformResetCenterLabel "Reset Center"
#define kParamTransformResetCenterHint "Reset the position of the center to the center of the input region of definition"
#define kParamTransformInteractOpen "transformInteractOpen"
#define kParamTransformInteractOpenLabel "Show Interact"
#define kParamTransformInteractOpenHint "If checked, the transform interact is displayed over the image."
#define kParamTransformInteractive "transformInteractive"
#define kParamTransformInteractiveLabel "Interactive Update"
#define kParamTransformInteractiveHint "If checked, update the parameter values during interaction with the image viewer, else update the values when pen is released."

// old parameter names (Transform DirBlur and GodRays only)
#define kParamTransformTranslateOld "translate"
#define kParamTransformRotateOld "rotate"
#define kParamTransformScaleOld "scale"
#define kParamTransformScaleUniformOld "uniform"
#define kParamTransformSkewXOld "skewX"
#define kParamTransformSkewYOld "skewY"
#define kParamTransformSkewOrderOld "skewOrder"
#define kParamTransformCenterOld "center"
#define kParamTransformResetCenterOld "resetCenter"
#define kParamTransformInteractiveOld "interactive"

namespace OFX {
struct TransformParams {
    OfxPointD center;
    OfxPointD translate;
    OfxPointD scale;
    bool scaleUniform;
    double rotate;
    double skewX, skewY;
    int skewOrder;
    bool inverted;
    TransformParams() {
        center = {0., 0.};
        translate = {0., 0.};
        scale = {1., 1.};
        scaleUniform = false;
        rotate = 0.;
        skewX = 0.;
        skewY = 0.;
        skewOrder = 0;
        inverted = false;
    }
};

inline void
ofxsTransformGetScale(const OfxPointD &scaleParam,
                      bool scaleUniform,
                      OfxPointD* scale)
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

/// add Transform params. page and group are optional
void ofxsTransformDescribeParams(OFX::ImageEffectDescriptor &desc, OFX::PageParamDescriptor *page, OFX::GroupParamDescriptor *group, bool isOpen, bool oldParams, bool hasAmount, bool noTranslate);

class TransformInteractHelper
    : private OFX::InteractAbstract
{
protected:
    enum DrawStateEnum
    {
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

    enum MouseStateEnum
    {
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
    ImageEffect* _effect;
    Interact* _interact;
    OfxPointD _lastMousePos;
    TransformParams _tpDrag;
    bool _interactiveDrag;

public:
    TransformInteractHelper(OFX::ImageEffect* effect, OFX::Interact* interact, bool oldParams = false);

    /** @brief virtual destructor */
    virtual ~TransformInteractHelper()
    {
        // fetched clips and params are owned and deleted by the ImageEffect and its ParamSet
    }

    // overridden functions from OFX::Interact to do things
    virtual bool draw(const OFX::DrawArgs &args) OVERRIDE;
    virtual bool penMotion(const OFX::PenArgs &args) OVERRIDE;
    virtual bool penDown(const OFX::PenArgs &args) OVERRIDE;
    virtual bool penUp(const OFX::PenArgs &args) OVERRIDE;
    virtual bool keyDown(const OFX::KeyArgs &args) OVERRIDE;
    virtual bool keyUp(const OFX::KeyArgs &args) OVERRIDE;
    virtual bool keyRepeat(const KeyArgs & /*args*/) OVERRIDE { return false; }

    virtual void gainFocus(const FocusArgs & /*args*/) OVERRIDE {}

    virtual void loseFocus(const FocusArgs &args) OVERRIDE;

private:
    void getTransformParams(double time, TransformParams& tp);

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
    OFX::BooleanParam* _interactOpen;
    OFX::BooleanParam* _interactive;
    OFX::BooleanParam* _hiDPI;
};

typedef OverlayInteractFromHelper<TransformInteractHelper> TransformInteract;

class TransformOverlayDescriptor
    : public DefaultEffectOverlayDescriptor<TransformOverlayDescriptor, TransformInteract>
{
};

class TransformInteractHelperOldParams
    : public TransformInteractHelper
{
public:
    TransformInteractHelperOldParams(OFX::ImageEffect* effect,
                                     OFX::Interact* interact)
        : TransformInteractHelper(effect, interact, true) {}
};

typedef OverlayInteractFromHelper<TransformInteractHelperOldParams> TransformInteractOldParams;

class TransformOverlayDescriptorOldParams
    : public DefaultEffectOverlayDescriptor<TransformOverlayDescriptorOldParams, TransformInteractOldParams>
{
};
} // namespace OFX
#endif /* defined(openfx_supportext_ofxsTransformInteract_h) */
