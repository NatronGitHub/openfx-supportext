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
 * Soft-edge and falloff curves, shared by the shape plug-ins of openfx-misc (Crop, CopyRectangle,
 * DiscBlades, Radial, Rectangle) and by ofxsRamp.
 *
 * Two NaN behaviours, so that each caller keeps its output:
 * - ofxsSmoothstep(t) clamps t and keeps a NaN t as NaN;
 * - ofxsFalloff() gives 0 for a NaN t with every curve but Quadratic (ofxsQuadraticFalloff(), not
 *   clamped, keeps it), and ofxsSoftEdgeWeight() gives 0 for a NaN t with every curve.
 * OfxsFalloffEnum has no numeric meaning: a plug-in maps its own Falloff choice options to it, so
 * their order and number stay the plug-in's.
 */

#ifndef openfx_supportext_ofxsSoftEdge_h
#define openfx_supportext_ofxsSoftEdge_h

namespace OFX {
/// t^2 (3 - 2t), the smoothstep polynomial, for t in (0, 1).
inline double
ofxsSmoothstepInUnit(double t)
{
    return t * t * (3. - 2. * t);
}

/// t^2 (2 - t), Catmull-Rom with a smooth start and a linear end (Smooth0, ofxsRamp's Ease-in),
/// for t in (0, 1).
inline double
ofxsSmooth0InUnit(double t)
{
    return t * t * (2. - t);
}

/// t (1 + t (1 - t)), Catmull-Rom with a linear start and a smooth end (Smooth1, ofxsRamp's
/// Ease-out), for t in (0, 1).
inline double
ofxsSmooth1InUnit(double t)
{
    return t * ( 1. + t * (1. - t) );
}

/// smoothstep of a t that is not yet clamped: 0 for t <= 0, 1 for t >= 1, t^2 (3 - 2t) between;
/// a NaN t stays NaN.
inline double
ofxsSmoothstep(double t)
{
    if (t <= 0.) {
        return 0.;
    }
    if (t >= 1.) {
        return 1.;
    }

    return ofxsSmoothstepInUnit(t);
}

/// The Quadratic falloff: two parabolas, 2t^2 for t < 1/2 then 1 - 2(1 - t)^2; not clamped (not
/// ofxsRamp's Smooth, which is the smoothstep). The softness of openfx-misc's older shape versions,
/// written as they had it so that their output is unchanged.
inline double
ofxsQuadraticFalloff(double t)
{
    t *= 2.;
    if (t < 1.) {
        return t * t / 2.;
    }
    t -= 1.;

    return -0.5 * (t * (t - 2.) - 1.);
}

/// The falloff curve of a soft edge. Its values have no numeric meaning: never cast a choice
/// param's value to it, map each option to a curve.
enum class OfxsFalloffEnum
{
    eLinear,
    eSmooth0,
    eSmooth1,
    eSmooth,
    eQuadratic, // ofxsQuadraticFalloff()
};

/// The clamped curves (all but Quadratic) for t in (0, 1).
inline double
ofxsClampedFalloffInUnit(OfxsFalloffEnum curve,
                         double t)
{
    switch (curve) {
    case OfxsFalloffEnum::eLinear:
        return t;
    case OfxsFalloffEnum::eSmooth0:
        return ofxsSmooth0InUnit(t);
    case OfxsFalloffEnum::eSmooth1:
        return ofxsSmooth1InUnit(t);
    case OfxsFalloffEnum::eSmooth:
    default:
        return ofxsSmoothstepInUnit(t);
    }
}

/// The weight at the distance t across the soft edge (0 at the outer edge, 1 at the inner one):
/// Quadratic is ofxsQuadraticFalloff(), not clamped (a NaN t stays NaN); the other curves are
/// clamped to [0, 1], and a NaN t gives 0.
inline double
ofxsFalloff(OfxsFalloffEnum curve,
            double t)
{
    if (curve == OfxsFalloffEnum::eQuadratic) {
        return ofxsQuadraticFalloff(t);
    }
    if ( !(t > 0.) ) {
        return 0.;
    }
    if (t >= 1.) {
        return 1.;
    }

    return ofxsClampedFalloffInUnit(curve, t);
}

/// The weight of a pixel whose center is at the distance d inside the edge (negative outside), for a
/// soft edge of width softness and a half pixel halfPixel, all three in the same units (e.g. pixels
/// at the render scale): the falloff curve of t = (d + halfPixel) / (softness + 2 halfPixel), so that
/// the edge is also antialiased over one pixel (halfPixel 0: no other antialiasing). 0 outside of the
/// edge (also for a NaN t) and 1 inside; unlike ofxsFalloff(), Quadratic is clamped too.
inline double
ofxsSoftEdgeWeight(OfxsFalloffEnum curve,
                   double d,
                   double softness,
                   double halfPixel)
{
    const double t = (d + halfPixel) / (softness + 2. * halfPixel);

    if ( !(t > 0.) ) {
        return 0.;
    }
    if (t >= 1.) {
        return 1.;
    }
    if (curve == OfxsFalloffEnum::eQuadratic) {
        return ofxsQuadraticFalloff(t);
    }

    return ofxsClampedFalloffInUnit(curve, t);
}
} // namespace OFX

#endif // ifndef openfx_supportext_ofxsSoftEdge_h
