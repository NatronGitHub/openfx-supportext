/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of openfx-supportext <https://github.com/devernay/openfx-supportext>,
 * Copyright (C) 2013-2016 INRIA
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
 * OFX color-spaces transformations support as-well as bit-depth conversions.
 */

#ifndef openfx_supportext_ofxsLut_h
#define openfx_supportext_ofxsLut_h

#include <string>
#include <map>
#include <cmath>
#include <cassert>
#include <cstring> // for memcpy
#include <cstdlib> // for rand
#include <memory> // for auto_ptr

#include "ofxCore.h"
#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include "ofxsPixelProcessor.h"
#include "ofxsMultiThread.h"

#define OFXS_HUE_CIRCLE 1. // if hue should be between 0 and 1
//#define OFXS_HUE_CIRCLE 360. // if hue should be in degrees

namespace OFX {
namespace Color {
/// numvals should be 256 for byte, 65536 for 16-bits, etc.

/// maps 0-(numvals-1) to 0.-1.
template<int numvals>
float
intToFloat(int value)
{
    return value / (float)(numvals - 1);
}

/// maps °.-1. to 0-(numvals-1)
template<int numvals>
int
floatToInt(float value)
{
    if (value <= 0) {
        return 0;
    } else if (value >= 1.) {
        return numvals - 1;
    }

    return value * (numvals - 1) + 0.5;
}

/// maps 0x0-0xffff to 0x0-0xff
inline unsigned char
uint16ToChar(unsigned short quantum)
{
    // see ScaleQuantumToChar() in ImageMagick's magick/quantum.h

    /* test:
       for(int i=0; i < 0x10000; ++i) {
       printf("%x -> %x,%x\n", i, uint16ToChar(i), floatToInt<256>(intToFloat<65536>(i)));
       assert(uint16ToChar(i) == floatToInt<256>(intToFloat<65536>(i)));
       }
     */
    return (unsigned char) ( ( (quantum + 128UL) - ( (quantum + 128UL) >> 8 ) ) >> 8 );
}

/// maps 0x0-0xff to 0x0-0xffff
inline unsigned short
charToUint16(unsigned char quantum)
{
    /* test:
       for(int i=0; i < 0x100; ++i) {
       printf("%x -> %x,%x\n", i, charToUint16(i), floatToInt<65536>(intToFloat<256>(i)));
       assert(charToUint16(i) == floatToInt<65536>(intToFloat<256>(i)));
       assert(i == uint16ToChar(charToUint16(i)));
       }
     */
    return (unsigned short) ( (quantum << 8) | quantum );
}

// maps 0x0-0xff00 to 0x0-0xff
inline unsigned char
uint8xxToChar(unsigned short quantum)
{
    /* test:
       for(int i=0; i < 0xff01; ++i) {
       printf("%x -> %x,%x, err=%d\n", i, uint8xxToChar(i), floatToInt<256>(intToFloat<0xff01>(i)),i - charToUint8xx(uint8xxToChar(i)));
       assert(uint8xxToChar(i) == floatToInt<256>(intToFloat<0xff01>(i)));
       }
     */
    return (unsigned char) ( (quantum + 0x80) >> 8 );
}

// maps 0x0-0xff to 0x0-0xff00
inline unsigned short
charToUint8xx(unsigned char quantum)
{
    /* test:
       for(int i=0; i < 0x100; ++i) {
       printf("%x -> %x,%x\n", i, charToUint8xx(i), floatToInt<0xff01>(intToFloat<256>(i)));
       assert(charToUint8xx(i) == floatToInt<0xff01>(intToFloat<256>(i)));
       assert(i == uint8xxToChar(charToUint8xx(i)));
       }
     */
    return (unsigned short) (quantum << 8);
}

/* @brief Converts a float ranging in [0 - 1.f] in the desired color-space to linear color-space also ranging in [0 - 1.f]*/
typedef float (*fromColorSpaceFunctionV1)(float v);

/* @brief Converts a float ranging in [0 - 1.f] in  linear color-space to the desired color-space to also ranging in [0 - 1.f]*/
typedef float (*toColorSpaceFunctionV1)(float v);


class LutBase
{
protected:
    std::string _name;                 ///< name of the lut
    fromColorSpaceFunctionV1 _fromFunc;
    toColorSpaceFunctionV1 _toFunc;

public:

    LutBase(const std::string & name,
            fromColorSpaceFunctionV1 fromFunc,
            toColorSpaceFunctionV1 toFunc)
        : _name(name)
        , _fromFunc(fromFunc)
        , _toFunc(toFunc)
    {
    }

    virtual ~LutBase()
    {
    }

    const std::string & getName() const
    {
        return _name;
    }

    /* @brief Converts a float ranging in [0 - 1.f] in the desired color-space to linear color-space also ranging in [0 - 1.f]
     * This function is not fast!
     * @see fromColorSpaceFloatToLinearFloatFast(float)
     */
    virtual float fromColorSpaceFloatToLinearFloat(float v) const = 0;

    /* @brief Converts a float ranging in [0 - 1.f] in  linear color-space to the desired color-space to also ranging in [0 - 1.f]
     * This function is not fast!
     */
    virtual float toColorSpaceFloatFromLinearFloat(float v) const = 0;

    /* @brief Converts a float ranging in [0 - 1.f] in linear color-space using the look-up tables.
     * @return A float in [0 - 1.f] in the destination color-space.
     */
    // It is not recommended to use this function, because the output is quantized
    // If one really needs float, one has to use the full function (or OpenColorIO)

    /* @brief Converts a float ranging in [0 - 1.f] in linear color-space using the look-up tables.
     * @return A byte in [0 - 255] in the destination color-space.
     */
    virtual unsigned char toColorSpaceUint8FromLinearFloatFast(float v) const = 0;

    /* @brief Converts a float ranging in [0 - 1.f] in linear color-space using the look-up tables.
     * @return An unsigned short in [0 - 0xff00] in the destination color-space.
     */
    virtual unsigned short toColorSpaceUint8xxFromLinearFloatFast(float v) const = 0;

    /* @brief Converts a float ranging in [0 - 1.f] in linear color-space using the look-up tables.
     * @return An unsigned short in [0 - 65535] in the destination color-space.
     * This function uses localluy linear approximations of the transfer function.
     */
    virtual unsigned short toColorSpaceUint16FromLinearFloatFast(float v) const = 0;

    /* @brief Converts a byte ranging in [0 - 255] in the destination color-space using the look-up tables.
     * @return A float in [0 - 1.f] in linear color-space.
     */
    virtual float fromColorSpaceUint8ToLinearFloatFast(unsigned char v) const = 0;

    /* @brief Converts a short ranging in [0 - 65535] in the destination color-space using the look-up tables.
     * @return A float in [0 - 1.f] in linear color-space.
     */
    virtual float fromColorSpaceUint16ToLinearFloatFast(unsigned short v) const = 0;

    /* @brief convert from float to byte with dithering (error diffusion).
       It uses random numbers for error diffusion, and thus the result is different at each function call. */
    virtual void to_byte_packed_dither(const void* pixelData,
                                       const OfxRectI & bounds,
                                       OFX::PixelComponentEnum pixelComponents,
                                       int pixelComponentCount,
                                       OFX::BitDepthEnum bitDepth,
                                       int rowBytes,
                                       const OfxRectI & renderWindow,
                                       void* dstPixelData,
                                       const OfxRectI & dstBounds,
                                       OFX::PixelComponentEnum dstPixelComponents,
                                       int dstPixelComponentCount,
                                       OFX::BitDepthEnum dstBitDepth,
                                       int dstRowBytes) const = 0;

    /* @brief convert from float to byte without dithering. */
    virtual void to_byte_packed_nodither(const void* pixelData,
                                         const OfxRectI & bounds,
                                         OFX::PixelComponentEnum pixelComponents,
                                         int pixelComponentCount,
                                         OFX::BitDepthEnum bitDepth,
                                         int rowBytes,
                                         const OfxRectI & renderWindow,
                                         void* dstPixelData,
                                         const OfxRectI & dstBounds,
                                         OFX::PixelComponentEnum dstPixelComponents,
                                         int dstPixelComponentCount,
                                         OFX::BitDepthEnum dstBitDepth,
                                         int dstRowBytes) const = 0;

    /* @brief uses Rec.709 to convert from color to grayscale. */
    virtual void to_byte_grayscale_nodither(const void* pixelData,
                                            const OfxRectI & bounds,
                                            OFX::PixelComponentEnum pixelComponents,
                                            int pixelComponentCount,
                                            OFX::BitDepthEnum bitDepth,
                                            int rowBytes,
                                            const OfxRectI & renderWindow,
                                            void* dstPixelData,
                                            const OfxRectI & dstBounds,
                                            OFX::PixelComponentEnum dstPixelComponents,
                                            int dstPixelComponentCount,
                                            OFX::BitDepthEnum dstBitDepth,
                                            int dstRowBytes) const = 0;

    /* @brief convert from float to short without dithering. */
    virtual void to_short_packed(const void* pixelData,
                                 const OfxRectI & bounds,
                                 OFX::PixelComponentEnum pixelComponents,
                                 int pixelComponentCount,
                                 OFX::BitDepthEnum bitDepth,
                                 int rowBytes,
                                 const OfxRectI & renderWindow,
                                 void* dstPixelData,
                                 const OfxRectI & dstBounds,
                                 OFX::PixelComponentEnum dstPixelComponents,
                                 int dstPixelComponentCount,
                                 OFX::BitDepthEnum dstBitDepth,
                                 int dstRowBytes) const = 0;
    virtual void from_byte_packed(const void* pixelData,
                                  const OfxRectI & bounds,
                                  OFX::PixelComponentEnum pixelComponents,
                                  int pixelComponentCount,
                                  OFX::BitDepthEnum bitDepth,
                                  int rowBytes,
                                  const OfxRectI & renderWindow,
                                  void* dstPixelData,
                                  const OfxRectI & dstBounds,
                                  OFX::PixelComponentEnum dstPixelComponents,
                                  int dstPixelComponentCount,
                                  OFX::BitDepthEnum dstBitDepth,
                                  int dstRowBytes) const = 0;
    virtual void from_short_packed(const void* pixelData,
                                   const OfxRectI & bounds,
                                   OFX::PixelComponentEnum pixelComponents,
                                   int pixelComponentCount,
                                   OFX::BitDepthEnum bitDepth,
                                   int rowBytes,
                                   const OfxRectI & renderWindow,
                                   void* dstPixelData,
                                   const OfxRectI & dstBounds,
                                   OFX::PixelComponentEnum dstPixelComponents,
                                   int dstPixelComponentCount,
                                   OFX::BitDepthEnum dstBitDepth,
                                   int dstRowBytes) const = 0;

protected:

    static float index_to_float(const unsigned short i);
    static unsigned short hipart(const float f);
};

/**
 * @brief A Lut (look-up table) used to speed-up color-spaces conversions.
 * If you plan on doing linear conversion, you should just use the Linear class instead.
 **/
template <class MUTEX>
class Lut
    : public LutBase
{

    typedef OFX::MultiThread::AutoMutexT<MUTEX> AutoMutex;

    /// the fast lookup tables are mutable, because they are automatically initialized post-construction,
    /// and never change afterwards
    mutable unsigned short toFunc_hipart_to_uint8xx[0x10000];                 /// contains  2^16 = 65536 values between 0-255
    mutable float fromFunc_uint8_to_float[256];                 /// values between 0-1.f
    mutable bool _init;                 ///< false if the tables are not yet initialized
    mutable std::auto_ptr<MUTEX> _lock;                 ///< protects _init

public:
    Lut(const std::string & name,
        fromColorSpaceFunctionV1 fromFunc,
        toColorSpaceFunctionV1 toFunc)
        : LutBase(name, fromFunc, toFunc)
        , _init(false)
        , _lock( new MUTEX() )
    {
    }

    virtual ~Lut()
    {
    }

private:
    

    ///init luts
    ///it uses fromColorSpaceFloatToLinearFloat(float) and toColorSpaceFloatFromLinearFloat(float)
    ///Called by validate()
    void fillTables() const
    {
        if (_init) {
            return;
        }
        // fill all
        for (int i = 0; i < 0x10000; ++i) {
            float inp = index_to_float( (unsigned short)i );
            float f = _toFunc(inp);
            toFunc_hipart_to_uint8xx[i] = Color::floatToInt<0xff01>(f);
        }
        // fill fromFunc_uint8_to_float, and make sure that
        // the entries of toFunc_hipart_to_uint8xx corresponding
        // to the transform of each byte value contain the same value,
        // so that toFunc(fromFunc(b)) is identity
        //
        for (int b = 0; b < 256; ++b) {
            float f = _fromFunc( Color::intToFloat<256>(b) );
            fromFunc_uint8_to_float[b] = f;
            int i = hipart(f);
            toFunc_hipart_to_uint8xx[i] = Color::charToUint8xx(b);
        }
    }

public:

    //Called by all public members
    void validate() const
    {
        AutoMutex locker(*_lock);

        if (_init) {
            return;
        }
        fillTables();
        _init = true;
    }

    virtual float fromColorSpaceFloatToLinearFloat(float v) const OVERRIDE FINAL WARN_UNUSED_RETURN
    {
        return _fromFunc(v);
    }

    virtual float toColorSpaceFloatFromLinearFloat(float v) const OVERRIDE FINAL WARN_UNUSED_RETURN
    {
        return _toFunc(v);
    }

    virtual unsigned char toColorSpaceUint8FromLinearFloatFast(float v) const OVERRIDE FINAL WARN_UNUSED_RETURN
    {
        assert(_init);

        return Color::uint8xxToChar(toFunc_hipart_to_uint8xx[hipart(v)]);
    }

    virtual unsigned short toColorSpaceUint8xxFromLinearFloatFast(float v) const OVERRIDE FINAL WARN_UNUSED_RETURN
    {
        assert(_init);

        return toFunc_hipart_to_uint8xx[hipart(v)];
    }

    // the following only works for increasing LUTs

    virtual unsigned short toColorSpaceUint16FromLinearFloatFast(float v) const OVERRIDE FINAL WARN_UNUSED_RETURN
    {
        assert(_init);
        // algorithm:
        // - convert to 8 bits -> val8u
        // - convert val8u-1, val8u and val8u+1 to float
        // - interpolate linearly in the right interval
        unsigned char v8u = toColorSpaceUint8FromLinearFloatFast(v);
        unsigned char v8u_next, v8u_prev;
        float v32f_next, v32f_prev;
        if (v8u == 0) {
            v8u_prev = 0;
            v8u_next = 1;
            v32f_prev = fromColorSpaceUint8ToLinearFloatFast(0);
            v32f_next = fromColorSpaceUint8ToLinearFloatFast(1);
        } else if (v8u == 255) {
            v8u_prev = 254;
            v8u_next = 255;
            v32f_prev = fromColorSpaceUint8ToLinearFloatFast(254);
            v32f_next = fromColorSpaceUint8ToLinearFloatFast(255);
        } else {
            float v32f = fromColorSpaceUint8ToLinearFloatFast(v8u);
            // we suppose the LUT is an increasing func
            if (v < v32f) {
                v8u_prev = v8u - 1;
                v32f_prev = fromColorSpaceUint8ToLinearFloatFast(v8u_prev);
                v8u_next = v8u;
                v32f_next = v32f;
            } else {
                v8u_prev = v8u;
                v32f_prev = v32f;
                v8u_next = v8u + 1;
                v32f_next = fromColorSpaceUint8ToLinearFloatFast(v8u_next);
            }
        }

        // interpolate linearly
        return (v8u_prev << 8) + v8u_prev + (v - v32f_prev) * ( ( (v8u_next - v8u_prev) << 8 ) + (v8u_next + v8u_prev) ) / (v32f_next - v32f_prev) + 0.5;
    }

    virtual float fromColorSpaceUint8ToLinearFloatFast(unsigned char v) const OVERRIDE FINAL WARN_UNUSED_RETURN
    {
        assert(_init);

        return fromFunc_uint8_to_float[v];
    }

    virtual float fromColorSpaceUint16ToLinearFloatFast(unsigned short v) const OVERRIDE FINAL WARN_UNUSED_RETURN
    {
        assert(_init);
        // the following is from ImageMagick's quantum.h
        unsigned char v8u_prev = ( v - (v >> 8) ) >> 8;
        unsigned char v8u_next = v8u_prev + 1;
        unsigned short v16u_prev = (v8u_prev << 8) + v8u_prev;
        unsigned short v16u_next = (v8u_next << 8) + v8u_next;
        float v32f_prev = fromColorSpaceUint8ToLinearFloatFast(v8u_prev);
        float v32f_next = fromColorSpaceUint8ToLinearFloatFast(v8u_next);

        // interpolate linearly
        return v32f_prev + (v - v16u_prev) * (v32f_next - v32f_prev) / (v16u_next - v16u_prev);
    }

    virtual void to_byte_packed_dither(const void* pixelData,
                                       const OfxRectI & bounds,
                                       OFX::PixelComponentEnum pixelComponents,
                                       int pixelComponentCount,
                                       OFX::BitDepthEnum bitDepth,
                                       int rowBytes,
                                       const OfxRectI & renderWindow,
                                       void* dstPixelData,
                                       const OfxRectI & dstBounds,
                                       OFX::PixelComponentEnum dstPixelComponents,
                                       int dstPixelComponentCount,
                                       OFX::BitDepthEnum dstBitDepth,
                                       int dstRowBytes) const OVERRIDE FINAL
    {
        assert(bitDepth == eBitDepthFloat && dstBitDepth == eBitDepthUByte && pixelComponents == dstPixelComponents);
        assert(bounds.x1 <= renderWindow.x1 && renderWindow.x2 <= bounds.x2 &&
               bounds.y1 <= renderWindow.y1 && renderWindow.y2 <= bounds.y2 &&
               dstBounds.x1 <= renderWindow.x1 && renderWindow.x2 <= dstBounds.x2 &&
               dstBounds.y1 <= renderWindow.y1 && renderWindow.y2 <= dstBounds.y2);
        if (pixelComponents == ePixelComponentAlpha) {
            // alpha: no dither
            return to_byte_packed_nodither(pixelData, bounds, pixelComponents, pixelComponentCount, bitDepth, rowBytes,
                                           renderWindow,
                                           dstPixelData, dstBounds, dstPixelComponents, dstPixelComponentCount, dstBitDepth, dstRowBytes);
        }
        validate();

        const int nComponents = dstPixelComponentCount;
        assert(dstPixelComponentCount == 3 || dstPixelComponentCount == 4);

        for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
            int xstart = renderWindow.x1 + std::rand() % (renderWindow.x2 - renderWindow.x1);
            unsigned error[3] = {
                0x80, 0x80, 0x80
            };
            const float *src_pixels = (const float*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, xstart, y);
            unsigned char *dst_pixels = (unsigned char*)OFX::getPixelAddress(dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes, xstart, y);

            /* go forward from starting point to end of line: */
            const float *src_end = (const float*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, renderWindow.x2, y, false);

            while (src_pixels < src_end) {
                for (int k = 0; k < 3; ++k) {
                    error[k] = (error[k] & 0xff) + toColorSpaceUint8xxFromLinearFloatFast(src_pixels[k]);
                    assert(error[k] < 0x10000);
                    dst_pixels[k] = (unsigned char)(error[k] >> 8);
                }
                if (nComponents == 4) {
                    // alpha channel: no dithering
                    dst_pixels[3] = floatToInt<256>(src_pixels[3]);
                }
                dst_pixels += nComponents;
                src_pixels += nComponents;
            }

            if (xstart > 0) {
                /* go backward from starting point to start of line: */
                src_pixels = (const float*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, xstart - 1, y);
                src_end = (const float*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, 0, y);
                dst_pixels = (unsigned char*)OFX::getPixelAddress(dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes, xstart - 1, y);

                for (int i = 0; i < 3; ++i) {
                    error[i] = 0x80;
                }

                while (src_pixels >= src_end) {
                    for (int k = 0; k < 3; ++k) {
                        error[k] = (error[k] & 0xff) + toColorSpaceUint8xxFromLinearFloatFast(src_pixels[k]);
                        assert(error[k] < 0x10000);
                        dst_pixels[k] = (unsigned char)(error[k] >> 8);
                    }
                    if (nComponents == 4) {
                        // alpha channel: no colorspace conversion & no dithering
                        dst_pixels[3] = floatToInt<256>(src_pixels[3]);
                    }
                    dst_pixels -= nComponents;
                    src_pixels -= nComponents;
                }
            }
        }
    } // to_byte_packed_dither

    virtual void to_byte_packed_nodither(const void* pixelData,
                                         const OfxRectI & bounds,
                                         OFX::PixelComponentEnum pixelComponents,
                                         int pixelComponentCount,
                                         OFX::BitDepthEnum bitDepth,
                                         int rowBytes,
                                         const OfxRectI & renderWindow,
                                         void* dstPixelData,
                                         const OfxRectI & dstBounds,
                                         OFX::PixelComponentEnum dstPixelComponents,
                                         int dstPixelComponentCount,
                                         OFX::BitDepthEnum dstBitDepth,
                                         int dstRowBytes) const OVERRIDE FINAL
    {
        assert(bitDepth == eBitDepthFloat && dstBitDepth == eBitDepthUByte);
        assert(pixelComponents == ePixelComponentRGBA || pixelComponents == ePixelComponentRGB || pixelComponents == ePixelComponentAlpha);
        assert(dstPixelComponents == ePixelComponentRGBA || dstPixelComponents == ePixelComponentRGB || dstPixelComponents == ePixelComponentAlpha);
        assert(bounds.x1 <= renderWindow.x1 && renderWindow.x2 <= bounds.x2 &&
               bounds.y1 <= renderWindow.y1 && renderWindow.y2 <= bounds.y2 &&
               dstBounds.x1 <= renderWindow.x1 && renderWindow.x2 <= dstBounds.x2 &&
               dstBounds.y1 <= renderWindow.y1 && renderWindow.y2 <= dstBounds.y2);
        validate();

        const int srcComponents = pixelComponentCount;
        const int dstComponents = dstPixelComponentCount;

        for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
            const float *src_pixels = (const float*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, 0, y);
            unsigned char *dst_pixels = (unsigned char*)OFX::getPixelAddress(dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes, 0, y);
            const float *src_end = (const float*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, renderWindow.x2, y, false);
            unsigned char tmpPixel[4] = {0, 0, 0, 0};
            while (src_pixels != src_end) {
                if (srcComponents == 1) {
                    // alpha channel: no colorspace conversion
                    tmpPixel[3] = floatToInt<256>(src_pixels[0]);
                } else {
                    for (int k = 0; k < 3; ++k) {
                        tmpPixel[k] = toColorSpaceUint8FromLinearFloatFast(src_pixels[k]);
                    }
                    if (srcComponents == 4) {
                        // alpha channel: no colorspace conversion
                        tmpPixel[3] = floatToInt<256>(src_pixels[3]);
                    }
                }
                if (dstComponents == 1) {
                    dst_pixels[0] = tmpPixel[3];
                } else {
                    for (int k = 0; k < dstComponents; ++k) {
                        dst_pixels[k] = tmpPixel[k];
                    }
                }
                dst_pixels += dstComponents;
                src_pixels += srcComponents;
            }
        }
    } // to_byte_packed_nodither

    // uses Rec.709 to convert from color to grayscale
    virtual void to_byte_grayscale_nodither(const void* pixelData,
                                            const OfxRectI & bounds,
                                            OFX::PixelComponentEnum pixelComponents,
                                            int pixelComponentCount,
                                            OFX::BitDepthEnum bitDepth,
                                            int rowBytes,
                                            const OfxRectI & renderWindow,
                                            void* dstPixelData,
                                            const OfxRectI & dstBounds,
                                            OFX::PixelComponentEnum dstPixelComponents,
                                            int dstPixelComponentCount,
                                            OFX::BitDepthEnum dstBitDepth,
                                            int dstRowBytes) const OVERRIDE FINAL
    {
        assert(bitDepth == eBitDepthFloat && dstBitDepth == eBitDepthUByte &&
               (pixelComponents == ePixelComponentRGB || pixelComponents == ePixelComponentRGBA) &&
               dstPixelComponents == ePixelComponentAlpha &&
               (pixelComponentCount == 3 || pixelComponentCount == 4) &&
               dstPixelComponentCount == 1);
        assert(bounds.x1 <= renderWindow.x1 && renderWindow.x2 <= bounds.x2 &&
               bounds.y1 <= renderWindow.y1 && renderWindow.y2 <= bounds.y2 &&
               dstBounds.x1 <= renderWindow.x1 && renderWindow.x2 <= dstBounds.x2 &&
               dstBounds.y1 <= renderWindow.y1 && renderWindow.y2 <= dstBounds.y2);
        validate();

        const int srcComponents = pixelComponentCount;

        for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
            const float *src_pixels = (const float*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, 0, y);
            unsigned char *dst_pixels = (unsigned char*)OFX::getPixelAddress(dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes, 0, y);
            const float *src_end = (const float*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, renderWindow.x2, y, false);

            while (src_pixels != src_end) {
                float l = 0.2126 * src_pixels[0] + 0.7152 * src_pixels[1] + 0.0722 * src_pixels[2]; // Rec.709 luminance formula
                dst_pixels[0] = toColorSpaceUint8FromLinearFloatFast(l);
                ++dst_pixels;
                src_pixels += srcComponents;
            }
        }
    } // to_byte_packed_nodither

    virtual void to_short_packed(const void* pixelData,
                                 const OfxRectI & bounds,
                                 OFX::PixelComponentEnum pixelComponents,
                                 int pixelComponentCount,
                                 OFX::BitDepthEnum bitDepth,
                                 int rowBytes,
                                 const OfxRectI & renderWindow,
                                 void* dstPixelData,
                                 const OfxRectI & dstBounds,
                                 OFX::PixelComponentEnum dstPixelComponents,
                                 int dstPixelComponentCount,
                                 OFX::BitDepthEnum dstBitDepth,
                                 int dstRowBytes) const OVERRIDE FINAL
    {
        assert(bitDepth == eBitDepthFloat && dstBitDepth == eBitDepthUShort && pixelComponents == dstPixelComponents && pixelComponentCount == dstPixelComponentCount);
        assert(bounds.x1 <= renderWindow.x1 && renderWindow.x2 <= bounds.x2 &&
               bounds.y1 <= renderWindow.y1 && renderWindow.y2 <= bounds.y2 &&
               dstBounds.x1 <= renderWindow.x1 && renderWindow.x2 <= dstBounds.x2 &&
               dstBounds.y1 <= renderWindow.y1 && renderWindow.y2 <= dstBounds.y2);
        validate();

        const int nComponents = pixelComponentCount;

        for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
            const float *src_pixels = (const float*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, 0, y);
            unsigned char *dst_pixels = (unsigned char*)OFX::getPixelAddress(dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes, 0, y);
            const float *src_end = (const float*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, renderWindow.x2, y, false);

            while (src_pixels != src_end) {
                if (nComponents == 1) {
                    // alpha channel: no colorspace conversion
                    dst_pixels[0] = floatToInt<65536>(src_pixels[0]);
                } else {
                    for (int k = 0; k < 3; ++k) {
                        dst_pixels[k] = toColorSpaceUint16FromLinearFloatFast(src_pixels[k]);
                    }
                    if (nComponents == 4) {
                        // alpha channel: no colorspace conversion
                        dst_pixels[3] = floatToInt<65536>(src_pixels[3]);
                    }
                }
                dst_pixels += nComponents;
                src_pixels += nComponents;
            }
        }
    }

    virtual void from_byte_packed(const void* pixelData,
                                  const OfxRectI & bounds,
                                  OFX::PixelComponentEnum pixelComponents,
                                  int pixelComponentCount,
                                  OFX::BitDepthEnum bitDepth,
                                  int rowBytes,
                                  const OfxRectI & renderWindow,
                                  void* dstPixelData,
                                  const OfxRectI & dstBounds,
                                  OFX::PixelComponentEnum dstPixelComponents,
                                  int dstPixelComponentCount,
                                  OFX::BitDepthEnum dstBitDepth,
                                  int dstRowBytes) const OVERRIDE FINAL
    {
        assert(bitDepth == eBitDepthUByte && dstBitDepth == eBitDepthFloat && pixelComponents == dstPixelComponents && pixelComponentCount == dstPixelComponentCount);
        assert(bounds.x1 <= renderWindow.x1 && renderWindow.x2 <= bounds.x2 &&
               bounds.y1 <= renderWindow.y1 && renderWindow.y2 <= bounds.y2 &&
               dstBounds.x1 <= renderWindow.x1 && renderWindow.x2 <= dstBounds.x2 &&
               dstBounds.y1 <= renderWindow.y1 && renderWindow.y2 <= dstBounds.y2);
        validate();

        const int nComponents = pixelComponentCount;

        for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
            const unsigned char *src_pixels = (const unsigned char*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, 0, y);
            float *dst_pixels = (float*)OFX::getPixelAddress(dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes, 0, y);
            const unsigned char *src_end = (const unsigned char*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, renderWindow.x2, y, false);


            while (src_pixels != src_end) {
                if (nComponents == 1) {
                    *dst_pixels++ = intToFloat<256>(*src_pixels++);
                } else {
                    for (int k = 0; k < 3; ++k) {
                        dst_pixels[k] = fromColorSpaceUint8ToLinearFloatFast(src_pixels[k]);
                    }
                    if (nComponents == 4) {
                        // alpha channel: no colorspace conversion
                        dst_pixels[3] = intToFloat<256>(src_pixels[3]);
                    }
                }
                dst_pixels += nComponents;
                src_pixels += nComponents;
            }
        }
    }

    virtual void from_short_packed(const void* pixelData,
                                   const OfxRectI & bounds,
                                   OFX::PixelComponentEnum pixelComponents,
                                   int pixelComponentCount,
                                   OFX::BitDepthEnum bitDepth,
                                   int rowBytes,
                                   const OfxRectI & renderWindow,
                                   void* dstPixelData,
                                   const OfxRectI & dstBounds,
                                   OFX::PixelComponentEnum dstPixelComponents,
                                   int dstPixelComponentCount,
                                   OFX::BitDepthEnum dstBitDepth,
                                   int dstRowBytes) const OVERRIDE FINAL
    {
        assert(bitDepth == eBitDepthUShort && dstBitDepth == eBitDepthFloat && pixelComponents == dstPixelComponents && pixelComponentCount == dstPixelComponentCount);
        assert(bounds.x1 <= renderWindow.x1 && renderWindow.x2 <= bounds.x2 &&
               bounds.y1 <= renderWindow.y1 && renderWindow.y2 <= bounds.y2 &&
               dstBounds.x1 <= renderWindow.x1 && renderWindow.x2 <= dstBounds.x2 &&
               dstBounds.y1 <= renderWindow.y1 && renderWindow.y2 <= dstBounds.y2);
        validate();

        const int nComponents = pixelComponentCount;

        for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
            const unsigned short *src_pixels = (const unsigned short*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, 0, y);
            float *dst_pixels = (float*)OFX::getPixelAddress(dstPixelData, dstBounds, dstPixelComponents, dstBitDepth, dstRowBytes, 0, y);
            const unsigned short *src_end = (const unsigned short*)OFX::getPixelAddress(pixelData, bounds, pixelComponents, bitDepth, rowBytes, renderWindow.x2, y, false);


            while (src_pixels != src_end) {
                if (nComponents == 1) {
                    *dst_pixels++ = intToFloat<65536>(*src_pixels++);
                } else {
                    for (int k = 0; k < 3; ++k) {
                        dst_pixels[k] = fromColorSpaceUint16ToLinearFloatFast(src_pixels[k]);
                    }
                    if (nComponents == 4) {
                        // alpha channel: no colorspace conversion
                        dst_pixels[3] = intToFloat<65536>(src_pixels[3]);
                    }
                }
                dst_pixels += nComponents;
                src_pixels += nComponents;
            }
        }
    }
};


inline float
from_func_linear(float v)
{
    return v;
}

inline float
to_func_linear(float v)
{
    return v;
}

inline float
from_func_srgb(float v)
{
    if (v < 0.04045f) {
        return (v < 0.0f) ? 0.0f : v * (1.0f / 12.92f);
    } else {
        return std::pow( (v + 0.055f) * (1.0f / 1.055f), 2.4f );
    }
}

inline float
to_func_srgb(float v)
{
    if (v < 0.0031308f) {
        return (v < 0.0f) ? 0.0f : v * 12.92f;
    } else {
        return 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
    }
}

inline float
from_func_Rec709(float v)
{
    if (v < 0.081f) {
        return (v < 0.0f) ? 0.0f : v * (1.0f / 4.5f);
    } else {
        return std::pow( (v + 0.099f) * (1.0f / 1.099f), (1.0f / 0.45f) );
    }
}

inline float
to_func_Rec709(float v)
{
    if (v < 0.018f) {
        return (v < 0.0f) ? 0.0f : v * 4.5f;
    } else {
        return 1.099f * std::pow(v, 0.45f) - 0.099f;
    }
}

inline float
from_func_gamma22(float v)
{
    return (v <= 0) ? 0. : std::pow(v, 2.2f);
}

inline float
to_func_gamma22(float v)
{
    return (v <= 0) ? 0. : std::pow(v, 1.f / 2.2f);
}

/*
   Following the formula:
   offset = pow(10,(blackpoint - whitepoint) * 0.002 / gammaSensito)
   gain = 1/(1-offset)
   linear = gain * pow(10,(1023*v - whitepoint)*0.002/gammaSensito)
   cineon = (log10((v + offset) /gain)/ (0.002 / gammaSensito) + whitepoint)/1023
   Here we're using: blackpoint = 95.0
   whitepoint = 685.0
   gammasensito = 0.6
 */
inline float
from_func_Cineon(float v)
{
    return ( 1.f / ( 1.f - std::pow(10.f, 1.97f) ) ) * std::pow(10.f, ( (1023.f * v) - 685.f ) * 0.002f / 0.6f);
}

inline float
to_func_Cineon(float v)
{
    float offset = std::pow(10.f, 1.97f);

    return (std::log10( (v + offset) / ( 1.f / (1.f - offset) ) ) / 0.0033f + 685.0f) / 1023.f;
}

inline float
from_func_Gamma1_8(float v)
{
    return std::pow(v, 0.55f);
}

inline float
to_func_Gamma1_8(float v)
{
    return std::pow(v, 1.8f);
}

inline float
from_func_Gamma2_2(float v)
{
    return std::pow(v, 0.45f);
}

inline float
to_func_Gamma2_2(float v)
{
    return std::pow(v, 2.2f);
}

inline float
from_func_Panalog(float v)
{
    return (std::pow(10.f, (1023.f * v - 681.f) / 444.f) - 0.0408f) / 0.96f;
}

inline float
to_func_Panalog(float v)
{
    return (444.f * std::log10(0.0408f + 0.96f * v) + 681.f) / 1023.f;
}

inline float
from_func_ViperLog(float v)
{
    return std::pow(10.f, (1023.f * v - 1023.f) / 500.f);
}

inline float
to_func_ViperLog(float v)
{
    return (500.f * std::log10(v) + 1023.f) / 1023.f;
}

inline float
from_func_RedLog(float v)
{
    return (std::pow(10.f, ( 1023.f * v - 1023.f ) / 511.f) - 0.01f) / 0.99f;
}

inline float
to_func_RedLog(float v)
{
    return (511.f * std::log10(0.01f + 0.99f * v) + 1023.f) / 1023.f;
}

inline float
from_func_AlexaV3LogC(float v)
{
    return v > 0.1496582f ? std::pow(10.f, (v - 0.385537f) / 0.2471896f) * 0.18f - 0.00937677f
           : ( v / 0.9661776f - 0.04378604f) * 0.18f - 0.00937677f;
}

inline float
to_func_AlexaV3LogC(float v)
{
    return v > 0.010591f ?  0.247190f * std::log10(5.555556f * v + 0.052272f) + 0.385537f
           : v * 5.367655f + 0.092809f;
}

/// convert RGB to HSV
/// In Nuke's viewer, sRGB values are used (apply to_func_srgb to linear
/// RGB values before calling this fuunction)
// r,g,b values are from 0 to 1
/// h = [0,360], s = [0,1], v = [0,1]
///		if s == 0, then h = -1 (undefined)
void rgb_to_hsv( float r, float g, float b, float *h, float *s, float *v );
void hsv_to_rgb( float h, float s, float v, float *r, float *g, float *b );

void rgb_to_hsl( float r, float g, float b, float *h, float *s, float *l );
void hsl_to_rgb( float h, float s, float l, float *r, float *g, float *b );

void rgb_to_hsi( float r, float g, float b, float *h, float *s, float *i );
void hsi_to_rgb( float h, float s, float i, float *r, float *g, float *b );

void rgb_to_ycbcr601( float r, float g, float b, float *y, float *cb, float *cr );
void ycbcr601_to_rgb( float y, float cb, float cr, float *r, float *g, float *b );

void rgb_to_ycbcr709( float r, float g, float b, float *y, float *cb, float *cr );
void ycbcr709_to_rgb( float y, float cb, float cr, float *r, float *g, float *b );

void rgb_to_ypbpr601( float r, float g, float b, float *y, float *pb, float *pr );
void ypbpr601_to_rgb( float y, float pb, float pr, float *r, float *g, float *b );

void rgb_to_ypbpr709( float r, float g, float b, float *y, float *pb, float *pr );
void ypbpr709_to_rgb( float y, float pb, float pr, float *r, float *g, float *b );

void rgb_to_yuv601( float r, float g, float b, float *y, float *u, float *v );
void yuv601_to_rgb( float y, float u, float v, float *r, float *g, float *b );

void rgb_to_yuv709( float r, float g, float b, float *y, float *u, float *v );
void yuv709_to_rgb( float y, float u, float v, float *r, float *g, float *b );

void rgb_to_xyz_rec709( float r, float g, float b, float *x, float *y, float *z );
void xyz_rec709_to_rgb( float x, float y, float z, float *r, float *g, float *b );

void xyz_rec709_to_lab( float x, float y, float z, float *l, float *a, float *b );
void lab_to_xyz_rec709( float l, float a, float b, float *x, float *y, float *z );

void rgb_to_lab( float r, float g, float b, float *l, float *a, float *b_ );
void lab_to_rgb( float l, float a, float b, float *r, float *g, float *b_ );


// a Singleton that holds precomputed LUTs for the whole application.
// The m_instance member is static and is thus built before the first call to Instance().
template <class MUTEX>
class LutManager
{

    typedef OFX::MultiThread::AutoMutexT<MUTEX> AutoMutex;

    //each lut with a ref count mapped against their name
    struct LutContainer
    {
        const LutBase* lut;
        int refCount;

        LutContainer() : lut(0), refCount(0) {}
    };
    typedef std::map<std::string, LutContainer > LutsMap;

public:
    static LutManager<MUTEX>& Instance()
    {
        static LutManager<MUTEX> m_instance;
        return m_instance;
    };

    /**
     * @brief Returns a pointer to a lut with the given name and the given from and to functions.
     * If a lut with the same name didn't already exist, then it will create one.
     * Ownership of the returned pointer remains to the LutManager.
     * You must release the lut when you are done using it.
     * @WARNING: Not thread-safe. You should call it in the load() action of your plug-in
     **/
    static const LutBase* getLut(const std::string & name,
                                 fromColorSpaceFunctionV1 fromFunc,
                                 toColorSpaceFunctionV1 toFunc)
    {

        typename LutsMap::iterator found = Instance().luts.find(name);

        if ( found != Instance().luts.end() ) {
            ++found->second.refCount;
            return found->second.lut;
        } else {
            LutContainer& c = Instance().luts[name];

            Lut<MUTEX>* lut = new Lut<MUTEX>(name, fromFunc, toFunc);;
            lut->validate();
            c.lut = lut;
            c.refCount = 1;
            return c.lut;
        }

        return NULL;
    }

    /**
     * @brief Release a lut previously retrieved with getLut()
    * @WARNING: Not thread-safe. You should call it in the unload() action of your plug-in
     **/
    static void releaseLut(const std::string& name)
    {
        typename LutsMap::iterator found = Instance().luts.find(name);
        if (found != Instance().luts.end()) {
            if (found->second.refCount == 1) {
                delete found->second.lut;
                Instance().luts.erase(found);
            } else {
                --found->second.refCount;
                assert(found->second.refCount > 0);
            }
        }
    }

    ///buit-ins color-spaces
    static const LutBase* linearLut()
    {
        return Instance().getLut("Linear", from_func_linear, to_func_linear);
    }

    static const LutBase* sRGBLut()
    {
        return Instance().getLut("sRGB", from_func_srgb, to_func_srgb);
    }

    static const LutBase* Rec709Lut()
    {
        return Instance().getLut("Rec709", from_func_Rec709, to_func_Rec709);
    }

    static const LutBase* CineonLut()
    {
        return Instance().getLut("Cineon", from_func_Cineon, to_func_Cineon);
    }

    static const LutBase* Gamma1_8Lut()
    {
        return Instance().getLut("Gamma1_8", from_func_Gamma1_8, to_func_Gamma1_8);
    }

    static const LutBase* Gamma2_2Lut()
    {
        return Instance().getLut("Gamma2_2", from_func_Gamma2_2, to_func_Gamma2_2);
    }

    static const LutBase* PanaLogLut()
    {
        return Instance().getLut("PanaLog", from_func_Panalog, to_func_Panalog);
    }

    static const LutBase* ViperLogLut()
    {
        return Instance().getLut("ViperLog", from_func_ViperLog, to_func_ViperLog);
    }

    static const LutBase* RedLogLut()
    {
        return Instance().getLut("RedLog", from_func_RedLog, to_func_RedLog);
    }

    static const LutBase* AlexaV3LogCLut()
    {
        return Instance().getLut("AlexaV3LogC", from_func_AlexaV3LogC, to_func_AlexaV3LogC);
    }

private:
    LutManager &operator= (const LutManager &)
    {
        return *this;
    }

    LutManager(const LutManager &)
    {
    }


    LutManager()
    : luts()
    {

    }

    ~LutManager()
    {

        // The luts must all have been released before!
        // This is because the Lut holds a OFX::MultiThread::Mutex and it can't be deleted
        // by this singleton because it makes their destruction time uncertain regarding to
        // the host multi-thread suite.
        // Make sure every call to getLut() is paired with a call to releaseLut()
        assert(luts.empty());
        for (typename LutsMap::iterator it = luts.begin(); it!=luts.end(); ++it) {
            delete it->second.lut;
        }
    }

    LutsMap luts;
};


template <class MUTEX>
const LutBase*
getLut(const std::string & name,
       fromColorSpaceFunctionV1 fromFunc,
       toColorSpaceFunctionV1 toFunc)
{
    return LutManager<MUTEX>::getLut(name, fromFunc, toFunc);
}

template <class MUTEX>
void
releaseLut(const std::string& name)
{
    return LutManager<MUTEX>::releaseLut(name);
}

}         //namespace Color
}     //namespace OFX

#endif // ifndef openfx_supportext_ofxsLut_h
