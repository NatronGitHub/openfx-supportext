#ifndef _ofxsPixelProcessor_h_
#define _ofxsPixelProcessor_h_

/*
   ofxsPixelProcessor: generic multithreaded OFX pixel processor

   Copyright (C) 2014 INRIA
   Author: Frederic Devernay <frederic.devernay@inria.fr>

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

#include <cassert>

#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"

/** @file This file contains a useful base class that can be used to process images

   The code below is not so much a skin on the base OFX classes, but code used in implementing
   specific image processing algorithms. As such it does not sit in the support include lib, but in
   its own include directory.
 */

namespace OFX {
inline void
getImageData(OFX::Image* img,
             void** pixelData,
             OfxRectI* bounds,
             OFX::PixelComponentEnum* pixelComponents,
             OFX::BitDepthEnum* bitDepth,
             int* rowBytes)
{
    *pixelData = img->getPixelData();
    *bounds = img->getBounds();
    *pixelComponents = img->getPixelComponents();
    *bitDepth = img->getPixelDepth();
    *rowBytes = img->getRowBytes();
}

inline void
getImageData(const OFX::Image* img,
             const void** pixelData,
             OfxRectI* bounds,
             OFX::PixelComponentEnum* pixelComponents,
             OFX::BitDepthEnum* bitDepth,
             int* rowBytes)
{
    if (img) {
        *pixelData = img->getPixelData();
        *bounds = img->getBounds();
        *pixelComponents = img->getPixelComponents();
        *bitDepth = img->getPixelDepth();
        *rowBytes = img->getRowBytes();
    } else {
        *pixelData = 0;
        bounds->x1 = bounds->x2 = bounds->y1 = bounds->y2 = 0.f;
        *pixelComponents = ePixelComponentNone;
        *bitDepth = eBitDepthNone;
        *rowBytes = 0;
    }
}

inline
int
getComponentBytes(OFX::BitDepthEnum bitDepth)
{
    // compute bytes per component
    switch (bitDepth) {
    case OFX::eBitDepthNone:       return 0; break;
    case OFX::eBitDepthUByte:      return 1; break;
    case OFX::eBitDepthUShort:     return 2; break;
    case OFX::eBitDepthHalf:       return 2; break;
    case OFX::eBitDepthFloat:      return 4; break;
#ifdef OFX_EXTENSIONS_VEGAS
    case OFX::eBitDepthUByteBGRA:  return 1; break;
    case OFX::eBitDepthUShortBGRA: return 2; break;
    case OFX::eBitDepthFloatBGRA:  return 4; break;
#endif
    case OFX::eBitDepthCustom:     return 0; break;
    }
    return 0;
}

inline
void*
getPixelAddress(void* pixelData,
                const OfxRectI & bounds,
                int pixelComponentCount,
                OFX::BitDepthEnum bitDepth,
                int rowBytes,
                int x,
                int y)
{
    int pixelBytes = pixelComponentCount * getComponentBytes(bitDepth);

    // are we in the image bounds
    if ( ( x < bounds.x1) || ( x >= bounds.x2) || ( y < bounds.y1) || ( y >= bounds.y2) || ( pixelBytes == 0) ) {
        return 0;
    }
    char *pix = (char *) ( ( (char *) pixelData ) + (size_t)(y - bounds.y1) * rowBytes );
    pix += (x - bounds.x1) * pixelBytes;

    return (void *) pix;
}

inline
const void*
getPixelAddress(const void* pixelData,
                const OfxRectI & bounds,
                int pixelComponentCount,
                OFX::BitDepthEnum bitDepth,
                int rowBytes,
                int x,
                int y,
                bool withinBoundsCheck = true)
{
    int pixelBytes = pixelComponentCount * getComponentBytes(bitDepth);

    // are we in the image bounds
    if ( withinBoundsCheck && (
        ( x < bounds.x1) || ( x >= bounds.x2) || ( y < bounds.y1) || ( y >= bounds.y2) || ( pixelBytes == 0)) ) {
        return 0;
    }
    char *pix = (char *) ( ( (char *) pixelData ) + (size_t)(y - bounds.y1) * rowBytes );
    pix += (x - bounds.x1) * pixelBytes;

    return (void *) pix;
}

////////////////////////////////////////////////////////////////////////////////
// base class to process images with
class PixelProcessor
    : public OFX::MultiThread::Processor
{
protected:
    OFX::ImageEffect &_effect;          /**< @brief effect to render with */
    //OFX::Image       *_dstImg;        /**< @brief image to process into */
    void* _dstPixelData;
    OfxRectI _dstBounds;
    OFX::PixelComponentEnum _dstPixelComponents;
    int _dstPixelComponentCount;
    OFX::BitDepthEnum _dstBitDepth;
    int _dstPixelBytes;
    int _dstRowBytes;
    OfxRectI _renderWindow;               /**< @brief render window to use */

public:
    /** @brief ctor */
    PixelProcessor(OFX::ImageEffect &effect)
        : _effect(effect)
          , _dstPixelData(0)
          , _dstBounds()
          , _dstPixelComponents(OFX::ePixelComponentNone)
          , _dstPixelComponentCount(0)
          , _dstBitDepth(OFX::eBitDepthNone)
          , _dstPixelBytes(0)
          , _dstRowBytes(0)
    {
        _renderWindow.x1 = _renderWindow.y1 = _renderWindow.x2 = _renderWindow.y2 = 0;
    }

    /** @brief set the destination image */
    void setDstImg(OFX::Image *v)
    {
        _dstPixelData = v->getPixelData();
        _dstBounds = v->getBounds();
        _dstPixelComponents = v->getPixelComponents();
        _dstPixelComponentCount = v->getPixelComponentCount();
        _dstBitDepth = v->getPixelDepth();
        _dstPixelBytes = _dstPixelComponentCount * getComponentBytes(_dstBitDepth);
        _dstRowBytes = v->getRowBytes();
    }

    /** @brief set the destination image */
    void setDstImg(void *dstPixelData,
                   const OfxRectI & dstBounds,
                   OFX::PixelComponentEnum dstPixelComponents,
                   int dstPixelComponentCount,
                   OFX::BitDepthEnum dstPixelDepth,
                   int dstRowBytes)
    {
        _dstPixelData = dstPixelData;
        _dstBounds = dstBounds;
        _dstPixelComponents = dstPixelComponents;
        _dstPixelComponentCount = dstPixelComponentCount;
        _dstBitDepth = dstPixelDepth;
        _dstPixelBytes = _dstPixelComponentCount * getComponentBytes(_dstBitDepth);
        _dstRowBytes = dstRowBytes;
    }

    /** @brief reset the render window */
    void setRenderWindow(OfxRectI rect)
    {
        _renderWindow = rect;
    }

    /** @brief overridden from OFX::MultiThread::Processor. This function is called once on each SMP thread by the base class */
    void multiThreadFunction(unsigned int threadId,
                             unsigned int nThreads)
    {
        // slice the y range into the number of threads it has
        unsigned int dy = _renderWindow.y2 - _renderWindow.y1;
        // the following is equivalent to std::ceil(dy/(double)nThreads);
        unsigned int h = (dy + nThreads - 1) / nThreads;

        if (h == 0) {
            // there are more threads than lines to process
            h = 1;
        }
        if (threadId * h >= dy) {
            // empty render subwindow
            return;
        }
        unsigned int y1 = _renderWindow.y1 + threadId * h;
        unsigned int step = (threadId + 1) * h;
        unsigned int y2 = _renderWindow.y1 + (step < dy ? step : dy);
        OfxRectI win = _renderWindow;
        win.y1 = y1; win.y2 = y2;

        // and render that thread on each
        multiThreadProcessImages(win);
    }

    /** @brief called before any MP is done */
    virtual void preProcess(void)
    {
    }

    /** @brief this is called by multiThreadFunction to actually process images, override in derived classes */
    virtual void multiThreadProcessImages(OfxRectI window) = 0;

    /** @brief called before any MP is done */
    virtual void postProcess(void)
    {
    }

    /** @brief called to process everything */
    virtual void process(void)
    {
        assert(_dstPixelData &&
               _dstBounds.x1 <= _renderWindow.x1 && _renderWindow.x2 <= _dstBounds.x2 &&
               _dstBounds.y1 <= _renderWindow.y1 && _renderWindow.y2 <= _dstBounds.y2);
        // is it OK ?
        if ( !( _dstPixelData &&
                ( _dstBounds.x1 <= _renderWindow.x1) && ( _renderWindow.x2 <= _dstBounds.x2) &&
                ( _dstBounds.y1 <= _renderWindow.y1) && ( _renderWindow.y2 <= _dstBounds.y2) ) ||
             (_renderWindow.x1 >= _renderWindow.x2) ||
             (_renderWindow.y1 >= _renderWindow.y2) ) {
            return;
        }

        // call the pre MP pass
        preProcess();

        // make sure there are at least 4096 pixels per CPU and at least 1 line par CPU
        unsigned int nCPUs = (std::min(_renderWindow.x2 - _renderWindow.x1, 4096) *
                              (_renderWindow.y2 - _renderWindow.y1)) / 4096;
        // make sure the number of CPUs is valid (and use at least 1 CPU)
        nCPUs = std::max(1u, std::min(nCPUs, OFX::MultiThread::getNumCPUs()));

        // call the base multi threading code, should put a pre & post thread calls in too
        multiThread(nCPUs);

        // call the post MP pass
        postProcess();
    }

protected:
    void* getDstPixelAddress(int x,
                             int y) const
    {
        // are we in the image bounds
        if ( !_dstPixelData || ( x < _dstBounds.x1) || ( x >= _dstBounds.x2) || ( y < _dstBounds.y1) || ( y >= _dstBounds.y2) || ( _dstPixelBytes == 0) ) {
            return 0;
        }

        char *pix = (char *) ( ( (char *) _dstPixelData ) + (size_t)(y - _dstBounds.y1) * _dstRowBytes );
        pix += (x - _dstBounds.x1) * _dstPixelBytes;

        return (void *) pix;
    }
};


// base class for a processor with a single source image
class PixelProcessorFilterBase
    : public OFX::PixelProcessor
{
protected:
    const void *_srcPixelData;
    OfxRectI _srcBounds;
    OFX::PixelComponentEnum _srcPixelComponents;
    int _srcPixelComponentCount;
    OFX::BitDepthEnum _srcBitDepth;
    int _srcPixelBytes;
    int _srcRowBytes;
    int _srcBoundary; // Boundary conditions 0: Black/Dirichlet 1:Nearest/Neymann 2:Repeat/Periodic
    const OFX::Image *_origImg;
    const OFX::Image *_maskImg;
    bool _premult;
    int _premultChannel;
    bool _doMasking;
    double _mix;
    bool _maskInvert;

public:
    /** @brief no arg ctor */
    PixelProcessorFilterBase(OFX::ImageEffect &instance)
        : OFX::PixelProcessor(instance)
          , _srcPixelData(0)
          , _srcBounds()
          , _srcPixelComponents(OFX::ePixelComponentNone)
          , _srcPixelComponentCount(0)
          , _srcBitDepth(OFX::eBitDepthNone)
          , _srcPixelBytes(0)
          , _srcRowBytes(0)
          , _srcBoundary(0)
          , _origImg(0)
          , _maskImg(0)
          , _premult(false)
          , _premultChannel(3)
          , _doMasking(false)
          , _mix(1.)
          , _maskInvert(false)
    {
    }

    /** @brief set the src image */
    void setSrcImg(const OFX::Image *v)
    {
        _srcPixelData = v->getPixelData();
        _srcBounds = v->getBounds();
        _srcPixelComponents = v->getPixelComponents();
        _srcPixelComponentCount = v->getPixelComponentCount();
        _srcBitDepth = v->getPixelDepth();
        _srcPixelBytes = _srcPixelComponentCount * getComponentBytes(_srcBitDepth);
        _srcRowBytes = v->getRowBytes();
    }

    /** @brief set the src image */
    void setSrcImg(const void *srcPixelData,
                   const OfxRectI & srcBounds,
                   OFX::PixelComponentEnum srcPixelComponents,
                   int srcPixelComponentCount,
                   OFX::BitDepthEnum srcPixelDepth,
                   int srcRowBytes,
                   int srcBoundary)
    {
        _srcPixelData = srcPixelData;
        _srcBounds = srcBounds;
        _srcPixelComponents = srcPixelComponents;
        _srcPixelComponentCount = srcPixelComponentCount;
        _srcBitDepth = srcPixelDepth;
        _srcPixelBytes = _srcPixelComponentCount * getComponentBytes(_srcBitDepth);
        _srcRowBytes = srcRowBytes;
        _srcBoundary = srcBoundary;
    }

    void setOrigImg(const OFX::Image *v)
    {
        _origImg = v;
    }

    void setMaskImg(const OFX::Image *v,
                    bool maskInvert)
    {
        _maskImg = v;
        _maskInvert = maskInvert;
    }

    void doMasking(bool v)
    {
        _doMasking = v;
    }

    void setPremultMaskMix(bool premult,
                           int premultChannel,
                           double mix)
    {
        _premult = premult;
        _premultChannel = premultChannel;
        _mix = mix;
    }

protected:
    const void* getSrcPixelAddress(int x,
                                   int y) const
    {
        if (!_srcPixelData  || _srcPixelBytes == 0 || _srcBounds.x2 <= _srcBounds.x1 || _srcBounds.y2 <= _srcBounds.y1) {
            return 0;
        }
        // are we in the image bounds
        bool outside = (x < _srcBounds.x1) || ( x >= _srcBounds.x2) || ( y < _srcBounds.y1) || ( y >= _srcBounds.y2);
        if (outside) {
            if (_srcBoundary == 1) {
                // Nearest/Neumann
                if (x < _srcBounds.x1) {
                    x = _srcBounds.x1;
                } else if (x >= _srcBounds.x2) {
                    x = _srcBounds.x2 - 1;
                }
                if (y < _srcBounds.y1) {
                    y = _srcBounds.y1;
                } else if (y >= _srcBounds.y2) {
                    y = _srcBounds.y2 - 1;
                }
            } else if (_srcBoundary == 2) {
                // Repeat/Periodic
                if (x < _srcBounds.x1 || x >= _srcBounds.x2) {
                    x = _srcBounds.x1 + positive_modulo(x - _srcBounds.x1, _srcBounds.x2 - _srcBounds.x1);
                }
                if (y < _srcBounds.y1 || y >= _srcBounds.y2) {
                    y = _srcBounds.y1 + positive_modulo(y - _srcBounds.y1, _srcBounds.y2 - _srcBounds.y1);
                }
            } else {
                // Black/Dirichlet
                return 0;
            }
        }

        char *pix = (char *) ( ( (char *) _srcPixelData ) + (size_t)(y - _srcBounds.y1) * _srcRowBytes );
        pix += (x - _srcBounds.x1) * _srcPixelBytes;

        return (void *) pix;
    }

    static int positive_modulo(int i, int n) {
        return (i % n + n) % n;
    }
};
};
#endif // ifndef _ofxsPixelProcessor_h_
