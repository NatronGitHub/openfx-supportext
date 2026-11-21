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
 * OFX Masking/Mixing help functions
 */

#ifndef Misc_ofxsMaskMix_h
#define Misc_ofxsMaskMix_h

#include <cfloat> // FLT_EPSILON, FLT_MAX
#include <cmath> // copysign

#include <ofxsImageEffect.h>
#include "ofxsCoords.h"

#define kParamPremult "premult"
#define kParamPremultLabel "(Un)premult"
#define kParamPremultHint \
    "Divide the image by the alpha channel before processing, and re-multiply it afterwards. " \
    "Use if the input images are premultiplied."

#define kParamPremultChannel "premultChannel"
#define kParamPremultChannelLabel "By"
#define kParamPremultChannelHint \
    "The channel to use for (un)premult."
#define kParamPremultChannelR "R", "R channel from input", "r"
#define kParamPremultChannelG "G", "G channel from input", "g"
#define kParamPremultChannelB "B", "B channel from input", "b"
#define kParamPremultChannelA "A", "A channel from input", "a"

#define kParamMix "mix"
#define kParamMixLabel "Mix"
#define kParamMixHint "Mix factor between the original and the transformed image."
#define kParamMaskApply "mask"
#define kParamMaskApplyLabel "Mask"
#define kParamMaskApplyHint "When checked, mask is applied."
#define kParamMaskInvert "maskInvert"
#define kParamMaskInvertLabel "Invert Mask"
#define kParamMaskInvertHint "When checked, the effect is fully applied where the mask is 0."

namespace OFX {
inline
void
ofxsPremultDescribeParams(OFX::ImageEffectDescriptor &desc,
                          OFX::PageParamDescriptor *page)
{
    {
        OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamPremult);
        param->setLabel(kParamPremultLabel);
        param->setHint(kParamPremultHint);
#ifdef OFX_EXTENSIONS_NUKE
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
#endif
        if (page) {
            page->addChild(*param);
        }
    }
    {
        // not yet implemented, for future use (whenever deep compositing is supported)
        OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamPremultChannel);
        param->setLabel(kParamPremultChannelLabel);
        param->setHint(kParamPremultChannelHint);
        param->appendOption(kParamPremultChannelR);
        param->appendOption(kParamPremultChannelG);
        param->appendOption(kParamPremultChannelB);
        param->appendOption(kParamPremultChannelA);
        param->setDefault(3); // alpha
        param->setIsSecret(true); // not yet implemented
        if (page) {
            page->addChild(*param);
        }
    }
}

inline
bool
ofxsMaskIsAlwaysConnected(OFX::ImageEffectHostDescription *desc)
{
    return (desc->hostName.compare(0, 14, "DaVinciResolve") == 0);
}

/// If the mask is applied and not inverted, set *maskRoD to the mask clip RoD
/// in pixel coordinates and return true. Outside of this rectangle, a masked
/// effect outputs its unmasked source. Return false otherwise (*maskRoD is
/// then left unchanged).
/// fieldToRender gives the field scale (ofxCoordSystem.rst, Canonical to
/// Pixel: "Y' = Y * SY * FS", FS = 0.5 for images whose kOfxImagePropField is
/// kOfxImageFieldLower or kOfxImageFieldUpper, i.e. half-height single-field
/// images). The default, eFieldNone (FS = 1), is right for progressive clips,
/// and for fielded clips with the default field extraction kOfxImageFieldDoubled
/// ("making a full height image") or kOfxImageFieldBoth. Pass args.fieldToRender
/// only if the plug-in sets kOfxImageFieldSingle extraction ("making a half
/// height image") on its clips.
/// Infinite bounds are scaled like finite ones; results outside the int range
/// are clamped to kOfxFlagInfiniteMin/Max, and NaN bounds become infinite
/// in IEEE builds (see Coords::pixelToIntClamped).
inline
bool
ofxsMaskGetRoDPixel(OFX::Clip* maskClip,
                    OFX::BooleanParam* maskApply,
                    OFX::BooleanParam* maskInvert,
                    double time,
                    const OfxPointD& renderScale,
                    OfxRectI* maskRoD,
                    OFX::FieldEnum fieldToRender = OFX::eFieldNone)
{
    bool doMasking = ( ( !maskApply || maskApply->getValueAtTime(time) ) && maskClip && maskClip->isConnected() );
    if ( !doMasking || maskInvert->getValueAtTime(time) ) {
        return false;
    }
    OfxPointD scale = renderScale;
    if ( (fieldToRender == OFX::eFieldLower) || (fieldToRender == OFX::eFieldUpper) ) {
        scale.y *= 0.5;
    }
    OFX::Coords::toPixelEnclosing(maskClip->getRegionOfDefinition(time), scale, maskClip->getPixelAspectRatio(), maskRoD);

    return true;
}

inline
void
ofxsMaskDescribeParams(OFX::ImageEffectDescriptor &desc,
                       OFX::PageParamDescriptor *page)
{
    // If the host always sees mask clips are connected, this is a problem because
    // mask will appear as black and transparent, although it is not connected
    if ( ofxsMaskIsAlwaysConnected( OFX::getImageEffectHostDescription() ) ) {
        OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamMaskApply);
        param->setLabel(kParamMaskApplyLabel);
        param->setHint(kParamMaskApplyHint);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamMaskInvert);
        param->setLabel(kParamMaskInvertLabel);
        param->setHint(kParamMaskInvertHint);
        if (page) {
            page->addChild(*param);
        }
    }
}

inline
void
ofxsMixDescribeParams(OFX::ImageEffectDescriptor &desc,
                          OFX::PageParamDescriptor *page)
{
    // GENERIC (MASKED)
    //
    {
        OFX::DoubleParamDescriptor* param = desc.defineDoubleParam(kParamMix);
        param->setLabel(kParamMixLabel);
        param->setHint(kParamMixHint);
        param->setDefault(1.);
        param->setIncrement(0.01);
        param->setRange(0., 1.);
        param->setDisplayRange(0., 1.);
        if (page) {
            page->addChild(*param);
        }
    }
}

inline
void
ofxsMaskMixDescribeParams(OFX::ImageEffectDescriptor &desc,
                          OFX::PageParamDescriptor *page)
{
    // GENERIC (MASKED)
    //
    ofxsMaskDescribeParams(desc, page);
    ofxsMixDescribeParams(desc, page);
}


template <class T>
inline
T
ofxsClamp(T v,
          int min,
          int max)
{
    if ( v < T(min) ) {
        return T(min);
    }
    if ( v > T(max) ) {
        return T(max);
    }

    return v;
}

// v is not normalized, it is within [0,maxValue] (but is allowed to be outside of this range)
template <typename PIX, int maxValue>
inline
PIX
ofxsClampIfInt(float v,
               int min,
               int max)
{
    if (maxValue == 1) {
        return (PIX)(v);
    }

    return (PIX)(ofxsClamp(v, min, max) + 0.5);
}

// normalize in [0,1]
template <class PIX, int nComponents, int maxValue>
void
ofxsToRGBA(const PIX *srcPix,
           float unpPix[4])
{
    if (!srcPix) {
        // no src pixel here, be black and transparent
        for (int c = 0; c < 4; ++c) {
            unpPix[c] = 0.f;
        }

        return;
    }

    if (nComponents == 1) {
        unpPix[0] = 0.f;
        unpPix[1] = 0.f;
        unpPix[2] = 0.f;
        unpPix[3] = srcPix[0] / (float)maxValue;

        return;
    }

    if (nComponents == 2) {
        unpPix[0] = srcPix[0] / (float)maxValue;
        unpPix[1] = srcPix[1] / (float)maxValue;
        unpPix[2] = 0.f;
        unpPix[3] = 1.0f;

        return;
    }

    unpPix[0] = srcPix[0] / (float)maxValue;
    unpPix[1] = srcPix[1] / (float)maxValue;
    unpPix[2] = srcPix[2] / (float)maxValue;
    unpPix[3] = (nComponents == 4) ? (srcPix[3] / (float)maxValue) : 1.0f;
}

// c / alpha for an alpha > 0, as the float division c / (float)alpha, except that a quotient
// beyond the float range gives +-FLT_MAX instead of +-inf (a huge colour with a tiny alpha,
// which then gave inf * 0 or inf - inf = NaN downstream; an infinite colour also gives
// +-FLT_MAX). The quotient is computed in double and clamped before the conversion: the double
// quotient of two floats rounded to float is the float quotient, so every finite result is
// unchanged. Only comparisons, no NaN or inf test: a NaN colour falls through unchanged.
inline float
ofxsUnPremultValue(double c,
                   double alpha)
{
    const double q = c / alpha;

    if (q > FLT_MAX) {
        return FLT_MAX;
    }
    if (q < -FLT_MAX) {
        return -FLT_MAX;
    }

    return (float)q;
}

// normalize in [0,1] and unpremultiply srcPix
// if premult is false, just normalize
// unpremult by alpha <= 0 gives identity
// premult(unpremult(p)) or premult(unpremult(p)) is thus not identity for alpha <= 0
template <class PIX, int nComponents, int maxValue>
void
ofxsUnPremult(const PIX *srcPix,
              float unpPix[4],
              bool premult,
              int /*premultChannel*/)
{
    if (!srcPix) {
        // no src pixel here, be black and transparent
        for (int c = 0; c < 4; ++c) {
            unpPix[c] = 0.f;
        }

        return;
    }

    if (nComponents == 1) {
        unpPix[0] = 0.f;
        unpPix[1] = 0.f;
        unpPix[2] = 0.f;
        unpPix[3] = srcPix[0] / (float)maxValue;

        return;
    }

    if (nComponents == 2) {
        unpPix[0] = srcPix[0] / (float)maxValue;
        unpPix[1] = srcPix[1] / (float)maxValue;
        unpPix[2] = 0.f;
        unpPix[3] = 1.0f;

        return;
    }

    // unpremult by alpha <= 0 gives identity
    if ( !premult || (nComponents == 3) || (srcPix[3] <= 0) ) {
        unpPix[0] = srcPix[0] / (float)maxValue;
        unpPix[1] = srcPix[1] / (float)maxValue;
        unpPix[2] = srcPix[2] / (float)maxValue;
        unpPix[3] = (nComponents == 4) ? (srcPix[3] / (float)maxValue) : 1.0f;

        return;
    }

    assert(nComponents == 4);
    PIX alpha = srcPix[3];
    if ( alpha > (PIX)(FLT_EPSILON * maxValue) ) {
        unpPix[0] = ofxsUnPremultValue(srcPix[0], alpha);
        unpPix[1] = ofxsUnPremultValue(srcPix[1], alpha);
        unpPix[2] = ofxsUnPremultValue(srcPix[2], alpha);
    } else {
        unpPix[0] = srcPix[0] / (float)maxValue;
        unpPix[1] = srcPix[1] / (float)maxValue;
        unpPix[2] = srcPix[2] / (float)maxValue;
    }
    unpPix[3] = srcPix[3] / (float)maxValue;
} // ofxsUnPremult

// unpPix is in [0, 1]
// premultiply and denormalize in [0, maxValue]
// if premult is false, just denormalize
// premult by alpha <= 0 gives 0
// premult(unpremult(p)) or premult(unpremult(p)) is thus not identity for alpha <= 0
template <class PIX, int nComponents, int maxValue>
void
ofxsPremult(const float unpPix[4],
            float *tmpPix,
            bool premult,
            int /*premultChannel*/)
{
    if (nComponents == 1) {
        tmpPix[0] = unpPix[3] * maxValue;

        return;
    }

    if ( !premult ) {
        tmpPix[0] = unpPix[0] * maxValue;
        if (nComponents >= 2) {
            tmpPix[1] = unpPix[1] * maxValue;
        }
        if (nComponents >= 3) {
            tmpPix[2] = unpPix[2] * maxValue;
        }
        if (nComponents >= 4) {
            tmpPix[3] = unpPix[3] * maxValue;
        }

        return;
    }

    // premult by alpha <= 0 gives 0
    float alpha = (std::max)(0.f, unpPix[3]);

    if (alpha == 0.f) {
        // a signed zero, as c * 0 gives for a finite c: an infinite colour (HDR values that
        // overflowed upstream) gave inf * 0 = NaN
        tmpPix[0] = std::copysign(0.f, unpPix[0]);
        if (nComponents >= 2) {
            tmpPix[1] = std::copysign(0.f, unpPix[1]);
        }
        if (nComponents >= 3) {
            tmpPix[2] = std::copysign(0.f, unpPix[2]);
        }
    } else {
        tmpPix[0] = unpPix[0] * alpha * maxValue;
        if (nComponents >= 2) {
            tmpPix[1] = unpPix[1] * alpha * maxValue;
        }
        if (nComponents >= 3) {
            tmpPix[2] = unpPix[2] * alpha * maxValue;
        }
    }
    if (nComponents >= 4) {
        tmpPix[3] = alpha * maxValue;
    }
}

// tmpPix is not normalized, it is within [0,maxValue] (but is allowed to be outside of this range)
template <class PIX, int nComponents, int maxValue>
void
ofxsPix(const float *tmpPix, //!< interpolated pixel
        PIX *dstPix) //!< destination pixel
{
    // no mask, no mix
    for (int c = 0; c < nComponents; ++c) {
        dstPix[c] = ofxsClampIfInt<PIX, maxValue>(tmpPix[c], 0, maxValue);
    }
} // ofxsMixPix

// unpPix is normalized between [0,1]
template <class PIX, int nComponents, int maxValue>
void
ofxsPremultPix(const float unpPix[4], //!< interpolated unpremultiplied pixel
               bool premult,
               int premultChannel,
               PIX *dstPix) //!< destination pixel
{
    float tmpPix[nComponents];

    // unpPix is in [0..1]
    ofxsPremult<PIX, nComponents, maxValue>(unpPix, tmpPix, premult, premultChannel);
    // tmpPix is in [0..maxValue]
    ofxsPix<PIX, nComponents, maxValue>(tmpPix, dstPix);
}


// A value made finite, with comparisons only: NaN gives 0, beyond the float range gives +-FLT_MAX,
// every finite float is unchanged.
inline float
ofxsFiniteFloat(double v)
{
    if ( (v >= -FLT_MAX) && (v <= FLT_MAX) ) {
        return (float)v;
    }
    if (v > 0.) {
        return FLT_MAX;
    }
    if (v < 0.) {
        return -FLT_MAX;
    }

    return 0.f; // NaN
}

// tmp * alpha + (1 - alpha) * src, the mix of ofxsMixPix and ofxsMaskMixPix, kept finite. The original
// float expression gives the result unless both products round to infinity in float (exact products,
// tested in double, at least 2^128 - 2^103): only then was the old result inf - inf = NaN or +-inf,
// whether or not the compiler fuses one product into an FMA, and it is computed in double instead. A
// result beyond the float range gives +-FLT_MAX, and NaN (from a NaN input) gives 0. So every finite
// result is unchanged.
inline float
ofxsMixValue(float tmp,
             float src,
             float alpha)
{
    // float rounds an exact value to inf from 2^128 - 2^103 (halfway between FLT_MAX and 2^128)
    const double kOverflow = 340282356779733661637539395458142568448.;
    const float beta = 1.f - alpha;
    const double p1 = std::fabs( (double)tmp * (double)alpha ); // exact
    const double p2 = std::fabs( (double)src * (double)beta ); // exact
    if ( (p1 >= kOverflow) && (p2 >= kOverflow) ) {
        return ofxsFiniteFloat( (double)tmp * alpha + (double)beta * src );
    }
    float v = tmp * alpha + (1.f - alpha) * src;

    return ofxsFiniteFloat(v);
}

// tmpPix is not normalized, it is within [0,maxValue] (but is allowed to be outside of this range)
template <class PIX, int nComponents, int maxValue>
void
ofxsMixPix(const float *tmpPix, //!< interpolated pixel
           const PIX *srcPix, //!< the background image (the output is srcImg where maskImg=0, else it is tmpPix)
           float mix, //!< mix factor between the output and bkImg
           PIX *dstPix) //!< destination pixel
{
    if (mix == 1.) {
        ofxsPix<PIX, nComponents, maxValue>(tmpPix, dstPix);
    } else {
        // just mix
        float alpha = mix;
        if (alpha == 0.) {
            if (srcPix) {
                for (int c = 0; c < nComponents; ++c) {
                    dstPix[c] = ofxsClampIfInt<PIX, maxValue>(srcPix[c], 0, maxValue);
                }
            } else {
                for (int c = 0; c < nComponents; ++c) {
                    dstPix[c] = 0;
                }
            }
        } else if (alpha == 1) {
            for (int c = 0; c < nComponents; ++c) {
                dstPix[c] = ofxsClampIfInt<PIX, maxValue>(tmpPix[c], 0, maxValue);
            }
        } else {
            if (srcPix) {
                for (int c = 0; c < nComponents; ++c) {
                    float v = ofxsMixValue(tmpPix[c], srcPix[c], alpha);
                    dstPix[c] = ofxsClampIfInt<PIX, maxValue>(v, 0, maxValue);
                }
            } else {
                for (int c = 0; c < nComponents; ++c) {
                    float v = ofxsFiniteFloat(tmpPix[c] * alpha); // a huge mix value could overflow
                    dstPix[c] = ofxsClampIfInt<PIX, maxValue>(v, 0, maxValue);
                }
            }
        }
    }
} // ofxsMixPix

// tmpPix is not normalized, it is within [0,maxValue] (but is allowed to be outside of this range)
template <class PIX, int nComponents, int maxValue, bool masked>
void
ofxsMaskMixPix(const float *tmpPix, //!< interpolated pixel
               int x, //!< coordinates for the pixel to be computed (PIXEL coordinates)
               int y,
               const PIX *srcPix, //!< the background image (the output is srcImg where maskImg=0, else it is tmpPix)
               bool domask, //!< apply the mask?
               const OFX::Image *maskImg, //!< the mask image (ignored if masked=false or domask=false), which must be Alpha
               float mix, //!< mix factor between the output and bkImg
               bool maskInvert, //<! invert mask behavior
               PIX *dstPix) //!< destination pixel
{
    // For a multi-planar effect, the mask image may have any components
    assert(!domask || !maskImg || maskImg->getPixelComponentCount() > 0);
    const PIX *maskPix = NULL;
    float maskScale = 1.f;

    // are we doing masking
    if (!masked) {
        ofxsMixPix<PIX, nComponents, maxValue>(tmpPix, srcPix, mix, dstPix);
    } else {
        if (domask) {
            // we do, get the pixel from the mask
            maskPix = maskImg ? (const PIX *)maskImg->getPixelAddress(x, y) : 0;
            // figure the scale factor from that pixel
            if (maskPix == 0) {
                maskScale = maskInvert ? 1.f : 0.f;
            } else {
                // a NaN or infinite mask sample gave inf * 0 or inf - inf = NaN: make it finite
                // (NaN gives 0, +-inf gives +-FLT_MAX); every finite mask, also outside [0, 1], is unchanged
                maskScale = ofxsFiniteFloat(*maskPix / float(maxValue));
                if (maskInvert) {
                    maskScale = 1.f - maskScale;
                }
            }
        }
        float alpha = maskScale * mix;
        if (alpha == 0.) {
            if (srcPix) {
                for (int c = 0; c < nComponents; ++c) {
                    dstPix[c] = ofxsClampIfInt<PIX, maxValue>(srcPix[c], 0, maxValue);
                }
            } else {
                for (int c = 0; c < nComponents; ++c) {
                    dstPix[c] = 0;
                }
            }
        } else if (alpha == 1.) {
            for (int c = 0; c < nComponents; ++c) {
                dstPix[c] = ofxsClampIfInt<PIX, maxValue>(tmpPix[c], 0, maxValue);
            }
        } else {
            if (srcPix) {
                for (int c = 0; c < nComponents; ++c) {
                    float v = ofxsMixValue(tmpPix[c], srcPix[c], alpha);
                    dstPix[c] = ofxsClampIfInt<PIX, maxValue>(v, 0, maxValue);
                }
            } else {
                for (int c = 0; c < nComponents; ++c) {
                    float v = ofxsFiniteFloat(tmpPix[c] * alpha); // an inf mask gives alpha = +-FLT_MAX
                    dstPix[c] = ofxsClampIfInt<PIX, maxValue>(v, 0, maxValue);
                }
            }
        }
    }
} // ofxsMaskMixPix

// unpPix is normalized between [0,1]
template <class PIX, int nComponents, int maxValue, bool masked>
void
ofxsPremultMaskMixPix(const float unpPix[4], //!< interpolated unpremultiplied pixel
                      bool premult,
                      int premultChannel,
                      int x, //!< coordinates for the pixel to be computed (PIXEL coordinates)
                      int y,
                      const PIX *srcPix, //!< the background image (the output is srcImg where maskImg=0, else it is tmpPix)
                      bool domask, //!< apply the mask?
                      const OFX::Image *maskImg, //!< the mask image (ignored if masked=false or domask=false)
                      float mix, //!< mix factor between the output and bkImg
                      bool maskInvert, //<! invert mask behavior
                      PIX *dstPix) //!< destination pixel
{
    assert(!domask || !maskImg || maskImg->getPixelComponents() == ePixelComponentAlpha);
    float tmpPix[nComponents];

    // unpPix is in [0..1]
    ofxsPremult<PIX, nComponents, maxValue>(unpPix, tmpPix, premult, premultChannel);
    // tmpPix is in [0..maxValue]
    ofxsMaskMixPix<PIX, nComponents, maxValue, masked>(tmpPix, x, y, srcPix, domask, maskImg, mix, maskInvert, dstPix);
}

// unpPix is normalized between [0,1]
template <class PIX, int nComponents, int maxValue>
void
ofxsPremultMixPix(const float unpPix[4], //!< interpolated unpremultiplied pixel
                  bool premult,
                  int premultChannel,
                  const PIX *srcPix, //!< the background image (the output is srcImg where maskImg=0, else it is tmpPix)
                  float mix, //!< mix factor between the output and bkImg
                  PIX *dstPix) //!< destination pixel
{
    float tmpPix[nComponents];

    // unpPix is in [0..1]
    ofxsPremult<PIX, nComponents, maxValue>(unpPix, tmpPix, premult, premultChannel);
    // tmpPix is in [0..maxValue]
    ofxsMixPix<PIX, nComponents, maxValue>(tmpPix, srcPix, mix, dstPix);
}

// tmpPix is not normalized, it is within [0,maxValue] (but is allowed to be outside of this range)
template <class PIX, int nComponents, int maxValue, bool masked>
void
ofxsMaskMix(const float *tmpPix, //!< interpolated pixel
            int x, //!< coordinates for the pixel to be computed (PIXEL coordinates)
            int y,
            const OFX::Image *srcImg, //!< the background image (the output is srcImg where maskImg=0, else it is tmpPix)
            bool domask, //!< apply the mask?
            const OFX::Image *maskImg, //!< the mask image (ignored if masked=false or domask=false)
            float mix, //!< mix factor between the output and bkImg
            bool maskInvert, //<! invert mask behavior
            PIX *dstPix) //!< destination pixel
{
    assert(!domask || !maskImg || maskImg->getPixelComponents() == ePixelComponentAlpha);
    const PIX *srcPix = NULL;

    // are we doing masking/mixing? in this case, retrieve srcPix
    if (masked && srcImg) {
        if ( (domask /*&& maskImg*/) || (mix != 1.) ) {
            srcPix = (const PIX *)srcImg->getPixelAddress(x, y);
        }
    }

    return ofxsMaskMixPix<PIX, nComponents, maxValue, masked>(tmpPix, x, y, srcPix, domask, maskImg, mix, maskInvert, dstPix);
}
} // OFX

#endif // ifndef Misc_ofxsMaskMix_h
