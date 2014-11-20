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

#ifndef __SUPPORT_EXT_LUT__
#define __SUPPORT_EXT_LUT__

#include <string>

#include "ofxsMacros.h"

namespace OFX {
namespace Color {



/// @enum An enum describing supported pixels packing formats
enum PixelPacking
{
    PACKING_RGBA = 0,
    PACKING_BGRA,
    PACKING_RGB,
    PACKING_BGR,
    PACKING_PLANAR
};

/* @brief Converts a float ranging in [0 - 1.f] in the desired color-space to linear color-space also ranging in [0 - 1.f]*/
typedef float (*fromColorSpaceFunctionV1)(float v);

/* @brief Converts a float ranging in [0 - 1.f] in  linear color-space to the desired color-space to also ranging in [0 - 1.f]*/
typedef float (*toColorSpaceFunctionV1)(float v);


class LutBase
{
protected:
    std::string _name;         ///< name of the lut
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

    virtual ~LutBase() {}

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
             * @see toColorSpaceFloatFromLinearFloatFast(float)
             */
    virtual float toColorSpaceFloatFromLinearFloat(float v) const = 0;

    /* @brief Converts a float ranging in [0 - 1.f] in linear color-space using the look-up tables.
             * @return A float in [0 - 1.f] in the destination color-space.
             */
    // It is not recommended to use this function, because the output is quantized
    // If one really needs float, one has to use the full function (or OpenColorIO)
    //float toColorSpaceFloatFromLinearFloatFast(float v) const;

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


    /**
             * @brief Convert an array of linear floating point pixel values to an
             * array of destination lut values, with error diffusion to avoid posterizing
             * artifacts.
             *
             * \a W is the number of pixels to convert.
             * \a inDelta is the distance between the input elements
             * \a outDelta is the distance between the output elements
             * The delta parameters are useful for:
             - interlacing a planar input buffer into a packed buffer.
             - deinterlacing a packed input buffer into a planar buffer.
             
             * \a alpha is a pointer to an extra alpha planar buffer if you want to premultiply by alpha the from channel.
             * The input and output buffers must not overlap in memory.
             **/
    virtual void to_byte_planar(unsigned char* to, const float* from,int W,const float* alpha = NULL,
                                int inDelta = 1, int outDelta = 1) const = 0;
    virtual void to_short_planar(unsigned short* to, const float* from,int W,const float* alpha = NULL,
                                 int inDelta = 1, int outDelta = 1) const = 0;
    virtual void to_float_planar(float* to, const float* from,int W,const float* alpha = NULL,
                                 int inDelta = 1, int outDelta = 1) const = 0;


    /**
             * @brief These functions work exactly like the to_X_planar functions but expect 2 buffers
             * pointing at (0,0) in an image and convert a rectangle of the image. It also supports
             * several pixel packing commonly used by openfx images.
             * Note that the conversionRect will be clipped to srcRoD and dstRoD to prevent bad memory access
             
             \arg     - from - A pointer to the input buffer, pointing at (0,0) in the image.
             \arg     - srcRoD - The region of definition of the input buffer.
             \arg     - inputPacking - The pixel packing of the input buffer.
             
             \arg     - conversionRect - The region we want to convert. This will be clipped
             agains srcRoD and dstRoD.
             
             \arg     - to - A pointer to the output buffer, pointing at (0,0) in the image.
             \arg     - dstRoD - The region of definition of the output buffer.
             \arg     - outputPacking - The pixel packing of the output buffer.
             
             \arg premult - If true, it indicates we want to premultiply by the alpha channel
             the R,G and B channels if they exist.
             
             \arg invertY - If true then the output scan-line y of the output buffer
             should be converted with the scan-line (srcRoD.y2 - y - 1) of the
             input buffer.
             
             **/
    virtual void to_byte_packed(unsigned char* to, const float* from,const OfxRectI & conversionRect,
                                const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                                PixelPacking inputPacking,PixelPacking outputPacking,bool invertY,bool premult) const = 0;
    virtual void to_short_packed(unsigned short* to, const float* from,const OfxRectI & conversionRect,
                                 const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                                 PixelPacking inputPacking,PixelPacking outputPacking,bool invertY,bool premult) const = 0;
    virtual void to_float_packed(float* to, const float* from,const OfxRectI & conversionRect,
                                 const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                                 PixelPacking inputPacking,PixelPacking outputPacking,bool invertY,bool premult) const = 0;


    /////@TODO the following functions expects a float output buffer, one could extend it to cover all bitdepths.

    /**
             * @brief Convert from a buffer in the input color-space to the output color-space.
             *
             * \a W is the number of pixels to convert.
             * \a inDelta is the distance between the input elements
             * \a outDelta is the distance between the output elements
             * The delta parameters are useful for:
             - interlacing a planar input buffer into a packed buffer.
             - deinterlacing a packed input buffer into a planar buffer.
             
             * \a alpha is a pointer to an extra planar buffer being the alpha channel of the image.
             * If the image was stored with premultiplication on, it will then unpremultiply by alpha
             * before doing the color-space conversion, and the multiply back by alpha.
             * The input and output buffers must not overlap in memory.
             **/
    virtual void from_byte_planar(float* to,const unsigned char* from,
                                  int W,const unsigned char* alpha = NULL,int inDelta = 1, int outDelta = 1) const = 0;
    virtual void from_short_planar(float* to,const unsigned short* from,
                                   int W,const unsigned short* alpha = NULL,int inDelta = 1, int outDelta = 1) const = 0;
    virtual void from_float_planar(float* to,const float* from,
                                   int W,const float* alpha = NULL,int inDelta = 1, int outDelta = 1) const = 0;

    /**
             * @brief These functions work exactly like the to_X_planar functions but expect 2 buffers
             * pointing at (0,0) in an image and convert a rectangle of the image. It also supports
             * several pixel packing commonly used by openfx images.
             * Note that the conversionRect will be clipped to srcRoD and dstRoD to prevent bad memory access
             
             \arg     - from - A pointer to the input buffer, pointing at (0,0) in the image.
             \arg     - srcRoD - The region of definition of the input buffer.
             \arg     - inputPacking - The pixel packing of the input buffer.
             
             \arg     - conversionRect - The region we want to convert. This will be clipped
             agains srcRoD and dstRoD.
             
             \arg     - to - A pointer to the output buffer, pointing at (0,0) in the image.
             \arg     - dstRoD - The region of definition of the output buffer.
             \arg     - outputPacking - The pixel packing of the output buffer.
             
             \arg premult - If true, it indicates we want to unpremultiply the R,G,B channels (if they exist) by the alpha channel
             before doing the color-space conversion, and multiply back by alpha.
             
             \arg invertY - If true then the output scan-line y of the output buffer
             should be converted with the scan-line (srcRoD.y2 - y - 1) of the
             input buffer.
             
             **/
    virtual void from_byte_packed(float* to, const unsigned char* from,const OfxRectI & conversionRect,
                                  const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                                  PixelPacking inputPacking,PixelPacking outputPacking,bool invertY,bool premult) const = 0;

    virtual void from_short_packed(float* to, const unsigned short* from,const OfxRectI & conversionRect,
                                   const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                                   PixelPacking inputPacking,PixelPacking outputPacking,bool invertY,bool premult) const = 0;

    virtual void from_float_packed(float* to, const float* from,const OfxRectI & conversionRect,
                                   const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                                   PixelPacking inputPacking,PixelPacking outputPacking,bool invertY,bool premult) const = 0;



};

/**
         * @brief A Lut (look-up table) used to speed-up color-spaces conversions.
         * If you plan on doing linear conversion, you should just use the Linear class instead.
         **/
template <class MUTEX>
class Lut : public LutBase
{

    /// the fast lookup tables are mutable, because they are automatically initialized post-construction,
    /// and never change afterwards
    mutable unsigned short toFunc_hipart_to_uint8xx[0x10000];         /// contains  2^16 = 65536 values between 0-255
    mutable float fromFunc_uint8_to_float[256];         /// values between 0-1.f
    mutable bool init_;         ///< false if the tables are not yet initialized
    mutable std::auto_ptr<MUTEX> _lock;         ///< protects init_

    friend class LutManager;
    ///private constructor, used by LutManager
    Lut(const std::string & name,
        fromColorSpaceFunctionV1 fromFunc,
        toColorSpaceFunctionV1 toFunc)
        : LutBase(name,fromFunc,toFunc)
        , init_(false)
        , _lock()
    {
    }

    virtual ~Lut() {}

    ///init luts
    ///it uses fromColorSpaceFloatToLinearFloat(float) and toColorSpaceFloatFromLinearFloat(float)
    ///Called by validate()
    void fillTables() const;

public:

    //Called by all public members
    void validate() const;


    virtual float fromColorSpaceFloatToLinearFloat(float v) const OVERRIDE FINAL WARN_UNUSED_RETURN
    {
        return _fromFunc(v);
    }

    virtual float toColorSpaceFloatFromLinearFloat(float v) const OVERRIDE FINAL WARN_UNUSED_RETURN
    {
        return _toFunc(v);
    }

    virtual unsigned char toColorSpaceUint8FromLinearFloatFast(float v) const OVERRIDE FINAL WARN_UNUSED_RETURN;

    virtual unsigned short toColorSpaceUint8xxFromLinearFloatFast(float v) const OVERRIDE FINAL WARN_UNUSED_RETURN;

    virtual unsigned short toColorSpaceUint16FromLinearFloatFast(float v) const OVERRIDE FINAL WARN_UNUSED_RETURN;

    virtual float fromColorSpaceUint8ToLinearFloatFast(unsigned char v) const OVERRIDE FINAL WARN_UNUSED_RETURN;

    virtual float fromColorSpaceUint16ToLinearFloatFast(unsigned short v) const OVERRIDE FINAL WARN_UNUSED_RETURN;



    virtual void to_byte_planar(unsigned char* to, const float* from,int W,const float* alpha = NULL,
                                int inDelta = 1, int outDelta = 1) const OVERRIDE FINAL;
    virtual void to_short_planar(unsigned short* to, const float* from,int W,const float* alpha = NULL,
                                 int inDelta = 1, int outDelta = 1) const OVERRIDE FINAL;
    virtual void to_float_planar(float* to, const float* from,int W,const float* alpha = NULL,
                                 int inDelta = 1, int outDelta = 1) const OVERRIDE FINAL;



    virtual void to_byte_packed(unsigned char* to, const float* from,const OfxRectI & conversionRect,
                                const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                                PixelPacking inputPacking,PixelPacking outputPacking,bool invertY,bool premult) const OVERRIDE FINAL;
    virtual void to_short_packed(unsigned short* to, const float* from,const OfxRectI & conversionRect,
                                 const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                                 PixelPacking inputPacking,PixelPacking outputPacking,bool invertY,bool premult) const OVERRIDE FINAL;
    virtual void to_float_packed(float* to, const float* from,const OfxRectI & conversionRect,
                                 const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                                 PixelPacking inputPacking,PixelPacking outputPacking,bool invertY,bool premult) const OVERRIDE FINAL;



    virtual void from_byte_planar(float* to,const unsigned char* from,
                                  int W,const unsigned char* alpha = NULL,int inDelta = 1, int outDelta = 1) const OVERRIDE FINAL;
    virtual void from_short_planar(float* to,const unsigned short* from,
                                   int W,const unsigned short* alpha = NULL,int inDelta = 1, int outDelta = 1) const OVERRIDE FINAL;
    virtual void from_float_planar(float* to,const float* from,
                                   int W,const float* alpha = NULL,int inDelta = 1, int outDelta = 1) const OVERRIDE FINAL;


    virtual void from_byte_packed(float* to, const unsigned char* from,const OfxRectI & conversionRect,
                                  const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                                  PixelPacking inputPacking,PixelPacking outputPacking,bool invertY,bool premult) const OVERRIDE FINAL;

    virtual void from_short_packed(float* to, const unsigned short* from,const OfxRectI & conversionRect,
                                   const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                                   PixelPacking inputPacking,PixelPacking outputPacking,bool invertY,bool premult) const OVERRIDE FINAL;

    virtual void from_float_packed(float* to, const float* from,const OfxRectI & conversionRect,
                                   const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                                   PixelPacking inputPacking,PixelPacking outputPacking,bool invertY,bool premult) const OVERRIDE FINAL;
};


namespace Linear {
/////the following functions expects a float input buffer, one could extend it to cover all bitdepths.

/**
             * @brief Convert an array of linear floating point pixel values to an
             * array of destination lut values, with error diffusion to avoid posterizing
             * artifacts.
             *
             * \a W is the number of pixels to convert.
             * \a inDelta is the distance between the input elements
             * \a outDelta is the distance between the output elements
             * The delta parameters are useful for:
             - interlacing a planar input buffer into a packed buffer.
             - deinterlacing a packed input buffer into a planar buffer.
             
             * \a alpha is a pointer to an extra alpha planar buffer if you want to premultiply by alpha the from channel.
             * The input and output buffers must not overlap in memory.
             **/
void to_byte_planar(unsigned char* to, const float* from,int W,const float* alpha = NULL,int inDelta = 1, int outDelta = 1);
void to_short_planar(unsigned short* to, const float* from,int W,const float* alpha = NULL,int inDelta = 1, int outDelta = 1);
void to_float_planar(float* to, const float* from,int W,const float* alpha = NULL,int inDelta = 1, int outDelta = 1);


/**
             * @brief These functions work exactly like the to_X_planar functions but expect 2 buffers
             * pointing at (0,0) in the image and convert a rectangle of the image. It also supports
             * several pixel packing commonly used by openfx images.
             * Note that the conversionRect will be clipped to srcRoD and dstRoD to prevent bad memory access
             \arg         - from - A pointer to the input buffer, pointing at (0,0) in the image.
             \arg         - srcRoD - The region of definition of the input buffer.
             \arg         - inputPacking - The pixel packing of the input buffer.
             
             \arg         - conversionRect - The region we want to convert. This will be clipped
             agains srcRoD and dstRoD.
             
             \arg         - to - A pointer to the output buffer, pointing at (0,0) in the image.
             \arg         - dstRoD - The region of definition of the output buffer.
             \arg         - outputPacking - The pixel packing of the output buffer.
             
             \arg premult - If true, it indicates we want to premultiply by the alpha channel
             the R,G and B channels if they exist.
             
             \arg invertY - If true then the output scan-line y of the output buffer
             should be converted with the scan-line (srcRoD.y2 - y - 1) of the
             input buffer.
             **/
void to_byte_packed(unsigned char* to, const float* from,const OfxRectI & conversionRect,
                    const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                    PixelPacking inputPacking,PixelPacking outputPacking,bool invertY,bool premult);
void to_short_packed(unsigned short* to, const float* from,const OfxRectI & conversionRect,
                     const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                     PixelPacking inputPacking,PixelPacking outputPacking,bool invertY,bool premult);
void to_float_packed(float* to, const float* from,const OfxRectI & conversionRect,
                     const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                     PixelPacking inputPacking,PixelPacking outputPacking,bool invertY,bool premult);


/////the following functions expects a float output buffer, one could extend it to cover all bitdepths.

/**
             * @brief Convert from a buffer in the input color-space to the output color-space.
             *
             * \a W is the number of pixels to convert.
             * \a inDelta is the distance between the input elements
             * \a outDelta is the distance between the output elements
             * The delta parameters are useful for:
             - interlacing a planar input buffer into a packed buffer.
             - deinterlacing a packed input buffer into a planar buffer.
             
             * \a alpha is a pointer to an extra planar buffer being the alpha channel of the image.
             * If the image was stored with premultiplication on, it will then unpremultiply by alpha
             * before doing the color-space conversion, and the multiply back by alpha.
             * The input and output buffers must not overlap in memory.
             **/

void from_byte_planar(float* to,const unsigned char* from, int W,int inDelta = 1, int outDelta = 1);
void from_short_planar(float* to,const unsigned short* from,int W,int inDelta = 1, int outDelta = 1);
void from_float_planar(float* to,const float* from,int W,int inDelta = 1, int outDelta = 1);


/**
             * @brief These functions work exactly like the from_X_planar functions but expect 2 buffers
             * pointing at (0,0) in the image and convert a rectangle of the image. It also supports
             * several pixel packing commonly used by openfx images.
             * Note that the conversionRect will be clipped to srcRoD and dstRoD to prevent bad memory access
             
             \arg         - from - A pointer to the input buffer, pointing at (0,0) in the image.
             \arg         - srcRoD - The region of definition of the input buffer.
             \arg         - inputPacking - The pixel packing of the input buffer.
             
             \arg         - conversionRect - The region we want to convert. This will be clipped
             agains srcRoD and dstRoD.
             
             \arg         - to - A pointer to the output buffer, pointing at (0,0) in the image.
             \arg         - dstRoD - The region of definition of the output buffer.
             \arg         - outputPacking - The pixel packing of the output buffer.
             
             \arg invertY - If true then the output scan-line y of the output buffer
             should be converted with the scan-line (srcRoD.y2 - y - 1) of the
             input buffer.
             **/
void from_byte_packed(float* to, const unsigned char* from,const OfxRectI & conversionRect,
                      const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                      PixelPacking inputPacking,PixelPacking outputPacking,bool invertY);

void from_short_packed(float* to, const unsigned short* from,const OfxRectI & conversionRect,
                       const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                       PixelPacking inputPacking,PixelPacking outputPacking,bool invertY);

void from_float_packed(float* to, const float* from,const OfxRectI & conversionRect,
                       const OfxRectI & srcRoD,const OfxRectI & dstRoD,
                       PixelPacking inputPacking,PixelPacking outputPacking,bool invertY);
}        //namespace Linear


///these are put in the header as they are used elsewhere

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

/// convert RGB to HSV
/// In Nuke's viewer, sRGB values are used (apply to_func_srgb to linear
/// RGB values before calling this fuunction)
// r,g,b values are from 0 to 1
/// h = [0,360], s = [0,1], v = [0,1]
///		if s == 0, then h = -1 (undefined)
void rgb_to_hsv( float r, float g, float b, float *h, float *s, float *v );

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


// a Singleton that holds precomputed LUTs for the whole application.
// The m_instance member is static and is thus built before the first call to Instance().
class LutManager
{
public:
    static LutManager &Instance()
    {
        return m_instance;
    };

    /**
             * @brief Returns a pointer to a lut with the given name and the given from and to functions.
             * If a lut with the same name didn't already exist, then it will create one.
             * WARNING : NOT THREAD-SAFE
             **/
    template <class MUTEX>
    static const LutBase*
    getLut(const std::string & name,
           fromColorSpaceFunctionV1 fromFunc,
           toColorSpaceFunctionV1 toFunc)
    {
        LutsMap::iterator found = LutManager::m_instance.luts.find(name);

        if ( found != LutManager::m_instance.luts.end() ) {
            return found->second;
        } else {
            std::pair<LutsMap::iterator,bool> ret =
                    LutManager::m_instance.luts.insert( std::make_pair( name,new Lut<MUTEX>(name,fromFunc,toFunc) ) );
            assert(ret.second);

            return ret.first->second;
        }

        return NULL;
    }


    ///buit-ins color-spaces
    template <class MUTEX>
    static const LutBase* sRGBLut();

    template <class MUTEX>
    static const LutBase* Rec709Lut();

    template <class MUTEX>
    static const LutBase* CineonLut();

    template <class MUTEX>
    static const LutBase* Gamma1_8Lut();

    template <class MUTEX>
    static const LutBase* Gamma2_2Lut();

    template <class MUTEX>
    static const LutBase* PanaLogLut();

    template <class MUTEX>
    static const LutBase* ViperLogLut();

    template <class MUTEX>
    static const LutBase* RedLogLut();

    template <class MUTEX>
    static const LutBase* AlexaV3LogCLut();

private:
    LutManager &operator= (const LutManager &)
    {
        return *this;
    }

    LutManager(const LutManager &)
    {
    }

    static LutManager m_instance;
    LutManager();

    ////the luts must all have been released before!
    ////This is because the Lut holds a OFX::MultiThread::Mutex and it can't be deleted
    //// by this singleton because it makes their destruction time uncertain regarding to
    ///the host multi-thread suite.
    ~LutManager();

    //each lut with a ref count mapped against their name
    typedef std::map<std::string,const LutBase * > LutsMap;
    LutsMap luts;
};


template <class MUTEX>
const LutBase*
getLut(const std::string & name,
       fromColorSpaceFunctionV1 fromFunc,
       toColorSpaceFunctionV1 toFunc)
{
    return LutManager::getLut<MUTEX>(name, fromFunc, toFunc);
}

}     //namespace Color
}     //namespace OFX

#endif
