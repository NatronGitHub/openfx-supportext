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
 * handles, and gives it back ("default") when the pen leaves them or the
 * interact stops handling events. It does not need
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
#include <map>
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
/// Cursor state of one interact helper (one per interact instance; the host
/// may share an instance between views, so this is per-instance state).
///
/// Several helpers may share one interact (OverlayInteractFromHelpers), and
/// each one reports its cursor in turn on every event. To let them coexist,
/// the owner of each interact's cursor (the helper that last set a non-default
/// cursor on it) is recorded, and a helper only gives the cursor back
/// ("default") if it is the current owner: release() does nothing otherwise,
/// even if the other helper requested the same cursor name. The property is
/// set on every call (it is cheap), so the last helper that wants a cursor
/// during an event wins.
class InteractCursor
{
public:
    InteractCursor()
    {
    }

    ~InteractCursor()
    {
#ifdef OFXS_HAVE_INTERACT_CURSOR
        // forget the interacts whose cursor this helper owns
        OwnerMap& owners = ownerMap();
        for (OwnerMap::iterator it = owners.begin(); it != owners.end(); ) {
            if (it->second == this) {
                owners.erase(it++);
            } else {
                ++it;
            }
        }
#endif
    }

    /// Request a cursor. Returns true if the host supports it (it is then
    /// displayed after the current action, subject to the host's arbitration
    /// between interacts sharing the view). If the host does not support that
    /// cursor, give the cursor back rather than leaving a previous request in
    /// place.
    bool set(Interact* interact,
             const char* name)
    {
#ifdef OFXS_HAVE_INTERACT_CURSOR
        if (!interact) {
            return false;
        }
        if ( interact->setCursor(name) ) {
            OwnerMap& owners = ownerMap();
            if (std::string(name) != kOfxsCursorDefault) {
                owners[interact] = this;
            } else {
                OwnerMap::iterator it = owners.find(interact);
                if ( (it != owners.end()) && (it->second == this) ) {
                    owners.erase(it);
                }
            }

            return true;
        }
        release(interact);

        return false;
#else
        (void)interact;
        (void)name;

        return false;
#endif
    }

    /// Give the cursor back to the host ("default"), if this helper owns the
    /// interact's cursor, i.e. it is the helper that last set a non-default
    /// cursor on it. Call it when the pen leaves the handles, when the
    /// interact stops handling events (overlay hidden, params disabled), and
    /// on loseFocus.
    void release(Interact* interact)
    {
#ifdef OFXS_HAVE_INTERACT_CURSOR
        if (!interact) {
            return;
        }
        OwnerMap& owners = ownerMap();
        OwnerMap::iterator it = owners.find(interact);
        if ( (it != owners.end()) && (it->second == this) ) {
            owners.erase(it);
            interact->setCursor(kOfxsCursorDefault);
        }
#else
        (void)interact;
#endif
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
#ifdef OFXS_HAVE_INTERACT_CURSOR
    // owner of each interact's cursor. Interact actions run on the host's UI
    // thread, so no locking is needed. Function-local static in an inline
    // function: one map per plug-in binary.
    typedef std::map<const Interact*, const InteractCursor*> OwnerMap;
    static OwnerMap& ownerMap()
    {
        static OwnerMap owners;

        return owners;
    }
#endif
};
} // namespace OFX

#endif // openfx_supportext_ofxsInteractCursor_h
