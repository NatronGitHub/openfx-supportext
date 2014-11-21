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
#include <map>
#include <cmath>
#include <cassert>
#include <cstring> // for memcpy

#include "ofxCore.h"
#include "ofxsMacros.h"

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
            
            
            virtual void to_byte_packed(unsigned char* to, const float* from,const OfxRectI & renderWindow, int nComponents,
                                        const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes) const = 0;
            
            virtual void to_short_packed(unsigned short* to, const float* from,const OfxRectI & renderWindow, int nComponents,
                                         const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes) const = 0;
            
            virtual void from_byte_packed(float* to, const unsigned char* from,const OfxRectI & renderWindow, int nComponents,
                                          const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes) const = 0;
            
            virtual void from_short_packed(float* to, const unsigned short* from,const OfxRectI & renderWindow, int nComponents,
                                           const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes) const = 0;
            
        protected:
            
            static float index_to_float(const unsigned short i);
            
            static unsigned short hipart(const float f);

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
            , _lock(new MUTEX())
            {
            }
            
            virtual ~Lut() {}
            
            ///init luts
            ///it uses fromColorSpaceFloatToLinearFloat(float) and toColorSpaceFloatFromLinearFloat(float)
            ///Called by validate()
            void fillTables() const
            {
                if (init_) {
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
                _lock->lock();
                
                if (init_) {
                    _lock->unlock();
                    return;
                }
                fillTables();
                init_ = true;
                _lock->unlock();
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
                assert(init_);
                
                return Color::uint8xxToChar(toFunc_hipart_to_uint8xx[hipart(v)]);
            }

            
            virtual unsigned short toColorSpaceUint8xxFromLinearFloatFast(float v) const OVERRIDE FINAL WARN_UNUSED_RETURN
            {
                assert(init_);
                
                return toFunc_hipart_to_uint8xx[hipart(v)];

            }
            
            // the following only works for increasing LUTs

            virtual unsigned short toColorSpaceUint16FromLinearFloatFast(float v) const OVERRIDE FINAL WARN_UNUSED_RETURN
            {
                assert(init_);
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
                assert(init_);
                
                return fromFunc_uint8_to_float[v];
            }
            
            virtual float fromColorSpaceUint16ToLinearFloatFast(unsigned short v) const OVERRIDE FINAL WARN_UNUSED_RETURN
            {
                assert(init_);
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
            
            
            virtual void to_byte_packed(unsigned char* to, const float* from,const OfxRectI & renderWindow, int nComponents,
                                        const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes) const OVERRIDE FINAL
            {
                validate();
                
                for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
                    
                    int start = rand() % (renderWindow.x2 - renderWindow.x1);
                    
                    unsigned error[3] = { 0x80, 0x80, 0x80 };
                    
                    const float *src_pixels = from + (y * srcRowBytes * nComponents) + start * nComponents;
                    unsigned char *dst_pixels = to + (y * dstRowBytes * nComponents) + start * nComponents;
                    
                    const float* originalSrc_pixels = src_pixels;
                    unsigned char* originalDst_pixels = dst_pixels;
                    
                    
                    /* go fowards from starting point to end of line: */
                    const float* src_end = src_pixels + renderWindow.x2 * nComponents;
                    
                    while (src_pixels != src_end) {
                        
                        if (nComponents == 1) {
                            *dst_pixels++ = floatToInt<256>(*src_pixels++);
                        } else {
                            for (int k = 0; k < nComponents; ++k) {
                                if (k == 3) {
                                    *dst_pixels++ = floatToInt<256>(*src_pixels++);
                                } else {
                                    error[k] = (error[k] & 0xff) + toColorSpaceUint8xxFromLinearFloatFast(*src_pixels++);
                                    assert(error[k] < 0x10000);
                                    *dst_pixels++ = (unsigned char)(error[k] >> 8);
                                }
                            }
                        }
                        
                    }
                    
                    /* go backwards from starting point to start of line: */
                    src_pixels = originalSrc_pixels - nComponents;
                    src_end = src_pixels - start * nComponents;
                    dst_pixels = originalDst_pixels - nComponents;
                    
                    for (int i = 0; i < 3; ++i) {
                        error[i] = 0x80;
                    }
                    
                    while (src_pixels != src_end) {
                        
                        if (nComponents == 1) {
                            *dst_pixels-- = floatToInt<256>(*src_pixels--);
                        } else {
                            for (int k = 0; k < nComponents; ++k) {
                                if (k == 3) {
                                    *dst_pixels-- = floatToInt<256>(*src_pixels--);
                                } else {
                                    error[k] = (error[k] & 0xff) + toColorSpaceUint8xxFromLinearFloatFast(*src_pixels--);
                                    assert(error[k] < 0x10000);
                                    *dst_pixels-- = (unsigned char)(error[k] >> 8);
                                }
                            }
                        }
                        
                    }
                }
                

            }
            
            virtual void to_short_packed(unsigned short* to, const float* from,const OfxRectI & renderWindow, int nComponents,
                                         const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes) const OVERRIDE FINAL
            {
                validate();
                
                int w = renderWindow.x2 - renderWindow.x1;
                
                for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
                    
                    const float *src_pixels = from + (y * srcRowBytes * nComponents) + renderWindow.x1 * nComponents;
                    unsigned short *dst_pixels = to + (y * dstRowBytes * nComponents) + renderWindow.x1 * nComponents;
                    
                    
                    const float* src_end = src_pixels + w * nComponents;
                    
                    while (src_pixels != src_end) {
                        
                        if (nComponents == 1) {
                            *dst_pixels++ = floatToInt<65536>(*src_pixels++);
                        } else {
                            for (int k = 0; k < nComponents; ++k) {
                                if (k == 3) {
                                    *dst_pixels++ = floatToInt<65536>(*src_pixels++);
                                } else {
                                    *dst_pixels++ = toColorSpaceUint16FromLinearFloatFast(*src_pixels++);
                                }
                            }
                        }
                        
                    }
                }

            }
            
            virtual void from_byte_packed(float* to, const unsigned char* from,const OfxRectI & renderWindow, int nComponents,
                                          const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes) const OVERRIDE FINAL
            {
                validate();
                
                int w = renderWindow.x2 - renderWindow.x1;
                for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
                    
                    const unsigned char *src_pixels = from + (y * srcRowBytes * nComponents) + renderWindow.x1 * nComponents;
                    float *dst_pixels = to + (y * dstRowBytes * nComponents) + renderWindow.x1 * nComponents;
                    
                    
                    const unsigned char* src_end = src_pixels + w * nComponents;
                    
                    
                    while (src_pixels != src_end) {
                        
                        if (nComponents == 1) {
                            *dst_pixels++ = intToFloat<256>(*src_pixels++);
                        } else {
                            for (int k = 0; k < nComponents; ++k) {
                                if (k == 3) {
                                    *dst_pixels++ = intToFloat<256>(*src_pixels++);
                                } else {
                                    *dst_pixels++ = fromColorSpaceUint8ToLinearFloatFast(*src_pixels++);
                                }
                            }
                        }
                        
                    }
                }
            }
            
            virtual void from_short_packed(float* to, const unsigned short* from,const OfxRectI & renderWindow, int nComponents,
                                           const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes) const OVERRIDE FINAL
            {
                validate();
                
                int w = renderWindow.x2 - renderWindow.x1;
                for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
                    
                    const unsigned short *src_pixels = from + (y * srcRowBytes * nComponents) + renderWindow.x1 * nComponents;
                    float *dst_pixels = to + (y * dstRowBytes * nComponents) + renderWindow.x1 * nComponents;
                    
                    
                    const unsigned short* src_end = src_pixels + w * nComponents;
                    
                    
                    while (src_pixels != src_end) {
                        
                        if (nComponents == 1) {
                            *dst_pixels++ = intToFloat<65536>(*src_pixels++);
                        } else {
                            for (int k = 0; k < nComponents; ++k) {
                                if (k == 3) {
                                    *dst_pixels++ = intToFloat<65536>(*src_pixels++);
                                } else {
                                    *dst_pixels++ = fromColorSpaceUint16ToLinearFloatFast(*src_pixels++);
                                }
                            }
                        }
                        
                    }
                }
            }
            
        };
        
        
        namespace Linear {
            /////the following functions expects a float input buffer, one could extend it to cover all bitdepths.
            
            
            void to_byte_packed(unsigned char* to, const float* from,const OfxRectI & renderWindow, int nComponents,
                                const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes);
            
            void to_short_packed(unsigned short* to, const float* from,const OfxRectI & renderWindow, int nComponents,
                                 const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes);
            
            
            void from_byte_packed(float* to, const unsigned char* from,const OfxRectI & renderWindow, int nComponents,
                                  const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes);
            
            void from_short_packed(float* to, const unsigned short* from,const OfxRectI & renderWindow, int nComponents,
                                   const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes);
            
        }        //namespace Linear
        
        
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
            return ( 1.f / ( 1.f - std::pow(10.f,1.97f) ) ) * std::pow(10.f,( (1023.f * v) - 685.f ) * 0.002f / 0.6f);
        }
        
        inline float
        to_func_Cineon(float v)
        {
            float offset = std::pow(10.f,1.97f);
            
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
            return (std::pow(10.f,(1023.f * v - 681.f) / 444.f) - 0.0408) / 0.96f;
        }
        
        inline float
        to_func_Panalog(float v)
        {
            return (444.f * std::log10(0.0408 + 0.96f * v) + 681.f) / 1023.f;
        }
        
        
        inline float
        from_func_ViperLog(float v)
        {
            return std::pow(10.f,(1023.f * v - 1023.f) / 500.f);
        }
        
        inline float
        to_func_ViperLog(float v)
        {
            return (500.f * std::log10(v) + 1023.f) / 1023.f;
        }
        
        
        inline float
        from_func_RedLog(float v)
        {
            return (std::pow(10.f,( 1023.f * v - 1023.f ) / 511.f) - 0.01f) / 0.99f;
        }
        
        inline float
        to_func_RedLog(float v)
        {
            return (511.f * std::log10(0.01f + 0.99f * v) + 1023.f) / 1023.f;
        }
        
        
        inline float
        from_func_AlexaV3LogC(float v)
        {
            return v > 0.1496582f ? std::pow(10.f,(v - 0.385537f) / 0.2471896f) * 0.18f - 0.00937677f
            : ( v / 0.9661776f - 0.04378604) * 0.18f - 0.00937677f;
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
        
        
        
        // a Singleton that holds precomputed LUTs for the whole application.
        // The m_instance member is static and is thus built before the first call to Instance().
        class LutManager
        {
            
            
            //each lut with a ref count mapped against their name
            typedef std::map<std::string,const LutBase * > LutsMap;
            
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
            static const LutBase* sRGBLut()
            {
                return LutManager::m_instance.getLut<MUTEX>("sRGB",from_func_srgb,to_func_srgb);
            }
            
            template <class MUTEX>
            static const LutBase* Rec709Lut()
            {
                return LutManager::m_instance.getLut<MUTEX>("Rec709",from_func_Rec709,to_func_Rec709);

            }
            
            template <class MUTEX>
            static const LutBase* CineonLut()
            {
                return LutManager::m_instance.getLut<MUTEX>("Cineon",from_func_Cineon,to_func_Cineon);
            }
            
            template <class MUTEX>
            static const LutBase* Gamma1_8Lut()
            {
                return LutManager::m_instance.getLut<MUTEX>("Gamma1_8",from_func_Gamma1_8,to_func_Gamma1_8);
            }
            
            template <class MUTEX>
            static const LutBase* Gamma2_2Lut()
            {
                return LutManager::m_instance.getLut<MUTEX>("Gamma2_2",from_func_Gamma2_2,to_func_Gamma2_2);
            }
            
            template <class MUTEX>
            static const LutBase* PanaLogLut()
            {
                return LutManager::m_instance.getLut<MUTEX>("PanaLog",from_func_Panalog,to_func_Panalog);
            }
            
            template <class MUTEX>
            static const LutBase* ViperLogLut()
            {
                return LutManager::m_instance.getLut<MUTEX>("ViperLog",from_func_ViperLog,to_func_ViperLog);
            }
            
            template <class MUTEX>
            static const LutBase* RedLogLut()
            {
                return LutManager::m_instance.getLut<MUTEX>("RedLog",from_func_RedLog,to_func_RedLog);
            }
            
            template <class MUTEX>
            static const LutBase* AlexaV3LogCLut()
            {
                return LutManager::m_instance.getLut<MUTEX>("AlexaV3LogC",from_func_AlexaV3LogC,to_func_AlexaV3LogC);
            }
            
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
