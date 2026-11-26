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
 * Comparison-only clamps for non-finite values, shared by the keyers (Keyer, ChromaKeyer, PIK,
 * Despill, Difference; issues #195 and #213), ofxsMaskMix.h and ofxsMerging.h.
 *
 * NaN rule: no behaviour depends on detecting NaN (there is no isnan() test), every operation that
 * could produce NaN is guarded by checks on its inputs, and every finite value in range is
 * unchanged. These clamps are used on the input samples (so that the computations never see
 * inf * 0 or inf - inf), on sums and products that may overflow to +-inf (never NaN, since their
 * operands are finite), and on double results converted to float (converting a double beyond the
 * float range to float is undefined).
 */

#ifndef openfx_supportext_ofxsNonFinite_h
#define openfx_supportext_ofxsNonFinite_h

#include <cfloat> // FLT_MAX

namespace OFX {
// The smallest exact value that float rounds to inf: 2^128 - 2^103 (halfway between FLT_MAX and
// 2^128). An exact product (or sum) of floats computed in double, compared with it, tells whether
// the same float expression overflows.
constexpr double kOfxsFloatOverflow = 340282356779733661637539395458142568448.;

// A value in the float range: +-inf (or a double beyond the float range) gives +-FLT_MAX, and NaN
// gives 0 (every comparison with NaN is false).
template<typename T>
inline T
clampToFloatRange(T v)
{
    return (v > -FLT_MAX) ? ( (v < FLT_MAX) ? v : T(FLT_MAX) ) : ( (v < 0) ? T(-FLT_MAX) : T(0) );
}

// A double converted to T (float or double) without leaving the float range: beyond it, +-FLT_MAX
// (converting such a double to float is undefined). Unlike clampToFloatRange, NaN falls through
// unchanged; every value in range is converted exactly as T(v).
template<typename T>
inline T
clampBeyondFloatRange(double v)
{
    if (v > FLT_MAX) {
        return T(FLT_MAX);
    }
    if (v < -FLT_MAX) {
        return T(-FLT_MAX);
    }

    return T(v);
}

// clamp to [0,1], NaN gives 0
template<typename T>
inline T
clampToUnit(T v)
{
    return (v >= 0) ? ( (v < 1) ? v : T(1) ) : T(0);
}

// A pixel sample copied to the output: a float sample (maxValue == 1) is made finite (see
// clampToFloatRange), an integer sample is unchanged.
template<class PIX, int maxValue>
inline PIX
finiteSample(PIX v)
{
    return (maxValue == 1) ? PIX( clampToFloatRange<float>(v) ) : v;
}
} // namespace OFX

#endif // ifndef openfx_supportext_ofxsNonFinite_h
