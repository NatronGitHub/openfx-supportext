/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of openfx-supportext <https://github.com/NatronGitHub/openfx-supportext>,
 * (C) 2018-2026 The Natron Developers
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
 * Interact cursors (proposed extension NatronGitHub/openfx#4).
 *
 * After each pen or key action that does not fail, trapped or not, the host
 * reads the cursor requested by the interact instance. When no interact traps
 * the event, the most recent non-default request of the view is shown. An
 * interact therefore sets a cursor while the pen hovers or drags one of its
 * handles, and sets "default" again when the pen leaves them. It does not need
 * to trap events for that: keep the existing return values, so that other
 * overlays sharing the view still receive pen motion, and hosts without
 * cursor support see no change in behaviour.
 *
 * Without the Support library API (OFX_EXTENSIONS_NATRON and
 * kOfxInteractPropCursor), set() does nothing and returns false.
 */

#ifndef openfx_supportext_ofxsInteractCursor_h
#define openfx_supportext_ofxsInteractCursor_h

#include <cmath>
#include <string>

#include "ofxsInteract.h"

#if defined(OFX_EXTENSIONS_NATRON) && defined(kOfxInteractPropCursor)
#define OFXS_HAVE_INTERACT_CURSOR 1
#endif

// CSS cursor names (kOfxInteractCursor* in ofxInteract.h), defined here so
// that the interacts compile with Support libraries that lack the extension.
#define kOfxsCursorDefault    "default"
#define kOfxsCursorMove       "move"
#define kOfxsCursorGrab       "grab"
#define kOfxsCursorGrabbing   "grabbing"
#define kOfxsCursorEWResize   "ew-resize"
#define kOfxsCursorNSResize   "ns-resize"
#define kOfxsCursorNESWResize "nesw-resize"
#define kOfxsCursorNWSEResize "nwse-resize"

namespace OFX {
/// Per-interact-instance cursor state. Hosts create one interact instance
/// per view, so this is per-view state, as the cursor is.
class InteractCursor
{
public:
    InteractCursor()
        : _current()
    {
    }

    /// Request a cursor. Returns true if the host supports it (it is then
    /// displayed after the current action, subject to the host's arbitration
    /// between interacts sharing the view).
    bool set(Interact* interact,
             const char* name)
    {
#ifdef OFXS_HAVE_INTERACT_CURSOR
        if (!interact) {
            return false;
        }
        if (_current == name) {
            return true;
        }
        if ( interact->setCursor(name) ) {
            _current = name;

            return true;
        }
        _current.clear();

        return false;
#else
        (void)interact;
        (void)name;

        return false;
#endif
    }

    /// Give the cursor back to the host. Returns true if the host supports
    /// interact cursors.
    bool setDefault(Interact* interact)
    {
        return set(interact, kOfxsCursorDefault);
    }

    /// Forget the cached cursor (e.g. on loseFocus, after which the host
    /// restores its own cursor), so that the next set() reports it again.
    void reset()
    {
        _current.clear();
    }

    /// The resize cursor closest to the direction (dx, dy), given in
    /// canonical coordinates (y up). pixelScale converts it to screen pixels,
    /// so that the pixel aspect ratio and anisotropic zoom are accounted for.
    static const char* resizeCursorForDirection(double dx,
                                                double dy,
                                                const OfxPointD& pixelScale)
    {
        const double sx = (pixelScale.x != 0.) ? dx / pixelScale.x : dx;
        const double sy = (pixelScale.y != 0.) ? dy / pixelScale.y : dy;

        if ( (sx == 0.) && (sy == 0.) ) {
            return kOfxsCursorMove;
        }
        // angle in [0, 180) degrees, counter-clockwise from the x axis, y up
        double a = std::atan2(sy, sx) * (180. / 3.14159265358979323846264338327950288);
        if (a < 0.) {
            a += 180.;
        }
        if (a >= 180.) {
            a -= 180.;
        }
        if ( (a < 22.5) || (a >= 157.5) ) {
            return kOfxsCursorEWResize;
        } else if (a < 67.5) {
            return kOfxsCursorNESWResize; // up-right / down-left
        } else if (a < 112.5) {
            return kOfxsCursorNSResize;
        }

        return kOfxsCursorNWSEResize; // up-left / down-right
    }

private:
    std::string _current; //< last cursor the host accepted, empty if unknown
};
} // namespace OFX

#endif // openfx_supportext_ofxsInteractCursor_h
