/*
   OFX color-spaces transformations support as-well as bit-depth conversions.

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

#include "ofxsLut.h"
#ifdef _WIN32
typedef unsigned __int32 uint32_t;
typedef unsigned char uint8_t;
#else
#include <stdint.h>
#endif
#include <limits>

namespace OFX {
namespace Color {
// compile-time endianness checking found on:
// http://stackoverflow.com/questions/2100331/c-macro-definition-to-determine-big-endian-or-little-endian-machine
// if(O32_HOST_ORDER == O32_BIG_ENDIAN) will always be optimized by gcc -O2
enum
{
    O32_LITTLE_ENDIAN = 0x03020100ul,
    O32_BIG_ENDIAN = 0x00010203ul,
    O32_PDP_ENDIAN = 0x01000302ul
};

union o32_host_order_t
{
    uint8_t bytes[4];
    uint32_t value;
};

static const o32_host_order_t o32_host_order = {
    { 0, 1, 2, 3 }
};
#define O32_HOST_ORDER (o32_host_order.value)
unsigned short
LutBase::hipart(const float f)
{
    union
    {
        float f;
        unsigned short us[2];
    }

    tmp;

    tmp.us[0] = tmp.us[1] = 0;
    tmp.f = f;

    if (O32_HOST_ORDER == O32_BIG_ENDIAN) {
        return tmp.us[0];
    } else if (O32_HOST_ORDER == O32_LITTLE_ENDIAN) {
        return tmp.us[1];
    } else {
        assert( (O32_HOST_ORDER == O32_LITTLE_ENDIAN) || (O32_HOST_ORDER == O32_BIG_ENDIAN) );

        return 0;
    }
}

float
LutBase::index_to_float(const unsigned short i)
{
    union
    {
        float f;
        unsigned short us[2];
    }

    tmp;

    /* positive and negative zeros, and all gradual underflow, turn into zero: */
    if ( ( i < 0x80) || ( ( i >= 0x8000) && ( i < 0x8080) ) ) {
        return 0;
    }
    /* All NaN's and infinity turn into the largest possible legal float: */
    if ( ( i >= 0x7f80) && ( i < 0x8000) ) {
        return std::numeric_limits<float>::max();
    }
    if (i >= 0xff80) {
        return -std::numeric_limits<float>::max();
    }
    if (O32_HOST_ORDER == O32_BIG_ENDIAN) {
        tmp.us[0] = i;
        tmp.us[1] = 0x8000;
    } else if (O32_HOST_ORDER == O32_LITTLE_ENDIAN) {
        tmp.us[0] = 0x8000;
        tmp.us[1] = i;
    } else {
        assert( (O32_HOST_ORDER == O32_LITTLE_ENDIAN) || (O32_HOST_ORDER == O32_BIG_ENDIAN) );
    }

    return tmp.f;
}

///initialize the singleton
LutManager LutManager::m_instance = LutManager();
LutManager::LutManager()
    : luts()
{
}

LutManager::~LutManager()
{
    ////the luts must all have been released before!
    ////This is because the Lut holds a OFX::MultiThread::Mutex and it can't be deleted
    //// by this singleton because it makes their destruction time uncertain regarding to
    ///the host multi-thread suite.
    for (LutsMap::iterator it = luts.begin(); it != luts.end(); ++it) {
        delete it->second;
    }
}

///////////////////////
/////////////////////////////////////////// LINEAR //////////////////////////////////////////////
///////////////////////

namespace Linear {
void
to_byte_packed(const void* pixelData,
               const OfxRectI & bounds,
               OFX::PixelComponentEnum pixelComponents,
               OFX::BitDepthEnum bitDepth,
               int rowBytes,
               const OfxRectI & renderWindow,
               void* dstPixelData,
               const OfxRectI & dstBounds,
               OFX::PixelComponentEnum dstPixelComponents,
               OFX::BitDepthEnum dstBitDepth,
               int dstRowBytes)
{
    assert(bitDepth == eBitDepthFloat && dstBitDepth == eBitDepthUByte && pixelComponents == dstPixelComponents);
    assert(bounds.x1 <= renderWindow.x1 && renderWindow.x2 <= bounds.x2 &&
           bounds.y1 <= renderWindow.y1 && renderWindow.y2 <= bounds.y2 &&
           dstBounds.x1 <= renderWindow.x1 && renderWindow.x2 <= dstBounds.x2 &&
           dstBounds.y1 <= renderWindow.y1 && renderWindow.y2 <= dstBounds.y2);

    int nComponents = getNComponents(pixelComponents);

    for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
        const float *src_pixels = (const float*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, 0, y);
        unsigned char *dst_pixels = (unsigned char*)OFX::getPixelAddress(dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes, 0, y);
        const float *src_end = (const float*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, renderWindow.x2, y, false);

        while (src_pixels != src_end) {
            for (int k = 0; k < nComponents; ++k) {
                dst_pixels[k] = floatToInt<256>(src_pixels[k]);
            }
            dst_pixels += nComponents;
            src_pixels += nComponents;
        }
    }
}

void
to_short_packed(const void* pixelData,
                const OfxRectI & bounds,
                OFX::PixelComponentEnum pixelComponents,
                OFX::BitDepthEnum bitDepth,
                int rowBytes,
                const OfxRectI & renderWindow,
                void* dstPixelData,
                const OfxRectI & dstBounds,
                OFX::PixelComponentEnum dstPixelComponents,
                OFX::BitDepthEnum dstBitDepth,
                int dstRowBytes)
{
    assert(bitDepth == eBitDepthFloat && dstBitDepth == eBitDepthUShort && pixelComponents == dstPixelComponents);
    assert(bounds.x1 <= renderWindow.x1 && renderWindow.x2 <= bounds.x2 &&
           bounds.y1 <= renderWindow.y1 && renderWindow.y2 <= bounds.y2 &&
           dstBounds.x1 <= renderWindow.x1 && renderWindow.x2 <= dstBounds.x2 &&
           dstBounds.y1 <= renderWindow.y1 && renderWindow.y2 <= dstBounds.y2);

    int nComponents = getNComponents(pixelComponents);

    for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
        const float *src_pixels = (const float*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, 0, y);
        unsigned short *dst_pixels = (unsigned short*)OFX::getPixelAddress(dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes, 0, y);
        const float *src_end = (const float*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, renderWindow.x2, y, false);

        while (src_pixels != src_end) {
            for (int k = 0; k < nComponents; ++k) {
                dst_pixels[k] = floatToInt<65536>(src_pixels[k]);
            }
            dst_pixels += nComponents;
            src_pixels += nComponents;
        }
    }
}

void
from_byte_packed(const void* pixelData,
                 const OfxRectI & bounds,
                 OFX::PixelComponentEnum pixelComponents,
                 OFX::BitDepthEnum bitDepth,
                 int rowBytes,
                 const OfxRectI & renderWindow,
                 void* dstPixelData,
                 const OfxRectI & dstBounds,
                 OFX::PixelComponentEnum dstPixelComponents,
                 OFX::BitDepthEnum dstBitDepth,
                 int dstRowBytes)
{
    assert(bitDepth == eBitDepthUByte && dstBitDepth == eBitDepthFloat && pixelComponents == dstPixelComponents);
    assert(bounds.x1 <= renderWindow.x1 && renderWindow.x2 <= bounds.x2 &&
           bounds.y1 <= renderWindow.y1 && renderWindow.y2 <= bounds.y2 &&
           dstBounds.x1 <= renderWindow.x1 && renderWindow.x2 <= dstBounds.x2 &&
           dstBounds.y1 <= renderWindow.y1 && renderWindow.y2 <= dstBounds.y2);

    int nComponents = getNComponents(pixelComponents);

    for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
        const unsigned char *src_pixels = (const unsigned char*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, 0, y);
        float *dst_pixels = (float*)OFX::getPixelAddress(dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes, 0, y);
        const unsigned char *src_end = (const unsigned char*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, renderWindow.x2, y, false);


        while (src_pixels != src_end) {
            for (int k = 0; k < nComponents; ++k) {
                dst_pixels[k] = intToFloat<256>(src_pixels[k]);
            }
            dst_pixels += nComponents;
            src_pixels += nComponents;
        }
    }
}

void
from_short_packed(const void* pixelData,
                  const OfxRectI & bounds,
                  OFX::PixelComponentEnum pixelComponents,
                  OFX::BitDepthEnum bitDepth,
                  int rowBytes,
                  const OfxRectI & renderWindow,
                  void* dstPixelData,
                  const OfxRectI & dstBounds,
                  OFX::PixelComponentEnum dstPixelComponents,
                  OFX::BitDepthEnum dstBitDepth,
                  int dstRowBytes)
{
    assert(bitDepth == eBitDepthUByte && dstBitDepth == eBitDepthFloat && pixelComponents == dstPixelComponents);
    assert(bounds.x1 <= renderWindow.x1 && renderWindow.x2 <= bounds.x2 &&
           bounds.y1 <= renderWindow.y1 && renderWindow.y2 <= bounds.y2 &&
           dstBounds.x1 <= renderWindow.x1 && renderWindow.x2 <= dstBounds.x2 &&
           dstBounds.y1 <= renderWindow.y1 && renderWindow.y2 <= dstBounds.y2);

    int nComponents = getNComponents(pixelComponents);

    for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
        const unsigned short *src_pixels = (const unsigned short*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, 0, y);
        float *dst_pixels = (float*)OFX::getPixelAddress(dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes, 0, y);
        const unsigned short *src_end = (const unsigned short*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, renderWindow.x2, y, false);


        while (src_pixels != src_end) {
            for (int k = 0; k < nComponents; ++k) {
                dst_pixels[k] = intToFloat<65536>(src_pixels[k]);
            }
            dst_pixels += nComponents;
            src_pixels += nComponents;
        }
    }
}
} // namespace Linear


// r,g,b values are from 0 to 1
// h = [0,360], s = [0,1], v = [0,1]
//		if s == 0, then h = 0 (undefined)
void
rgb_to_hsv( float r,
            float g,
            float b,
            float *h,
            float *s,
            float *v )
{
    float min, max, delta;

    min = std::min(std::min(r, g), b);
    max = std::max(std::max(r, g), b);
    *v = max;                               // v

    delta = max - min;

    if (max != 0.) {
        *s = delta / max;                       // s
    } else {
        // r = g = b = 0		// s = 0, v is undefined
        *s = 0.;
        *h = 0.;

        return;
    }

    if (delta == 0.) {
        *h = 0.;                 // gray
    } else if (r == max) {
        *h = (g - b) / delta;                       // between yellow & magenta
    } else if (g == max) {
        *h = 2 + (b - r) / delta;                   // between cyan & yellow
    } else {
        *h = 4 + (r - g) / delta;                   // between magenta & cyan
    }
    *h *= 60;                               // degrees
    if (*h < 0) {
        *h += 360;
    }
}

void
hsv_to_rgb(float h,
           float s,
           float v,
           float *r,
           float *g,
           float *b)
{
    int i;
    float f, p, q, t;

    if (s == 0) {
        // achromatic (grey)
        *r = *g = *b = v;

        return;
    }

    h /= 60;            // sector 0 to 5
    i = std::floor(h);
    f = h - i;          // factorial part of h
    i = (i >= 0) ? (i % 6) : (i % 6) + 6; // take h modulo 360
    p = v * ( 1 - s );
    q = v * ( 1 - s * f );
    t = v * ( 1 - s * ( 1 - f ) );

    switch (i) {
    case 0:
        *r = v;
        *g = t;
        *b = p;
        break;
    case 1:
        *r = q;
        *g = v;
        *b = p;
        break;
    case 2:
        *r = p;
        *g = v;
        *b = t;
        break;
    case 3:
        *r = p;
        *g = q;
        *b = v;
        break;
    case 4:
        *r = t;
        *g = p;
        *b = v;
        break;
    default:                // case 5:
        *r = v;
        *g = p;
        *b = q;
        break;
    }
} // hsv_to_rgb
}         // namespace Color
} //namespace OFX

