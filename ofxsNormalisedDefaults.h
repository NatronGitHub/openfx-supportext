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
 * Normalised defaults on hosts that ignore kOfxParamPropDefaultCoordinateSystem.
 *
 * A spatial double param can give its default in normalised coordinates
 * (kOfxParamPropDefaultCoordinateSystem = kOfxParamCoordinatesNormalised, OFX 1.2): "by saying
 * normalised and 0.5 it would be in the 'middle'". Some hosts (e.g. DaVinci Resolve) don't
 * support the property and give the plug-in the normalised default as is, so a default size of
 * (1, 1) becomes a 1x1 pixel rectangle.
 *
 * - In describe, call ofxsSetDefaultCoordinateSystemNormalised(param) instead of
 *   param.setDefaultCoordinateSystem(eCoordinatesNormalised): it also records whether the host
 *   ignores the property.
 * - Last in the effect's constructor (after fetching every member that changedParam() uses: a
 *   host may send kOfxActionInstanceChanged from setValue), call ofxsDenormaliseUntouchedDefault()
 *   for each such param. On those hosts only, a value that still equals its normalised default (a
 *   new instance, or an old project's untouched default: "if the values are being loaded from an
 *   old setup, that load should have taken place before the create instance action is called")
 *   is converted to canonical coordinates as the spec says ("you use the project size and
 *   offset": size * value for a size, size * value + offset for a position). Edited values are
 *   kept. A value the user set to exactly the default can't be told apart from an untouched one;
 *   for a rectangle, convert its size only if its corner was untouched too (see below).
 *   Converting again is a no-op, and a value that the conversion leaves unchanged (e.g. a (0, 0)
 *   position with a (0, 0) offset) is not set.
 * - Setting a value in the constructor (kOfxActionCreateInstance) goes beyond ofxParam.h's
 *   "paramSetValue should only be called from within a kOfxActionInstanceChanged or interact
 *   action", as ofxsGenerator.cpp and several plug-ins already do: it is the only time an
 *   untouched default can be recognised.
 */

#ifndef openfx_supportext_ofxsNormalisedDefaults_h
#define openfx_supportext_ofxsNormalisedDefaults_h

#include "ofxsImageEffect.h"

namespace OFX {
// Whether the host ignores kOfxParamPropDefaultCoordinateSystem: a host property, so one flag is
// shared by all the plug-ins of a binary (e.g. a combined bundle). Written in describe (which
// always comes before creating instances, in every process that loads the binary) and read when
// creating instances; it only goes from false to true. Like the other describe-time host
// capabilities, it is global state.
inline bool&
ofxsHostIgnoresNormalisedDefaults()
{
    static bool ignores = false;

    return ignores;
}

// Describe: the default of param is in normalised coordinates.
inline void
ofxsSetDefaultCoordinateSystemNormalised(BaseDoubleParamDescriptor& param)
{
    if ( param.supportsDefaultCoordinateSystem() ) {
        param.setDefaultCoordinateSystem(eCoordinatesNormalised);
    } else {
        ofxsHostIgnoresNormalisedDefaults() = true;
    }
}

// Instance (last in the effect's constructor): on a host that ignores normalised defaults, convert
// a value that still equals its normalised default to canonical coordinates: project size *
// value, plus the project offset if isPosition. Animated params (with keys) are left alone.
// Returns true if the value was its untouched default on such a host (whether or not the
// conversion changed it), so that a rectangle's size can be converted only if its corner was:
//     if ( ofxsDenormaliseUntouchedDefault(*this, *_btmLeft, /*isPosition=*/true) ) {
//         ofxsDenormaliseUntouchedDefault(*this, *_size, /*isPosition=*/false);
//     }
inline bool
ofxsDenormaliseUntouchedDefault(const ImageEffect& effect,
                                Double2DParam& param,
                                bool isPosition)
{
    if ( !ofxsHostIgnoresNormalisedDefaults() || (param.getNumKeys() > 0) ) {
        return false;
    }
    double dx, dy;
    param.getDefault(dx, dy);
    const OfxPointD v = param.getValue();
    if ( (v.x != dx) || (v.y != dy) ) {
        return false; // edited, or already converted
    }
    const OfxPointD size = effect.getProjectSize();
    const OfxPointD offset = isPosition ? effect.getProjectOffset() : OfxPointD{0., 0.};
    const double x = v.x * size.x + offset.x;
    const double y = v.y * size.y + offset.y;
    if ( (x != v.x) || (y != v.y) ) {
        param.setValue(x, y);
    }

    return true;
}
} // namespace OFX

#endif // openfx_supportext_ofxsNormalisedDefaults_h
