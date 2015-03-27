/*
   OFX Transform3x3 plugin: a base plugin for 2D homographic transform,
   represented by a 3x3 matrix.

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

/*
 Although the indications from nuke/fnOfxExtensions.h were followed, and the
 kFnOfxImageEffectActionGetTransform action was implemented in the Support
 library, that action is never called by the Nuke host.

 The extension was implemented as specified in Natron and in the Support library.
 
 @see gHostDescription.canTransform, ImageEffectDescriptor::setCanTransform(),
 and ImageEffect::getTransform().

 There is also an open question about how the last plugin in a transform chain
 may get the concatenated transform from upstream, the untransformed source image,
 concatenate its own transform and apply the resulting transform in its render
 action.
 
 Our solution is to have kFnOfxImageEffectCanTransform set on source clips for which
 a transform can be attached to fetched images.
 @see ClipDescriptor::setCanTransform().

 In this case, images fetched from the host may have a kFnOfxPropMatrix2D attached,
 which must be combined with the transformation applied by the effect (which
 may be any deformation function, not only a homography).
 @see ImageBase::getTransform() and ImageBase::getTransformIsIdentity
 */
// Uncomment the following to enable the experimental host transform code.
#define ENABLE_HOST_TRANSFORM

#include <memory>

#include "ofxsTransform3x3.h"
#include "ofxsTransform3x3Processor.h"
#include "ofxsMerging.h"


#ifndef ENABLE_HOST_TRANSFORM
#undef OFX_EXTENSIONS_NUKE // host transform is the only nuke extension used
#endif

#ifdef OFX_EXTENSIONS_NUKE
#include "nuke/fnOfxExtensions.h"
#endif

#define kSupportsTiles 1
#define kSupportsMultiResolution 1
#define kSupportsRenderScale 1
#define kRenderThreadSafety eRenderFullySafe

using namespace OFX;

// It would be nice to be able to cache the set of transforms (with motion blur) used to compute the
// current frame between two renders.
// Unfortunately, we cannot rely on the host sending changedParam() when the animation changes
// (Nuke doesn't call the action when a linked animation is changed),
// nor on dst->getUniqueIdentifier (which is "ffffffffffffffff" on Nuke)

#define kTransform3x3MotionBlurCount 1000 // number of transforms used in the motion

static void
shutterRange(double time,
             double shutter,
             int shutteroffset_i,
             double shuttercustomoffset,
             OfxRangeD* range)
{
    Transform3x3ShutterOffsetEnum shutteroffset = (Transform3x3ShutterOffsetEnum)shutteroffset_i;

    switch (shutteroffset) {
    case eTransform3x3ShutterOffsetCentered:
        range->min = time - shutter / 2;
        range->max = time + shutter / 2;
        break;
    case eTransform3x3ShutterOffsetStart:
        range->min = time;
        range->max = time + shutter;
        break;
    case eTransform3x3ShutterOffsetEnd:
        range->min = time - shutter;
        range->max = time;
        break;
    case eTransform3x3ShutterOffsetCustom:
        range->min = time + shuttercustomoffset;
        range->max = time + shuttercustomoffset + shutter;
        break;
    default:
        range->min = time;
        range->max = time;
        break;
    }
}

Transform3x3Plugin::Transform3x3Plugin(OfxImageEffectHandle handle,
                                       bool masked,
                                       bool isDirBlur)
    : ImageEffect(handle)
      , _dstClip(0)
      , _srcClip(0)
      , _maskClip(0)
      , _invert(0)
      , _filter(0)
      , _clamp(0)
      , _blackOutside(0)
      , _motionblur(0)
      , _directionalBlur(0)
      , _shutter(0)
      , _shutteroffset(0)
      , _shuttercustomoffset(0)
      , _masked(masked)
      , _mix(0)
      , _maskInvert(0)
{
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    assert(_dstClip->getPixelComponents() == ePixelComponentAlpha || _dstClip->getPixelComponents() == ePixelComponentRGB || _dstClip->getPixelComponents() == ePixelComponentRGBA);
    _srcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
    assert(_srcClip->getPixelComponents() == ePixelComponentAlpha || _srcClip->getPixelComponents() == ePixelComponentRGB || _srcClip->getPixelComponents() == ePixelComponentRGBA);
    // name of mask clip depends on the context
    if (masked) {
        _maskClip = getContext() == OFX::eContextFilter ? NULL : fetchClip(getContext() == OFX::eContextPaint ? "Brush" : "Mask");
        assert(!_maskClip || _maskClip->getPixelComponents() == ePixelComponentAlpha);
    }

    if (paramExists(kParamTransform3x3Invert)) {
        // Transform3x3-GENERIC
        _invert = fetchBooleanParam(kParamTransform3x3Invert);
        // GENERIC
        _filter = fetchChoiceParam(kParamFilterType);
        _clamp = fetchBooleanParam(kParamFilterClamp);
        _blackOutside = fetchBooleanParam(kParamFilterBlackOutside);
        assert(_invert && _filter && _clamp && _blackOutside);
        if (paramExists(kParamTransform3x3MotionBlur)) {
            _motionblur = fetchDoubleParam(kParamTransform3x3MotionBlur); // GodRays may not have have _motionblur
            assert(_motionblur);
        }
        if (!isDirBlur) {
            _directionalBlur = fetchBooleanParam(kParamTransform3x3DirectionalBlur);
            _shutter = fetchDoubleParam(kParamTransform3x3Shutter);
            _shutteroffset = fetchChoiceParam(kParamTransform3x3ShutterOffset);
            _shuttercustomoffset = fetchDoubleParam(kParamTransform3x3ShutterCustomOffset);
            assert(_directionalBlur && _shutter && _shutteroffset && _shuttercustomoffset);
        }
        if (masked) {
            _mix = fetchDoubleParam(kParamMix);
            _maskInvert = fetchBooleanParam(kParamMaskInvert);
            assert(_mix && _maskInvert);
        }

        if (!isDirBlur) {
            bool directionalBlur;
            _directionalBlur->getValue(directionalBlur);
            _shutter->setEnabled(!directionalBlur);
            _shutteroffset->setEnabled(!directionalBlur);
            _shuttercustomoffset->setEnabled(!directionalBlur);
        }
    }
}

Transform3x3Plugin::~Transform3x3Plugin()
{
}

////////////////////////////////////////////////////////////////////////////////
/** @brief render for the filter */

////////////////////////////////////////////////////////////////////////////////
// basic plugin render function, just a skelington to instantiate templates from

/* set up and run a processor */
void
Transform3x3Plugin::setupAndProcess(Transform3x3ProcessorBase &processor,
                                    const OFX::RenderArguments &args)
{
    assert(!_invert || _motionblur); // this method should be overridden in GodRays
    const double time = args.time;
    std::auto_ptr<OFX::Image> dst( _dstClip->fetchImage(time) );

    if ( !dst.get() ) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    OFX::BitDepthEnum         dstBitDepth    = dst->getPixelDepth();
    OFX::PixelComponentEnum   dstComponents  = dst->getPixelComponents();
    if (dstBitDepth != _dstClip->getPixelDepth() ||
        dstComponents != _dstClip->getPixelComponents()) {
        setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong depth or components");
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    if ( (dst->getRenderScale().x != args.renderScale.x) ||
         ( dst->getRenderScale().y != args.renderScale.y) ||
         ( (dst->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && dst->getField() != args.fieldToRender)) ) {
        setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    std::auto_ptr<const OFX::Image> src((_srcClip && _srcClip->isConnected()) ?
                                        _srcClip->fetchImage(args.time) : 0);
    size_t invtransformsizealloc = 0;
    size_t invtransformsize = 0;
    std::vector<OFX::Matrix3x3> invtransform;
    double motionblur = 0.;
    bool directionalBlur = (_directionalBlur == 0);
    bool blackOutside = false;
    double mix = 1.;

    if ( !src.get() ) {
        // no source image, use a dummy transform
        invtransformsizealloc = 1;
        invtransform.resize(invtransformsizealloc);
        invtransformsize = 1;
        invtransform[0].a = 0.;
        invtransform[0].b = 0.;
        invtransform[0].c = 0.;
        invtransform[0].d = 0.;
        invtransform[0].e = 0.;
        invtransform[0].f = 0.;
        invtransform[0].g = 0.;
        invtransform[0].h = 0.;
        invtransform[0].i = 1.;
    } else {
        OFX::BitDepthEnum dstBitDepth       = dst->getPixelDepth();
        OFX::PixelComponentEnum dstComponents  = dst->getPixelComponents();
        OFX::BitDepthEnum srcBitDepth      = src->getPixelDepth();
        OFX::PixelComponentEnum srcComponents = src->getPixelComponents();
        if ( (srcBitDepth != dstBitDepth) || (srcComponents != dstComponents) ) {
            OFX::throwSuiteStatusException(kOfxStatFailed);
        }

        bool invert = false;
        if (_invert) {
            _invert->getValueAtTime(time, invert);
        }

        if (_blackOutside) {
            _blackOutside->getValueAtTime(time, blackOutside);
        }
        if (_masked && _mix) {
            _mix->getValueAtTime(time, mix);
        }
        if (_motionblur) {
            _motionblur->getValueAtTime(time, motionblur);
        }
        if (_directionalBlur) {
            _directionalBlur->getValueAtTime(time, directionalBlur);
        }
        double shutter = 0.;
        if (!directionalBlur) {
            if (_shutter) {
                _shutter->getValueAtTime(time, shutter);
            }
        }
        const bool fielded = args.fieldToRender == OFX::eFieldLower || args.fieldToRender == OFX::eFieldUpper;
        const double pixelAspectRatio = src->getPixelAspectRatio();

        if ( (shutter != 0.) && (motionblur != 0.) ) {
            invtransformsizealloc = kTransform3x3MotionBlurCount;
            invtransform.resize(invtransformsizealloc);
            int shutteroffset_i;
            assert(_shutteroffset);
            _shutteroffset->getValueAtTime(time, shutteroffset_i);
            double shuttercustomoffset;
            assert(_shuttercustomoffset);
            _shuttercustomoffset->getValueAtTime(time, shuttercustomoffset);

            invtransformsize = getInverseTransforms(time, args.renderScale, fielded, pixelAspectRatio, invert, shutter, shutteroffset_i, shuttercustomoffset, &invtransform.front(), invtransformsizealloc);
        } else if (directionalBlur) {
            invtransformsizealloc = kTransform3x3MotionBlurCount;
            invtransform.resize(invtransformsizealloc);
            invtransformsize = getInverseTransformsBlur(time, args.renderScale, fielded, pixelAspectRatio, invert, &invtransform.front(), invtransformsizealloc);
        } else {
            invtransformsizealloc = 1;
            invtransform.resize(invtransformsizealloc);
            invtransformsize = 1;
            bool success = getInverseTransformCanonical(time, 1., invert, &invtransform[0]); // virtual function
            if (!success) {
                invtransform[0].a = 0.;
                invtransform[0].b = 0.;
                invtransform[0].c = 0.;
                invtransform[0].d = 0.;
                invtransform[0].e = 0.;
                invtransform[0].f = 0.;
                invtransform[0].g = 0.;
                invtransform[0].h = 0.;
                invtransform[0].i = 1.;
            } else {
                OFX::Matrix3x3 canonicalToPixel = OFX::ofxsMatCanonicalToPixel(pixelAspectRatio, args.renderScale.x,
                                                                               args.renderScale.y, fielded);
                OFX::Matrix3x3 pixelToCanonical = OFX::ofxsMatPixelToCanonical(pixelAspectRatio,  args.renderScale.x,
                                                                               args.renderScale.y, fielded);
                invtransform[0] = canonicalToPixel * invtransform[0] * pixelToCanonical;
            }
        }
        if (invtransformsize == 1) {
            motionblur  = 0.;
        }
        // compose with the input transform
        if ( !src->getTransformIsIdentity() ) {
            double srcTransform[9]; // transform to apply to the source image, in pixel coordinates, from source to destination
            src->getTransform(srcTransform);
            OFX::Matrix3x3 srcTransformMat;
            srcTransformMat.a = srcTransform[0];
            srcTransformMat.b = srcTransform[1];
            srcTransformMat.c = srcTransform[2];
            srcTransformMat.d = srcTransform[3];
            srcTransformMat.e = srcTransform[4];
            srcTransformMat.f = srcTransform[5];
            srcTransformMat.g = srcTransform[6];
            srcTransformMat.h = srcTransform[7];
            srcTransformMat.i = srcTransform[8];
            // invert it
            double det = ofxsMatDeterminant(srcTransformMat);
            if (det != 0.) {
                OFX::Matrix3x3 srcTransformInverse = ofxsMatInverse(srcTransformMat, det);

                for (size_t i = 0; i < invtransformsize; ++i) {
                    invtransform[i] = srcTransformInverse * invtransform[i];
                }
            }
        }
    }

    // auto ptr for the mask.
    std::auto_ptr<const OFX::Image> mask((_masked && getContext() != OFX::eContextFilter && _maskClip && _maskClip->isConnected()) ?
                                         _maskClip->fetchImage(time) : 0);

    // do we do masking
    if ( _masked && getContext() != OFX::eContextFilter && _maskClip && _maskClip->isConnected() ) {
        bool maskInvert = false;
        if (_maskInvert) {
            _maskInvert->getValueAtTime(time, maskInvert);
        }
        // say we are masking
        processor.doMasking(true);

        // Set it in the processor
        processor.setMaskImg(mask.get(), maskInvert);
    }

    // set the images
    processor.setDstImg( dst.get() );
    processor.setSrcImg( src.get() );

    // set the render window
    processor.setRenderWindow(args.renderWindow);
    assert(invtransform.size() && invtransformsize);
    processor.setValues(&invtransform.front(),
                        invtransformsize,
                        blackOutside,
                        motionblur,
                        mix);

    // Call the base class process member, this will call the derived templated process code
    processor.process();
} // setupAndProcess

// compute the bounding box of the transform of four points
static void
ofxsTransformRegionFromPoints(const OFX::Point3D p[4],
                              OfxRectD &rod)
{
    // extract the x/y bounds
    double x1, y1, x2, y2;

    // if all z's have the same sign, we can compute a reasonable ROI, else we give the whole image (the line at infinity crosses the rectangle)
    bool allpositive = true;
    bool allnegative = true;

    for (int i = 0; i < 4; ++i) {
        allnegative = allnegative && (p[i].z < 0.);
        allpositive = allpositive && (p[i].z > 0.);
    }

    if (!allpositive && !allnegative) {
        // the line at infinity crosses the source RoD
        x1 = kOfxFlagInfiniteMin;
        x2 = kOfxFlagInfiniteMax;
        y1 = kOfxFlagInfiniteMin;
        y2 = kOfxFlagInfiniteMax;
    } else {
        OfxPointD q[4];
        for (int i = 0; i < 4; ++i) {
            q[i].x = p[i].x / p[i].z;
            q[i].y = p[i].y / p[i].z;
        }

        x1 = x2 = q[0].x;
        y1 = y2 = q[0].y;
        for (int i = 1; i < 4; ++i) {
            if (q[i].x < x1) {
                x1 = q[i].x;
            } else if (q[i].x > x2) {
                x2 = q[i].x;
            }
            if (q[i].y < y1) {
                y1 = q[i].y;
            } else if (q[i].y > y2) {
                y2 = q[i].y;
            }
        }
    }

    // GENERIC
    rod.x1 = x1;
    rod.x2 = x2;
    rod.y1 = y1;
    rod.y2 = y2;
    assert(rod.x1 <= rod.x2 && rod.y1 <= rod.y2);
} // ofxsTransformRegionFromPoints

// compute the bounding box of the transform of a rectangle
static void
ofxsTransformRegionFromRoD(const OfxRectD &srcRoD,
                           const OFX::Matrix3x3 &transform,
                           OFX::Point3D p[4],
                           OfxRectD &rod)
{
    /// now transform the 4 corners of the source clip to the output image
    p[0] = transform * OFX::Point3D(srcRoD.x1,srcRoD.y1,1);
    p[1] = transform * OFX::Point3D(srcRoD.x1,srcRoD.y2,1);
    p[2] = transform * OFX::Point3D(srcRoD.x2,srcRoD.y2,1);
    p[3] = transform * OFX::Point3D(srcRoD.x2,srcRoD.y1,1);

    ofxsTransformRegionFromPoints(p, rod);
}

void
Transform3x3Plugin::transformRegion(const OfxRectD &rectFrom,
                                    double time,
                                    bool invert,
                                    double motionblur,
                                    bool directionalBlur,
                                    double shutter,
                                    int shutteroffset_i,
                                    double shuttercustomoffset,
                                    bool isIdentity,
                                    OfxRectD *rectTo)
{
    // Algorithm:
    // - Compute positions of the four corners at start and end of shutter, and every multiple of 0.25 within this range.
    // - Update the bounding box from these positions.
    // - At the end, expand the bounding box by the maximum L-infinity distance between consecutive positions of each corner.

    OfxRangeD range;
    bool hasmotionblur = ((shutter != 0. || directionalBlur) && motionblur != 0.);

    if (hasmotionblur && !directionalBlur) {
        shutterRange(time, shutter, shutteroffset_i, shuttercustomoffset, &range);
    } else {
        ///if is identity return the input rod instead of transforming
        if (isIdentity) {
            *rectTo = rectFrom;

            return;
        }
        range.min = range.max = time;
    }

    // initialize with a super-empty RoD (note that max and min are reversed)
    rectTo->x1 = kOfxFlagInfiniteMax;
    rectTo->x2 = kOfxFlagInfiniteMin;
    rectTo->y1 = kOfxFlagInfiniteMax;
    rectTo->y2 = kOfxFlagInfiniteMin;
    double t = range.min;
    bool first = true;
    bool last = !hasmotionblur; // ony one iteration if there is no motion blur
    bool finished = false;
    double expand = 0.;
    double amount = 1.;
    int dirBlurIter = 0;
    OFX::Point3D p_prev[4];
    while (!finished) {
        // compute transformed positions
        OfxRectD thisRoD;
        OFX::Matrix3x3 transform;
        bool success = getInverseTransformCanonical(t, amount, invert, &transform); // RoD is computed using the *DIRECT* transform, which is why we use !invert
        if (!success) {
            // return infinite region
            rectTo->x1 = kOfxFlagInfiniteMin;
            rectTo->x2 = kOfxFlagInfiniteMax;
            rectTo->y1 = kOfxFlagInfiniteMin;
            rectTo->y2 = kOfxFlagInfiniteMax;

            return;
        }
        OFX::Point3D p[4];
        ofxsTransformRegionFromRoD(rectFrom, transform, p, thisRoD);

        // update min/max
        MergeImages2D::rectBoundingBox(*rectTo, thisRoD, rectTo);

        // if first iteration, continue
        if (first) {
            first = false;
        } else {
            // compute the L-infinity distance between consecutive tested points
            expand = std::max( expand, std::fabs(p_prev[0].x - p[0].x) );
            expand = std::max( expand, std::fabs(p_prev[0].y - p[0].y) );
            expand = std::max( expand, std::fabs(p_prev[1].x - p[1].x) );
            expand = std::max( expand, std::fabs(p_prev[1].y - p[1].y) );
            expand = std::max( expand, std::fabs(p_prev[2].x - p[2].x) );
            expand = std::max( expand, std::fabs(p_prev[2].y - p[2].y) );
            expand = std::max( expand, std::fabs(p_prev[3].x - p[3].x) );
            expand = std::max( expand, std::fabs(p_prev[3].y - p[3].y) );
        }

        if (last) {
            finished = true;
        } else {
            // prepare for next iteration
            p_prev[0] = p[0];
            p_prev[1] = p[1];
            p_prev[2] = p[2];
            p_prev[3] = p[3];
            if (directionalBlur) {
                const int dirBlurIterMax = 8;
                ++ dirBlurIter;
                amount = 1. - dirBlurIter/(double)dirBlurIterMax;
                last = dirBlurIter == dirBlurIterMax;
            } else {
                t = std::floor(t * 4 + 1) / 4; // next quarter-frame
                if (t >= range.max) {
                    // last iteration should be done with range.max
                    t = range.max;
                    last = true;
                }
            }
        }
    }
    // expand to take into account errors due to motion blur
    if (rectTo->x1 > kOfxFlagInfiniteMin) {
        rectTo->x1 -= expand;
    }
    if (rectTo->x2 < kOfxFlagInfiniteMax) {
        rectTo->x2 += expand;
    }
    if (rectTo->y1 > kOfxFlagInfiniteMin) {
        rectTo->y1 -= expand;
    }
    if (rectTo->y2 < kOfxFlagInfiniteMax) {
        rectTo->y2 += expand;
    }
} // transformRegion

// override the rod call
// Transform3x3-GENERIC
// the RoD should at least contain the region of definition of the source clip,
// which will be filled with black or by continuity.
bool
Transform3x3Plugin::getRegionOfDefinition(const RegionOfDefinitionArguments &args,
                                          OfxRectD &rod)
{
    const double time = args.time;
    const OfxRectD& srcRoD = _srcClip->getRegionOfDefinition(time);

    if ( MergeImages2D::rectIsInfinite(srcRoD) ) {
        // return an infinite RoD
        rod.x1 = kOfxFlagInfiniteMin;
        rod.x2 = kOfxFlagInfiniteMax;
        rod.y1 = kOfxFlagInfiniteMin;
        rod.y2 = kOfxFlagInfiniteMax;

        return true;
    }

    double mix = 1.;
    const bool doMasking = _masked && getContext() != OFX::eContextFilter && _maskClip->isConnected();
    if (doMasking && _mix) {
        _mix->getValueAtTime(time, mix);
        if (mix == 0.) {
            // identity transform
            rod = srcRoD;

            return true;
        }
    }

    bool invert = false;
    if (_invert) {
        _invert->getValueAtTime(time, invert);
    }
    invert = !invert; // only for getRoD
    double motionblur = 1.; // default for GodRays
    if (_motionblur) {
        _motionblur->getValueAtTime(time, motionblur);
    }
    bool directionalBlur = (_directionalBlur == 0);
    double shutter = 0.;
    int shutteroffset_i = 0;
    double shuttercustomoffset = 0.;
    if (_directionalBlur) {
        _directionalBlur->getValueAtTime(time, directionalBlur);
        _shutter->getValueAtTime(time, shutter);
        _shutteroffset->getValueAtTime(time, shutteroffset_i);
        _shuttercustomoffset->getValueAtTime(time, shuttercustomoffset);
    }

    bool identity = isIdentity(args.time);

    // set rod from srcRoD
    transformRegion(srcRoD, time, invert, motionblur, directionalBlur, shutter, shutteroffset_i, shuttercustomoffset, identity, &rod);

    // If identity do not expand for black outside, otherwise we would never be able to have identity.
    // We want the RoD to be the same as the src RoD when we are identity.
    if (!identity) {
        bool blackOutside = false;
        if (_blackOutside) {
            _blackOutside->getValueAtTime(time, blackOutside);
        }

        ofxsFilterExpandRoD(this, _dstClip->getPixelAspectRatio(), args.renderScale, blackOutside, &rod);
    }

    if ( doMasking && (mix != 1. || _maskClip->isConnected()) ) {
        // for masking or mixing, we also need the source image.
        // compute the union of both RODs
        MergeImages2D::rectBoundingBox(rod, srcRoD, &rod);
    }

    // say we set it
    return true;
} // getRegionOfDefinition

// override the roi call
// Transform3x3-GENERIC
// Required if the plugin requires a region from the inputs which is different from the rendered region of the output.
// (this is the case for transforms)
// It may be difficult to implement for complicated transforms:
// consequently, these transforms cannot support tiles.
void
Transform3x3Plugin::getRegionsOfInterest(const OFX::RegionsOfInterestArguments &args,
                                         OFX::RegionOfInterestSetter &rois)
{
    const double time = args.time;
    const OfxRectD roi = args.regionOfInterest;
    OfxRectD srcRoI;
    double mix = 1.;
    const bool doMasking = _masked && getContext() != OFX::eContextFilter && _maskClip->isConnected();

    if (doMasking) {
        _mix->getValueAtTime(time, mix);
        if (mix == 0.) {
            // identity transform
            srcRoI = roi;
            rois.setRegionOfInterest(*_srcClip, srcRoI);

            return;
        }
    }

    bool invert = false;
    if (_invert) {
        _invert->getValueAtTime(time, invert);
    }
    //invert = !invert; // only for getRoD
    double motionblur = 1; // default for GodRays
    if (_motionblur) {
        _motionblur->getValueAtTime(time, motionblur);
    }
    bool directionalBlur = (_directionalBlur == 0);
    double shutter = 0.;
    int shutteroffset_i = 0;
    double shuttercustomoffset = 0.;
    if (_directionalBlur) {
        _directionalBlur->getValueAtTime(time, directionalBlur);
        _shutter->getValueAtTime(time, shutter);
        _shutteroffset->getValueAtTime(time, shutteroffset_i);
        _shuttercustomoffset->getValueAtTime(time, shuttercustomoffset);
    }
    // set srcRoI from roi
    transformRegion(roi, time, invert, motionblur, directionalBlur, shutter, shutteroffset_i, shuttercustomoffset, isIdentity(time), &srcRoI);

    int filter = eFilterCubic;
    if (_filter) {
        _filter->getValueAtTime(time, filter);
    }
    bool blackOutside = false;
    if (_blackOutside) {
        _blackOutside->getValueAtTime(time, blackOutside);
    }

    assert(srcRoI.x1 <= srcRoI.x2 && srcRoI.y1 <= srcRoI.y2);

    ofxsFilterExpandRoI(roi, _srcClip->getPixelAspectRatio(), args.renderScale, (FilterEnum)filter, doMasking, mix, &srcRoI);

    if ( MergeImages2D::rectIsInfinite(srcRoI) ) {
        // RoI cannot be infinite.
        // This is not a mathematically correct solution, but better than nothing: set to the project size
        OfxPointD size = getProjectSize();
        OfxPointD offset = getProjectOffset();

        if (srcRoI.x1 <= kOfxFlagInfiniteMin) {
            srcRoI.x1 = offset.x;
        }
        if (srcRoI.x2 >= kOfxFlagInfiniteMax) {
            srcRoI.x2 = offset.x + size.x;
        }
        if (srcRoI.y1 <= kOfxFlagInfiniteMin) {
            srcRoI.y1 = offset.y;
        }
        if (srcRoI.y2 >= kOfxFlagInfiniteMax) {
            srcRoI.y2 = offset.y + size.y;
        }
    }

    if ( _masked && (mix != 1.) ) {
        // compute the bounding box with the default ROI
        MergeImages2D::rectBoundingBox(srcRoI, args.regionOfInterest, &srcRoI);
    }

    // no need to set it on mask (the default ROI is OK)
    rois.setRegionOfInterest(*_srcClip, srcRoI);
} // getRegionsOfInterest

template <class PIX, int nComponents, int maxValue, bool masked>
void
Transform3x3Plugin::renderInternalForBitDepth(const OFX::RenderArguments &args)
{
    const double time = args.time;
    int filter = eFilterCubic;
    if (_filter) {
        _filter->getValueAtTime(time, filter);
    }
    bool clamp = false;
    if (_clamp) {
        _clamp->getValueAtTime(time, clamp);
    }

    // as you may see below, some filters don't need explicit clamping, since they are
    // "clamped" by construction.
    switch ( (FilterEnum)filter ) {
    case eFilterImpulse: {
        Transform3x3Processor<PIX, nComponents, maxValue, masked, eFilterImpulse, false> fred(*this);
        setupAndProcess(fred, args);
        break;
    }
    case eFilterBilinear: {
        Transform3x3Processor<PIX, nComponents, maxValue, masked, eFilterBilinear, false> fred(*this);
        setupAndProcess(fred, args);
        break;
    }
    case eFilterCubic: {
        Transform3x3Processor<PIX, nComponents, maxValue, masked, eFilterCubic, false> fred(*this);
        setupAndProcess(fred, args);
        break;
    }
    case eFilterKeys:
        if (clamp) {
            Transform3x3Processor<PIX, nComponents, maxValue, masked, eFilterKeys, true> fred(*this);
            setupAndProcess(fred, args);
        } else {
            Transform3x3Processor<PIX, nComponents, maxValue, masked, eFilterKeys, false> fred(*this);
            setupAndProcess(fred, args);
        }
        break;
    case eFilterSimon:
        if (clamp) {
            Transform3x3Processor<PIX, nComponents, maxValue, masked, eFilterSimon, true> fred(*this);
            setupAndProcess(fred, args);
        } else {
            Transform3x3Processor<PIX, nComponents, maxValue, masked, eFilterSimon, false> fred(*this);
            setupAndProcess(fred, args);
        }
        break;
    case eFilterRifman:
        if (clamp) {
            Transform3x3Processor<PIX, nComponents, maxValue, masked, eFilterRifman, true> fred(*this);
            setupAndProcess(fred, args);
        } else {
            Transform3x3Processor<PIX, nComponents, maxValue, masked, eFilterRifman, false> fred(*this);
            setupAndProcess(fred, args);
        }
        break;
    case eFilterMitchell:
        if (clamp) {
            Transform3x3Processor<PIX, nComponents, maxValue, masked, eFilterMitchell, true> fred(*this);
            setupAndProcess(fred, args);
        } else {
            Transform3x3Processor<PIX, nComponents, maxValue, masked, eFilterMitchell, false> fred(*this);
            setupAndProcess(fred, args);
        }
        break;
    case eFilterParzen: {
        Transform3x3Processor<PIX, nComponents, maxValue, masked, eFilterParzen, false> fred(*this);
        setupAndProcess(fred, args);
        break;
    }
    case eFilterNotch: {
        Transform3x3Processor<PIX, nComponents, maxValue, masked, eFilterNotch, false> fred(*this);
        setupAndProcess(fred, args);
        break;
    }
    } // switch
} // renderInternalForBitDepth

// the internal render function
template <int nComponents, bool masked>
void
Transform3x3Plugin::renderInternal(const OFX::RenderArguments &args,
                                   OFX::BitDepthEnum dstBitDepth)
{
    switch (dstBitDepth) {
    case OFX::eBitDepthUByte:
        renderInternalForBitDepth<unsigned char, nComponents, 255, masked>(args);
        break;
    case OFX::eBitDepthUShort:
        renderInternalForBitDepth<unsigned short, nComponents, 65535, masked>(args);
        break;
    case OFX::eBitDepthFloat:
        renderInternalForBitDepth<float, nComponents, 1, masked>(args);
        break;
    default:
        OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
    }
}

// the overridden render function
void
Transform3x3Plugin::render(const OFX::RenderArguments &args)
{
    // instantiate the render code based on the pixel depth of the dst clip
    OFX::BitDepthEnum dstBitDepth    = _dstClip->getPixelDepth();
    OFX::PixelComponentEnum dstComponents  = _dstClip->getPixelComponents();

    assert(dstComponents == OFX::ePixelComponentAlpha || dstComponents == OFX::ePixelComponentRGB || dstComponents == OFX::ePixelComponentRGBA);
    if (dstComponents == OFX::ePixelComponentRGBA) {
        if (_masked) {
            renderInternal<4,true>(args, dstBitDepth);
        } else {
            renderInternal<4,false>(args, dstBitDepth);
        }
    } else if (dstComponents == OFX::ePixelComponentRGB) {
        if (_masked) {
            renderInternal<3,true>(args, dstBitDepth);
        } else {
            renderInternal<3,false>(args, dstBitDepth);
        }
    } else {
        assert(dstComponents == OFX::ePixelComponentAlpha);
        if (_masked) {
            renderInternal<1,true>(args, dstBitDepth);
        } else {
            renderInternal<1,false>(args, dstBitDepth);
        }
    }
}

bool
Transform3x3Plugin::isIdentity(const IsIdentityArguments &args,
                               OFX::Clip * &identityClip,
                               double &identityTime)
{
    const double time = args.time;

    // if there is motion blur, we suppose the transform is not identity
    double motionblur = _invert ? 1. : 0.; // default is 1 for GodRays, 0 for Mirror
    if (_motionblur) {
        _motionblur->getValueAtTime(time, motionblur);
    }
    double shutter = 0.;
    if (_shutter) {
        _shutter->getValueAtTime(time, shutter);
    }
    bool hasmotionblur = (shutter != 0. && motionblur != 0.);
    if (hasmotionblur) {
        return false;
    }

    if (_clamp) {
        // if image has values above 1., they will be clamped.
        bool clamp;
        _clamp->getValueAtTime(time, clamp);
        if (clamp) {
            return false;
        }
    }

    if ( isIdentity(time) ) { // let's call the Transform-specific one first
        identityClip = _srcClip;
        identityTime = time;

        return true;
    }

    // GENERIC
    if (_masked) {
        double mix = 1.;
        if (_mix) {
            _mix->getValueAtTime(time, mix);
        }
        if (mix == 0.) {
            identityClip = _srcClip;
            identityTime = time;

            return true;
        }
    }

    return false;
}

#ifdef OFX_EXTENSIONS_NUKE
// overridden getTransform
bool
Transform3x3Plugin::getTransform(const TransformArguments &args,
                                 Clip * &transformClip,
                                 double transformMatrix[9])
{
    assert(!_masked); // this should never get called for masked plugins, since they don't advertise that they can transform
    if (_masked) {
        return false;
    }
    const double time = args.time;
    bool invert = false;

    //std::cout << "getTransform called!" << std::endl;
    // Transform3x3-GENERIC
    if (_invert) {
        _invert->getValueAtTime(time, invert);
    }

    OFX::Matrix3x3 invtransform;
    bool success = getInverseTransformCanonical(time, 1., invert, &invtransform);
    if (!success) {
        return false;
    }


    // invert it
    double det = ofxsMatDeterminant(invtransform);
    if (det == 0.) {
        return false; // no transform available, render as usual
    }
    OFX::Matrix3x3 transformCanonical = ofxsMatInverse(invtransform, det);
    double pixelaspectratio = _srcClip->getPixelAspectRatio();
    bool fielded = args.fieldToRender == eFieldLower || args.fieldToRender == eFieldUpper;
    OFX::Matrix3x3 transformPixel = ( OFX::ofxsMatCanonicalToPixel(pixelaspectratio, args.renderScale.x, args.renderScale.y, fielded) *
                                      transformCanonical *
                                      OFX::ofxsMatPixelToCanonical(pixelaspectratio, args.renderScale.x, args.renderScale.y, fielded) );
    transformClip = _srcClip;
    transformMatrix[0] = transformPixel.a;
    transformMatrix[1] = transformPixel.b;
    transformMatrix[2] = transformPixel.c;
    transformMatrix[3] = transformPixel.d;
    transformMatrix[4] = transformPixel.e;
    transformMatrix[5] = transformPixel.f;
    transformMatrix[6] = transformPixel.g;
    transformMatrix[7] = transformPixel.h;
    transformMatrix[8] = transformPixel.i;

    return true;
}

#endif

size_t
Transform3x3Plugin::getInverseTransforms(double time,
                                         OfxPointD renderscale,
                                         bool fielded,
                                         double pixelaspectratio,
                                         bool invert,
                                         double shutter,
                                         int shutteroffset,
                                         double shuttercustomoffset,
                                         OFX::Matrix3x3* invtransform,
                                         size_t invtransformsizealloc) const
{
    OfxRangeD range;

    shutterRange(time, shutter, shutteroffset, shuttercustomoffset, &range);
    double t_start = range.min;
    double t_end = range.max; // shutter time
    bool allequal = true;
    size_t invtransformsize = invtransformsizealloc;
    OFX::Matrix3x3 canonicalToPixel = OFX::ofxsMatCanonicalToPixel(pixelaspectratio, renderscale.x, renderscale.y, fielded);
    OFX::Matrix3x3 pixelToCanonical = OFX::ofxsMatPixelToCanonical(pixelaspectratio, renderscale.x, renderscale.y, fielded);
    OFX::Matrix3x3 invtransformCanonical;

    for (size_t i = 0; i < invtransformsize; ++i) {
        double t = (i == 0) ? t_start : ( t_start + i * (t_end - t_start) / (double)(invtransformsizealloc - 1) );
        bool success = getInverseTransformCanonical(t, 1., invert, &invtransformCanonical); // virtual function
        if (success) {
            invtransform[i] = canonicalToPixel * invtransformCanonical * pixelToCanonical;
        } else {
            invtransform[i].a = 0.;
            invtransform[i].b = 0.;
            invtransform[i].c = 0.;
            invtransform[i].d = 0.;
            invtransform[i].e = 0.;
            invtransform[i].f = 0.;
            invtransform[i].g = 0.;
            invtransform[i].h = 0.;
            invtransform[i].i = 1.;
        }
        allequal = allequal && (invtransform[i].a == invtransform[0].a &&
                                invtransform[i].b == invtransform[0].b &&
                                invtransform[i].c == invtransform[0].c &&
                                invtransform[i].d == invtransform[0].d &&
                                invtransform[i].e == invtransform[0].e &&
                                invtransform[i].f == invtransform[0].f &&
                                invtransform[i].g == invtransform[0].g &&
                                invtransform[i].h == invtransform[0].h &&
                                invtransform[i].i == invtransform[0].i);
    }
    if (allequal) { // there is only one transform, no need to do motion blur!
        invtransformsize = 1;
    }

    return invtransformsize;
}

size_t
Transform3x3Plugin::getInverseTransformsBlur(double time,
                                             OfxPointD renderscale,
                                             bool fielded,
                                             double pixelaspectratio,
                                             bool invert,
                                             OFX::Matrix3x3* invtransform,
                                             size_t invtransformsizealloc) const
{
    bool allequal = true;
    size_t invtransformsize = invtransformsizealloc;
    OFX::Matrix3x3 canonicalToPixel = OFX::ofxsMatCanonicalToPixel(pixelaspectratio, renderscale.x, renderscale.y, fielded);
    OFX::Matrix3x3 pixelToCanonical = OFX::ofxsMatPixelToCanonical(pixelaspectratio, renderscale.x, renderscale.y, fielded);
    OFX::Matrix3x3 invtransformCanonical;

    for (size_t i = 0; i < invtransformsize; ++i) {
        //double amount = 1. - i / (double)(invtransformsizealloc - 1); // Theoretically better
        double amount = 1. - (i+1) / (double)(invtransformsizealloc); // To be compatible with Nuke (Nuke bug?)
        bool success = getInverseTransformCanonical(time, amount, invert, &invtransformCanonical); // virtual function
        if (success) {
            invtransform[i] = canonicalToPixel * invtransformCanonical * pixelToCanonical;
        } else {
            invtransform[i].a = 0.;
            invtransform[i].b = 0.;
            invtransform[i].c = 0.;
            invtransform[i].d = 0.;
            invtransform[i].e = 0.;
            invtransform[i].f = 0.;
            invtransform[i].g = 0.;
            invtransform[i].h = 0.;
            invtransform[i].i = 1.;
        }
        allequal = allequal && (invtransform[i].a == invtransform[0].a &&
                                invtransform[i].b == invtransform[0].b &&
                                invtransform[i].c == invtransform[0].c &&
                                invtransform[i].d == invtransform[0].d &&
                                invtransform[i].e == invtransform[0].e &&
                                invtransform[i].f == invtransform[0].f &&
                                invtransform[i].g == invtransform[0].g &&
                                invtransform[i].h == invtransform[0].h &&
                                invtransform[i].i == invtransform[0].i);
    }
    if (allequal) { // there is only one transform, no need to do motion blur!
        invtransformsize = 1;
    }

    return invtransformsize;
}

// override changedParam
void
Transform3x3Plugin::changedParam(const OFX::InstanceChangedArgs &args,
                                 const std::string &paramName)
{
    if ( (paramName == kParamTransform3x3Invert) ||
         ( paramName == kParamTransform3x3Shutter) ||
         ( paramName == kParamTransform3x3ShutterOffset) ||
         ( paramName == kParamTransform3x3ShutterCustomOffset) ) {
        // Motion Blur is the only parameter that doesn't matter
        assert(paramName != kParamTransform3x3MotionBlur);

        changedTransform(args);
    }
    if (paramName == kParamTransform3x3DirectionalBlur) {
        bool directionalBlur;
        _directionalBlur->getValueAtTime(args.time, directionalBlur);
        _shutter->setEnabled(!directionalBlur);
        _shutteroffset->setEnabled(!directionalBlur);
        _shuttercustomoffset->setEnabled(!directionalBlur);
    }
}

// this method must be called by the derived class when the transform was changed
void
Transform3x3Plugin::changedTransform(const OFX::InstanceChangedArgs &args)
{
    (void)args;
}

void
OFX::Transform3x3Describe(OFX::ImageEffectDescriptor &desc,
                          bool masked)
{
    desc.addSupportedContext(eContextFilter);
    desc.addSupportedContext(eContextGeneral);
    if (masked) {
        desc.addSupportedContext(eContextPaint);
    }
    desc.addSupportedBitDepth(eBitDepthUByte);
    desc.addSupportedBitDepth(eBitDepthUShort);
    desc.addSupportedBitDepth(eBitDepthFloat);

    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setTemporalClipAccess(false);
    // each field has to be transformed separately, or you will get combing effect
    // this should be true for all geometric transforms
    desc.setRenderTwiceAlways(true);
    desc.setSupportsMultipleClipPARs(false);
    desc.setRenderThreadSafety(kRenderThreadSafety);

    // Transform3x3-GENERIC

    // in order to support tiles, the transform plugin must implement the getRegionOfInterest function
    desc.setSupportsTiles(kSupportsTiles);

    // in order to support multiresolution, render() must take into account the pixelaspectratio and the renderscale
    // and scale the transform appropriately.
    // All other functions are usually in canonical coordinates.
    desc.setSupportsMultiResolution(kSupportsMultiResolution);

#ifdef OFX_EXTENSIONS_NUKE
    if (!masked) {
        // Enable transform by the host.
        // It is only possible for transforms which can be represented as a 3x3 matrix.
        desc.setCanTransform(true);
        if (getImageEffectHostDescription()->canTransform) {
            //std::cout << "kFnOfxImageEffectCanTransform (describe) =" << desc.getPropertySet().propGetInt(kFnOfxImageEffectCanTransform) << std::endl;
        }
    }
    // ask the host to render all planes
    desc.setPassThroughForNotProcessedPlanes(ePassThroughLevelRenderAllRequestedPlanes);
#endif
}

OFX::PageParamDescriptor *
OFX::Transform3x3DescribeInContextBegin(OFX::ImageEffectDescriptor &desc,
                                        OFX::ContextEnum context,
                                        bool masked)
{
    // GENERIC

    // Source clip only in the filter context
    // create the mandated source clip
    // always declare the source clip first, because some hosts may consider
    // it as the default input clip (e.g. Nuke)
    ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);

    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
#ifdef OFX_EXTENSIONS_NATRON
    //This will add only if host supports Natron extensions
    srcClip->addSupportedComponent(ePixelComponentsXY);
#endif
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setIsMask(false);
    srcClip->setCanTransform(true); // source images can have a transform attached

    if ( masked && ( (context == eContextGeneral) || (context == eContextPaint) ) ) {
        // GENERIC (MASKED)
        //
        // if general or paint context, define the mask clip
        // if paint context, it is a mandated input called 'brush'
        ClipDescriptor *maskClip = context == eContextGeneral ? desc.defineClip("Mask") : desc.defineClip("Brush");
        maskClip->addSupportedComponent(ePixelComponentAlpha);
        maskClip->setTemporalClipAccess(false);
        if (context == eContextGeneral) {
            maskClip->setOptional(true);
        }
        maskClip->setSupportsTiles(kSupportsTiles);
        maskClip->setIsMask(true); // we are a mask input
    }

    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
#ifdef OFX_EXTENSIONS_NATRON
    //This will add only if host supports Natron extensions
    dstClip->addSupportedComponent(ePixelComponentsXY);
#endif
    dstClip->setSupportsTiles(kSupportsTiles);


    // make some pages and to things in
    PageParamDescriptor *page = desc.definePageParam("Controls");

    return page;
}

void
OFX::Transform3x3DescribeInContextEnd(OFX::ImageEffectDescriptor &desc,
                                      OFX::ContextEnum /*context*/,
                                      OFX::PageParamDescriptor* page,
                                      bool masked,
                                      bool isDirBlur)
{
    // invert
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamTransform3x3Invert);
        param->setLabel(kParamTransform3x3InvertLabel);
        param->setHint(kParamTransform3x3InvertHint);
        param->setDefault(false);
        param->setAnimates(true);
        if (page) {
            page->addChild(*param);
        }
    }
    // GENERIC PARAMETERS
    //

    ofxsFilterDescribeParamsInterpolate2D(desc, page, !isDirBlur);

    // motionBlur
    {
        DoubleParamDescriptor* param = desc.defineDoubleParam(kParamTransform3x3MotionBlur);
        param->setLabel(kParamTransform3x3MotionBlurLabel);
        param->setHint(kParamTransform3x3MotionBlurHint);
        param->setDefault(isDirBlur ? 1. : 0.);
        param->setRange(0., 100.);
        param->setIncrement(0.01);
        param->setDisplayRange(0., 4.);
        if (page) {
            page->addChild(*param);
        }
    }

    if (!isDirBlur) {
        // directionalBlur
        {
            BooleanParamDescriptor* param = desc.defineBooleanParam(kParamTransform3x3DirectionalBlur);
            param->setLabel(kParamTransform3x3DirectionalBlurLabel);
            param->setHint(kParamTransform3x3DirectionalBlurHint);
            param->setDefault(false);
            param->setAnimates(true);
            if (page) {
                page->addChild(*param);
            }
        }

        // shutter
        {
            DoubleParamDescriptor* param = desc.defineDoubleParam(kParamTransform3x3Shutter);
            param->setLabel(kParamTransform3x3ShutterLabel);
            param->setHint(kParamTransform3x3ShutterHint);
            param->setDefault(0.5);
            param->setRange(0., 2.);
            param->setIncrement(0.01);
            param->setDisplayRange(0., 2.);
            if (page) {
                page->addChild(*param);
            }
        }

        // shutteroffset
        {
            ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamTransform3x3ShutterOffset);
            param->setLabel(kParamTransform3x3ShutterOffsetLabel);
            param->setHint(kParamTransform3x3ShutterOffsetHint);
            assert(param->getNOptions() == eTransform3x3ShutterOffsetCentered);
            param->appendOption(kParamTransform3x3ShutterOffsetOptionCentered, kParamTransform3x3ShutterOffsetOptionCenteredHint);
            assert(param->getNOptions() == eTransform3x3ShutterOffsetStart);
            param->appendOption(kParamTransform3x3ShutterOffsetOptionStart, kParamTransform3x3ShutterOffsetOptionStartHint);
            assert(param->getNOptions() == eTransform3x3ShutterOffsetEnd);
            param->appendOption(kParamTransform3x3ShutterOffsetOptionEnd, kParamTransform3x3ShutterOffsetOptionEndHint);
            assert(param->getNOptions() == eTransform3x3ShutterOffsetCustom);
            param->appendOption(kParamTransform3x3ShutterOffsetOptionCustom, kParamTransform3x3ShutterOffsetOptionCustomHint);
            param->setAnimates(true);
            param->setDefault(eTransform3x3ShutterOffsetStart);
            if (page) {
                page->addChild(*param);
            }
        }

        // shuttercustomoffset
        {
            DoubleParamDescriptor* param = desc.defineDoubleParam(kParamTransform3x3ShutterCustomOffset);
            param->setLabel(kParamTransform3x3ShutterCustomOffsetLabel);
            param->setHint(kParamTransform3x3ShutterCustomOffsetHint);
            param->setDefault(0.);
            param->setRange(-1., 1.);
            param->setIncrement(0.1);
            param->setDisplayRange(-1., 1.);
            if (page) {
                page->addChild(*param);
            }
        }
    }
    
    if (masked) {
        // GENERIC (MASKED)
        //
        ofxsMaskMixDescribeParams(desc, page);
#ifdef OFX_EXTENSIONS_NUKE
    } else if (getImageEffectHostDescription()->canTransform) {
        // Transform3x3-GENERIC (NON-MASKED)
        //
        //std::cout << "kFnOfxImageEffectCanTransform in describeincontext(" << context << ")=" << desc.getPropertySet().propGetInt(kFnOfxImageEffectCanTransform) << std::endl;
#endif
    }
} // Transform3x3DescribeInContextEnd

