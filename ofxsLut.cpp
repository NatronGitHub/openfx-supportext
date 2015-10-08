/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of openfx-supportext <https://github.com/devernay/openfx-supportext>,
 * Copyright (C) 2015 INRIA
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
#ifdef _WIN32
typedef unsigned __int32 uint32_t;
typedef unsigned char uint8_t;
#else
#include <stdint.h>
#endif
#include <limits>
#include <cmath>

#ifndef M_PI
#define M_PI        3.14159265358979323846264338327950288   /* pi             */
#endif

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
    float min = std::min(std::min(r, g), b);
    float max = std::max(std::max(r, g), b);
    *v = max;                               // v

    float delta = max - min;

    if (max != 0.) {
        *s = delta / max;                       // s
    } else {
        // r = g = b = 0		// s = 0, v is undefined
        *s = 0.f;
        *h = 0.f;

        return;
    }

    if (delta == 0.) {
        *h = 0.f;                 // gray
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
    if (s == 0) {
        // achromatic (grey)
        *r = *g = *b = v;

        return;
    }

    h /= 60;            // sector 0 to 5
    int i = std::floor(h);
    float f = h - i;          // factorial part of h
    i = (i >= 0) ? (i % 6) : (i % 6) + 6; // take h modulo 360
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

void
rgb_to_hsl( float r,
            float g,
            float b,
            float *h,
            float *s,
            float *l )
{
    float min = std::min(std::min(r, g), b);
    float max = std::max(std::max(r, g), b);
    *l = (min + max)/2;

    float delta = max - min;

    if (max != 0.) {
        *s = (*l <= 0.5) ? (delta/(max+min)) : (delta/(2-max-min)); // s = delta/(1-abs(2L-1))
    } else {
        // r = g = b = 0		// s = 0
        *s = 0.f;
        *h = 0.f;

        return;
    }

    if (delta == 0.) {
        *h = 0.f;                 // gray
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
hsl_to_rgb(float h,
           float s,
           float l,
           float *r,
           float *g,
           float *b)
{
    if (s == 0) {
        // achromatic (grey)
        *r = *g = *b = l;

        return;
    }

    h /= 60;            // sector 0 to 5
    int i = std::floor(h);
    float f = h - i;          // factorial part of h
    i = (i >= 0) ? (i % 6) : (i % 6) + 6; // take h modulo 360
    float v = (l <= 0.5f) ? (l * (1.0f + s)) : (l + s - l * s);
    float p = l + l - v;
    float sv = (v - p ) / v;
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
    float m =std::min(std::min(nR,nG),nB);
    float theta = (float)(std::acos(0.5f*((nR-nG)+(nR-nB))/std::sqrt(std::max(0.f,(nR-nG)*(nR-nG)+(nR-nB)*(nG-nB))))*180/M_PI);
    float sum = nR + nG + nB;
    if (theta > 0) {
        *h = (nB<=nG) ? theta : (360 - theta);
    } else {
        *h = 0.;
    }
    if (sum > 0) {
        *s = 1 - 3/sum*m;
    } else {
        *s = 0.;
    }
    *i = sum/3;

}

void
hsi_to_rgb(float h,
           float s,
           float i,
           float *r,
           float *g,
           float *b)
{
    float a = i * (1 - s);
    if (h < 120) {
        *b = a;
        *r = (float)(i*(1 + s * std::cos(h * M_PI/180)/std::cos((60-h) * M_PI/180)));
        *g = 3 * i-(*r +*b);
    } else if (h < 240) {
        h -= 120;
        *r = a;
        *g = (float)(i * (1 + s * std::cos(h * M_PI/180)/std::cos((60 - h) * M_PI/180)));
        *b = 3 * i - (*r + *g);
    } else {
        h -= 240;
        *g = a;
        *b = (float)(i * (1 + s * std::cos(h * M_PI/180)/std::cos((60 - h) * M_PI/180)));
        *r = 3 * i - (*g + *b);
    }
} // hsi_to_rgb

void
rgb_to_ycbcr(float r,
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

    /// ref: http://www.poynton.com/PDFs/coloureq.pdf (BT.709)
    *y  =  0.2215 * r +0.7154 * g +0.0721 * b;
    *cb = -0.1145 * r -0.3855 * g +0.5000 * b + 128./255;
    *cr =  0.5016 * r -0.4556 * g -0.0459 * b + 128./255;
}

void
ycbcr_to_rgb(float y,
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

    /// ref: http://www.poynton.com/PDFs/coloureq.pdf (BT.709)
    /// that document has the wrong sign for 1.8556
    *r = y +0.0000 * (cb - 128./255) +1.5701 * (cr - 128./255);
    *g = y -0.1870 * (cb - 128./255) -0.4664 * (cr - 128./255);
    *b = y +1.8556 * (cb - 128./255) +0.0000 * (cr - 128./255);
} // ycbcr_to_rgb

void
rgb_to_yuv(float r,
           float g,
           float b,
           float *y,
           float *u,
           float *v)
{
    /// ref: https://en.wikipedia.org/wiki/YUV#SDTV_with_BT.601
    //*y =  0.299f   * r +0.587f   * g +0.114f  * b;
    //*u = -0.14713f * r -0.28886f * g +0.114f  * b;
    //*v =  0.615f   * r -0.51499f * g -0.10001 * b;

    /// ref: https://en.wikipedia.org/wiki/YUV#HDTV_with_BT.709
    *y =  0.2126f  * r +0.7152f  * g +0.0722f  * b;
    *u = -0.09991f * r -0.33609f * g +0.436f   * b;
    *v =  0.615f   * r -0.55861f * g -0.05639f * b;
}

void
yuv_to_rgb(float y,
           float u,
           float v,
           float *r,
           float *g,
           float *b)
{
    /// ref: https://en.wikipedia.org/wiki/YUV#SDTV_with_BT.601
    //*r = y                + 1.13983f * v,
    //*g = y - 0.39465f * u - 0.58060f * v;
    //*b = y + 2.03211f * u;

    /// ref: https://en.wikipedia.org/wiki/YUV#HDTV_with_BT.709
    *r = y               +1.28033f * v,
    *g = y -0.21482f * u -0.38059f * v;
    *b = y +2.12798f * u;
} // yuv_to_rgb

// r,g,b values are from 0 to 1
// Convert pixel values from RGB to XYZ_709 color spaces.
// Uses the standard D65 white point.
void
rgb_to_xyz_rec709(float r,
                  float g,
                  float b,
                  float *x,
                  float *y,
                  float *z)
{
    *x = 0.412453f*r +0.357580f*g +0.180423f*b;
    *y = 0.212671f*r +0.715160f*g +0.072169f*b;
    *z = 0.019334f*r +0.119193f*g +0.950227f*b;
}

// Convert pixel values from XYZ_709 to RGB color spaces.
// Uses the standard D65 white point.
void
xyz_rec709_to_rgb(float x,
                  float y,
                  float z,
                  float *r,
                  float *g,
                  float *b)
{
    *r =  3.240479f*x -1.537150f*y -0.498535f*z;
    *g = -0.969256f*x +1.875992f*y +0.041556f*z;
    *b =  0.055648f*x -0.204043f*y +1.057311f*z;
}

static inline
float labf(float x)
{
    return ((x)>=0.008856f ? (std::pow(x,(float)1/3)):(7.787f*x+16.0f/116));
}

// Convert pixel values from XYZ_709 to Lab color spaces.
// Uses the standard D65 white point.
void
xyz_rec709_to_lab(float x,
                  float y,
                  float z,
                  float *l,
                  float *a,
                  float *b)
{
    const float fx = labf(x/(0.412453f + 0.357580f + 0.180423f));
    const float fy = labf(y/(0.212671f + 0.715160f + 0.072169f));
    const float fz = labf(z/(0.019334f + 0.119193f + 0.950227f));
    *l = 116*fy - 16;
    *a = 500*(fx - fy);
    *b = 200*(fy - fz);
}

static inline
float labfi(float x)
{
    return (x >= 0.206893f ? (x * x * x) : ((x-16.0f/116)/7.787f));
}

// Convert pixel values from Lab to XYZ_709 color spaces.
// Uses the standard D65 white point.
void
lab_to_xyz_rec709(float l,
                  float a,
                  float b,
                  float *x,
                  float *y,
                  float *z)
{
    const float cy = (l+16)/116;
    *y = (0.212671f + 0.715160f + 0.072169f) * labfi(cy);
    const float cx = a/500 + cy;
    *x = (0.412453f + 0.357580f + 0.180423f) * labfi(cx);
    const float cz = cy - b/200;
    *z = (0.019334f + 0.119193f + 0.950227f) * labfi(cz);
}

// Convert pixel values from RGB to Lab color spaces.
// Uses the standard D65 white point.
void
rgb_to_lab(float r,
           float g,
           float b,
           float *l,
           float *a,
           float *b_)
{
    float x, y, z;
    rgb_to_xyz_rec709(r, g, b, &x, &y, &z);
    xyz_rec709_to_lab(x, y, z, l, a, b_);
}

// Convert pixel values from RGB to Lab color spaces.
// Uses the standard D65 white point.
void
lab_to_rgb(float l,
           float a,
           float b,
           float *r,
           float *g,
           float *b_)
{
    float x, y, z;
    lab_to_xyz_rec709(l, a, b, &x, &y, &z);
    xyz_rec709_to_rgb(x, y, z, r, g, b_);
}


}         // namespace Color
} //namespace OFX

