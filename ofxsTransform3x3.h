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
 * OFX Transform3x3 plugin: a base plugin for 2D homographic transform,
 * represented by a 3x3 matrix.
 */

#ifndef openfx_supportext_ofxsTransform3x3_h
#define openfx_supportext_ofxsTransform3x3_h

#include <memory>

#include <vector>

#include "ofxsImageEffect.h"
#include "ofxsTransform3x3Processor.h"
#include "ofxsShutter.h"
#include "ofxsMacros.h"

#define kParamTransform3x3Invert "invert"
#define kParamTransform3x3InvertLabel "Invert"
#define kParamTransform3x3InvertHint "Invert the transform."

#define kParamTransform3x3MotionBlur "motionBlur"
#define kParamTransform3x3MotionBlurLabel "Motion Blur"
#define kParamTransform3x3MotionBlurHint "Quality of motion blur rendering. 0 disables motion blur, 1 is a good value. Increasing this slows down rendering."

// extra parameters for DirBlur:

#define kParamTransform3x3DirBlurAmount "amount"
#define kParamTransform3x3DirBlurAmountLabel "Amount"
#define kParamTransform3x3DirBlurAmountHint "Amount of blur transform to apply. A value of 1 means to apply the full transform range. A value of 0 means to apply no blur at all. Default is 1."

#define kParamTransform3x3DirBlurCentered "centered"
#define kParamTransform3x3DirBlurCenteredLabel "Centered"
#define kParamTransform3x3DirBlurCenteredHint "When checked, apply directional blur symmetrically around the neutral position."

#define kParamTransform3x3DirBlurFading "fading"
#define kParamTransform3x3DirBlurFadingLabel "Fading"
#define kParamTransform3x3DirBlurFadingHint "Controls the fading function. A value of 1 corresponds to linear fading. A value of 0 disables fading. Default is 0."

// extra parameters for non-DirBlur

#define kParamTransform3x3DirectionalBlur "directionalBlur"
#define kParamTransform3x3DirectionalBlurLabel "Directional Blur Mode"
#define kParamTransform3x3DirectionalBlurHint "Motion blur is computed from the original image to the transformed image, each parameter being interpolated linearly. The motionBlur parameter must be set to a nonzero value, and the blackOutside parameter may have an important effect on the result."

namespace OFX {
////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class Transform3x3Plugin
    : public OFX::ImageEffect
{
protected:
    // do not need to delete these, the ImageEffect is managing them for us
    OFX::Clip *_dstClip;
    OFX::Clip *_srcClip;
    OFX::Clip *_maskClip;

public:

    enum Transform3x3ParamsTypeEnum
    {
        eTransform3x3ParamsTypeNone = 0,
        eTransform3x3ParamsTypeDirBlur,
        eTransform3x3ParamsTypeMotionBlur
    };

    /** @brief ctor */
    Transform3x3Plugin(OfxImageEffectHandle handle,
                       bool masked,
                       Transform3x3ParamsTypeEnum paramsType);

    /** @brief destructor */
    virtual ~Transform3x3Plugin();

    // a default implementation of isIdentity is provided, which may be overridden by the derived class
    virtual bool isIdentity(double /*time*/)
    {
        return false;
    }

    /** @brief recover a transform matrix from an effect */
    virtual bool getInverseTransformCanonical(double time, int view, double amount, bool invert, OFX::Matrix3x3* invtransform) const = 0;


    // The following functions override those in OFX::ImageEffect

    // override the rod call (not final, since overriden by Reformat)
    virtual bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE;

    // override the roi call
    virtual void getRegionsOfInterest(const OFX::RegionsOfInterestArguments &args, OFX::RegionOfInterestSetter &rois) OVERRIDE FINAL;

    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE;

    // override isIdentity
    virtual bool isIdentity(const OFX::IsIdentityArguments &args, OFX::Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
                            , int& view
                            , std::string& plane
#endif
                            ) OVERRIDE FINAL;

#ifdef OFX_EXTENSIONS_NUKE
    /** @brief recover a transform matrix from an effect */
    virtual bool getTransform(const OFX::TransformArguments & args, OFX::Clip * &transformClip, double transformMatrix[9]) OVERRIDE;
#endif

    // override changedParam. note that the derived class MUST explicitly call this method after handling its own parameter changes
    virtual void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) OVERRIDE;

    // this method must be called by the derived class when the transform was changed
    void changedTransform(const OFX::InstanceChangedArgs &args);

protected:
    size_t getInverseTransforms(double time,
                                int view,
                                OfxPointD renderscale,
                                bool fielded,
                                double srcpixelAspectRatio,
                                double dstpixelAspectRatio,
                                bool invert,
                                double shutter,
                                ShutterOffsetEnum shutteroffset,
                                double shuttercustomoffset,
                                OFX::Matrix3x3* invtransform,
                                size_t invtransformsizealloc) const;

    size_t getInverseTransformsBlur(double time,
                                    int view,
                                    OfxPointD renderscale,
                                    bool fielded,
                                    double srcpixelAspectRatio,
                                    double dstpixelAspectRatio,
                                    bool invert,
                                    double amountFrom,
                                    double amountTo,
                                    OFX::Matrix3x3* invtransform,
                                    double* amount,
                                    size_t invtransformsizealloc) const;

    // the inverse transforms of the render (see getRenderInverseTransforms() in the .cpp)
    struct RenderInverseTransforms
    {
        std::vector<OFX::Matrix3x3> invtransform; // in pixel coordinates, from the output to the source
        std::vector<double> invtransformalpha; // their weights (empty: uniform)
        double motionblur; // the motion blur quality (0 for a single transform)
    };

    RenderInverseTransforms getRenderInverseTransforms(double time, int view, const OfxPointD& renderScale, bool fielded,
                                                       double srcpixelAspectRatio, double dstpixelAspectRatio) const;

    // how much the prefilter widens the source RoI, in canonical coordinates (see the .cpp)
    OfxPointD getPrefilterRoIExpansion(double time, int view, const OfxPointD& renderScale, int prefilterRadius) const;

    // The exact RoD margins, off by default (the RoD of the released plug-in versions). The subclass's
    // constructor sets it according to its plug-in version, since it changes the rendered output:
    // - with Black Outside, the RoD contains the filter's soft edge beyond the transformed source
    //   (the source RoD is expanded by the filter's support, and by the prefilter widening), instead
    //   of one pixel only, which cuts it when scaling up by more than 2x (SupportExt#258);
    // - with motion blur or directional blur, the margin between the sampled transforms of the RoD
    //   and RoI is measured in the image plane (x/z, y/z), not in homogeneous coordinates, whose
    //   arbitrary scale made it too large or too small for projective transforms (SupportExt#259).
    void setExactRoDMargins(bool exact)
    {
        _exactRoDMargins = exact;
    }

private:
    /* internal render function */
    template <class PIX, int nComponents, int maxValue, bool masked>
    void renderInternalForBitDepth(const OFX::RenderArguments &args);

    template <int nComponents, bool masked>
    void renderInternal(const OFX::RenderArguments &args, OFX::BitDepthEnum dstBitDepth);

    /* set up and run a processor */
    void setupAndProcess(Transform3x3ProcessorBase &, const OFX::RenderArguments &args);

    bool isIdentity(double time, OFX::Clip * &identityClip, double &identityTime);

    void transformRegion(const OfxRectD &rectFrom,
                         double time,
                         int view,
                         bool invert,
                         double motionblur,
                         bool directionalBlur,
                         double amountFrom,
                         double amountTo,
                         double shutter,
                         ShutterOffsetEnum shutteroffset,
                         double shuttercustomoffset,
                         bool isIdentity,
                         OfxRectD *rectTo);

protected:
    // Transform3x3-GENERIC
    Transform3x3ParamsTypeEnum _paramsType;
    OFX::BooleanParam* _invert;
    // GENERIC
    OFX::ChoiceParam* _filter;
    OFX::BooleanParam* _clamp;
    OFX::BooleanParam* _blackOutside;
    OFX::BooleanParam* _prefilter; // may be NULL (kParamFilterPrefilter, optional)
    OFX::DoubleParam* _motionblur;
    OFX::DoubleParam* _dirBlurAmount; // DirBlur only
    OFX::BooleanParam* _dirBlurCentered; // DirBlur only
    OFX::DoubleParam* _dirBlurFading; // DirBlur only
    OFX::BooleanParam* _directionalBlur; // non-DirBlur
    OFX::DoubleParam* _shutter; // non-DirBlur
    OFX::ChoiceParam* _shutteroffset; // non-DirBlur
    OFX::DoubleParam* _shuttercustomoffset; // non-DirBlur
    bool _masked;
    OFX::DoubleParam* _mix;
    OFX::BooleanParam* _maskApply;
    OFX::BooleanParam* _maskInvert;

private:
    bool _exactRoDMargins; // see setExactRoDMargins()
};

void Transform3x3Describe(OFX::ImageEffectDescriptor &desc, bool masked);

OFX::PageParamDescriptor * Transform3x3DescribeInContextBegin(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context, bool masked);

// Which filtering parameters Transform3x3DescribeInContextEnd() defines: the default keeps the parameters
// of the previous versions; a plug-in opts in to the sinc-based filters (Lanczos4, Lanczos6, Sinc4,
// appended to Filter) and the Prefilter Downscaling checkbox, right after Black Outside, so that the
// parameters that Nuke also has keep its order. OFX hosts identify parameters by name, so a parameter
// added in the middle doesn't affect saved projects. prefilterDefault is the checkbox's default (with
// eTransform3x3FilteringSincPrefilter only).
enum Transform3x3FilteringEnum
{
    eTransform3x3FilteringCubic = 0, // Impulse to Notch, supersampling when scaling down
    eTransform3x3FilteringSincPrefilter, // also Lanczos4, Lanczos6, Sinc4 and Prefilter Downscaling
};

void Transform3x3DescribeInContextEnd(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context, OFX::PageParamDescriptor* page, bool masked, OFX::Transform3x3Plugin::Transform3x3ParamsTypeEnum paramsType,
                                      Transform3x3FilteringEnum filtering = eTransform3x3FilteringCubic,
                                      bool prefilterDefault = false);
} // namespace OFX
#endif /* defined(openfx_supportext_ofxsTransform3x3_h) */
