//
//  ofxsCopier.h
//  IO
//
//  Created by Frédéric Devernay on 03/02/2014.
//
//

#ifndef IO_ofxsCopier_h
#define IO_ofxsCopier_h

#include <cstring>
#include "ofxsPixelProcessor.h"
#include "ofxsMaskMix.h"

namespace OFX {

// Base class for the RGBA and the Alpha processor

// template to do the RGBA processing
template <class PIX, int nComponents, int maxValue>
class PixelCopier : public OFX::PixelProcessorFilterBase {
public:
    // ctor
    PixelCopier(OFX::ImageEffect &instance)
    : OFX::PixelProcessorFilterBase(instance)
    {
    }

    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        int rowBytes = sizeof(PIX) * nComponents * (procWindow.x2 - procWindow.x1);
        for(int y = procWindow.y1; y < procWindow.y2; ++y) {
            if(_effect.abort()) {
                break;
            }

            PIX *dstPix = (PIX *) getDstPixelAddress(procWindow.x1, y);
            assert(dstPix);

            if (y < _srcBounds.y1 || _srcBounds.y2 <= y) {
                std::memset(dstPix, 0, rowBytes);
            } else {
                int x1 = std::max(_srcBounds.x1, procWindow.x1);
                int x2 = std::min(_srcBounds.x2, procWindow.x2);
                // start of line may be black
                if (procWindow.x1 < x1) {
                    std::memset(dstPix, 0, sizeof(PIX) * nComponents * (x1 - procWindow.x1));
                    dstPix += nComponents * (x1 - procWindow.x1);
                }
                // then, copy the relevant fraction of src
                if (x1 < x2 && procWindow.x1 <= x1 && x2 <= procWindow.x2) {
                    const PIX *srcPix = (const PIX *) getSrcPixelAddress(x1, y);
                    assert(srcPix);
                    std::memcpy(dstPix, srcPix, sizeof(PIX) * nComponents * (x2 - x1));
                    dstPix += nComponents * (x2 - x1);
                }
                // end of line may be black
                if (x2 < procWindow.x2) {
                    std::memset(dstPix, 0, sizeof(PIX) * nComponents * (procWindow.x2 - x2));
                }
            }
        }
    }
};

template <class PIX, int nComponents, int maxValue, bool masked>
class PixelCopierMaskMix : public OFX::PixelProcessorFilterBase {
public:
    // ctor
    PixelCopierMaskMix(OFX::ImageEffect &instance)
    : OFX::PixelProcessorFilterBase(instance)
    {
    }

    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        float tmpPix[nComponents];
        for(int y = procWindow.y1; y < procWindow.y2; ++y) {
            if(_effect.abort()) {
                break;
            }

            PIX *dstPix = (PIX *) getDstPixelAddress(procWindow.x1, y);
            assert(dstPix);

            for (int x = procWindow.x1; x < procWindow.x2; x++) {
                const PIX *origPix = (const PIX *)  (_origImg ? _origImg->getPixelAddress(x, y) : 0);
                const PIX *srcPix = (const PIX *) getSrcPixelAddress(x, y);
                if (srcPix) {
                    std::copy(srcPix, srcPix + nComponents, tmpPix);
                } else {
                    std::fill(tmpPix, tmpPix + nComponents, 0.); // no src pixel here, be black and transparent
                }
                ofxsMaskMixPix<PIX, nComponents, maxValue, masked>(tmpPix, x, y, origPix, _doMasking, _maskImg, _mix, _maskInvert, dstPix);
                // increment the dst pixel
                dstPix += nComponents;
            }
        }
    }
};

// be careful, if _premult is false this processor does nothing!
template <class SRCPIX, int srcNComponents, int srcMaxValue, class DSTPIX, int dstNComponents, int dstMaxValue>
class PixelCopierUnPremult : public OFX::PixelProcessorFilterBase {
public:
    // ctor
    PixelCopierUnPremult(OFX::ImageEffect &instance)
    : OFX::PixelProcessorFilterBase(instance)
    {
        assert((srcNComponents == 3 || srcNComponents == 4) && (dstNComponents == 3 || dstNComponents == 4));
    }

    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        float unpPix[4];
        for(int y = procWindow.y1; y < procWindow.y2; ++y) {
            if(_effect.abort()) {
                break;
            }

            DSTPIX *dstPix = (DSTPIX *) getDstPixelAddress(procWindow.x1, y);
            assert(dstPix);

            for (int x = procWindow.x1; x < procWindow.x2; x++) {
                const SRCPIX *srcPix = (const SRCPIX *) getSrcPixelAddress(x, y);
                ofxsUnPremult<SRCPIX, srcNComponents, srcMaxValue>(srcPix, unpPix, _premult, _premultChannel);
                for (int c = 0; c < dstNComponents; ++c) {
                    float v = unpPix[c] * dstMaxValue;
                    dstPix[c] = DSTPIX(ofxsClampIfInt<dstMaxValue>(v, 0, dstMaxValue));
                }
                // increment the dst pixel
                dstPix += dstNComponents;
            }
        }
    }
};

// be careful, if _premult is false this processor does nothing!
template <class SRCPIX, int srcNComponents, int srcMaxValue, class DSTPIX, int dstNComponents, int dstMaxValue>
class PixelCopierPremult : public OFX::PixelProcessorFilterBase {
public:
    // ctor
    PixelCopierPremult(OFX::ImageEffect &instance)
    : OFX::PixelProcessorFilterBase(instance)
    {
        assert((srcNComponents == 3 || srcNComponents == 4) && (dstNComponents == 3 || dstNComponents == 4));
    }

    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        float unpPix[4];
        for(int y = procWindow.y1; y < procWindow.y2; ++y) {
            if(_effect.abort()) {
                break;
            }

            DSTPIX *dstPix = (DSTPIX *) getDstPixelAddress(procWindow.x1, y);
            assert(dstPix);

            for (int x = procWindow.x1; x < procWindow.x2; x++) {
                const SRCPIX *srcPix = (const SRCPIX *) getSrcPixelAddress(x, y);
                ofxsPremult<SRCPIX, srcNComponents, srcMaxValue>(srcPix, unpPix, _premult, _premultChannel);
                for (int c = 0; c < dstNComponents; ++c) {
                    float v = unpPix[c] * dstMaxValue;
                    dstPix[c] = DSTPIX(ofxsClampIfInt<dstMaxValue>(v, 0, dstMaxValue));
                }
                // increment the dst pixel
                dstPix += dstNComponents;
            }
        }
    }
};

// template to do the RGBA processing
template <class SRCPIX, int srcNComponents, int srcMaxValue, class DSTPIX, int dstNComponents, int dstMaxValue>
class PixelCopierPremultMaskMix : public OFX::PixelProcessorFilterBase {
public:
    // ctor
    PixelCopierPremultMaskMix(OFX::ImageEffect &instance)
    : OFX::PixelProcessorFilterBase(instance)
    {
        assert((srcNComponents == 3 || srcNComponents == 4) && (dstNComponents == 3 || dstNComponents == 4));
    }

    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        float unpPix[4];
        if (srcNComponents == 3) {
            unpPix[3] = 1.;
        }
        for(int y = procWindow.y1; y < procWindow.y2; ++y) {
            if(_effect.abort()) {
                break;
            }

            DSTPIX *dstPix = (DSTPIX *) getDstPixelAddress(procWindow.x1, y);
            assert(dstPix);

            for (int x = procWindow.x1; x < procWindow.x2; x++) {
                const DSTPIX *origPix = (const DSTPIX *)  (_origImg ? _origImg->getPixelAddress(x, y) : 0);
                const SRCPIX *srcPix = (const SRCPIX *) getSrcPixelAddress(x, y);
                for (int c = 0; c < srcNComponents; ++c) {
                    unpPix[c] = (srcPix ? (srcPix[c] / (double)srcMaxValue) : 0.);
                }
                ofxsPremultMaskMixPix<DSTPIX, dstNComponents, dstMaxValue, true>(unpPix, _premult, _premultChannel, x, y, origPix, _doMasking, _maskImg, _mix, _maskInvert, dstPix);
                // increment the dst pixel
                dstPix += dstNComponents;
            }
        }
    }
};

template <class PIX, int nComponents>
class BlackFiller : public OFX::PixelProcessorFilterBase {
public:
    // ctor
    BlackFiller(OFX::ImageEffect &instance)
    : OFX::PixelProcessorFilterBase(instance)
    {
    }
    
    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        int rowSize =  nComponents * (procWindow.x2 - procWindow.x1);
        for(int y = procWindow.y1; y < procWindow.y2; ++y) {
            if(_effect.abort()) {
                break;
            }

            PIX *dstPix = (PIX *) getDstPixelAddress(procWindow.x1, y);
            assert(dstPix);
            std::fill(dstPix, dstPix + rowSize,0);
        }
    }
};

template<class PIX,int nComponents>
void copyPixels(const OfxRectI& renderWindow,
                const PIX *srcPixelData,
                const OfxRectI& srcBounds,
                OFX::PixelComponentEnum srcPixelComponents,
                OFX::BitDepthEnum srcBitDepth,
                int srcRowBytes,
                PIX *dstPixelData,
                const OfxRectI& dstBounds,
                OFX::PixelComponentEnum dstPixelComponents,
                OFX::BitDepthEnum dstBitDepth,
                int dstRowBytes)
{
    assert(srcPixelComponents == dstPixelComponents && srcBitDepth == dstBitDepth);
    (void)srcPixelComponents;
    (void)srcBitDepth;
    (void)dstPixelComponents;
    (void)dstBitDepth;

    int srcRowElements = srcRowBytes / sizeof(PIX);
    assert(srcBounds.y1 <= renderWindow.y1 && renderWindow.y1 <= renderWindow.y2 && renderWindow.y2 <= srcBounds.y2);
    assert(srcBounds.x1 <= renderWindow.x1 && renderWindow.x1 <= renderWindow.x2 && renderWindow.x2 <= srcBounds.x2);
    const PIX* srcPixels = srcPixelData + (size_t)(renderWindow.y1 - srcBounds.y1) * srcRowElements + (renderWindow.x1 - srcBounds.x1) * nComponents;
    
    int dstRowElements = dstRowBytes / sizeof(PIX);
    
    PIX* dstPixels = dstPixelData + (size_t)(renderWindow.y1 - dstBounds.y1) * dstRowElements + (renderWindow.x1 - dstBounds.x1) * nComponents;
    
    int rowBytes = sizeof(PIX) * nComponents * (renderWindow.x2 - renderWindow.x1);
    
    for (int y = renderWindow.y1; y < renderWindow.y2; ++y,srcPixels += srcRowElements, dstPixels += dstRowElements) {
        std::memcpy(dstPixels, srcPixels, rowBytes);
    }
}

} // OFX

#endif
