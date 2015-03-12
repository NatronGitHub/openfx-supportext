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

template <class PIX, int nComponents>
class PixelCopier
    : public OFX::PixelProcessorFilterBase
{
public:
    // ctor
    PixelCopier(OFX::ImageEffect &instance)
        : OFX::PixelProcessorFilterBase(instance)
    {
    }

    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        assert(_srcBounds.x1 < _srcBounds.x2 && _srcBounds.y1 < _srcBounds.y2); // image should be non-empty

        int rowBytes = sizeof(PIX) * nComponents * (procWindow.x2 - procWindow.x1);

        for (int dsty = procWindow.y1; dsty < procWindow.y2; ++dsty) {
            if ( _effect.abort() ) {
                break;
            }

            PIX *dstPix = (PIX *) getDstPixelAddress(procWindow.x1, dsty);
            assert(dstPix);

            int srcy = dsty;

            if (_srcBoundary == 1) {
                if (_srcBounds.y2 <= srcy) {
                    srcy = _srcBounds.y2 - 1;
                }
                if (srcy < _srcBounds.y1) {
                    srcy = _srcBounds.y1;
                }
            } else if (_srcBoundary == 2) {
                if (srcy < _srcBounds.y1 || _srcBounds.y2 <= srcy) {
                    srcy = _srcBounds.y1 + positive_modulo(srcy - _srcBounds.y1, _srcBounds.y2 - _srcBounds.y1);
                }
            }

            if ( (srcy < _srcBounds.y1) || (_srcBounds.y2 <= srcy) || (_srcBounds.y2 <= _srcBounds.y1)) {
                assert(_srcBoundary == 0);
                std::memset(dstPix, 0, rowBytes);
            } else {
                int x1 = std::max(_srcBounds.x1, procWindow.x1);
                int x2 = std::min(_srcBounds.x2, procWindow.x2);
                // start of line may be black
                if (procWindow.x1 < x1) {
                    if (_srcBoundary != 1 && _srcBoundary != 2) {
                        std::memset( dstPix, 0, sizeof(PIX) * nComponents * (x1 - procWindow.x1) );
                        dstPix += nComponents * (x1 - procWindow.x1);
                    } else if (_srcBoundary == 1) {
                        const PIX *srcPix = (const PIX *) getSrcPixelAddress(x1, srcy);
                        assert(srcPix);
                        for (int x = procWindow.x1; x < x1; ++x) {
                            std::memcpy( dstPix, srcPix, sizeof(PIX) * nComponents );
                            dstPix += nComponents;
                        }
                    } else if (_srcBoundary == 2) {
                        int srcx = procWindow.x1;
                        if (srcx < _srcBounds.x1 || _srcBounds.x2 <= srcx) {
                            srcx = _srcBounds.x1 + positive_modulo(srcx - _srcBounds.x1, _srcBounds.x2 - _srcBounds.x1);
                        }

                        const PIX *srcPix = (const PIX *) getSrcPixelAddress(srcx, srcy);
                        assert(srcPix);
                        for (int x = procWindow.x1; x < x1; ++x) {
                            std::memcpy( dstPix, srcPix, sizeof(PIX) * nComponents );
                            dstPix += nComponents;
                            ++srcx;
                            if (_srcBounds.x2 <= srcx) {
                                srcx -= (_srcBounds.y2 - _srcBounds.y1);
                                srcPix -= (_srcBounds.y2 - _srcBounds.y1)*nComponents;
                            }
                        }
                    }
                }
                // then, copy the relevant fraction of src
                if ( (x1 < x2) && (procWindow.x1 <= x1) && (x2 <= procWindow.x2) ) {
                    const PIX *srcPix = (const PIX *) getSrcPixelAddress(x1, srcy);
                    assert(srcPix);
                    std::memcpy( dstPix, srcPix, sizeof(PIX) * nComponents * (x2 - x1) );
                    dstPix += nComponents * (x2 - x1);
                }
                // end of line may be black
                if (x2 < procWindow.x2) {
                    if (_srcBoundary != 1 && _srcBoundary != 2) {
                        std::memset( dstPix, 0, sizeof(PIX) * nComponents * (procWindow.x2 - x2) );
                    } else if (_srcBoundary == 1) {
                        const PIX *srcPix = (const PIX *) getSrcPixelAddress(x2-1, srcy);
                        assert(srcPix);
                        for (int x = x2; x < procWindow.x2; ++x) {
                            std::memcpy( dstPix, srcPix, sizeof(PIX) * nComponents );
                            dstPix += nComponents;
                        }
                    } else if (_srcBoundary == 2) {
                        int srcx = x2;
                        while (_srcBounds.x2 <= srcx) {
                            srcx -= (_srcBounds.x2 - _srcBounds.x1);
                        }

                        const PIX *srcPix = (const PIX *) getSrcPixelAddress(srcx, srcy);
                        assert(srcPix);
                        for (int x = x2; x < procWindow.x2; ++x) {
                            std::memcpy( dstPix, srcPix, sizeof(PIX) * nComponents );
                            dstPix += nComponents;
                            ++srcx;
                            if (_srcBounds.x2 <= srcx) {
                                srcx -= (_srcBounds.y2 - _srcBounds.y1);
                                srcPix -= (_srcBounds.y2 - _srcBounds.y1)*nComponents;
                            }
                        }
                    }
                }
            }
        }
    }
};

template <class PIX, int nComponents, int maxValue, bool masked>
class PixelCopierMaskMix
    : public OFX::PixelProcessorFilterBase
{
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

        for (int dsty = procWindow.y1; dsty < procWindow.y2; ++dsty) {
            if ( _effect.abort() ) {
                break;
            }

            int srcy = dsty;

            if (_srcBoundary == 1) {
                if (_srcBounds.y2 <= srcy) {
                    srcy = _srcBounds.y2 - 1;
                }
                if (srcy < _srcBounds.y1) {
                    srcy = _srcBounds.y1;
                }
            } else if (_srcBoundary == 2) {
                if (srcy < _srcBounds.y1 || _srcBounds.y2 <= srcy) {
                    srcy = _srcBounds.y1 + positive_modulo(srcy - _srcBounds.y1, _srcBounds.y2 - _srcBounds.y1);
                }
            }

            PIX *dstPix = (PIX *) getDstPixelAddress(procWindow.x1, dsty);
            assert(dstPix);

            for (int dstx = procWindow.x1; dstx < procWindow.x2; ++dstx) {

                int srcx = dstx;

                if (_srcBoundary == 1) {
                    if (_srcBounds.x2 <= srcx) {
                        srcx = _srcBounds.x2 - 1;
                    }
                    if (srcx < _srcBounds.x1) {
                        srcx = _srcBounds.x1;
                    }
                } else if (_srcBoundary == 2) {
                    if (srcx < _srcBounds.x1 || _srcBounds.x2 <= srcx) {
                        srcx = _srcBounds.x1 + positive_modulo(srcx - _srcBounds.x1, _srcBounds.x2 - _srcBounds.x1);
                    }
                }

                // origPix is at dstx,dsty
                const PIX *origPix = (const PIX *)  (_origImg ? _origImg->getPixelAddress(dstx, dsty) : 0);
                const PIX *srcPix = (const PIX *) getSrcPixelAddress(srcx, srcy);
                if (srcPix) {
                    std::copy(srcPix, srcPix + nComponents, tmpPix);
                } else {
                    std::fill(tmpPix, tmpPix + nComponents, 0.); // no src pixel here, be black and transparent
                }
                // dstx,dsty are the mask image coordinates (no boundary conditions)
                ofxsMaskMixPix<PIX, nComponents, maxValue, masked>(tmpPix, dstx, dsty, origPix, _doMasking, _maskImg, (float)_mix, _maskInvert, dstPix);
                // increment the dst pixel
                dstPix += nComponents;
            }
        }
    }
};

template <class SRCPIX, int srcNComponents, int srcMaxValue, class DSTPIX, int dstNComponents, int dstMaxValue>
class PixelCopierUnPremult
    : public OFX::PixelProcessorFilterBase
{
public:
    // ctor
    PixelCopierUnPremult(OFX::ImageEffect &instance)
        : OFX::PixelProcessorFilterBase(instance)
    {
        assert( (srcNComponents == 3 || srcNComponents == 4) && (dstNComponents == 3 || dstNComponents == 4) );
    }

    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        float unpPix[4];

        assert(_srcBounds.x1 < _srcBounds.x2 && _srcBounds.y1 < _srcBounds.y2);

        for (int dsty = procWindow.y1; dsty < procWindow.y2; ++dsty) {
            if ( _effect.abort() ) {
                break;
            }

            int srcy = dsty;

            if (_srcBoundary == 1) {
                if (_srcBounds.y2 <= srcy) {
                    srcy = _srcBounds.y2 - 1;
                }
                if (srcy < _srcBounds.y1) {
                    srcy = _srcBounds.y1;
                }
            } else if (_srcBoundary == 2) {
                if (srcy < _srcBounds.y1 || _srcBounds.y2 <= srcy) {
                    srcy = _srcBounds.y1 + positive_modulo(srcy - _srcBounds.y1, _srcBounds.y2 - _srcBounds.y1);
                }
            }

            DSTPIX *dstPix = (DSTPIX *) getDstPixelAddress(procWindow.x1, dsty);
            assert(dstPix);

            for (int dstx = procWindow.x1; dstx < procWindow.x2; ++dstx) {

                int srcx = dstx;

                if (_srcBoundary == 1) {
                    if (_srcBounds.x2 <= srcx) {
                        srcx = _srcBounds.x2 - 1;
                    }
                    if (srcx < _srcBounds.x1) {
                        srcx = _srcBounds.x1;
                    }
                } else if (_srcBoundary == 2) {
                    if (srcx < _srcBounds.x1 || _srcBounds.x2 <= srcx) {
                        srcx = _srcBounds.x1 + positive_modulo(srcx - _srcBounds.x1, _srcBounds.x2 - _srcBounds.x1);
                    }
                }

                const SRCPIX *srcPix = (const SRCPIX *) getSrcPixelAddress(srcx, srcy);
                ofxsUnPremult<SRCPIX, srcNComponents, srcMaxValue>(srcPix, unpPix, _premult, _premultChannel);
                for (int c = 0; c < dstNComponents; ++c) {
                    float v = unpPix[c] * dstMaxValue;
                    dstPix[c] = DSTPIX( ofxsClampIfInt<dstMaxValue>(v, 0, dstMaxValue) );
                }
                // increment the dst pixel
                dstPix += dstNComponents;
            }
        }
    }
};

template <class SRCPIX, int srcNComponents, int srcMaxValue, class DSTPIX, int dstNComponents, int dstMaxValue>
class PixelCopierPremult
    : public OFX::PixelProcessorFilterBase
{
public:
    // ctor
    PixelCopierPremult(OFX::ImageEffect &instance)
        : OFX::PixelProcessorFilterBase(instance)
    {
        assert( (srcNComponents == 3 || srcNComponents == 4) && (dstNComponents == 3 || dstNComponents == 4) );
    }

    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        float unpPix[4];

        assert(_srcBounds.x1 < _srcBounds.x2 && _srcBounds.y1 < _srcBounds.y2);

        for (int dsty = procWindow.y1; dsty < procWindow.y2; ++dsty) {
            if ( _effect.abort() ) {
                break;
            }

            int srcy = dsty;

            if (_srcBoundary == 1) {
                if (_srcBounds.y2 <= srcy) {
                    srcy = _srcBounds.y2 - 1;
                }
                if (srcy < _srcBounds.y1) {
                    srcy = _srcBounds.y1;
                }
            } else if (_srcBoundary == 2) {
                if (srcy < _srcBounds.y1 || _srcBounds.y2 <= srcy) {
                    srcy = _srcBounds.y1 + positive_modulo(srcy - _srcBounds.y1, _srcBounds.y2 - _srcBounds.y1);
                }
            }

            DSTPIX *dstPix = (DSTPIX *) getDstPixelAddress(procWindow.x1, dsty);
            assert(dstPix);

            for (int dstx = procWindow.x1; dstx < procWindow.x2; ++dstx) {

                int srcx = dstx;

                if (_srcBoundary == 1) {
                    if (_srcBounds.x2 <= srcx) {
                        srcx = _srcBounds.x2 - 1;
                    }
                    if (srcx < _srcBounds.x1) {
                        srcx = _srcBounds.x1;
                    }
                } else if (_srcBoundary == 2) {
                    if (srcx < _srcBounds.x1 || _srcBounds.x2 <= srcx) {
                        srcx = _srcBounds.x1 + positive_modulo(srcx - _srcBounds.x1, _srcBounds.x2 - _srcBounds.x1);
                    }
                }

                const SRCPIX *srcPix = (const SRCPIX *) getSrcPixelAddress(srcx, srcy);
                ofxsPremult<SRCPIX, srcNComponents, srcMaxValue>(srcPix, unpPix, _premult, _premultChannel);
                for (int c = 0; c < dstNComponents; ++c) {
                    float v = unpPix[c] * dstMaxValue;
                    dstPix[c] = DSTPIX( ofxsClampIfInt<dstMaxValue>(v, 0, dstMaxValue) );
                }
                // increment the dst pixel
                dstPix += dstNComponents;
            }
        }
    }
};

// _srcBoundarys The border condition type { 0=zero |  1=dirichlet | 2=periodic }.
// template to do the RGBA processing
template <class SRCPIX, int srcNComponents, int srcMaxValue, class DSTPIX, int dstNComponents, int dstMaxValue>
class PixelCopierPremultMaskMix
    : public OFX::PixelProcessorFilterBase
{
public:
    // ctor
    PixelCopierPremultMaskMix(OFX::ImageEffect &instance)
        : OFX::PixelProcessorFilterBase(instance)
    {
        assert( (srcNComponents == 3 || srcNComponents == 4) && (dstNComponents == 3 || dstNComponents == 4) );
    }

    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        float unpPix[4];

        assert(_srcBounds.x1 < _srcBounds.x2 && _srcBounds.y1 < _srcBounds.y2);

        if (srcNComponents == 3) {
            unpPix[3] = 1.f;
        }
        for (int dsty = procWindow.y1; dsty < procWindow.y2; ++dsty) {
            if ( _effect.abort() ) {
                break;
            }

            int srcy = dsty;

            if (_srcBoundary == 1) {
                if (_srcBounds.y2 <= srcy) {
                    srcy = _srcBounds.y2 - 1;
                }
                if (srcy < _srcBounds.y1) {
                    srcy = _srcBounds.y1;
                }
            } else if (_srcBoundary == 2) {
                if (srcy < _srcBounds.y1 || _srcBounds.y2 <= srcy) {
                    srcy = _srcBounds.y1 + positive_modulo(srcy - _srcBounds.y1, _srcBounds.y2 - _srcBounds.y1);
                }
            }
            
            DSTPIX *dstPix = (DSTPIX *) getDstPixelAddress(procWindow.x1, dsty);
            assert(dstPix);

            for (int dstx = procWindow.x1; dstx < procWindow.x2; ++dstx) {

                int srcx = dstx;

                if (_srcBoundary == 1) {
                    if (_srcBounds.x2 <= srcx) {
                        srcx = _srcBounds.x2 - 1;
                    }
                    if (srcx < _srcBounds.x1) {
                        srcx = _srcBounds.x1;
                    }
                } else if (_srcBoundary == 2) {
                    if (srcx < _srcBounds.x1 || _srcBounds.x2 <= srcx) {
                        srcx = _srcBounds.x1 + positive_modulo(srcx - _srcBounds.x1, _srcBounds.x2 - _srcBounds.x1);
                    }
                }
                 // origPix is at dstx,dsty
                const DSTPIX *origPix = (const DSTPIX *)  (_origImg ? _origImg->getPixelAddress(dstx, dsty) : 0);
                const SRCPIX *srcPix = (const SRCPIX *) getSrcPixelAddress(srcx, srcy);
                for (int c = 0; c < srcNComponents; ++c) {
                    unpPix[c] = (srcPix ? (srcPix[c] / (float)srcMaxValue) : 0.f);
                }
                // dstx,dsty are the mask image coordinates (no boundary conditions)
                ofxsPremultMaskMixPix<DSTPIX, dstNComponents, dstMaxValue, true>(unpPix, _premult, _premultChannel, dstx, dsty, origPix, _doMasking, _maskImg, (float)_mix, _maskInvert, dstPix);
                // increment the dst pixel
                dstPix += dstNComponents;
            }
        }
    }
};

template <class PIX>
class BlackFiller
    : public OFX::PixelProcessorFilterBase
{
public:
    // ctor
    BlackFiller(OFX::ImageEffect &instance,
                int comps)
        : OFX::PixelProcessorFilterBase(instance)
        , _nComponents(comps)
    {
    }

    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        int rowSize =  _nComponents * (procWindow.x2 - procWindow.x1);

        for (int y = procWindow.y1; y < procWindow.y2; ++y) {
            if ( _effect.abort() ) {
                break;
            }

            PIX *dstPix = (PIX *) getDstPixelAddress(procWindow.x1, y);
            assert(dstPix);
            if (!dstPix) {
                // coverity[dead_error_line]
                continue;
            }
            std::fill(dstPix, dstPix + rowSize, PIX());
        }
    }
private:
    int _nComponents;
};

// black fillers, non-threaded versions
template<class PIX,int nComponents>
void
fillBlackNTForDepthAndComponents(OFX::ImageEffect &instance,
                                 const OfxRectI & renderWindow,
                                 PIX *dstPixelData,
                                 const OfxRectI & dstBounds,
                                 OFX::PixelComponentEnum dstPixelComponents,
                                 OFX::BitDepthEnum dstBitDepth,
                                 int dstRowBytes)
{
    (void)dstPixelComponents;
    (void)dstBitDepth;
    (void)instance;

    int dstRowElements = dstRowBytes / sizeof(PIX);
    PIX* dstPixels = dstPixelData + (size_t)(renderWindow.y1 - dstBounds.y1) * dstRowElements + (renderWindow.x1 - dstBounds.x1) * nComponents;
    int rowBytes = sizeof(PIX) * nComponents * (renderWindow.x2 - renderWindow.x1);

    for (int y = renderWindow.y1; y < renderWindow.y2; ++y, dstPixels += dstRowElements) {
        std::fill(dstPixels, dstPixels + rowBytes, PIX()); // no src pixel here, be black and transparent
    }
}

template<class PIX>
void
fillBlackNTForDepth(OFX::ImageEffect &instance,
                    const OfxRectI & renderWindow,
                    void *dstPixelData,
                    const OfxRectI & dstBounds,
                    OFX::PixelComponentEnum dstPixelComponents,
                    OFX::BitDepthEnum dstBitDepth,
                    int dstRowBytes)
{
    assert(dstPixelData);
    // do the rendering
    if (dstPixelComponents != OFX::ePixelComponentRGBA && dstPixelComponents != OFX::ePixelComponentRGB && dstPixelComponents != OFX::ePixelComponentAlpha) {
        OFX::throwSuiteStatusException(kOfxStatErrFormat);
    }
    if (dstPixelComponents == OFX::ePixelComponentRGBA) {
        fillBlackNTForDepthAndComponents<PIX,4>(instance, renderWindow,
                                               (PIX *)dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } else if (dstPixelComponents == OFX::ePixelComponentRGB) {
        fillBlackNTForDepthAndComponents<PIX,3>(instance, renderWindow,
                                               (PIX *)dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    }  else if (dstPixelComponents == OFX::ePixelComponentAlpha) {
        fillBlackNTForDepthAndComponents<PIX,1>(instance, renderWindow,
                                               (PIX *)dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } // switch
}

inline void
fillBlackNT(OFX::ImageEffect &instance,
             const OfxRectI & renderWindow,
             void *dstPixelData,
             const OfxRectI & dstBounds,
             OFX::PixelComponentEnum dstPixelComponents,
             OFX::BitDepthEnum dstBitDepth,
             int dstRowBytes)
{
    assert(dstPixelData);

    // do the rendering
    if (dstBitDepth != OFX::eBitDepthUByte && dstBitDepth != OFX::eBitDepthUShort && dstBitDepth != OFX::eBitDepthHalf && dstBitDepth != OFX::eBitDepthFloat) {
        OFX::throwSuiteStatusException(kOfxStatErrFormat);
    }
    if (dstBitDepth == OFX::eBitDepthUByte) {
        fillBlackNTForDepth<unsigned char>(instance, renderWindow,
                                          dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } else if (dstBitDepth == OFX::eBitDepthUShort || dstBitDepth == OFX::eBitDepthHalf) {
        fillBlackNTForDepth<unsigned short>(instance, renderWindow,
                                          dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } else if (dstBitDepth == OFX::eBitDepthFloat) {
        fillBlackNTForDepth<float>(instance, renderWindow,
                                          dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } // switch
}

inline void
fillBlackNT(OFX::ImageEffect &instance,
           const OfxRectI & renderWindow,
           OFX::Image* dstImg)
{
    void* dstPixelData;
    OfxRectI dstBounds;
    OFX::PixelComponentEnum dstPixelComponents;
    OFX::BitDepthEnum dstBitDepth;
    int dstRowBytes;
    getImageData(dstImg, &dstPixelData, &dstBounds, &dstPixelComponents, &dstBitDepth, &dstRowBytes);
    return fillBlackNT(instance, renderWindow, dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
}

// black fillers, threaded versions
template<class PIX,int nComponents>
void
fillBlackForDepthAndComponents(OFX::ImageEffect &instance,
                               const OfxRectI & renderWindow,
                               PIX *dstPixelData,
                               const OfxRectI & dstBounds,
                               OFX::PixelComponentEnum dstPixelComponents,
                               OFX::BitDepthEnum dstBitDepth,
                               int dstRowBytes)
{
    (void)dstPixelComponents;
    (void)dstBitDepth;

    OFX::BlackFiller<PIX> processor(instance, nComponents);
    // set the images
    processor.setDstImg(dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);

    // set the render window
    processor.setRenderWindow(renderWindow);

    // Call the base class process member, this will call the derived templated process code
    processor.process();
}

template<class PIX>
void
fillBlackForDepth(OFX::ImageEffect &instance,
                  const OfxRectI & renderWindow,
                  void *dstPixelData,
                  const OfxRectI & dstBounds,
                  OFX::PixelComponentEnum dstPixelComponents,
                  OFX::BitDepthEnum dstBitDepth,
                  int dstRowBytes)
{
    assert(dstPixelData);
    // do the rendering
    if (dstPixelComponents != OFX::ePixelComponentRGBA && dstPixelComponents != OFX::ePixelComponentRGB && dstPixelComponents != OFX::ePixelComponentAlpha) {
        OFX::throwSuiteStatusException(kOfxStatErrFormat);
    }
    if (dstPixelComponents == OFX::ePixelComponentRGBA) {
        fillBlackForDepthAndComponents<PIX,4>(instance, renderWindow,
                                              (PIX *)dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } else if (dstPixelComponents == OFX::ePixelComponentRGB) {
        fillBlackForDepthAndComponents<PIX,3>(instance, renderWindow,
                                              (PIX *)dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    }  else if (dstPixelComponents == OFX::ePixelComponentAlpha) {
        fillBlackForDepthAndComponents<PIX,1>(instance, renderWindow,
                                              (PIX *)dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } // switch
}

inline void
fillBlack(OFX::ImageEffect &instance,
          const OfxRectI & renderWindow,
          void *dstPixelData,
          const OfxRectI & dstBounds,
          OFX::PixelComponentEnum dstPixelComponents,
          OFX::BitDepthEnum dstBitDepth,
          int dstRowBytes)
{
    assert(dstPixelData);
    // do the rendering
    if (dstBitDepth != OFX::eBitDepthUByte && dstBitDepth != OFX::eBitDepthUShort && dstBitDepth != OFX::eBitDepthHalf && dstBitDepth != OFX::eBitDepthFloat) {
        OFX::throwSuiteStatusException(kOfxStatErrFormat);
    }
    if (dstBitDepth == OFX::eBitDepthUByte) {
        fillBlackForDepth<unsigned char>(instance, renderWindow,
                                         dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } else if (dstBitDepth == OFX::eBitDepthUShort || dstBitDepth == OFX::eBitDepthHalf) {
        fillBlackForDepth<unsigned short>(instance, renderWindow,
                                          dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } else if (dstBitDepth == OFX::eBitDepthFloat) {
        fillBlackForDepth<float>(instance, renderWindow,
                                 dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } // switch
}

inline void
fillBlack(OFX::ImageEffect &instance,
           const OfxRectI & renderWindow,
           OFX::Image* dstImg)
{
    void* dstPixelData;
    OfxRectI dstBounds;
    OFX::PixelComponentEnum dstPixelComponents;
    OFX::BitDepthEnum dstBitDepth;
    int dstRowBytes;
    getImageData(dstImg, &dstPixelData, &dstBounds, &dstPixelComponents, &dstBitDepth, &dstRowBytes);
    return fillBlack(instance, renderWindow, dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
}

// pixel copiers, non-threaded versions
template<class PIX,int nComponents>
void
copyPixelsNTForDepthAndComponents(OFX::ImageEffect &instance,
                                  const OfxRectI & renderWindow,
                                  const PIX *srcPixelData,
                                  const OfxRectI & srcBounds,
                                  OFX::PixelComponentEnum srcPixelComponents,
                                  OFX::BitDepthEnum srcBitDepth,
                                  int srcRowBytes,
                                  PIX *dstPixelData,
                                  const OfxRectI & dstBounds,
                                  OFX::PixelComponentEnum dstPixelComponents,
                                  OFX::BitDepthEnum dstBitDepth,
                                  int dstRowBytes)
{
    assert(srcPixelComponents == dstPixelComponents && srcBitDepth == dstBitDepth);
    (void)srcPixelComponents;
    (void)srcBitDepth;
    (void)dstPixelComponents;
    (void)dstBitDepth;
    (void)instance;

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

template<class PIX>
void
copyPixelsNTForDepth(OFX::ImageEffect &instance,
                     const OfxRectI & renderWindow,
                     const void *srcPixelData,
                     const OfxRectI & srcBounds,
                     OFX::PixelComponentEnum srcPixelComponents,
                     OFX::BitDepthEnum srcBitDepth,
                     int srcRowBytes,
                     void *dstPixelData,
                     const OfxRectI & dstBounds,
                     OFX::PixelComponentEnum dstPixelComponents,
                     OFX::BitDepthEnum dstBitDepth,
                     int dstRowBytes)
{
    assert(srcPixelData && dstPixelData);
    assert(srcPixelComponents == dstPixelComponents && srcBitDepth == dstBitDepth);
    // do the rendering
    if (dstPixelComponents != OFX::ePixelComponentRGBA && dstPixelComponents != OFX::ePixelComponentRGB && dstPixelComponents != OFX::ePixelComponentAlpha) {
        OFX::throwSuiteStatusException(kOfxStatErrFormat);
    }
    if (dstPixelComponents == OFX::ePixelComponentRGBA) {
        copyPixelsNTForDepthAndComponents<PIX,4>(instance, renderWindow,
                                                 (const PIX*)srcPixelData, srcBounds, srcPixelComponents, srcBitDepth, srcRowBytes,
                                                 (PIX *)dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } else if (dstPixelComponents == OFX::ePixelComponentRGB) {
        copyPixelsNTForDepthAndComponents<PIX,3>(instance, renderWindow,
                                                 (const PIX*)srcPixelData, srcBounds, srcPixelComponents, srcBitDepth, srcRowBytes,
                                                 (PIX *)dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    }  else if (dstPixelComponents == OFX::ePixelComponentAlpha) {
        copyPixelsNTForDepthAndComponents<PIX,1>(instance, renderWindow,
                                                 (const PIX*)srcPixelData, srcBounds, srcPixelComponents, srcBitDepth, srcRowBytes,
                                                 (PIX *)dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } // switch
}

inline void
copyPixelsNT(OFX::ImageEffect &instance,
             const OfxRectI & renderWindow,
             const void *srcPixelData,
             const OfxRectI & srcBounds,
             OFX::PixelComponentEnum srcPixelComponents,
             OFX::BitDepthEnum srcBitDepth,
             int srcRowBytes,
             void *dstPixelData,
             const OfxRectI & dstBounds,
             OFX::PixelComponentEnum dstPixelComponents,
             OFX::BitDepthEnum dstBitDepth,
             int dstRowBytes)
{
    assert(srcPixelData && dstPixelData);
    assert(srcPixelComponents == dstPixelComponents && srcBitDepth == dstBitDepth);

    // do the rendering
    if (dstBitDepth != OFX::eBitDepthUByte && dstBitDepth != OFX::eBitDepthUShort && dstBitDepth != OFX::eBitDepthHalf && dstBitDepth != OFX::eBitDepthFloat) {
        OFX::throwSuiteStatusException(kOfxStatErrFormat);
    }
    if (dstBitDepth == OFX::eBitDepthUByte) {
        copyPixelsNTForDepth<unsigned char>(instance, renderWindow,
                                            srcPixelData, srcBounds, srcPixelComponents, srcBitDepth, srcRowBytes,
                                            dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } else if (dstBitDepth == OFX::eBitDepthUShort || dstBitDepth == OFX::eBitDepthHalf) {
        copyPixelsNTForDepth<unsigned short>(instance, renderWindow,
                                             srcPixelData, srcBounds, srcPixelComponents, srcBitDepth, srcRowBytes,
                                             dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } else if (dstBitDepth == OFX::eBitDepthFloat) {
        copyPixelsNTForDepth<float>(instance, renderWindow,
                                    srcPixelData, srcBounds, srcPixelComponents, srcBitDepth, srcRowBytes,
                                    dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } // switch
}

// pixel copiers, threaded versions
template<class PIX,int nComponents>
void
copyPixelsForDepthAndComponents(OFX::ImageEffect &instance,
                                const OfxRectI & renderWindow,
                                const PIX *srcPixelData,
                                const OfxRectI & srcBounds,
                                OFX::PixelComponentEnum srcPixelComponents,
                                OFX::BitDepthEnum srcBitDepth,
                                int srcRowBytes,
                                PIX *dstPixelData,
                                const OfxRectI & dstBounds,
                                OFX::PixelComponentEnum dstPixelComponents,
                                OFX::BitDepthEnum dstBitDepth,
                                int dstRowBytes)
{
    assert(srcPixelData && dstPixelData);
    //assert(srcBounds.y1 <= renderWindow.y1 && renderWindow.y1 <= renderWindow.y2 && renderWindow.y2 <= srcBounds.y2); // not necessary, PixelCopier should handle this
    //assert(srcBounds.x1 <= renderWindow.x1 && renderWindow.x1 <= renderWindow.x2 && renderWindow.x2 <= srcBounds.x2); // not necessary, PixelCopier should handle this
    assert(srcPixelComponents == dstPixelComponents && srcBitDepth == dstBitDepth);
    (void)srcPixelComponents;
    (void)srcBitDepth;
    (void)dstPixelComponents;
    (void)dstBitDepth;

    OFX::PixelCopier<PIX, nComponents> processor(instance);
    // set the images
    processor.setDstImg(dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    processor.setSrcImg(srcPixelData, srcBounds, srcPixelComponents, srcBitDepth, srcRowBytes, 0);

    // set the render window
    processor.setRenderWindow(renderWindow);

    // Call the base class process member, this will call the derived templated process code
    processor.process();
}

template<class PIX>
void
copyPixelsForDepth(OFX::ImageEffect &instance,
                   const OfxRectI & renderWindow,
                   const void *srcPixelData,
                   const OfxRectI & srcBounds,
                   OFX::PixelComponentEnum srcPixelComponents,
                   OFX::BitDepthEnum srcBitDepth,
                   int srcRowBytes,
                   void *dstPixelData,
                   const OfxRectI & dstBounds,
                   OFX::PixelComponentEnum dstPixelComponents,
                   OFX::BitDepthEnum dstBitDepth,
                   int dstRowBytes)
{
    assert(srcPixelData && dstPixelData);
    assert(srcPixelComponents == dstPixelComponents && srcBitDepth == dstBitDepth);
    // do the rendering
    if (dstPixelComponents != OFX::ePixelComponentRGBA && dstPixelComponents != OFX::ePixelComponentRGB && dstPixelComponents != OFX::ePixelComponentAlpha) {
        OFX::throwSuiteStatusException(kOfxStatErrFormat);
    }
    if (dstPixelComponents == OFX::ePixelComponentRGBA) {
        copyPixelsForDepthAndComponents<PIX,4>(instance, renderWindow,
                                               (const PIX*)srcPixelData, srcBounds, srcPixelComponents, srcBitDepth, srcRowBytes,
                                               (PIX *)dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } else if (dstPixelComponents == OFX::ePixelComponentRGB) {
        copyPixelsForDepthAndComponents<PIX,3>(instance, renderWindow,
                                               (const PIX*)srcPixelData, srcBounds, srcPixelComponents, srcBitDepth, srcRowBytes,
                                               (PIX *)dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    }  else if (dstPixelComponents == OFX::ePixelComponentAlpha) {
        copyPixelsForDepthAndComponents<PIX,1>(instance, renderWindow,
                                               (const PIX*)srcPixelData, srcBounds, srcPixelComponents, srcBitDepth, srcRowBytes,
                                               (PIX *)dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } // switch
}

inline void
copyPixels(OFX::ImageEffect &instance,
           const OfxRectI & renderWindow,
           const void *srcPixelData,
           const OfxRectI & srcBounds,
           OFX::PixelComponentEnum srcPixelComponents,
           OFX::BitDepthEnum srcBitDepth,
           int srcRowBytes,
           void *dstPixelData,
           const OfxRectI & dstBounds,
           OFX::PixelComponentEnum dstPixelComponents,
           OFX::BitDepthEnum dstBitDepth,
           int dstRowBytes)
{
    assert(dstPixelData);
    if (!srcPixelData) {
        // no input, be black and transparent
        return fillBlack(instance, renderWindow,
                         dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    }
    assert(srcPixelComponents == dstPixelComponents && srcBitDepth == dstBitDepth);
    // do the rendering
    if (dstBitDepth != OFX::eBitDepthUByte && dstBitDepth != OFX::eBitDepthUShort && dstBitDepth != OFX::eBitDepthHalf && dstBitDepth != OFX::eBitDepthFloat) {
        OFX::throwSuiteStatusException(kOfxStatErrFormat);
    }
    if (dstBitDepth == OFX::eBitDepthUByte) {
        copyPixelsForDepth<unsigned char>(instance, renderWindow,
                                          srcPixelData, srcBounds, srcPixelComponents, srcBitDepth, srcRowBytes,
                                          dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } else if (dstBitDepth == OFX::eBitDepthUShort || dstBitDepth == OFX::eBitDepthHalf) {
        copyPixelsForDepth<unsigned short>(instance, renderWindow,
                                           srcPixelData, srcBounds, srcPixelComponents, srcBitDepth, srcRowBytes,
                                           dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } else if (dstBitDepth == OFX::eBitDepthFloat) {
        copyPixelsForDepth<float>(instance, renderWindow,
                                  srcPixelData, srcBounds, srcPixelComponents, srcBitDepth, srcRowBytes,
                                  dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
    } // switch
}

inline void
copyPixels(OFX::ImageEffect &instance,
           const OfxRectI & renderWindow,
           const OFX::Image* srcImg,
           void *dstPixelData,
           const OfxRectI & dstBounds,
           OFX::PixelComponentEnum dstPixelComponents,
           OFX::BitDepthEnum dstBitDepth,
           int dstRowBytes)
{
    const void* srcPixelData;
    OfxRectI srcBounds;
    OFX::PixelComponentEnum srcPixelComponents;
    OFX::BitDepthEnum srcBitDepth;
    int srcRowBytes;
    getImageData(srcImg, &srcPixelData, &srcBounds, &srcPixelComponents, &srcBitDepth, &srcRowBytes);
    return copyPixels(instance, renderWindow, srcPixelData, srcBounds, srcPixelComponents, srcBitDepth, srcRowBytes, dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
}

inline void
copyPixels(OFX::ImageEffect &instance,
           const OfxRectI & renderWindow,
           const OFX::Image* srcImg,
           OFX::Image* dstImg)
{
    void* dstPixelData;
    OfxRectI dstBounds;
    OFX::PixelComponentEnum dstPixelComponents;
    OFX::BitDepthEnum dstBitDepth;
    int dstRowBytes;
    getImageData(dstImg, &dstPixelData, &dstBounds, &dstPixelComponents, &dstBitDepth, &dstRowBytes);
    return copyPixels(instance, renderWindow, srcImg, dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
}

inline void
copyPixels(OFX::ImageEffect &instance,
           const OfxRectI & renderWindow,
           const void *srcPixelData,
           const OfxRectI & srcBounds,
           OFX::PixelComponentEnum srcPixelComponents,
           OFX::BitDepthEnum srcBitDepth,
           int srcRowBytes,
           OFX::Image* dstImg)
{
    void* dstPixelData;
    OfxRectI dstBounds;
    OFX::PixelComponentEnum dstPixelComponents;
    OFX::BitDepthEnum dstBitDepth;
    int dstRowBytes;
    getImageData(dstImg, &dstPixelData, &dstBounds, &dstPixelComponents, &dstBitDepth, &dstRowBytes);
    return copyPixels(instance, renderWindow, srcPixelData, srcBounds, srcPixelComponents, srcBitDepth, srcRowBytes, dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes);
}

} // OFX

#endif // ifndef IO_ofxsCopier_h
