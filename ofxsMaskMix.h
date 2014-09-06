/*
 OFX Masking/Mixing help functions

 Author: Frederic Devernay <frederic.devernay@inria.fr>

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

#ifndef Misc_ofxsMaskMix_h
#define Misc_ofxsMaskMix_h

#include <limits>

#include <ofxsImageEffect.h>
#include <ofxsImageBlender.H>

#define kParamPremult "premult"
#define kParamPremultLabel "(Un)premult"
#define kParamPremultHint \
"Divide the image by the alpha channel before processing, and re-multiply it afterwards. " \
"Use if the input images are premultiplied."

#define kParamPremultChannel "premultChannel"
#define kParamPremultChannelLabel "By"
#define kParamPremultChannelHint \
"The channel to use for (un)premult."
#define kParamPremultChannelR "R"
#define kParamPremultChannelRHint "R channel from input"
#define kParamPremultChannelG "G"
#define kParamPremultChannelGHint "G channel from input"
#define kParamPremultChannelB "B"
#define kParamPremultChannelBHint "B channel from input"
#define kParamPremultChannelA "A"
#define kParamPremultChannelAHint "A channel from input"

#define kParamMix "mix"
#define kParamMixLabel "Mix"
#define kParamMixHint "Mix factor between the original and the transformed image"
#define kParamMaskInvert "maskInvert"
#define kParamMaskInvertLabel "Invert Mask"
#define kParamMaskInvertHint "When checked, the effect is fully applied where the mask is 0"

namespace OFX {

inline
void
ofxsPremultDescribeParams(OFX::ImageEffectDescriptor &desc, OFX::PageParamDescriptor *page)
{
    {
        OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamPremult);
        param->setLabels(kParamPremultLabel, kParamPremultLabel, kParamPremultLabel);
        param->setHint(kParamPremultHint);
        param->setLayoutHint(eLayoutHintNoNewLine);
        page->addChild(*param);
    }
    {
        // not yet implemented, for future use (whenever deep compositing is supported)
        OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamPremultChannel);
        param->setLabels(kParamPremultChannelLabel, kParamPremultChannelLabel, kParamPremultChannelLabel);
        param->setHint(kParamPremultChannelHint);
        param->appendOption(kParamPremultChannelR, kParamPremultChannelRHint);
        param->appendOption(kParamPremultChannelG, kParamPremultChannelGHint);
        param->appendOption(kParamPremultChannelB, kParamPremultChannelBHint);
        param->appendOption(kParamPremultChannelA, kParamPremultChannelAHint);
        param->setDefault(3); // alpha
        param->setIsSecret(true); // not yet implemented
        page->addChild(*param);
    }
}

inline
void
ofxsMaskMixDescribeParams(OFX::ImageEffectDescriptor &desc, OFX::PageParamDescriptor *page)
{
    // GENERIC (MASKED)
    //
    {
        OFX::DoubleParamDescriptor* param = desc.defineDoubleParam(kParamMix);
        param->setLabels(kParamMixLabel, kParamMixLabel, kParamMixLabel);
        param->setHint(kParamMixHint);
        param->setDefault(1.);
        param->setRange(0.,1.);
        param->setIncrement(0.01);
        param->setDisplayRange(0.,1.);
        page->addChild(*param);
    }
    {
        OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamMaskInvert);
        param->setLabels(kParamMaskInvertLabel, kParamMaskInvertLabel, kParamMaskInvertLabel);
        param->setHint(kParamMaskInvertHint);
        page->addChild(*param);
    }
}

template <class T>
inline
T ofxsClamp(T v, int min, int max)
{
    if(v < T(min)) return T(min);
    if(v > T(max)) return T(max);
    return v;
}

template <int maxValue>
inline
float ofxsClampIfInt(float v, int min, int max)
{
    if (maxValue == 1) {
        return v;
    }
    return ofxsClamp(v, min, max);
}

// normalize in [0,1] and unpremultiply srcPix
template <class PIX, int nComponents, int maxValue>
void
ofxsUnPremult(const PIX *srcPix, float unpPix[4], bool premult, int /*premultChannel*/)
{
    if (!srcPix) {
        // no src pixel here, be black and transparent
        for (int c = 0; c < 4; ++c) {
            unpPix[c] = 0.;
        }

        return;
    }

    if (nComponents == 1) {
        unpPix[0] = 0.;
        unpPix[1] = 0.;
        unpPix[2] = 0.;
        unpPix[3] = srcPix[0] / (double)maxValue;

        return;
    }

    if (!premult || (nComponents == 3) || (srcPix[3] <= 0.)) {
        unpPix[0] = srcPix[0] / (double)maxValue;
        unpPix[1] = srcPix[1] / (double)maxValue;
        unpPix[2] = srcPix[2] / (double)maxValue;
        unpPix[3] = (nComponents == 4) ? (srcPix[3] / (double)maxValue) : 0.0;

        return;
    }

    assert(nComponents == 4);
    const float fltmin = std::numeric_limits<float>::min();
    PIX alpha = srcPix[3];
    if (alpha > (PIX)(fltmin * maxValue)) {
        unpPix[0] = srcPix[0] / alpha;
        unpPix[1] = srcPix[1] / alpha;
        unpPix[2] = srcPix[2] / alpha;
    } else {
        unpPix[0] = srcPix[0] / (double)maxValue;
        unpPix[1] = srcPix[1] / (double)maxValue;
        unpPix[2] = srcPix[2] / (double)maxValue;
    }
    unpPix[3] = srcPix[3] / (double)maxValue;
}

// premultiply and denormalize in [0, maxValue]
template <class PIX, int nComponents, int maxValue>
void
ofxsPremult(const float unpPix[4], float *tmpPix, bool premult, int /*premultChannel*/)
{
    if (nComponents == 1) {
        tmpPix[0] = unpPix[3] * maxValue;

        return;
    }

    if (!premult) {
        tmpPix[0] = unpPix[0] * maxValue;
        tmpPix[1] = unpPix[1] * maxValue;
        tmpPix[2] = unpPix[2] * maxValue;
        if (nComponents == 4) {
            tmpPix[3] = unpPix[3] * maxValue;
        }

        return;
    }

    tmpPix[0] = unpPix[0] * unpPix[3] * maxValue;
    tmpPix[1] = unpPix[1] * unpPix[3] * maxValue;
    tmpPix[2] = unpPix[2] * unpPix[3] * maxValue;
    if (nComponents == 4) {
        tmpPix[3] = unpPix[3] * maxValue;
    }
}

// tmpPix is not normalized, it is within [0,maxValue]
template <class PIX, int nComponents, int maxValue, bool masked>
void
ofxsMaskMixPix(const float *tmpPix, //!< interpolated pixel
               int x, //!< coordinates for the pixel to be computed (PIXEL coordinates)
               int y,
               const PIX *srcPix, //!< the background image (the output is srcImg where maskImg=0, else it is tmpPix)
               bool domask, //!< apply the mask?
               const OFX::Image *maskImg, //!< the mask image (ignored if masked=false or domask=false)
               float mix, //!< mix factor between the output and bkImg
               bool maskInvert, //<! invert mask behavior
               PIX *dstPix) //!< destination pixel
{
    const PIX *maskPix = NULL;
    float maskScale = 1.;

    // are we doing masking
    if (!masked) {
        if (mix == 1.) {
            // no mask, no mix
            for (int c = 0; c < nComponents; ++c) {
                dstPix[c] = PIX(ofxsClampIfInt<maxValue>(tmpPix[c], 0, maxValue));
            }
        } else {
            // just mix
            float alpha = mix;
            if (srcPix) {
                for (int c = 0; c < nComponents; ++c) {
                    float v = tmpPix[c] * alpha + (1. - alpha) * srcPix[c];
                    dstPix[c] = PIX(ofxsClampIfInt<maxValue>(v, 0, maxValue));
                }
            } else {
                for (int c = 0; c < nComponents; ++c) {
                    float v = tmpPix[c] * alpha;
                    dstPix[c] = PIX(ofxsClampIfInt<maxValue>(v, 0, maxValue));
                }
            }
        }
    } else {
        if (domask && maskImg) {
            // we do, get the pixel from the mask
            maskPix = (const PIX *)maskImg->getPixelAddress(x, y);
            // figure the scale factor from that pixel
            if (maskPix == 0) {
                maskScale = maskInvert ? 1. : 0.;
            } else {
                maskScale = *maskPix/float(maxValue);
                if (maskInvert) {
                    maskScale = 1. - maskScale;
                }
            }
        }
        float alpha = maskScale * mix;
        if (srcPix) {
            for (int c = 0; c < nComponents; ++c) {
                float v = tmpPix[c] * alpha + (1. - alpha) * srcPix[c];
                dstPix[c] = PIX(ofxsClampIfInt<maxValue>(v, 0, maxValue));
            }
        } else {
            for (int c = 0; c < nComponents; ++c) {
                float v = tmpPix[c] * alpha;
                dstPix[c] = PIX(ofxsClampIfInt<maxValue>(v, 0, maxValue));
            }
        }
    }
}

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
    float tmpPix[nComponents];
    ofxsPremult<PIX, nComponents, maxValue>(unpPix, tmpPix, premult, premultChannel);
    for (int c = 0; c < nComponents; ++c) {
        tmpPix[c] *= maxValue;
    }
    ofxsMaskMixPix<PIX, nComponents, maxValue, true>(tmpPix, x, y, srcPix, domask, maskImg, mix, maskInvert, dstPix);
}

// tmpPix is not normalized, it is within [0,maxValue]
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
    const PIX *srcPix = NULL;

    // are we doing masking/mixing? in this case, retrieve srcPix
    if (masked && srcImg) {
        if ((domask && maskImg) || mix != 1.) {
            srcPix = (const PIX *)srcImg->getPixelAddress(x, y);
        }
    }

    return ofxsMaskMixPix<PIX,nComponents,maxValue,masked>(tmpPix, x, y, srcPix, domask, maskImg, mix, maskInvert, dstPix);
}

/** @brief  Base class used to blend two images together */
class ImageBlenderMaskedBase : public ImageBlenderBase {
protected:
    bool   _doMasking;
    const OFX::Image *_maskImg;
    bool _maskInvert;

public:
    /** @brief no arg ctor */
    ImageBlenderMaskedBase(OFX::ImageEffect &instance)
    : ImageBlenderBase(instance)
    , _doMasking(false)
    , _maskInvert(false)
    {
    }

    void setMaskImg(const OFX::Image *v) {_maskImg = v;}
    
    void doMasking(bool v) {_doMasking = v;}

    /** @brief set the mask invert flag */
    void setMaskInvert(bool maskInvert) {_maskInvert = maskInvert;}
};

/** @brief templated class to blend between two images */
template <class PIX, int nComponents, int maxValue>
class ImageBlenderMasked : public ImageBlenderMaskedBase {
public:
    // ctor
    ImageBlenderMasked(OFX::ImageEffect &instance)
    : ImageBlenderMaskedBase(instance)
    {}

    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        float tmpPix[nComponents];

        for (int y = procWindow.y1; y < procWindow.y2; y++) {
            if (_effect.abort()) {
                break;
            }

            PIX *dstPix = (PIX *) _dstImg->getPixelAddress(procWindow.x1, y);

            for (int x = procWindow.x1; x < procWindow.x2; x++) {

                PIX *fromPix = (PIX *)  (_fromImg ? _fromImg->getPixelAddress(x, y) : 0);
                PIX *toPix   = (PIX *)  (_toImg   ? _toImg->getPixelAddress(x, y)   : 0);

                if (fromPix || toPix) {

                    for (int c = 0; c < nComponents; ++c) {
                        // all images are supposed to be black and transparent outside o
                        tmpPix[c] = fromPix ? (float)fromPix[c] : 0.;
                    }
                    ofxsMaskMixPix<PIX, nComponents, maxValue, true>(tmpPix, x, y, toPix, _doMasking, _maskImg, _blend, _maskInvert, dstPix);
                } else {
                    // everything is black and transparent
                    for (int c = 0; c < nComponents; ++c) {
                        dstPix[c] = 0;
                    }
                }

                dstPix += nComponents;
            }
        }
    }

};

} // OFX

#endif
