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
ofxsPremultDescribeParams(OFX::ImageEffectDescriptor &desc,
                          OFX::PageParamDescriptor *page)
{
    {
        OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamPremult);
        param->setLabel(kParamPremultLabel);
        param->setHint(kParamPremultHint);
        param->setLayoutHint(eLayoutHintNoNewLine);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        // not yet implemented, for future use (whenever deep compositing is supported)
        OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamPremultChannel);
        param->setLabel(kParamPremultChannelLabel);
        param->setHint(kParamPremultChannelHint);
        param->appendOption(kParamPremultChannelR, kParamPremultChannelRHint);
        param->appendOption(kParamPremultChannelG, kParamPremultChannelGHint);
        param->appendOption(kParamPremultChannelB, kParamPremultChannelBHint);
        param->appendOption(kParamPremultChannelA, kParamPremultChannelAHint);
        param->setDefault(3); // alpha
        param->setIsSecret(true); // not yet implemented
        if (page) {
            page->addChild(*param);
        }
    }
}

inline
void
ofxsMaskDescribeParams(OFX::ImageEffectDescriptor &desc,
                       OFX::PageParamDescriptor *page)
{
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
ofxsMaskMixDescribeParams(OFX::ImageEffectDescriptor &desc,
                          OFX::PageParamDescriptor *page)
{
    // GENERIC (MASKED)
    //
    {
        OFX::DoubleParamDescriptor* param = desc.defineDoubleParam(kParamMix);
        param->setLabel(kParamMixLabel);
        param->setHint(kParamMixHint);
        param->setDefault(1.);
        param->setRange(0.,1.);
        param->setIncrement(0.01);
        param->setDisplayRange(0.,1.);
        if (page) {
            page->addChild(*param);
        }
    }
    ofxsMaskDescribeParams(desc, page);
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

template <int maxValue>
inline
float
ofxsClampIfInt(float v,
               int min,
               int max)
{
    if (maxValue == 1) {
        return v;
    }

    return ofxsClamp(v, min, max);
}

// normalize in [0,1] and unpremultiply srcPix
// if premult is false, just normalize
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
		unpPix[3] = (float)srcPix[0] / (float)maxValue;

        return;
    }

    if ( !premult || (nComponents == 3) || (srcPix[3] <= 0.) ) {
		unpPix[0] = (float)srcPix[0] / (float)maxValue;
		unpPix[1] = (float)srcPix[1] / (float)maxValue;
		unpPix[2] = (float)srcPix[2] / (float)maxValue;
		unpPix[3] = (nComponents == 4) ? ((float)srcPix[3] / (float)maxValue) : 1.0f;

        return;
    }

    assert(nComponents == 4);
    const float fltmin = std::numeric_limits<float>::min();
    PIX alpha = srcPix[3];
    if ( alpha > (PIX)(fltmin * maxValue) ) {
		unpPix[0] = (float)srcPix[0] / (float)alpha;
		unpPix[1] = (float)srcPix[1] / (float)alpha;
		unpPix[2] = (float)srcPix[2] / (float)alpha;
    } else {
		unpPix[0] = (float)srcPix[0] / (float)maxValue;
		unpPix[1] = (float)srcPix[1] / (float)maxValue;
		unpPix[2] = (float)srcPix[2] / (float)maxValue;
    }
	unpPix[3] = (float)srcPix[3] / (float)maxValue;
}

// premultiply and denormalize in [0, maxValue]
// if premult is false, just denormalize
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
    float maskScale = 1.f;

    // are we doing masking
    if (!masked) {
        if (mix == 1.) {
            // no mask, no mix
            for (int c = 0; c < nComponents; ++c) {
                dstPix[c] = PIX( ofxsClampIfInt<maxValue>(tmpPix[c], 0, maxValue) );
            }
        } else {
            // just mix
            float alpha = mix;
            if (srcPix) {
                for (int c = 0; c < nComponents; ++c) {
                    float v = tmpPix[c] * alpha + (1.f - alpha) * srcPix[c];
                    dstPix[c] = PIX( ofxsClampIfInt<maxValue>(v, 0, maxValue) );
                }
            } else {
                for (int c = 0; c < nComponents; ++c) {
                    float v = tmpPix[c] * alpha;
                    dstPix[c] = PIX( ofxsClampIfInt<maxValue>(v, 0, maxValue) );
                }
            }
        }
    } else {
        if (domask) {
            // we do, get the pixel from the mask
            maskPix = maskImg ? (const PIX *)maskImg->getPixelAddress(x, y) : 0;
            // figure the scale factor from that pixel
            if (maskPix == 0) {
                maskScale = maskInvert ? 1.f : 0.f;
            } else {
				if (maskImg->getPixelComponents() == ePixelComponentAlpha)
					maskScale = *maskPix / float(maxValue);
				else
					maskScale = *(maskPix + 3) / float(maxValue);

                if (maskInvert) {
                    maskScale = 1.f - maskScale;
                }
            }
        }
        float alpha = maskScale * mix;
        if (srcPix) {
            for (int c = 0; c < nComponents; ++c) {
                float v = tmpPix[c] * alpha + (1.f - alpha) * srcPix[c];
                dstPix[c] = PIX( ofxsClampIfInt<maxValue>(v, 0, maxValue) );
            }
        } else {
            for (int c = 0; c < nComponents; ++c) {
                float v = tmpPix[c] * alpha;
                dstPix[c] = PIX( ofxsClampIfInt<maxValue>(v, 0, maxValue) );
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
    float tmpPix[nComponents];

    ofxsPremult<PIX, nComponents, maxValue>(unpPix, tmpPix, premult, premultChannel);
    //for (int c = 0; c < nComponents; ++c) {
        //tmpPix[c] *= maxValue;
    //}
	ofxsMaskMixPix<PIX, nComponents, maxValue, masked>(tmpPix, x, y, srcPix, domask, maskImg, mix, maskInvert, dstPix);
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
        if ( (domask /*&& maskImg*/) || (mix != 1.) ) {
            srcPix = (const PIX *)srcImg->getPixelAddress(x, y);
        }
    }

    return ofxsMaskMixPix<PIX,nComponents,maxValue,masked>(tmpPix, x, y, srcPix, domask, maskImg, mix, maskInvert, dstPix);
}
} // OFX

#endif // ifndef Misc_ofxsMaskMix_h
