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
 * Support for the OFX 1.4 host native origin hint (kOfxImageEffectHostPropNativeOrigin)
 * in overlay interacts.
 *
 * OFX overlays are specified in canonical coordinates (origin at the bottom-left of
 * the project, y up): ofxInteract.h states that the projection matrix set by the host
 * "will correspond to the interact's canonical view", and that kOfxInteractPropPenPosition
 * is "in the interact's canonical coordinates". A host that follows the spec therefore
 * needs no correction, whatever its native origin.
 *
 * kOfxImageEffectHostPropNativeOrigin tells the plugin that the host's own UI space is
 * different (TopLeft: 0,0 at the top-left, y down; Center: 0,0 at the center).
 * If a host sets up the overlay projection and the pen position in that native space
 * instead of canonical space, the helpers below convert once, at the interact entry
 * points, so that the rest of the drawing and picking code keeps working in canonical
 * coordinates:
 *  - OverlayNativeOriginGLScope multiplies the GL modelview matrix by the
 *    canonical->native transform for the duration of draw();
 *  - overlayPenArgsToCanonical() converts PenArgs::penPosition native->canonical at the
 *    start of penMotion()/penDown()/penUp().
 *
 * The native frame used for the conversion is the project extent [0,0]-[extent.x,extent.y]
 * (kOfxImageEffectPropProjectExtent), in canonical coordinates:
 *  - BottomLeft: identity (the canonical OFX space);
 *  - TopLeft:    xn = x, yn = extent.y - y;
 *  - Center:     xn = x - extent.x/2, yn = y - extent.y/2 (y up, "screen space" center).
 *
 * Because no host is known to set up the overlay projection in native space (which would
 * contradict the spec), and because the OFX Support library also infers TopLeft for Fusion
 * and Center for Toxik when the property is missing, the correction is disabled by default:
 * define OFX_OVERLAY_NATIVE_ORIGIN=1 at compile time to enable it. With the default
 * (OFX_OVERLAY_NATIVE_ORIGIN=0), or on any bottom-left host (Natron, Nuke, ...), the helpers
 * are no-ops and overlays behave exactly as before.
 *
 * Limitations:
 *  - the host's native frame is not specified by OFX; the project extent is used, which may
 *    differ from the frame of the viewer or of a given input clip;
 *  - with the TopLeft flip, offsets expressed in screen pixels (e.g. text placed "above" a
 *    point using pixelScale.y) end up mirrored; bitmap text itself stays upright, because
 *    glBitmap draws in window coordinates.
 */

#ifndef openfx_supportext_ofxsOGLOrigin_h
#define openfx_supportext_ofxsOGLOrigin_h

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <GL/gl.h>
#endif

#include "ofxsImageEffect.h"
#include "ofxsInteract.h"
#include "ofxsMacros.h"

#ifndef OFX_OVERLAY_NATIVE_ORIGIN
#define OFX_OVERLAY_NATIVE_ORIGIN 0
#endif

namespace OFX {
/// Convert a point from canonical coordinates to host-native coordinates.
/// extent is the project extent (in canonical coordinates) that defines the native frame.
inline OfxPointD
canonicalToNativeOrigin(NativeOriginEnum origin,
                        const OfxPointD& extent,
                        const OfxPointD& p)
{
    OfxPointD n = p;

    switch (origin) {
    case eNativeOriginBottomLeft:
        break;
    case eNativeOriginTopLeft:
        n.y = extent.y - p.y;
        break;
    case eNativeOriginCenter:
        n.x = p.x - extent.x / 2.;
        n.y = p.y - extent.y / 2.;
        break;
    }

    return n;
}

/// Convert a point from host-native coordinates to canonical coordinates (inverse of canonicalToNativeOrigin).
inline OfxPointD
nativeOriginToCanonical(NativeOriginEnum origin,
                        const OfxPointD& extent,
                        const OfxPointD& n)
{
    OfxPointD p = n;

    switch (origin) {
    case eNativeOriginBottomLeft:
        break;
    case eNativeOriginTopLeft:
        p.y = extent.y - n.y; // the flip is an involution
        break;
    case eNativeOriginCenter:
        p.x = n.x + extent.x / 2.;
        p.y = n.y + extent.y / 2.;
        break;
    }

    return p;
}

/// Multiply the current GL matrix by the canonical->native transform.
inline void
glMultCanonicalToNativeOrigin(NativeOriginEnum origin,
                              const OfxPointD& extent)
{
    switch (origin) {
    case eNativeOriginBottomLeft:
        break;
    case eNativeOriginTopLeft:
        // (x,y) -> (x, extent.y - y)
        glTranslated(0., extent.y, 0.);
        glScaled(1., -1., 1.);
        break;
    case eNativeOriginCenter:
        // (x,y) -> (x - extent.x/2, y - extent.y/2)
        glTranslated(-extent.x / 2., -extent.y / 2., 0.);
        break;
    }
}

/// The native origin for which overlays must be corrected: always BottomLeft (no correction)
/// unless OFX_OVERLAY_NATIVE_ORIGIN is enabled and the host reports another origin.
inline NativeOriginEnum
overlayNativeOrigin()
{
    if (!OFX_OVERLAY_NATIVE_ORIGIN) {
        return eNativeOriginBottomLeft;
    }
    const ImageEffectHostDescription* host = getImageEffectHostDescription();

    return host ? host->nativeOrigin : eNativeOriginBottomLeft;
}

/// Return the pen arguments with penPosition in canonical coordinates.
/// This is a plain copy on bottom-left hosts (or if the correction is disabled).
inline PenArgs
overlayPenArgsToCanonical(const ImageEffect* effect,
                          const PenArgs& args)
{
    PenArgs a = args;
    const NativeOriginEnum origin = overlayNativeOrigin();

    if ( effect && (origin != eNativeOriginBottomLeft) ) {
        a.penPosition = nativeOriginToCanonical( origin, effect->getProjectExtent(), args.penPosition );
    }

    return a;
}

/// RAII: for the lifetime of this object, drawing in canonical coordinates appears at the right
/// place on hosts whose overlay projection is in host-native coordinates.
/// Does nothing (no GL call) on bottom-left hosts (or if the correction is disabled).
class OverlayNativeOriginGLScope
{
public:
    explicit OverlayNativeOriginGLScope(const ImageEffect* effect)
        : _active(false)
        , _matrixMode(GL_MODELVIEW)
    {
        const NativeOriginEnum origin = overlayNativeOrigin();

        if ( !effect || (origin == eNativeOriginBottomLeft) ) {
            return;
        }
        _active = true;
        glGetIntegerv(GL_MATRIX_MODE, &_matrixMode);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glMultCanonicalToNativeOrigin( origin, effect->getProjectExtent() );
        glMatrixMode(_matrixMode);
    }

    ~OverlayNativeOriginGLScope()
    {
        if (_active) {
            GLint matrixMode = GL_MODELVIEW;
            glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
            glMatrixMode(matrixMode);
        }
    }

private:
    // non-copyable
    OverlayNativeOriginGLScope(const OverlayNativeOriginGLScope&);
    OverlayNativeOriginGLScope& operator=(const OverlayNativeOriginGLScope&);

    bool _active;
    GLint _matrixMode;
};
} // namespace OFX

#endif /* openfx_supportext_ofxsOGLOrigin_h */
