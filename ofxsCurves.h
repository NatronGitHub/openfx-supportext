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
 * The curves of a ramp or a soft edge, for t in [0, 1]: ofxsRamp (the Ramp plug-in) and the shape
 * plug-ins of openfx-misc (Crop, CopyRectangle, DiscBlades, Radial, Rectangle) share them.
 *
 * ofxsCurveInUnit() is the only place each polynomial is written. Two clamps around it:
 * - ofxsCurve() clamps t to [0, 1] and keeps a NaN t as NaN (ofxsRamp, and openfx-misc's smoothstep);
 * - ofxsFalloff() and ofxsSoftEdgeWeight() give 0 for a NaN t, as openfx-misc's Radial and Rectangle
 *   do (ofxsFalloff()'s Quadratic is the exception: not clamped, it keeps a NaN).
 * OfxsCurveEnum has no numeric meaning: never cast a choice param's value to it, map each option
 * to a curve, so the plug-in keeps the order and number of its options.
 */

#ifndef openfx_supportext_ofxsCurves_h
#define openfx_supportext_ofxsCurves_h

namespace OFX {
enum class OfxsCurveEnum
{
    eLinear,    // t
    ePLinear,   // t^3, perceptually linear in Rec.709 (ofxsRamp)
    eSmooth0,   // t^2 (2 - t), Catmull-Rom: smooth start, linear end (ofxsRamp's Ease-in)
    eSmooth1,   // t (1 + t (1 - t)), Catmull-Rom: linear start, smooth end (ofxsRamp's Ease-out)
    eSmooth,    // t^2 (3 - 2t), the smoothstep
    eQuadratic, // two parabolas, 2t^2 for t < 1/2 then 1 - 2(1 - t)^2 (openfx-misc's older shapes)
};

/// The curve at t, for t in (0, 1). Quadratic is also defined, and used unclamped, outside of it.
/// The expressions are those of the old code, so that its output is unchanged.
inline double
ofxsCurveInUnit(OfxsCurveEnum curve,
                double t)
{
    switch (curve) {
    case OfxsCurveEnum::eLinear:
        return t;
    case OfxsCurveEnum::ePLinear:
        return t * t * t;
    case OfxsCurveEnum::eSmooth0:
        return t * t * (2. - t);
    case OfxsCurveEnum::eSmooth1:
        return t * ( 1. + t * (1. - t) );
    case OfxsCurveEnum::eQuadratic:
        t *= 2.;
        if (t < 1.) {
            return t * t / 2.;
        }
        t -= 1.;

        return -0.5 * (t * (t - 2.) - 1.);
    case OfxsCurveEnum::eSmooth:
    default:
        return t * t * (3. - 2. * t);
    }
}

/// The curve of a t that is not yet clamped: 0 for t <= 0, 1 for t >= 1, ofxsCurveInUnit() between;
/// a NaN t stays NaN.
inline double
ofxsCurve(OfxsCurveEnum curve,
          double t)
{
    if (t >= 1.) {
        return 1.;
    }
    if (t <= 0.) {
        return 0.;
    }

    return ofxsCurveInUnit(curve, t);
}

/// The weight at the distance t across a soft edge (0 at the outer edge, 1 at the inner one): the
/// curve clamped to [0, 1], a NaN t giving 0; but Quadratic is not clamped (openfx-misc's older shape
/// versions), so it keeps a NaN.
inline double
ofxsFalloff(OfxsCurveEnum curve,
            double t)
{
    if (curve == OfxsCurveEnum::eQuadratic) {
        return ofxsCurveInUnit(curve, t);
    }
    if ( !(t > 0.) ) {
        return 0.;
    }

    return ofxsCurve(curve, t);
}

/// The weight of a pixel whose center is at the distance d inside the edge (negative outside), for a
/// soft edge of width softness and a half pixel halfPixel, all three in the same units (e.g. pixels
/// at the render scale): the curve of t = (d + halfPixel) / (softness + 2 halfPixel), so that the edge
/// is also antialiased over one pixel (halfPixel 0: no other antialiasing). 0 outside of the edge
/// (also for a NaN t) and 1 inside, for every curve.
inline double
ofxsSoftEdgeWeight(OfxsCurveEnum curve,
                   double d,
                   double softness,
                   double halfPixel)
{
    const double t = (d + halfPixel) / (softness + 2. * halfPixel);

    if ( !(t > 0.) ) {
        return 0.;
    }

    return ofxsCurve(curve, t);
}
} // namespace OFX

#endif // ifndef openfx_supportext_ofxsCurves_h
