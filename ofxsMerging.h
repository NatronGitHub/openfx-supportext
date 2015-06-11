/*
   OFX Merge helpers

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

#ifndef Misc_Merging_helper_h
#define Misc_Merging_helper_h

#include <cmath>
#include <cfloat>
#include "ofxsImageEffect.h"

#ifndef M_PI
#define M_PI        3.14159265358979323846264338327950288   /* pi             */
#endif

namespace OFX {
// References:
//
// SVG Compositing Specification:
//   http://www.w3.org/TR/SVGCompositing/
// PDF Reference v1.7:
//   http://www.adobe.com/content/dam/Adobe/en/devnet/acrobat/pdfs/pdf_reference_1-7.pdf
//   http://www.adobe.com/devnet/pdf/pdf_reference_archive.html
// Adobe photoshop blending modes:
//   http://helpx.adobe.com/en/photoshop/using/blending-modes.html
//   http://www.deepskycolors.com/archive/2010/04/21/formulas-for-Photoshop-blending-modes.html
// ImageMagick:
//   http://www.imagemagick.org/Usage/compose/
//
// Note about the Soft-Light operation:
// Soft-light as implemented in Nuke comes from the SVG 2004 specification, which is wrong.
// In SVG 2004, 'Soft_Light' did not work as expected, producing a brightening for any non-gray shade
// image overlay.
// It was fixed in the March 2009 SVG specification, which was used for this implementation.

namespace MergeImages2D {
enum MergingFunctionEnum
{
    eMergeATop = 0,
    eMergeAverage,
    eMergeColorBurn,
    eMergeColorDodge,
    eMergeConjointOver,
    eMergeCopy,
    eMergeDifference,
    eMergeDisjointOver,
    eMergeDivide,
    eMergeExclusion,
    eMergeFreeze,
    eMergeFrom,
    eMergeGeometric,
    eMergeHardLight,
    eMergeHypot,
    eMergeIn,
    eMergeInterpolated,
    eMergeMask,
    eMergeMatte,
    eMergeLighten,
    eMergeDarken,
    eMergeMinus,
    eMergeMultiply,
    eMergeOut,
    eMergeOver,
    eMergeOverlay,
    eMergePinLight,
    eMergePlus,
    eMergeReflect,
    eMergeScreen,
    eMergeSoftLight,
    eMergeStencil,
    eMergeUnder,
    eMergeXOR,
    eMergeHue,
    eMergeSaturation,
    eMergeColor,
    eMergeLuminosity
};

inline bool
isMaskable(MergingFunctionEnum operation)
{
    switch (operation) {
    case eMergeAverage:
    case eMergeColorBurn:
    case eMergeColorDodge:
    case eMergeDifference:
    case eMergeDivide:
    case eMergeExclusion:
    case eMergeFrom:
    case eMergeFreeze:
    case eMergeGeometric:
    case eMergeHardLight:
    case eMergeHypot:
    case eMergeInterpolated:
    case eMergeLighten:
    case eMergeDarken:
    case eMergeMinus:
    case eMergeMultiply:
    case eMergeOverlay:
    case eMergePinLight:
    case eMergePlus:
    case eMergeReflect:
    case eMergeSoftLight:

        return true;
    case eMergeATop:
    case eMergeConjointOver:
    case eMergeCopy:
    case eMergeDisjointOver:
    case eMergeIn:
    case eMergeMask:
    case eMergeMatte:
    case eMergeOut:
    case eMergeOver:
    case eMergeScreen:
    case eMergeStencil:
    case eMergeUnder:
    case eMergeXOR:
    case eMergeHue:
    case eMergeSaturation:
    case eMergeColor:
    case eMergeLuminosity:

        return false;
    }

    return true;
}

// is the operator separable for R,G,B components, or do they have to be processed simultaneously?
inline bool
isSeparable(MergingFunctionEnum operation)
{
    switch (operation) {
    case eMergeHue:
    case eMergeSaturation:
    case eMergeColor:
    case eMergeLuminosity:

        return false;

    default:

        return true;
    }
}

inline std::string
getOperationString(MergingFunctionEnum operation)
{
    switch (operation) {
    case eMergeATop:

        return "atop";
    case eMergeAverage:

        return "average";
    case eMergeColorBurn:

        return "color-burn";
    case eMergeColorDodge:

        return "color-dodge";
    case eMergeConjointOver:

        return "conjoint-over";
    case eMergeCopy:

        return "copy";
    case eMergeDifference:

        return "difference";
    case eMergeDisjointOver:

        return "disjoint-over";
    case eMergeDivide:

        return "divide";
    case eMergeExclusion:

        return "exclusion";
    case eMergeFreeze:

        return "freeze";
    case eMergeFrom:

        return "from";
    case eMergeGeometric:

        return "geometric";
    case eMergeHardLight:

        return "hard-light";
    case eMergeHypot:

        return "hypot";
    case eMergeIn:

        return "in";
    case eMergeInterpolated:

        return "interpolated";
    case eMergeMask:

        return "mask";
    case eMergeMatte:

        return "matte";
    case eMergeLighten:

        return "max";
    case eMergeDarken:

        return "min";
    case eMergeMinus:

        return "minus";
    case eMergeMultiply:

        return "multiply";
    case eMergeOut:

        return "out";
    case eMergeOver:

        return "over";
    case eMergeOverlay:

        return "overlay";
    case eMergePinLight:

        return "pinlight";
    case eMergePlus:

        return "plus";
    case eMergeReflect:

        return "reflect";
    case eMergeScreen:

        return "screen";
    case eMergeSoftLight:

        return "soft-light";
    case eMergeStencil:

        return "stencil";
    case eMergeUnder:

        return "under";
    case eMergeXOR:

        return "xor";
    case eMergeHue:

        return "hue";
    case eMergeSaturation:

        return "saturation";
    case eMergeColor:

        return "color";
    case eMergeLuminosity:

        return "luminosity";
    } // switch

    return "unknown";
} // getOperationString

inline std::string
getOperationGroupString(MergingFunctionEnum operation)
{
    switch (operation) {
            // Porter Duff Compositing Operators
        case eMergeCopy: // aka source
        case eMergeOver:
        case eMergeIn:
        case eMergeOut:
        case eMergeATop:
        case eMergeUnder: // aka dest-over
        case eMergeMask: // aka dest-in
        case eMergeStencil: // aka dest-out
        case eMergeXOR:
        case eMergePlus:

            return "Operator";

            // Blend modes, see https://en.wikipedia.org/wiki/Blend_modes

            // Multiply and screen
        case eMergeMultiply:
        case eMergeScreen:
        case eMergeOverlay:
        case eMergeDarken:
        case eMergeLighten:

            return "Multiply & Screen";

            // Dodge and burn
        case eMergeColorDodge:
        case eMergeColorBurn:
        case eMergeHardLight:
        case eMergeSoftLight:
        case eMergePinLight:
        case eMergeDifference:
        case eMergeExclusion:
        case eMergeDivide:

            return "Dodge & Burn";

            // Nonseparable blend modes
        case eMergeHue:
        case eMergeSaturation:
        case eMergeColor:
        case eMergeLuminosity:

            return "HSL";

        case eMergeAverage:
        case eMergeConjointOver:
        case eMergeDisjointOver:
        case eMergeFreeze:
        case eMergeFrom:
        case eMergeGeometric:
        case eMergeHypot:
        case eMergeInterpolated:
        case eMergeMatte:
        case eMergeMinus:
        case eMergeReflect:
            
            return "Other";
    } // switch

    return "unknown";
} // getOperationGroupString

template <typename PIX>
PIX
averageFunctor(PIX A,
               PIX B)
{
    return (A + B) / 2;
}

template <typename PIX>
PIX
copyFunctor(PIX A,
            PIX /*B*/)
{
    return A;
}

template <typename PIX>
PIX
plusFunctor(PIX A,
            PIX B)
{
    return A + B;
}

template <typename PIX>
PIX
differenceFunctor(PIX A,
                  PIX B)
{
    return std::abs(A - B);
}

template <typename PIX>
PIX
divideFunctor(PIX A,
              PIX B)
{
    if (B <= 0) {
        return 0;
    }

    return A / B;
}

template <typename PIX,int maxValue>
PIX
exclusionFunctor(PIX A,
                 PIX B)
{
    return PIX(A + B - 2 * A * B / (double)maxValue);
}

template <typename PIX>
PIX
fromFunctor(PIX A,
            PIX B)
{
    return B - A;
}

template <typename PIX>
PIX
geometricFunctor(PIX A,
                 PIX B)
{
    return 2 * A * B / (A + B);
}

template <typename PIX,int maxValue>
PIX
multiplyFunctor(PIX A,
                PIX B)
{
    return PIX(A * B / (double)maxValue);
}

template <typename PIX,int maxValue>
PIX
screenFunctor(PIX A,
              PIX B)
{
    return PIX(A + B - A * B / (double)maxValue);
}

template <typename PIX,int maxValue>
PIX
hardLightFunctor(PIX A,
                 PIX B)
{
    if ( A < ( (double)maxValue / 2. ) ) {
        return PIX(2 * A * B / (double)maxValue);
    } else {
        return PIX(maxValue * ( 1. - 2 * (1. - A / (double)maxValue) * (1. - B / (double)maxValue) ));
    }
}

template <typename PIX,int maxValue>
PIX
softLightFunctor(PIX A,
                 PIX B)
{
    double An = A / (double)maxValue;
    double Bn = B / (double)maxValue;

    if (2 * An <= 1) {
        return PIX(maxValue * ( Bn - (1 - 2 * An) * Bn * (1 - Bn) ));
    } else if (4 * Bn <= 1) {
        return PIX(maxValue * ( Bn + (2 * An - 1) * (4 * Bn * (4 * Bn + 1) * (Bn - 1) + 7 * Bn) ));
    } else {
        return PIX(maxValue * ( Bn + (2 * An - 1) * (sqrt(Bn) - Bn) ));
    }
}

template <typename PIX>
PIX
hypotFunctor(PIX A,
             PIX B)
{
    return PIX(std::sqrt( (double)(A * A + B * B) ));
}

template <typename PIX>
PIX
minusFunctor(PIX A,
             PIX B)
{
    return A - B;
}

template <typename PIX>
PIX
darkenFunctor(PIX A,
              PIX B)
{
    return std::min(A,B);
}

template <typename PIX>
PIX
lightenFunctor(PIX A,
               PIX B)
{
    return std::max(A,B);
}

template <typename PIX,int maxValue>
PIX
overlayFunctor(PIX A,
               PIX B)
{
    double An = A / (double)maxValue;
    double Bn = B / (double)maxValue;

    if (2 * Bn <= 1.) {
        // multiply
        return PIX(maxValue * (2 * An * Bn));
    } else {
        // screen
        return PIX(maxValue * ( 1 - 2 * (1 - Bn) * (1 - An) ));
    }
}

template <typename PIX,int maxValue>
PIX
colorDodgeFunctor(PIX A,
                  PIX B)
{
    if (A >= maxValue) {
        return A;
    } else {
        return PIX(maxValue * std::min( 1., B / (maxValue - (double)A) ));
    }
}

template <typename PIX,int maxValue>
PIX
colorBurnFunctor(PIX A,
                 PIX B)
{
    if (A <= 0) {
        return A;
    } else {
        return PIX(maxValue * ( 1. - std::min(1., (maxValue - B) / (double)A) ));
    }
}

template <typename PIX,int maxValue>
PIX
pinLightFunctor(PIX A,
                PIX B)
{
    PIX max2 = PIX( (double)maxValue / 2. );

    return A >= max2 ? std::max(B,(A - max2) * 2) : std::min(B,A * 2);
}

template <typename PIX,int maxValue>
PIX
reflectFunctor(PIX A,
               PIX B)
{
    if (B >= maxValue) {
        return maxValue;
    } else {
        return PIX(std::min( (double)maxValue, A * A / (double)(maxValue - B) ));
    }
}

template <typename PIX,int maxValue>
PIX
freezeFunctor(PIX A,
              PIX B)
{
    if (B <= 0) {
        return 0;
    } else {
        double An = A / (double)maxValue;
        double Bn = B / (double)maxValue;

        return PIX(std::max( 0., maxValue * (1 - std::sqrt( std::max(0., 1. - An) ) / Bn) ));
    }
}

template <typename PIX,int maxValue>
PIX
interpolatedFunctor(PIX A,
                    PIX B)
{
    double An = A / (double)maxValue;
    double Bn = B / (double)maxValue;

    return PIX(maxValue * ( 0.5 - 0.25 * ( std::cos(M_PI * An) - std::cos(M_PI * Bn) ) ));
}

template <typename PIX,int maxValue>
PIX
atopFunctor(PIX A,
            PIX B,
            PIX alphaA,
            PIX alphaB)
{
    return PIX(A * alphaB / (double)maxValue + B * (1. - alphaA / (double)maxValue));
}

template <typename PIX,int maxValue>
PIX
conjointOverFunctor(PIX A,
                    PIX B,
                    PIX alphaA,
                    PIX alphaB)
{
    if (alphaA > alphaB) {
        return A;
    } else {
        return A + B * (maxValue - alphaA) / alphaB;
    }
}

template <typename PIX,int maxValue>
PIX
disjointOverFunctor(PIX A,
                    PIX B,
                    PIX alphaA,
                    PIX alphaB)
{
    if ( (alphaA + alphaB) < maxValue ) {
        return A + B;
    } else {
        return A + B * (maxValue - alphaA) / alphaB;
    }
}

template <typename PIX,int maxValue>
PIX
inFunctor(PIX A,
          PIX /*B*/,
          PIX /*alphaA*/,
          PIX alphaB)
{
    return PIX(A * alphaB / (double)maxValue);
}

template <typename PIX,int maxValue>
PIX
matteFunctor(PIX A,
             PIX B,
             PIX alphaA,
             PIX /*alphaB*/)
{
    return PIX(A * alphaA / (double)maxValue + B * (1. - alphaA / (double)maxValue));
}

template <typename PIX,int maxValue>
PIX
maskFunctor(PIX /*A*/,
            PIX B,
            PIX alphaA,
            PIX /*alphaB*/)
{
    return PIX(B * alphaA / (double)maxValue);
}

template <typename PIX,int maxValue>
PIX
outFunctor(PIX A,
           PIX /*B*/,
           PIX /*alphaA*/,
           PIX alphaB)
{
    return PIX(A * (1. - alphaB / (double)maxValue));
}

template <typename PIX,int maxValue>
PIX
overFunctor(PIX A,
            PIX B,
            PIX alphaA,
            PIX /*alphaB*/)
{
    return PIX(A + B * (1 - alphaA / (double)maxValue));
}

template <typename PIX,int maxValue>
PIX
stencilFunctor(PIX /*A*/,
               PIX B,
               PIX alphaA,
               PIX /*alphaB*/)
{
    return PIX(B * (1 - alphaA / (double)maxValue));
}

template <typename PIX,int maxValue>
PIX
underFunctor(PIX A,
             PIX B,
             PIX /*alphaA*/,
             PIX alphaB)
{
    return PIX(A * (1 - alphaB / (double)maxValue) + B);
}

template <typename PIX,int maxValue>
PIX
xorFunctor(PIX A,
           PIX B,
           PIX alphaA,
           PIX alphaB)
{
    return PIX(A * (1 - alphaB / (double)maxValue) + B * (1 - alphaA / (double)maxValue));
}

///////////////////////////////////////////////////////////////////////////////
//
// Code from pixman-combine-float.c
// START
/*
 * Copyright © 2010, 2012 Soren Sandmann Pedersen
 * Copyright © 2010, 2012 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Soren Sandmann Pedersen (sandmann@cs.au.dk)
 */
/*
 * PDF nonseperable blend modes are implemented using the following functions
 * to operate in Hsl space, with Cmax, Cmid, Cmin referring to the max, mid
 * and min value of the red, green and blue components.
 *
 * LUM (C) = 0.3 × Cred + 0.59 × Cgreen + 0.11 × Cblue
 *
 * clip_color (C):
 *     l = LUM (C)
 *     min = Cmin
 *     max = Cmax
 *     if n < 0.0
 *         C = l + (((C – l) × l) ⁄ (l – min))
 *     if x > 1.0
 *         C = l + (((C – l) × (1 – l) ) ⁄ (max – l))
 *     return C
 *
 * set_lum (C, l):
 *     d = l – LUM (C)
 *     C += d
 *     return clip_color (C)
 *
 * SAT (C) = CH_MAX (C) - CH_MIN (C)
 *
 * set_sat (C, s):
 *     if Cmax > Cmin
 *         Cmid = ( ( ( Cmid – Cmin ) × s ) ⁄ ( Cmax – Cmin ) )
 *         Cmax = s
 *     else
 *         Cmid = Cmax = 0.0
 *         Cmin = 0.0
 *     return C
 */

/* For premultiplied colors, we need to know what happens when C is
 * multiplied by a real number. LUM and SAT are linear:
 *
 *     LUM (r × C) = r × LUM (C)	SAT (r * C) = r * SAT (C)
 *
 * If we extend clip_color with an extra argument a and change
 *
 *     if x >= 1.0
 *
 * into
 *
 *     if x >= a
 *
 * then clip_color is also linear:
 *
 *     r * clip_color (C, a) = clip_color (r * C, r * a);
 *
 * for positive r.
 *
 * Similarly, we can extend set_lum with an extra argument that is just passed
 * on to clip_color:
 *
 *       r * set_lum (C, l, a)
 *
 *     = r × clip_color (C + l - LUM (C), a)
 *
 *     = clip_color (r * C + r × l - r * LUM (C), r * a)
 *
 *     = set_lum (r * C, r * l, r * a)
 *
 * Finally, set_sat:
 *
 *       r * set_sat (C, s) = set_sat (x * C, r * s)
 *
 * The above holds for all non-zero x, because the x'es in the fraction for
 * C_mid cancel out. Specifically, it holds for x = r:
 *
 *       r * set_sat (C, s) = set_sat (r * C, r * s)
 *
 */
typedef struct
{
    float	r;
    float	g;
    float	b;
} rgb_t;

inline bool
float_is_zero(float f) {
    return (-FLT_MIN < (f) && (f) < FLT_MIN);
}

inline float
channel_min (const rgb_t *c)
{
    return std::min(std::min(c->r, c->g), c->b);
}

inline float
channel_max (const rgb_t *c)
{
    return std::max(std::max(c->r, c->g), c->b);
}

inline float
get_lum (const rgb_t *c)
{
    return c->r * 0.3f + c->g * 0.59f + c->b * 0.11f;
}

inline float
get_sat (const rgb_t *c)
{
    return channel_max(c) - channel_min(c);
}

inline void
clip_color (rgb_t *color, float a)
{
    float l = get_lum(color);
    float n = channel_min(color);
    float x = channel_max(color);
    float t;

    if (n < 0.0f) {
	t = l - n;
	if (float_is_zero(t)) {
	    color->r = 0.0f;
	    color->g = 0.0f;
	    color->b = 0.0f;
	} else {
	    color->r = l + (((color->r - l) * l) / t);
	    color->g = l + (((color->g - l) * l) / t);
	    color->b = l + (((color->b - l) * l) / t);
	}
    }
    if (x > a) {
	t = x - l;
	if (float_is_zero(t)) {
	    color->r = a;
	    color->g = a;
	    color->b = a;
	} else {
	    color->r = l + (((color->r - l) * (a - l) / t));
	    color->g = l + (((color->g - l) * (a - l) / t));
	    color->b = l + (((color->b - l) * (a - l) / t));
	}
    }
}

static void
set_lum (rgb_t *color, float sa, float l)
{
    float d = l - get_lum(color);

    color->r = color->r + d;
    color->g = color->g + d;
    color->b = color->b + d;

    clip_color(color, sa);
}

inline void
set_sat (rgb_t *src, float sat)
{
    float *max, *mid, *min;
    float t;

    if (src->r > src->g) {
        if (src->r > src->b) {
            max = &(src->r);

            if (src->g > src->b) {
                mid = &(src->g);
                min = &(src->b);
            } else {
                mid = &(src->b);
                min = &(src->g);
            }
        } else {
            max = &(src->b);
            mid = &(src->r);
            min = &(src->g);
        }
    } else {
        if (src->r > src->b) {
            max = &(src->g);
            mid = &(src->r);
            min = &(src->b);
        } else {
            min = &(src->r);

            if (src->g > src->b) {
                max = &(src->g);
                mid = &(src->b);
            } else {
                max = &(src->b);
                mid = &(src->g);
            }
        }
    }

    t = *max - *min;

    if (float_is_zero(t)) {
        *mid = *max = 0.0f;
    } else {
        *mid = ((*mid - *min) * sat) / t;
        *max = sat;
    }
    
    *min = 0.0f;
}

/* Hue:
 *
 *       as * ad * B(s/as, d/as)
 *     = as * ad * set_lum (set_sat (s/as, SAT (d/ad)), LUM (d/ad), 1)
 *     = set_lum (set_sat (ad * s, as * SAT (d)), as * LUM (d), as * ad)
 *
 */
inline void
blend_hsl_hue (rgb_t *res,
	       const rgb_t *dest, float da,
	       const rgb_t *src, float sa)
{
    res->r = src->r * da;
    res->g = src->g * da;
    res->b = src->b * da;

    set_sat(res, get_sat(dest) * sa);
    set_lum(res, sa * da, get_lum(dest) * sa);
}

/* 
 * Saturation
 *
 *     as * ad * B(s/as, d/ad)
 *   = as * ad * set_lum (set_sat (d/ad, SAT (s/as)), LUM (d/ad), 1)
 *   = set_lum (as * ad * set_sat (d/ad, SAT (s/as)),
 *                                       as * LUM (d), as * ad)
 *   = set_lum (set_sat (as * d, ad * SAT (s), as * LUM (d), as * ad))
 */
inline void
blend_hsl_saturation (rgb_t *res,
		      const rgb_t *dest, float da,
		      const rgb_t *src, float sa)
{
    res->r = dest->r * sa;
    res->g = dest->g * sa;
    res->b = dest->b * sa;

    set_sat(res, get_sat(src) * da);
    set_lum(res, sa * da, get_lum(dest) * sa);
}

/* 
 * Color
 *
 *     as * ad * B(s/as, d/as)
 *   = as * ad * set_lum (s/as, LUM (d/ad), 1)
 *   = set_lum (s * ad, as * LUM (d), as * ad)
 */
inline void
blend_hsl_color (rgb_t *res,
		 const rgb_t *dest, float da,
		 const rgb_t *src, float sa)
{
    res->r = src->r * da;
    res->g = src->g * da;
    res->b = src->b * da;

    set_lum(res, sa * da, get_lum(dest) * sa);
}

/*
 * Luminosity
 *
 *     as * ad * B(s/as, d/ad)
 *   = as * ad * set_lum (d/ad, LUM (s/as), 1)
 *   = set_lum (as * d, ad * LUM (s), as * ad)
 */
inline void
blend_hsl_luminosity (rgb_t *res,
		      const rgb_t *dest, float da,
		      const rgb_t *src, float sa)
{
    res->r = dest->r * sa;
    res->g = dest->g * sa;
    res->b = dest->b * sa;

    set_lum (res, sa * da, get_lum (src) * da);
}

// END
// Code from pixman-combine-float.c
///////////////////////////////////////////////////////////////////////////////

template <MergingFunctionEnum f,typename PIX,int nComponents,int maxValue>
void
mergePixel(bool doAlphaMasking,
           const PIX A[4],
           const PIX B[4],
           PIX* dst)
{
    doAlphaMasking = doAlphaMasking && isMaskable(f);
    PIX a = A[3];
    PIX b = B[3];

    ///When doAlphaMasking is enabled and we're in RGBA the output alpha is set to alphaA+alphaB-alphaA*alphaB
    int maxComp = nComponents;
    if (!isSeparable(f)) {
        // HSL modes
        rgb_t src, dest, res;
        if (a == 0) {
            src.r = src.g = src.b = 0;
        } else {
            src.r = A[0] / (float)a;
            src.g = A[1] / (float)a;
            src.b = A[2] / (float)a;
        }
        if (b == 0) {
            dest.r = dest.g = dest.b = 0;
        } else {
            dest.r = B[0] / (float)b;
            dest.g = B[1] / (float)b;
            dest.b = B[2] / (float)b;
        }
        float sa = a/(float)maxValue;
        float da = b/(float)maxValue;

        switch (f) {
            case eMergeHue:
                blend_hsl_hue(&res, &dest, da, &src, sa);
                break;

            case eMergeSaturation:
                blend_hsl_saturation(&res, &dest, da, &src, sa);
                break;

            case eMergeColor:
                blend_hsl_color(&res, &dest, da, &src, sa);
                break;

            case eMergeLuminosity:
                blend_hsl_luminosity(&res, &dest, da, &src, sa);
                break;

            default:
                res.r = res.g = res.b = 0.f;
                assert(false);
                break;
        }
        float R[3] = { res.r, res.g, res.b };
        for (int i = 0; i < std::min(nComponents, 3); ++i) {
            dst[i] = PIX((1 - sa) * B[i] + (1 - da) * A[i] + R[i] * maxValue);
        }
        if (nComponents == 4) {
            dst[3] = PIX(a + b - a * b / (double)maxValue);
        }

        return;
    }

    // separable modes
    if (doAlphaMasking && nComponents == 4) {
        maxComp = 3;
        dst[3] = PIX(a + b - a * b / (double)maxValue);
    }
    for (int i = 0; i < maxComp; ++i) {
        switch (f) {
        case eMergeATop:
            dst[i] = atopFunctor<PIX, maxValue>(A[i], B[i], a, b);
            break;
        case eMergeAverage:
            dst[i] = averageFunctor(A[i], B[i]);
            break;
        case eMergeColorBurn:
            dst[i] = colorBurnFunctor<PIX, maxValue>(A[i], B[i]);
            break;
        case eMergeColorDodge:
            dst[i] = colorDodgeFunctor<PIX, maxValue>(A[i], B[i]);
            break;
        case eMergeConjointOver:
            dst[i] = conjointOverFunctor<PIX, maxValue>(A[i], B[i], a, b);
            break;
        case eMergeCopy:
            dst[i] = copyFunctor(A[i], B[i]);
            break;
        case eMergeDifference:
            dst[i] = differenceFunctor(A[i], B[i]);
            break;
        case eMergeDisjointOver:
            dst[i] = disjointOverFunctor<PIX, maxValue>(A[i], B[i], a, b);
            break;
        case eMergeDivide:
            dst[i] = divideFunctor(A[i], B[i]);
            break;
        case eMergeExclusion:
            dst[i] = exclusionFunctor<PIX, maxValue>(A[i], B[i]);
            break;
        case eMergeFreeze:
            dst[i] = freezeFunctor<PIX, maxValue>(A[i], B[i]);
            break;
        case eMergeFrom:
            dst[i] = fromFunctor(A[i], B[i]);
            break;
        case eMergeGeometric:
            dst[i] = geometricFunctor(A[i], B[i]);
            break;
        case eMergeHardLight:
            dst[i] = hardLightFunctor<PIX, maxValue>(A[i], B[i]);
            break;
        case eMergeHypot:
            dst[i] = hypotFunctor(A[i], B[i]);
            break;
        case eMergeIn:
            dst[i] = inFunctor<PIX, maxValue>(A[i], B[i], a, b);
            break;
        case eMergeInterpolated:
            dst[i] = interpolatedFunctor<PIX, maxValue>(A[i], B[i]);
            break;
        case eMergeMask:
            dst[i] = maskFunctor<PIX, maxValue>(A[i], B[i], a, b);
            break;
        case eMergeMatte:
            dst[i] = matteFunctor<PIX, maxValue>(A[i], B[i], a, b);
            break;
        case eMergeLighten:
            dst[i] = lightenFunctor(A[i], B[i]);
            break;
        case eMergeDarken:
            dst[i] = darkenFunctor(A[i], B[i]);
            break;
        case eMergeMinus:
            dst[i] = minusFunctor(A[i], B[i]);
            break;
        case eMergeMultiply:
            dst[i] = multiplyFunctor<PIX, maxValue>(A[i], B[i]);
            break;
        case eMergeOut:
            dst[i] = outFunctor<PIX, maxValue>(A[i], B[i], a, b);
            break;
        case eMergeOver:
            dst[i] = overFunctor<PIX, maxValue>(A[i], B[i], a, b);
            break;
        case eMergeOverlay:
            dst[i] = overlayFunctor<PIX, maxValue>(A[i], B[i]);
            break;
        case eMergePinLight:
            dst[i] = pinLightFunctor<PIX, maxValue>(A[i], B[i]);
            break;
        case eMergePlus:
            dst[i] = plusFunctor(A[i], B[i]);
            break;
        case eMergeReflect:
            dst[i] = reflectFunctor<PIX, maxValue>(A[i], B[i]);
            break;
        case eMergeScreen:
            dst[i] = screenFunctor<PIX, maxValue>(A[i], B[i]);
            break;
        case eMergeSoftLight:
            dst[i] = softLightFunctor<PIX, maxValue>(A[i], B[i]);
            break;
        case eMergeStencil:
            dst[i] = stencilFunctor<PIX, maxValue>(A[i], B[i], a, b);
            break;
        case eMergeUnder:
            dst[i] = underFunctor<PIX,maxValue>(A[i], B[i], a, b);
            break;
        case eMergeXOR:
            dst[i] = xorFunctor<PIX, maxValue>(A[i], B[i], a, b);
            break;
        default:
            dst[i] = 0;
            assert(false);
            break;
        } // switch
    }
} // mergePixel


///Bounding box of two rectangles
inline void
rectBoundingBox(const OfxRectD & a,
                const OfxRectD & b,
                OfxRectD* bbox)
{
    bbox->x1 = std::min(a.x1, b.x1);
    bbox->x2 = std::max( bbox->x1, std::max(a.x2, b.x2) );
    bbox->y1 = std::min(a.y1, b.y1);
    bbox->y2 = std::max( bbox->y1, std::max(a.y2, b.y2) );
}

template <typename Rect>
bool
rectIsEmpty(const Rect & r)
{
    return (r.x2 <= r.x1) || (r.y2 <= r.y1);
}

template <typename Rect>
bool
rectIsInfinite(const Rect & r)
{
    return (r.x1 <= kOfxFlagInfiniteMin) || (r.x2 >= kOfxFlagInfiniteMax) ||
           (r.y1 <= kOfxFlagInfiniteMin) || (r.y2 >= kOfxFlagInfiniteMax);
}

/// compute the intersection of two rectangles, and return true if they intersect
template <typename Rect>
bool
rectIntersection(const Rect & r1,
                 const Rect & r2,
                 Rect* intersection)
{
    if ( rectIsEmpty(r1) || rectIsEmpty(r2) ) {
        if (intersection) {
            intersection->x1 = 0;
            intersection->x2 = 0;
            intersection->y1 = 0;
            intersection->y2 = 0;
        }
        return false;
    }

    if ( ( r1.x1 > r2.x2) || ( r2.x1 > r1.x2) || ( r1.y1 > r2.y2) || ( r2.y1 > r1.y2) ) {
        if (intersection) {
            intersection->x1 = 0;
            intersection->x2 = 0;
            intersection->y1 = 0;
            intersection->y2 = 0;
        }
        return false;
    }

    if (intersection) {
        intersection->x1 = std::max(r1.x1,r2.x1);
        // the region must be *at least* empty, thus the maximin.
        intersection->x2 = std::max( intersection->x1,std::min(r1.x2,r2.x2) );
        intersection->y1 = std::max(r1.y1,r2.y1);
        // the region must be *at least* empty, thus the maximin.
        intersection->y2 = std::max( intersection->y1,std::min(r1.y2,r2.y2) );
    }
    return true;
}

/**
 * @brief Scales down the rectangle in pixel coordinates by the given power of 2, and return the smallest *enclosing* rectangle in pixel coordinates
 * Never use this with canonical coordinates, or never round canonical coordinates to use this: use toPixelEnclosing instead.
 **/
inline
OfxRectI
downscalePowerOfTwoSmallestEnclosing(const OfxRectI & r,
                                     unsigned int thisLevel)
{
    if (thisLevel == 0) {
        return r;
    }
    OfxRectI ret;
    int pot = (1 << thisLevel);
    int pot_minus1 = pot - 1;
    if (r.x1 <= kOfxFlagInfiniteMin) {
        ret.x1 = kOfxFlagInfiniteMin;
    } else {
        ret.x1 = r.x1 >> thisLevel;
        assert(ret.x1 * pot <= r.x1);
    }
    if (r.x2 >= kOfxFlagInfiniteMax) {
        ret.x2 = kOfxFlagInfiniteMax;
    } else {
        ret.x2 = (r.x2 + pot_minus1) >> thisLevel;
        assert(ret.x2 * pot >= r.x2);
    }
    if (r.y1 <= kOfxFlagInfiniteMin) {
        ret.y1 = kOfxFlagInfiniteMin;
    } else {
        ret.y1 = r.y1 >> thisLevel;
        assert(ret.y1 * pot <= r.y1);
    }
    if (r.y2 >= kOfxFlagInfiniteMax) {
        ret.y2 = kOfxFlagInfiniteMax;
    } else {
        ret.y2 = (r.y2 + pot_minus1) >> thisLevel;
        assert(ret.y2 * pot >= r.y2);
    }

    return ret;
}

inline
double
scaleFromMipmapLevel(unsigned int level)
{
    return 1. / (1 << level);
}

inline void
toPixelEnclosing(const OfxRectD & regionOfInterest,
                 const OfxPointD & renderScale,
                 double par,
                 OfxRectI *rect)
{
    rect->x1 = (int)std::floor(regionOfInterest.x1 * renderScale.x / par);
    rect->y1 = (int)std::floor(regionOfInterest.y1 * renderScale.y);
    rect->x2 = (int)std::ceil(regionOfInterest.x2 * renderScale.x / par);
    rect->y2 = (int)std::ceil(regionOfInterest.y2 * renderScale.y);
}

inline void
toPixel(const OfxPointD & p_canonical,
        const OfxPointD & renderScale,
        double par,
        OfxPointI *p_pixel)
{
    p_pixel->x = (int)std::floor(p_canonical.x * renderScale.x / par);
    p_pixel->y = (int)std::floor(p_canonical.y * renderScale.y);
}

// subpixel version (no rounding)
inline void
toPixelSub(const OfxPointD & p_canonical,
           const OfxPointD & renderScale,
           double par,
           OfxPointD *p_pixel)
{
    p_pixel->x = p_canonical.x * renderScale.x / par - 0.5;
    p_pixel->y = p_canonical.y * renderScale.y - 0.5;
}

// transforms the middle of the given pixel to canonical coordinates
inline void
toCanonical(const OfxPointI & p_pixel,
            const OfxPointD & renderScale,
            double par,
            OfxPointD *p_canonical)
{
    p_canonical->x = (p_pixel.x + 0.5) * par / renderScale.x;
    p_canonical->y = (p_pixel.y + 0.5) / renderScale.y;
}

// subpixel version (no rounding)
inline void
toCanonicalSub(const OfxPointD & p_pixel,
               const OfxPointD & renderScale,
               double par,
               OfxPointD *p_canonical)
{
    p_canonical->x = (p_pixel.x + 0.5) * par / renderScale.x;
    p_canonical->y = (p_pixel.y + 0.5) / renderScale.y;
}

inline void
toCanonical(const OfxRectI & rect,
            const OfxPointD & renderScale,
            double par,
            OfxRectD *regionOfInterest)
{
    regionOfInterest->x1 = rect.x1 * par / renderScale.x;
    regionOfInterest->y1 = rect.y1 / renderScale.y;
    regionOfInterest->x2 = rect.x2 * par / renderScale.x;
    regionOfInterest->y2 = rect.y2 / renderScale.y;
}

inline void
enlargeRectI(const OfxRectI & rect,
             int delta_pix,
             const OfxRectI & bounds,
             OfxRectI* rectOut)
{
    rectOut->x1 = std::max(bounds.x1, rect.x1 - delta_pix);
    rectOut->x2 = std::min(bounds.x2, rect.x2 + delta_pix);
    rectOut->y1 = std::max(bounds.y1, rect.y1 - delta_pix);
    rectOut->y2 = std::min(bounds.y2, rect.y2 + delta_pix);
}

#ifndef M_LN2
#define M_LN2       0.693147180559945309417232121458176568  /* loge(2)        */
#endif
inline
unsigned int
mipmapLevelFromScale(double s)
{
    assert(0. < s && s <= 1.);
    int retval = -(int)std::floor(std::log(s) / M_LN2 + 0.5);
    assert(retval >= 0);

    return retval;
}
} // MergeImages2D
} // OFX


#endif // Misc_Merging_helper_h
