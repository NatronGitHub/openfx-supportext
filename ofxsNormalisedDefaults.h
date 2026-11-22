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
 * - In the effect's constructor, call ofxsDenormaliseUntouchedDefault() for each such param: on
 *   those hosts only, a value that still equals its normalised default (a new instance, or an old
 *   project's untouched default) is converted to canonical coordinates. Edited values are kept,
 *   and the conversion is idempotent (a converted value no longer equals the default).
 */

#ifndef openfx_supportext_ofxsNormalisedDefaults_h
#define openfx_supportext_ofxsNormalisedDefaults_h

#include "ofxsImageEffect.h"

namespace OFX {
// Whether the host ignores kOfxParamPropDefaultCoordinateSystem. Written in describe, read when
// creating instances, which the host does after describing the plug-in (the same rule as the
// other describe-time host capabilities; no multithread there).
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

// Instance (in the effect's constructor): on a host that ignores normalised defaults, convert a
// value that still equals its normalised default to canonical coordinates, using the project
// extent (and offset, for a position). Returns whether it converted the value. Animated params
// (with keys) are left alone.
inline bool
ofxsDenormaliseUntouchedDefault(const ImageEffect& effect,
                                Double2DParam* param,
                                bool isPosition)
{
    if ( !param || !ofxsHostIgnoresNormalisedDefaults() || (param->getNumKeys() > 0) ) {
        return false;
    }
    double dx, dy;
    param->getDefault(dx, dy);
    const OfxPointD v = param->getValue();
    if ( (v.x != dx) || (v.y != dy) ) {
        return false; // edited, or already converted
    }
    const OfxPointD extent = effect.getProjectExtent();
    const OfxPointD offset = isPosition ? effect.getProjectOffset() : OfxPointD{0., 0.};
    param->setValue(v.x * extent.x + offset.x, v.y * extent.y + offset.y);

    return true;
}
} // namespace OFX

#endif // openfx_supportext_ofxsNormalisedDefaults_h
