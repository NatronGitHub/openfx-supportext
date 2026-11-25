/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of openfx-supportext <https://github.com/NatronGitHub/openfx-supportext>,
 * (C) 2018-2021 The Natron Developers
 * (C) 2013-2018 INRIA
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

#include "ofxsLut.h"

#include <algorithm>
#include <cstdint>
#include <cstring> // std::memcpy
#include <limits>
#include <cmath>

#include "ofxsMacros.h"

namespace OFX {
namespace Color {
// The 16-bit LUT index of a float is the high half of its IEEE-754 bit pattern. The bits are
// copied with std::memcpy (reading the inactive member of a union is undefined behaviour in C++,
// C.183) and split with shifts, which act on values, so no endianness test is needed.
static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559, "IEEE-754 binary32 float required");

unsigned short
Lut::hipart(const float f)
{
    std::uint32_t bits;
    std::memcpy( &bits, &f, sizeof(bits) );

    return static_cast<unsigned short>(bits >> 16);
}

float
Lut::index_to_float(const unsigned short i)
{
    /* positive and negative zeros, and all gradual underflow, turn into zero: */
    if ( ( i < 0x80) || ( ( i >= 0x8000) && ( i < 0x8080) ) ) {
        return 0;
    }
    /* All NaN's and infinity turn into the largest possible legal float: */
    if ( ( i >= 0x7f80) && ( i < 0x8000) ) {
        return (std::numeric_limits<float>::max)();
    }
    if (i >= 0xff80) {
        return -(std::numeric_limits<float>::max)();
    }
    // the middle of the bucket: high half i, low half 0x8000
    const std::uint32_t bits = (static_cast<std::uint32_t>(i) << 16) | 0x8000u;
    float f;
    std::memcpy( &f, &bits, sizeof(f) );

    return f;
}

// The wrapHue option of hsv_to_rgb and hsl_to_rgb: wrap a finite hue into [0, OFXS_HUE_CIRCLE).
// Without it, a hue in [-1, -5/6) (mod 1), e.g. after a hue rotation of -360 to -300 degrees, gave
// the sector 5 formula (pink/magenta) instead of sector 0 (red to yellow). Opt-in, since it changes
// existing outputs. (hsi_to_rgb wraps differently, see there.)
static inline void
wrapHueToCircle(float* hue)
{
    float h = *hue;

    if ( ( (h < 0) || (h >= OFXS_HUE_CIRCLE) ) && (std::fabs(h) <= FLT_MAX) ) {
        h -= OFXS_HUE_CIRCLE * std::floor(h / OFXS_HUE_CIRCLE);
        if (h >= OFXS_HUE_CIRCLE) {
            h = 0.f; // rounding of a tiny negative hue
        }
        *hue = h;
    }
}

// The sector (0 to 5) of h, a hue times 6 / OFXS_HUE_CIRCLE, taken modulo 6, and its fractional
// part *f, for hsv_to_rgb and hsl_to_rgb.
// A huge or infinite sector value (degenerate input, e.g. rgb_to_hsl of a pixel whose channels are
// all <= 0) made the int conversion undefined: it is reduced modulo the 6 sectors, and gives hue 0
// if it is not finite. Unchanged wherever the int conversion was defined (|floor| < 2^31; -2^31
// gives the same sector either way).
static inline int
hueSector(float h,
          float* f)
{
    if (std::fabs(h) >= 2147483648.f) {
        h = (std::fabs(h) <= FLT_MAX) ? std::fmod(h, 6.f) : 0.f;
    }
    const float hfloor = std::floor(h);
    int i = 0;
    if ( (hfloor >= -2147483648.f) && (hfloor < 2147483648.f) ) {
        i = (int)hfloor;
    } else {
        h = 0.f; // NaN hue (input data): sector 0, f = 0
    }
    *f = h - i;

    return (i >= 0) ? (i % 6) : (i % 6) + 6; // take h modulo 360
}

// r,g,b values are linear values from 0 to 1
// h = [0,OFXS_HUE_CIRCLE], s = [0,1], v = [0,1]
//		if s == 0, then h = 0 (undefined)
// Reference:
// "Color gamut transform pairs", Alvy Ray Smith, Proceeding SIGGRAPH '78
// https://doi.org/10.1145/800248.807361
// http://www.icst.pku.edu.cn/F/course/ImageProcessing/2018/resource/Color78.pdf
void
rgb_to_hsv( float r,
            float g,
            float b,
            float *h,
            float *s,
            float *v )
{
    float minv = (std::min)((std::min)(r, g), b);
    float maxv = (std::max)((std::max)(r, g), b);

    *v = maxv;                               // v

    float delta = maxv - minv;

    if (maxv != 0.) {
        *s = delta / maxv;                       // s
    } else {
        // r = g = b = 0		// s = 0, v is undefined
        *s = 0.f;
        *h = 0.f;

        return;
    }

    if (delta == 0.) {
        *h = 0.f;                 // gray
    } else if (r == maxv) {
        *h = (g - b) / delta;                       // between yellow & magenta
    } else if (g == maxv) {
        *h = 2 + (b - r) / delta;                   // between cyan & yellow
    } else {
        *h = 4 + (r - g) / delta;                   // between magenta & cyan
    }
    *h *= OFXS_HUE_CIRCLE / 6;
    if (*h < 0) {
        *h += OFXS_HUE_CIRCLE;
    }
}

// r,g,b values are linear values from 0 to 1
// h = [0,OFXS_HUE_CIRCLE], s = [0,1], v = [0,1]
//		if s == 0, then h = 0 (undefined)
// Reference:
// "Color gamut transform pairs", Alvy Ray Smith, Proceeding SIGGRAPH '78
// https://doi.org/10.1145/800248.807361
// http://www.icst.pku.edu.cn/F/course/ImageProcessing/2018/resource/Color78.pdf
void
hsv_to_rgb(float h,
           float s,
           float v,
           float *r,
           float *g,
           float *b,
           bool wrapHue)
{
    if (wrapHue) {
        wrapHueToCircle(&h);
    }
    if (s == 0) {
        // achromatic (grey)
        *r = *g = *b = v;

        return;
    }

    h *= 6. / OFXS_HUE_CIRCLE;            // sector 0 to 5
    float f;          // factorial part of h
    int i = hueSector(h, &f);
    float p = v * ( 1 - s );
    float q = v * ( 1 - s * f );
    float t = v * ( 1 - s * ( 1 - f ) );

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

// The hue of rgb_to_hsl, in [0, OFXS_HUE_CIRCLE), from the channel maxv that is the largest one and
// the difference delta (> 0) between the largest and the smallest one.
static inline float
hslHue(float r,
       float g,
       float b,
       float maxv,
       float delta)
{
    float h;

    if (r == maxv) {
        h = (g - b) / delta;                       // between yellow & magenta
    } else if (g == maxv) {
        h = 2 + (b - r) / delta;                   // between cyan & yellow
    } else {
        h = 4 + (r - g) / delta;                   // between magenta & cyan
    }
    h *= OFXS_HUE_CIRCLE / 6;
    if (h < 0) {
        h += OFXS_HUE_CIRCLE;
    }

    return h;
}

void
rgb_to_hsl( float r,
            float g,
            float b,
            float *h,
            float *s,
            float *l,
            bool fixedOutOfRange )
{
    float minv = (std::min)((std::min)(r, g), b);
    float maxv = (std::max)((std::max)(r, g), b);

    *l = (minv + maxv) / 2;

    if (fixedOutOfRange) {
        // the hue is computed from the unclamped channels
        const float delta = maxv - minv;
        if ( !(delta > 0.f) ) { // NaN (e.g. inf - inf) is a grey too
            *h = 0.f;                 // gray
            *s = 0.f;

            return;
        }
        // the saturation is that of the colour clamped to [0,1] (clamp both ends to [0,1]), and the
        // lightness of the clamped colour decides the formula; deltac is 0 when all the channels are on
        // the same side of [0,1]. s = delta/(1-abs(2L-1)), 1 if the denominator rounds to 0 (see below).
        const float minc = (std::min)( 1.f, (std::max)(0.f, minv) );
        const float maxc = (std::min)( 1.f, (std::max)(0.f, maxv) );
        const float deltac = maxc - minc;
        const float lc = (minc + maxc) / 2;
        const float sdenc = (lc <= 0.5f) ? (maxc + minc) : ( 2 - (maxc + minc) );
        *s = (deltac == 0.f) ? 0.f : ( (sdenc != 0.f) ? (std::min)(1.f, deltac / sdenc) : 1.f );
        *h = hslHue(r, g, b, maxv, delta);

        return;
    }

    // old output: comparing the unclamped channels with the clamped maxv below gives a
    // wrong hue when a channel is outside [0,1] (e.g. (2, 0.5, 0) or (0.5, -1, 0)), and
    // a wrong saturation when all are above 1
    minv = (std::max)(0.f, minv);
    maxv = (std::min)(1.f, maxv);

    const float delta = maxv - minv;

    if (delta == 0.) {
        *h = 0.f;                 // gray
        *s = 0.f;

        return;
    }
    // s = delta/(1-abs(2L-1)); near white, 2 - (maxv + minv) can round to 0 (e.g. (1, 1, 0.99999994)),
    // which gave inf, then NaN in hsl_to_rgb: the saturation is then 1
    const float sden = (*l <= 0.5) ? (maxv + minv) : ( 2 - (maxv + minv) );
    *s = (sden != 0.f) ? (delta / sden) : 1.f;
    *h = hslHue(r, g, b, maxv, delta);
}

void
hsl_to_rgb(float h,
           float s,
           float l,
           float *r,
           float *g,
           float *b,
           bool wrapHue)
{
    if (wrapHue) {
        wrapHueToCircle(&h);
    }
    if (s == 0) {
        // achromatic (grey)
        *r = *g = *b = l;

        return;
    }

    h *= 6.f / OFXS_HUE_CIRCLE;            // sector 0 to 5
    float f;          // factorial part of h
    int i = hueSector(h, &f);
    float v = (l <= 0.5f) ? ( l * (1.0f + s) ) : (l + s - l * s);
    float p = l + l - v;
    // v == 0 (l == 0, or s == -1) would give 0/0 or x/0, then NaN in t or q
    float sv = (v != 0.f) ? (v - p ) / v : 0.f;
    float vsf = v * sv * f;
    float t = p + vsf;
    float q = v - vsf;

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
} // hsl_to_rgb

//! Convert pixel values from RGB to HSI color spaces.
void
rgb_to_hsi( float r,
            float g,
            float b,
            float *h,
            float *s,
            float *i )
{
    float nR = r; //(r < 0 ? 0 : (r > 1. ? 1. : r));
    float nG = g; //(g < 0 ? 0 : (g > 1. ? 1. : g));
    float nB = b; //(b < 0 ? 0 : (b > 1. ? 1. : b));
    float m = (std::min)((std::min)(nR, nG), nB);
    // the hue angle; 0 for a gray pixel (den2 == 0 would give 0/0), and the cosine is clamped
    // to [-1, 1], which rounding can exceed (acos would give NaN)
    float theta = 0.f;
    const float den2 = (nR - nG) * (nR - nG) + (nR - nB) * (nG - nB);
    if (den2 > 0.f) {
        float c = 0.5f * ( (nR - nG) + (nR - nB) ) / std::sqrt(den2);
        c = (c > 1.f) ? 1.f : ( (c < -1.f) ? -1.f : c );
        theta = (float)(std::acos(c) * (OFXS_HUE_CIRCLE / 2) / OFXS_PI);
    }
    float sum = nR + nG + nB;

    if (theta > 0) {
        *h = (nB <= nG) ? theta : (OFXS_HUE_CIRCLE - theta);
    } else {
        *h = 0.;
    }
    if (sum > 0) {
        // m == 0 gives 1 (as 1 - 3 / sum * 0 does), also when sum is so small (subnormal) that
        // 3 / sum overflows to inf, which gave inf * 0 = NaN
        *s = (m == 0.f) ? 1.f : (1 - 3 / sum * m);
    } else {
        *s = 0.;
    }
    *i = sum / 3;
}

void
hsi_to_rgb(float h,
           float s,
           float i,
           float *r,
           float *g,
           float *b,
           bool wrapHue)
{
    float a = i * (1 - s);

    if ( wrapHue && ( (h < 0) || (h > OFXS_HUE_CIRCLE) ) ) {
        // a hue outside the circle gave a non-periodic result, and a division by ~0 (inf) near
        // h = -1/12 and 13/12: wrap it. Opt-in, since it changes existing out-of-range outputs.
        // A non-finite hue gives hue 0, as in hsv_to_rgb and hsl_to_rgb (inf - inf was NaN).
        h = ( std::fabs(h) <= FLT_MAX ) ? ( h - OFXS_HUE_CIRCLE * std::floor(h / OFXS_HUE_CIRCLE) ) : 0.f;
    }

    if ( h < (OFXS_HUE_CIRCLE / 3) ) {
        *b = a;
        *r = (float)( i * ( 1 + s * std::cos( h * OFXS_PI / (OFXS_HUE_CIRCLE / 2) ) / std::cos( ( (OFXS_HUE_CIRCLE / 6) - h ) * OFXS_PI / (OFXS_HUE_CIRCLE / 2) ) ) );
        *g = 3 * i - (*r + *b);
    } else if ( h < (OFXS_HUE_CIRCLE * 2 / 3) ) {
        h -= OFXS_HUE_CIRCLE / 3;
        *r = a;
        *g = (float)( i * ( 1 + s * std::cos( h * OFXS_PI / (OFXS_HUE_CIRCLE / 2) ) / std::cos( ( (OFXS_HUE_CIRCLE / 6) - h ) * OFXS_PI / (OFXS_HUE_CIRCLE / 2) ) ) );
        *b = 3 * i - (*r + *g);
    } else {
        h -= OFXS_HUE_CIRCLE * 2 / 3;
        *g = a;
        *b = (float)( i * ( 1 + s * std::cos( h * OFXS_PI / (OFXS_HUE_CIRCLE / 2) ) / std::cos( ( (OFXS_HUE_CIRCLE / 6) - h ) * OFXS_PI / (OFXS_HUE_CIRCLE / 2) ) ) );
        *r = 3 * i - (*g + *b);
    }
} // hsi_to_rgb

// R'G'B' in the range 0-1 to Y'CbCr in the video range
// (Y' = 16/255 to 235/255, CbCr = 16/255 to 240/255)
void
rgb_to_ycbcr601(float r,
                float g,
                float b,
                float *y,
                float *cb,
                float *cr)
{
    /// ref: CImg (BT.601)
    //*y  = ((255*(66*r + 129*g + 25*b) + 128)/256 + 16)/255;
    //*cb = ((255*(-38*r - 74*g + 112*b) + 128)/256 + 128)/255,
    //*cr = ((255*(112*r - 94*g - 18*b) + 128)/256 + 128)/255;

    /// ref: http://www.equasys.de/colorconversion.html (BT.601)
    /// also http://www.intersil.com/data/an/AN9717.pdf
    *y  =  0.257f * r + 0.504f * g + 0.098f * b + 16 / 255.f;
    *cb = -0.148f * r - 0.291f * g + 0.439f * b + 128 / 255.f;
    *cr =  0.439f * r - 0.368f * g - 0.071f * b + 128 / 255.f;
}

// Y'CbCr in the video range (Y' = 16/255 to 235/255, CbCr = 16/255 to 240/255)
// to R'G'B' in the range 0-1
void
ycbcr_to_rgb601(float y,
                float cb,
                float cr,
                float *r,
                float *g,
                float *b)
{
    /// ref: CImg (BT.601)
    //y  = y * 255 - 16;
    //cb = cb * 255 - 128;
    //cr = cr * 255 - 128;
    //*r = (298 * y + 409 * cr + 128)/256/255;
    //*g = (298 * y - 100 * cb - 208 * cr + 128)/256/255;
    //*b = (298 * y + 516 * cb + 128)/256/255;

    /// ref: http://www.equasys.de/colorconversion.html (BT.601)
    /// also http://www.intersil.com/data/an/AN9717.pdf
    *r = 1.164f * (y - 16 / 255.f) + 1.596f * (cr - 128 / 255.f);
    *g = 1.164f * (y - 16 / 255.f) - 0.813f * (cr - 128 / 255.f) - 0.392f * (cb - 128 / 255.f);
    *b = 1.164f * (y - 16 / 255.f) + 2.017f * (cb - 128 / 255.f);
} // ycbcr_to_rgb601

// R'G'B' in the range 0-1 to Y'CbCr in the video range
// (Y' = 16/255 to 235/255, CbCr = 16/255 to 240/255)
void
rgb_to_ycbcr709(float r,
                float g,
                float b,
                float *y,
                float *cb,
                float *cr)
{
    // ref: http://www.poynton.com/PDFs/coloureq.pdf (BT.709)
    //*y  =  0.2215 * r +0.7154 * g +0.0721 * b;
    //*cb = -0.1145 * r -0.3855 * g +0.5000 * b + 128./255;
    //*cr =  0.5016 * r -0.4556 * g -0.0459 * b + 128./255;

    // ref: http://www.equasys.de/colorconversion.html (BT.709)
    *y  =  0.183f * r + 0.614f * g + 0.062f * b + 16 / 255.f;
    *cb = -0.101f * r - 0.339f * g + 0.439f * b + 128 / 255.f;
    *cr =  0.439f * r - 0.399f * g - 0.040f * b + 128 / 255.f;
}

// Y'CbCr in the video range (Y' = 16/255 to 235/255, CbCr = 16/255 to 240/255)
// to R'G'B' in the range 0-1
void
ycbcr_to_rgb709(float y,
                float cb,
                float cr,
                float *r,
                float *g,
                float *b)
{
    // ref: http://www.equasys.de/colorconversion.html (BT.709)
    *r = 1.164f * (y - 16 / 255.f) + 1.793f * (cr - 128 / 255.f);
    *g = 1.164f * (y - 16 / 255.f) - 0.533f * (cr - 128 / 255.f) - 0.213f * (cb - 128 / 255.f);
    *b = 1.164f * (y - 16 / 255.f) + 2.112f * (cb - 128 / 255.f);
} // ycbcr_to_rgb709

// R'G'B' in the range 0-1 to Y'CbCr Analog (Y' in the range 0-1, PbPr in the range -0.5 - 0.5)
void
rgb_to_ypbpr601(float r,
                float g,
                float b,
                float *y,
                float *pb,
                float *pr)
{
    // ref: https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
    // also http://www.equasys.de/colorconversion.html (BT.601)
    // and http://public.kitware.com/vxl/doc/release/core/vil/html/vil__colour__space_8cxx_source.html
    //*y  =  0.299f    * r + 0.587f    * g + 0.114f * b;
    //*pb = -0.168736f * r - 0.331264f * g + 0.500f * b;
    //*pr =  0.500f    * r - 0.418688f * g - 0.081312f * b;

#define Kb 0.114f
#define Kr 0.299f
    *y  =  Kr * r + (1 - Kr - Kb) * g + Kb * b;
    *pb =  (b - *y) / ( 2 * (1 - Kb) );
    *pr =  (r - *y) / ( 2 * (1 - Kr) );
}

// Y'CbCr Analog (Y' in the range 0-1, PbPr in the range -0.5 - 0.5) to R'G'B' in the range 0-1
void
ypbpr_to_rgb601(float y,
                float pb,
                float pr,
                float *r,
                float *g,
                float *b)
{
    // https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
    // also ref: http://www.equasys.de/colorconversion.html (BT.601)
    // and http://public.kitware.com/vxl/doc/release/core/vil/html/vil__colour__space_8cxx_source.html
    //*r = y                + 1.402f    * pr,
    //*g = y - 0.344136 * pb - 0.714136f * pr;
    //*b = y + 1.772f   * pb;

    *b = pb * ( 2 * (1 - Kb) ) + y;
    *r = pr * ( 2 * (1 - Kr) ) + y;
    *g = (y - Kr * *r - Kb * *b) / (1 - Kr - Kb);
#undef Kb
#undef Kr
} // ypbpr_to_rgb601

// R'G'B' in the range 0-1 to Y'CbCr Analog (Y' in the range 0-1, PbPr in the range -0.5 - 0.5)
void
rgb_to_ypbpr709(float r,
                float g,
                float b,
                float *y,
                float *pb,
                float *pr)
{
    // ref: http://www.equasys.de/colorconversion.html (BT.709)
    //*y  =  0.2126f * r + 0.7152f * g + 0.0722f * b;
    //*pb = -0.115f  * r - 0.385f  * g + 0.500f  * b; // or (b - y)/1.8556
    //*pr =  0.500f  * r - 0.454f  * g - 0.046f  * b; // or (r - y)/1.5748

    // ref: http://www.poynton.com/PDFs/coloureq.pdf (10.5)
    //*y  =  0.2215f * r + 0.7154f * g + 0.0721f * b;
    //*pb = -0.1145f * r - 0.3855f * g + 0.5000f  * b;
    //*pr =  0.5016f * r - 0.4556f * g - 0.0459f  * b;

    //*y  =  0.2126390058 * r + 0.7151686783 * g + 0.07219231534 * b;
    //*pb =  (b - *y) / 1.8556;
    //*pr =  (r - *y) / 1.5748;
#define Kb 0.07219231534f
#define Kr 0.2126390058f
    *y  =  Kr * r + (1 - Kr - Kb) * g + Kb * b;
    *pb =  (b - *y) / ( 2 * (1 - Kb) );
    *pr =  (r - *y) / ( 2 * (1 - Kr) );
}

// Y'CbCr Analog (Y in the range 0-1, PbPr in the range -0.5 - 0.5) to R'G'B' in the range 0-1
void
ypbpr_to_rgb709(float y,
                float pb,
                float pr,
                float *r,
                float *g,
                float *b)
{
    // ref: http://www.equasys.de/colorconversion.html (BT.709)
    //*r = y               + 1.575f * pr,
    //*g = y - 0.187f * pb - 0.468f * pr;
    //*b = y + 1.856f * pb;

    // ref: http://www.poynton.com/PDFs/coloureq.pdf (10.5)
    // (there is a sign error on the R' coeff for Cr in Poynton's doc)
    //*r = y                + 1.5701f * pr,
    //*g = y - 0.1870f * pb - 0.4664f * pr;
    //*b = y + 1.8556f * pb;

    //*b = pb * 1.8556 + y;
    //*r = pr * 1.5748 + y;
    ////*g = (y - 0.2126f * *r - 0.0722f * *b) / 0.7152f;
    //*g = (y - 0.2126390058 * *r - 0.07219231534 * *b) / 0.7151686783;
    *b = pb * ( 2 * (1 - Kb) ) + y;
    *r = pr * ( 2 * (1 - Kr) ) + y;
    *g = (y - Kr * *r - Kb * *b) / (1 - Kr - Kb);
#undef Kb
#undef Kr
} // ypbpr_to_rgb709

// R'G'B' in the range 0-1 to Y'CbCr Analog (Y' in the range 0-1, PbPr in the range -0.5 - 0.5)
void
rgb_to_ypbpr2020(float r,
                 float g,
                 float b,
                 float *y,
                 float *pb,
                 float *pr)
{
    // ref: https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2020-0-201208-S!!PDF-E.pdf
    // (Rec.2020, table 4 p4)
    //
    //*y  =  0.2627f * r + 0.6780f * g + 0.0593f * b;
    //*pb =  (b - *y) / 1.8814;
    //*pr =  (r - *y) / 1.4746;

    // ref: http://www.poynton.com/PDFs/coloureq.pdf (10.5)
    //*y  =  0.2215f * r + 0.7154f * g + 0.0721f * b;
    //*pb = -0.1145f * r - 0.3855f * g + 0.5000f  * b;
    //*pr =  0.5016f * r - 0.4556f * g - 0.0459f  * b;
#define Kb 0.0593f
#define Kr 0.2627f
    *y  =  Kr * r + (1 - Kr - Kb) * g + Kb * b;
    *pb =  (b - *y) / ( 2 * (1 - Kb) );
    *pr =  (r - *y) / ( 2 * (1 - Kr) );
}

// Y'CbCr Analog (Y in the range 0-1, PbPr in the range -0.5 - 0.5) to R'G'B' in the range 0-1
void
ypbpr_to_rgb2020(float y,
                 float pb,
                 float pr,
                 float *r,
                 float *g,
                 float *b)
{
    // ref: https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2020-0-201208-S!!PDF-E.pdf
    // (Rec.2020, table 4 p4)
    //
    //*b = pb * 1.8814 + y;
    //*r = pr * 1.4746 + y;
    //*g = (y - 0.2627f * *r - 0.0593f * *b) / 0.6780f;
    *b = pb * ( 2 * (1 - Kb) ) + y;
    *r = pr * ( 2 * (1 - Kr) ) + y;
    *g = (y - Kr * *r - Kb * *b) / (1 - Kr - Kb);
#undef Kb
#undef Kr
} // ypbpr_to_rgb2020

// R'G'B' in the range 0-1 to Y'UV (Y' in the range 0-1, U in the range -0.436 - 0.436,
// V in the range -0.615 - 0.615)
void
rgb_to_yuv601(float r,
              float g,
              float b,
              float *y,
              float *u,
              float *v)
{
    /// ref: https://en.wikipedia.org/wiki/YUV#SDTV_with_BT.601
    *y =  0.299f   * r + 0.587f   * g + 0.114f  * b;
    *u = -0.14713f * r - 0.28886f * g + 0.436f  * b;
    *v =  0.615f   * r - 0.51499f * g - 0.10001f * b;
}

// Y'UV (Y' in the range 0-1, U in the range -0.436 - 0.436,
// V in the range -0.615 - 0.615) to R'G'B' in the range 0-1
void
yuv_to_rgb601(float y,
              float u,
              float v,
              float *r,
              float *g,
              float *b)
{
    /// ref: https://en.wikipedia.org/wiki/YUV#SDTV_with_BT.601
    *r = y                + 1.13983f * v;
    *g = y - 0.39465f * u - 0.58060f * v;
    *b = y + 2.03211f * u;
} // yuv_to_rgb601

// R'G'B' in the range 0-1 to Y'UV (Y' in the range 0-1, U in the range -0.436 - 0.436,
// V in the range -0.615 - 0.615)
void
rgb_to_yuv709(float r,
              float g,
              float b,
              float *y,
              float *u,
              float *v)
{
    /// ref: https://en.wikipedia.org/wiki/YUV#HDTV_with_BT.709
    *y =  0.2126f  * r + 0.7152f  * g + 0.0722f  * b;
    *u = -0.09991f * r - 0.33609f * g + 0.436f   * b;
    *v =  0.615f   * r - 0.55861f * g - 0.05639f * b;
}

// Y'UV (Y in the range 0-1, U in the range -0.436 - 0.436,
// V in the range -0.615 - 0.615) to R'G'B' in the range 0-1
void
yuv_to_rgb709(float y,
              float u,
              float v,
              float *r,
              float *g,
              float *b)
{
    /// ref: https://en.wikipedia.org/wiki/YUV#HDTV_with_BT.709
    *r = y               + 1.28033f * v;
    *g = y - 0.21482f * u - 0.38059f * v;
    *b = y + 2.12798f * u;
} // yuv_to_rgb709

static inline
float
labf(float x)
{
    return ( (x) >= 0.008856f ? ( std::pow(x, (float)1 / 3) ) : (7.787f * x + 16.0f / 116) );
}

// Convert pixel values from XYZ to Lab color spaces.
// Uses the standard D65 white point.
void
xyz_to_lab(float x,
           float y,
           float z,
           float *l,
           float *a,
           float *b)
{
    const float fx = labf( x / (0.412453f + 0.357580f + 0.180423f) );
    const float fy = labf( y / (0.212671f + 0.715160f + 0.072169f) );
    const float fz = labf( z / (0.019334f + 0.119193f + 0.950227f) );

    *l = 116 * fy - 16;
    *a = 500 * (fx - fy);
    *b = 200 * (fy - fz);
}

static inline
float
labfi(float x)
{
    return ( x >= 0.206893f ? (x * x * x) : ( (x - 16.0f / 116) / 7.787f ) );
}

// Convert pixel values from Lab to XYZ color spaces.
// Uses the standard D65 white point.
void
lab_to_xyz(float l,
           float a,
           float b,
           float *x,
           float *y,
           float *z)
{
    const float cy = (l + 16) / 116;

    *y = (0.212671f + 0.715160f + 0.072169f) * labfi(cy);
    const float cx = a / 500 + cy;
    *x = (0.412453f + 0.357580f + 0.180423f) * labfi(cx);
    const float cz = cy - b / 200;
    *z = (0.019334f + 0.119193f + 0.950227f) * labfi(cz);
}

// Convert pixel values from RGB to Lab color spaces.
// Uses the standard D65 white point.
void
rgb709_to_lab(float r,
              float g,
              float b,
              float *l,
              float *a,
              float *b_)
{
    float x, y, z;

    rgb709_to_xyz(r, g, b, &x, &y, &z);
    xyz_to_lab(x, y, z, l, a, b_);
}

// Convert pixel values from RGB to Lab color spaces.
// Uses the standard D65 white point.
void
lab_to_rgb709(float l,
              float a,
              float b,
              float *r,
              float *g,
              float *b_)
{
    float x, y, z;

    lab_to_xyz(l, a, b, &x, &y, &z);
    xyz_to_rgb709(x, y, z, r, g, b_);
}
}         // namespace Color
} //namespace OFX

