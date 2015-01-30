#ifndef _ofxsImageBlenderMasked_h_
#define _ofxsImageBlenderMasked_h_

#include "ofxsProcessing.H"
#include "ofxsMaskMix.h"

namespace OFX {
/** @brief  Base class used to blend two images together */
class ImageBlenderMaskedBase
    : public ImageBlenderBase
{
protected:
    bool _doMasking;
    const OFX::Image *_maskImg;
    bool _maskInvert;

public:
    /** @brief no arg ctor */
    ImageBlenderMaskedBase(OFX::ImageEffect &instance)
        : ImageBlenderBase(instance)
          , _doMasking(false)
          , _maskImg(0)
          , _maskInvert(false)
    {
    }

    void setMaskImg(const OFX::Image *v,
                    bool maskInvert)
    {
        _maskImg = v; _maskInvert = maskInvert;
    }

    void doMasking(bool v)
    {
        _doMasking = v;
    }
};

/** @brief templated class to blend between two images */
template <class PIX, int nComponents, int maxValue, bool masked>
class ImageBlenderMasked
    : public ImageBlenderMaskedBase
{
public:
    // ctor
    ImageBlenderMasked(OFX::ImageEffect &instance)
        : ImageBlenderMaskedBase(instance)
    {
    }

    static PIX Lerp(const PIX &v1,
                    const PIX &v2,
                    float blend)
    {
        return PIX( (v2 - v1) * blend + v1 );
    }

    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        float tmpPix[nComponents];
        float blend = _blend;
        float blendComp = 1.0f - blend;

        for (int y = procWindow.y1; y < procWindow.y2; y++) {
            if ( _effect.abort() ) {
                break;
            }

            PIX *dstPix = (PIX *) _dstImg->getPixelAddress(procWindow.x1, y);

            for (int x = procWindow.x1; x < procWindow.x2; x++) {
                PIX *fromPix = (PIX *)  (_fromImg ? _fromImg->getPixelAddress(x, y) : 0);
                PIX *toPix   = (PIX *)  (_toImg   ? _toImg->getPixelAddress(x, y)   : 0);

                if ( masked && (fromPix || toPix) ) {
                    for (int c = 0; c < nComponents; ++c) {
                        // all images are supposed to be black and transparent outside o
                        tmpPix[c] = toPix ? (float)toPix[c] : 0.f;
                    }
                    ofxsMaskMixPix<PIX, nComponents, maxValue, masked>(tmpPix, x, y, fromPix, _doMasking, _maskImg, blend, _maskInvert, dstPix);
                } else if (fromPix && toPix) {
                    assert(!masked);
                    for (int c = 0; c < nComponents; ++c) {
                        dstPix[c] = Lerp(fromPix[c], toPix[c], blend);
                    }
                } else if (fromPix) {
                    assert(!masked);
                    for (int c = 0; c < nComponents; ++c) {
                        dstPix[c] = PIX(fromPix[c] * blendComp);
                    }
                } else if (toPix) {
                    assert(!masked);
                    for (int c = 0; c < nComponents; ++c) {
                        dstPix[c] = PIX(toPix[c] * blend);
                    }
                } else {
                    // everything is black and transparent
                    for (int c = 0; c < nComponents; ++c) {
                        dstPix[c] = PIX(0);
                    }
                }

                dstPix += nComponents;
            }
        }
    }
};
};

#endif // ifndef _ofxsImageBlenderMasked_h_


