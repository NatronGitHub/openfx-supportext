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
 * The image fetch of the multiview plug-ins (Anaglyph, JoinViews, MixViews, OneView, SideBySide).
 */

#ifndef openfx_supportext_ofxsMultiView_h
#define openfx_supportext_ofxsMultiView_h

#include "ofxsImageEffect.h"

namespace OFX {
// The image of a view of clip at time. OFX 1.4 hosts have neither the multiview plane suite
// (kFnOfxImageEffectPlaneSuite v2, OFX_EXTENSIONS_NUKE) nor the Vegas stereoscopic suite
// (OFX_EXTENSIONS_VEGAS): they have a single view, which is then every view (clip->fetchImage()).
// The caller owns the image, which may be NULL, as with fetchImage().
inline Image*
fetchViewImage(Clip* clip,
               double time,
               int view)
{
#ifdef OFX_EXTENSIONS_NUKE
    if ( fetchSuite(kFnOfxImageEffectPlaneSuite, 2, true) ) {
        return clip->fetchStereoscopicImage(time, view);
    }
#endif
#ifdef OFX_EXTENSIONS_VEGAS
    if ( fetchSuite(kOfxVegasStereoscopicImageEffectSuite, 1, true) ) {
        return clip->fetchStereoscopicImage(time, view);
    }
#endif
#if !defined(OFX_EXTENSIONS_NUKE) && !defined(OFX_EXTENSIONS_VEGAS)
    (void)view;
#endif

    return clip->fetchImage(time);
}
} // namespace OFX

#endif // ifndef openfx_supportext_ofxsMultiView_h
