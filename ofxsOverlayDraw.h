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
 * Overlay drawing through OpenGL or the OFX 1.5 DrawSuite.
 *
 * OFX::OverlayDraw implements the small subset of the OpenGL 1.x immediate-mode
 * API used by the SupportExt overlays, with two backends:
 *
 * - OpenGL: every call is forwarded unchanged to the GL function of the same
 *   name, so overlays registered with kOfxImageEffectPluginPropOverlayInteractV1
 *   issue exactly the same OpenGL calls as before.
 * - DrawSuite (OfxDrawSuiteV1): the calls are emulated, and no OpenGL function
 *   is ever called. ofxInteract.h, kOfxInteractActionDraw: "If this is called
 *   via kOfxImageEffectPluginPropOverlayInteractV2, drawing MUST use DrawSuite."
 *   - Vertices are transformed on the CPU by the emulated modelview matrix
 *     (glPushMatrix, glTranslate, glRotate, glMultMatrix) and passed in
 *     canonical coordinates, like OpenGL overlay vertices.
 *   - Translations done in GL_PROJECTION mode are applied after the modelview
 *     matrix, in canonical coordinates (the OpenGL overlays use them for the
 *     one-pixel shadow, which they don't draw with the DrawSuite). glGetDoublev(GL_PROJECTION_MATRIX) and
 *     glGetIntegerv(GL_VIEWPORT) return a projection for which one viewport
 *     pixel is args.pixelScale ("The size of a real screen pixel under the
 *     interact's canonical projection").
 *   - glLineWidth and glPointSize values are in device pixels in the OpenGL
 *     overlays; the DrawSuite line width is in screen pixels (see "Sizes").
 *   - GL_POINTS have no DrawSuite primitive: each point is drawn as a filled
 *     square (or a filled disc with GL_POINT_SMOOTH) of the point size.
 *   - Colour is DrawSuite state, not per vertex: a primitive whose vertices
 *     have different colours is split (lines and points), or drawn with the
 *     colour OpenGL uses with flat shading (polygons: first vertex).
 *   - GL_LINE_STIPPLE maps to the closest OfxDrawLineStipplePattern.
 *   - Blending, smoothing and hints are left to the host.
 *   - TextRenderer::bitmapString maps to drawText (font chosen by the host).
 *
 * Which backend is used:
 * - the DrawSuite one if the overlay was created by a
 *   DefaultEffectOverlayDescriptorV2 (the plugin opted in with
 *   ofxsSetOverlayInteractDescriptorV2) and the host passed a draw context;
 * - OpenGL otherwise. Overlays registered as V1 keep using OpenGL even on
 *   hosts with a DrawSuite ("If this is called via
 *   kOfxImageEffectPluginPropOverlayInteractV1, drawing SHOULD use OpenGL"),
 *   and so do V2 descriptors registered as V1 because the host or the
 *   Support library has no DrawSuite.
 * - An overlay really registered as V2 that gets no draw context, or whose
 *   draw() was compiled without the DrawSuite backend, draws nothing:
 *   OverlayDraw::canDraw() is false, and draw() returns false.
 * - With an openfx that also registers V2 overlays as V1, through a separate
 *   OpenGL entry (OFXS_SUPPORT_HAS_OVERLAY_INTERACT_V1_ENTRY), each draw action
 *   decides: one called through the V1 entry
 *   (!DrawArgs::calledViaOverlayInteractV2()) draws with OpenGL, and with
 *   OFXS_SUPPORT_HAS_OPENGL_CONTEXT_CHECK draws nothing if no OpenGL context is current
 *   (that entry exists on DrawSuite hosts, which may have no OpenGL).
 * - V1 draws use OpenGL only, V2 draws the DrawSuite only ("If this is called via
 *   kOfxImageEffectPluginPropOverlayInteractV2, drawing MUST use DrawSuite. If this is
 *   called via kOfxImageEffectPluginPropOverlayInteractV1, drawing SHOULD use OpenGL"):
 *   a V1 draw never falls back to the DrawSuite.
 * - Shadows: the OpenGL overlays draw a one-pixel shadow; with the DrawSuite they don't
 *   (usesDrawSuite()): shadows are the host's job there.
 * Whether the overlay is registered as V2 is the result of
 * ImageEffectDescriptor::setOverlayInteractDescriptorV2(), which
 * ofxsSetOverlayInteractDescriptorV2() records per descriptor class: openfx
 * master also registers it as V1 when the host refuses
 * kOfxImageEffectPluginPropOverlayInteractV2. When nothing was recorded (the
 * overlay was registered with desc.setOverlayInteractDescriptorV2() directly),
 * it is V2 if, and only if, getImageEffectHostDescription()->supportsDrawSuite
 * (filled at kOfxActionLoad, before describe and instance creation), which is
 * wrong only on a DrawSuite host that refuses the V2 property: prefer
 * ofxsSetOverlayInteractDescriptorV2().
 *
 * Sizes: the OpenGL overlays multiply their line widths, point sizes and
 * pixel offsets by their screen pixel ratio, (hiDPI param ? 2 : 1) *
 * args.screenPixelRatio. They pass that ratio through
 * OverlayDraw::overlayScreenPixelRatio(), which returns it unchanged for
 * OpenGL, and 1 for the DrawSuite: "The host should adjust for screen
 * density", so neither the density nor the hiDPI param change the DrawSuite
 * sizes, and a missing kOfxInteractPropScreenPixelRatio (ratio 0) cannot give
 * zero sizes. (If an overlay does not call it, OverlayDraw divides its sizes
 * by args.screenPixelRatio, or 1 if it is <= 0.)
 *
 * How a plugin opts in (all the overlay's drawing must go through
 * OverlayDraw, including the draw() of any subclass or extra helper):
 *
 *     class MyOverlayDescriptorV2
 *         : public OFX::DefaultEffectOverlayDescriptorV2<MyOverlayDescriptorV2, MyInteract> {};
 *     ...
 *     OFX::ofxsSetOverlayInteractDescriptorV2(desc, new MyOverlayDescriptorV2);
 *
 * instead of desc.setOverlayInteractDescriptor(new MyOverlayDescriptor). The
 * SupportExt overlays provide ready-made V2 descriptors (e.g.
 * TransformOverlayDescriptorV2). ofxsSetOverlayInteractDescriptorV2 only
 * accepts V2 descriptors, so a V1 descriptor (which draws with OpenGL) cannot
 * be registered as V2 by mistake. The Support library's own
 * ImageEffectDescriptor::setOverlayInteractDescriptorV2() accepts any
 * descriptor: never call it with a V1 descriptor. Conversely, never register a
 * V2 descriptor with the V1 setOverlayInteractDescriptor(): on a DrawSuite host
 * its overlay would draw with the DrawSuite through a V1 registration (which
 * the spec discourages), or draw nothing on hosts that pass no draw context to
 * V1 overlays.
 *
 * Enabling the DrawSuite backend: it needs a Support library with OFX::DrawContext,
 * DrawArgs::drawContext and ImageEffectDescriptor::setOverlayInteractDescriptorV2
 * (ofxDrawSuite.h alone is not enough: the upstream Support library ships it
 * without that API). It is enabled automatically when the Support library defines
 * OFXS_SUPPORT_HAS_DRAWCONTEXT (NatronGitHub/openfx #87), or with
 * -DOFXS_HAVE_DRAWSUITE=1 for NatronGitHub openfx 05c5a75d and later without it;
 * -DOFXS_HAVE_DRAWSUITE=0 disables it. An empty value (-DOFXS_HAVE_DRAWSUITE=) is
 * an error. Without it, everything draws with OpenGL and
 * ofxsSetOverlayInteractDescriptorV2 registers V1 overlays.
 * OFXS_HAVE_DRAWSUITE must be a global compiler flag with the same value for
 * every translation unit (the plugins and the SupportExt sources), never a
 * #define in a source file: this rule is what guarantees correct drawing.
 * Mixing values is an ODR violation, and some inline functions that depend on
 * the value (e.g. the draw() of template interacts such as PositionInteract,
 * or the V2 descriptor classes of the SupportExt headers) may then be linked
 * with either variant. The rest only limits the damage: the macro-dependent
 * helpers and OverlayDraw live in an inline namespace named after the value
 * (C++11), MSVC reports mismatched object files at link time
 * (#pragma detect_mismatch), and an overlay registered as V2 whose draw() was
 * compiled without the DrawSuite draws nothing rather than call OpenGL.
 *
 * Consumers updating SupportExt: the class layouts of the SupportExt
 * interacts changed, so do a clean build (Makefile.master has no header
 * dependency tracking). The OpenGL headers now come through ofxsOverlayDraw.h
 * and the SupportExt interact headers: a translation unit that uses glad.h
 * must include it before them.
 */

#ifndef openfx_supportext_ofxsOverlayDraw_h
#define openfx_supportext_ofxsOverlayDraw_h

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32) && !defined(__MINGW32__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h> // for WINGDIAPI and APIENTRY, used by GL/gl.h
#endif

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "ofxsInteract.h"
#include "ofxsImageEffect.h"
#include "ofxsOGLTextRenderer.h"

// The openfx Support library tracks the keyboard modifiers of interacts
// (OFX::Interact::getKeyModifiers(), ofxNatron.h extension 13, openfx ec562898 and later):
// SupportExt's overlays then take Shift and Control from it, which also gives the modifiers
// pressed before the overlay got the focus when the host passes them. Older openfx: the
// overlays count the modifier keyDown/keyUp themselves, as before.
#if defined(OFX_EXTENSIONS_NATRON) && defined(kNatronOfxInteractPropKeyModifiers)
#define OFXS_HAVE_KEY_MODIFIERS 1
#endif

// DrawSuite backend: see "Enabling the DrawSuite backend" above.
// OFXS_OVERLAYDRAW_DS is the normalized 0/1 value.
#if defined(OFXS_HAVE_DRAWSUITE)
#if (1 - OFXS_HAVE_DRAWSUITE - 1) == 2
#error "OFXS_HAVE_DRAWSUITE is defined but empty: use -DOFXS_HAVE_DRAWSUITE=1 or -DOFXS_HAVE_DRAWSUITE=0"
#elif OFXS_HAVE_DRAWSUITE
#define OFXS_OVERLAYDRAW_DS 1
#else
#define OFXS_OVERLAYDRAW_DS 0
#endif
#elif defined(OFXS_SUPPORT_HAS_DRAWCONTEXT)
#define OFXS_OVERLAYDRAW_DS 1
#else
#define OFXS_OVERLAYDRAW_DS 0
#endif
#if OFXS_OVERLAYDRAW_DS && !defined(OFXS_SUPPORT_HAS_DRAWCONTEXT) && !defined(kOfxDrawSuite)
#error "OFXS_HAVE_DRAWSUITE=1 needs a Support library with the OFX 1.5 DrawSuite API (OFX::DrawContext, NatronGitHub openfx 05c5a75d or later)"
#endif

#if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900)
#define OFXS_OVERLAYDRAW_THREAD_LOCAL thread_local
#elif defined(_MSC_VER)
#define OFXS_OVERLAYDRAW_THREAD_LOCAL __declspec(thread)
#else
#define OFXS_OVERLAYDRAW_THREAD_LOCAL __thread
#endif

namespace OFX {
/// How an interact draws, decided by the descriptor that created it.
enum OverlayDrawModeEnum
{
    eOverlayDrawOpenGL = 0, //< V1 descriptor: always OpenGL
    eOverlayDrawSuiteFallback, //< V2 descriptor registered as V1 (no DrawSuite), or called through openfx's V1 entry: OpenGL
    eOverlayDrawSuite //< V2 descriptor registered as V2: DrawSuite only
};

/// The draw mode of the interact being created, on the creating thread (set by
/// DefaultEffectOverlayDescriptorV2::createInstance).
inline OverlayDrawModeEnum&
ofxsOverlayDrawConstructionMode()
{
    static OFXS_OVERLAYDRAW_THREAD_LOCAL OverlayDrawModeEnum mode = eOverlayDrawOpenGL;

    return mode;
}

/// Read it in the interact (or helper) constructor, and pass it to OverlayDraw.
inline OverlayDrawModeEnum
ofxsOverlayDrawModeForNewInteract()
{
    return ofxsOverlayDrawConstructionMode();
}

class OverlayDrawConstructionScope
{
public:
    explicit OverlayDrawConstructionScope(OverlayDrawModeEnum mode)
        : _previous( ofxsOverlayDrawConstructionMode() )
    {
        ofxsOverlayDrawConstructionMode() = mode;
    }

    ~OverlayDrawConstructionScope()
    {
        ofxsOverlayDrawConstructionMode() = _previous;
    }

private:
    OverlayDrawModeEnum _previous;
};

// Everything below depends on OFXS_OVERLAYDRAW_DS: put it in an inline namespace
// named after the value (C++11), and let MSVC check that all the object files
// agree. This limits the damage of mixed values, which are not allowed (see
// "Enabling the DrawSuite backend").
#ifdef _MSC_VER
#if OFXS_OVERLAYDRAW_DS
#pragma detect_mismatch("OFXS_OVERLAYDRAW_DS", "1")
#else
#pragma detect_mismatch("OFXS_OVERLAYDRAW_DS", "0")
#endif
#endif
#if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900)
#define OFXS_OVERLAYDRAW_INLINE_NAMESPACE 1
#if OFXS_OVERLAYDRAW_DS
inline namespace OverlayDrawWithDrawSuite {
#else
inline namespace OverlayDrawOpenGLOnly {
#endif
#endif
/// Is an overlay registered with setOverlayInteractDescriptorV2() registered as
/// V2, when its registration was not recorded? The DrawSuite test of
/// ImageEffectDescriptor::setOverlayInteractDescriptorV2().
inline bool
ofxsOverlayIsRegisteredAsV2()
{
#if OFXS_OVERLAYDRAW_DS
    return getImageEffectHostDescription()->supportsDrawSuite;
#else
    return false;
#endif
}

/// Base class of the overlay descriptors accepted by ofxsSetOverlayInteractDescriptorV2.
class EffectOverlayDescriptorV2
    : public EffectOverlayDescriptor
{
public:
    /// Called by ofxsSetOverlayInteractDescriptorV2() with the registration's result.
    virtual void setRegisteredAsV2(bool /*v2*/) {}
};

/// Overlay descriptor for overlays that draw through OverlayDraw: the interact
/// instances it creates use the DrawSuite when it is registered as V2.
/// Register it with ofxsSetOverlayInteractDescriptorV2() (or
/// desc.setOverlayInteractDescriptorV2()).
template<class DESC, class INSTANCE>
class DefaultEffectOverlayDescriptorV2
    : public EffectOverlayDescriptorV2, public InteractMainEntry<DESC>
{
public:
    Interact* createInstance(OfxInteractHandle handle,
                             ImageEffect *effect)
    {
        OverlayDrawConstructionScope scope(isRegisteredAsV2() ? eOverlayDrawSuite : eOverlayDrawSuiteFallback);

        return new INSTANCE(handle, effect);
    }

    // The registration is recorded per descriptor class (per overlay main entry), since the
    // host calls createInstance() on a static DESC (InteractMainEntry<DESC>), not on the
    // registered object. The host may refuse kOfxImageEffectPluginPropOverlayInteractV2 even
    // when it has the DrawSuite; the overlay is then registered as V1 and must use OpenGL.
    // A descriptor class shared by several plug-ins (e.g. TransformOverlayDescriptorV2) and
    // registered as V2 by any of them is V2, as in openfx's overlay registry: a V2 overlay
    // "MUST use DrawSuite".
    virtual void setRegisteredAsV2(bool v2)
    {
        int &r = registration();

        if (v2) {
            r = 1;
        } else if (r < 0) {
            r = 0;
        }
    }

    /// Was this descriptor class registered as V2? Without a record (it was registered with
    /// desc.setOverlayInteractDescriptorV2() directly), use the host's DrawSuite support,
    /// as before. That is wrong only if the host also refused the V2 property: use
    /// ofxsSetOverlayInteractDescriptorV2().
    static bool isRegisteredAsV2()
    {
        const int r = registration();

        return (r >= 0) ? (r != 0) : ofxsOverlayIsRegisteredAsV2();
    }

    virtual OfxPluginEntryPoint* getMainEntry() { return InteractMainEntry<DESC>::overlayInteractMainEntry; }

private:
    static int& registration() //< -1: unknown, 0: V1, 1: V2
    {
        static int r = -1;

        return r;
    }
};

/// Register an overlay as kOfxImageEffectPluginPropOverlayInteractV2 if the
/// host and the Support library have the DrawSuite (and, with openfx master,
/// the host accepts the property), else as
/// kOfxImageEffectPluginPropOverlayInteractV1. Returns true if it was
/// registered as V2, and records it for the descriptor class's instances.
/// Only V2 descriptors are accepted (see above).
inline bool
ofxsSetOverlayInteractDescriptorV2(ImageEffectDescriptor &desc,
                                   EffectOverlayDescriptorV2* overlay)
{
#if OFXS_OVERLAYDRAW_DS
    const bool v2 = desc.setOverlayInteractDescriptorV2(overlay); // takes ownership
#else
    desc.setOverlayInteractDescriptor(overlay); // takes ownership
    const bool v2 = false;
#endif
    if (overlay) {
        overlay->setRegisteredAsV2(v2); // desc owns overlay, which is still valid
    }

    return v2;
}

/// OpenGL 1.x immediate-mode subset, drawn with OpenGL or the DrawSuite.
/// One instance per draw action.
class OverlayDraw
{
public:
    /// mode: the interact's OverlayDrawModeEnum (ofxsOverlayDrawModeForNewInteract()
    /// in its constructor)
    OverlayDraw(const DrawArgs &args,
                OverlayDrawModeEnum mode)
        : _ctx(NULL)
        , _canDraw(true)
        , _pixelScale(args.pixelScale)
        , _screenPixelRatio(1.)
        , _mode(0)
        , _matrixMode(GL_MODELVIEW)
        , _stack()
        , _projOffsetX(0.)
        , _projOffsetY(0.)
        , _vertices()
        , _state()
        , _attribStack()
        , _ctxColour()
        , _ctxColourSet(false)
        , _ctxLineWidth(-1.f)
        , _ctxStipple(-1)
    {
#ifdef OFX_EXTENSIONS_NATRON
        // default size conversion, if the overlay does not call overlayScreenPixelRatio()
        if (args.screenPixelRatio > 0.) {
            _screenPixelRatio = args.screenPixelRatio;
        }
#endif
#ifdef OFXS_SUPPORT_HAS_OVERLAY_INTERACT_V1_ENTRY
        // openfx also registers a V2 overlay's class as V1 (a separate OpenGL entry): a draw
        // action called through that entry uses OpenGL only (it gets drawContext == NULL).
        // The per-call flag decides, not the registration.
        if ( (mode == eOverlayDrawSuite) && !args.calledViaOverlayInteractV2() ) {
            mode = eOverlayDrawSuiteFallback;
#ifdef OFXS_SUPPORT_HAS_OPENGL_CONTEXT_CHECK
            // that entry only exists on hosts with the DrawSuite, which may have no OpenGL: the
            // OpenGL calls need a current context (openfx: "test this first"), else draw
            // nothing (never the DrawSuite in a V1 draw)
            if ( !args.isOpenGLContextCurrent() ) {
                _canDraw = false;
            }
#endif
        }
#endif
#if OFXS_OVERLAYDRAW_DS
        if (mode == eOverlayDrawSuite) {
            _ctx = args.drawContext;
            // called as V2: "drawing MUST use DrawSuite", never OpenGL
            _canDraw = (_ctx != NULL);
        }
#else
        // compiled without the DrawSuite backend: OpenGL is not allowed for an
        // overlay called as V2 (OFXS_HAVE_DRAWSUITE differs between translation units)
        _canDraw = _canDraw && (mode != eOverlayDrawSuite);
#endif
        if (_ctx) {
            _stack.push_back( Mat4() );
        }
    }

    /// false if the overlay must not draw at all (called as V2 but no draw context, or
    /// through openfx's V1 entry without a current OpenGL context): draw() should then
    /// return false.
    bool canDraw() const { return _canDraw; }

    /// Call with the overlay's screen pixel ratio, i.e. the factor of its OpenGL
    /// line widths, point sizes and pixel offsets, and use the returned value
    /// instead:
    /// - OpenGL: the ratio is returned unchanged (the OpenGL path is unchanged);
    /// - DrawSuite: 1 is returned, since the host adjusts the DrawSuite sizes for
    ///   screen density. The hiDPI param and the host's screen pixel ratio thus
    ///   have no effect, and a ratio <= 0 (no kOfxInteractPropScreenPixelRatio)
    ///   cannot give zero sizes.
    double overlayScreenPixelRatio(double ratio)
    {
        if (_ctx) {
            _screenPixelRatio = 1.;

            return 1.;
        }
        _screenPixelRatio = (ratio > 0.) ? ratio : 1.;

        return ratio;
    }

    /// The host's screen pixel ratio (kOfxInteractPropScreenPixelRatio, a Natron
    /// extension), or 1 if the host does not set it: the Support library then
    /// reads 0, which would give zero OpenGL point sizes and line widths
    /// (glPointSize(0) and glLineWidth(0) are GL_INVALID_VALUE).
    static double hostScreenPixelRatio(const DrawArgs& args)
    {
#ifdef OFX_EXTENSIONS_NATRON
        return (args.screenPixelRatio > 0.) ? args.screenPixelRatio : 1.;
#else
        (void)args;

        return 1.;
#endif
    }

    /// true if drawing goes to the DrawSuite (no OpenGL call at all)
    bool usesDrawSuite() const { return _ctx != NULL; }

    // state
    void glEnable(GLenum cap) { if (_ctx) { enableDS(cap, true); } else { ::glEnable(cap); } }
    void glDisable(GLenum cap) { if (_ctx) { enableDS(cap, false); } else { ::glDisable(cap); } }
    void glHint(GLenum target, GLenum mode) { if (!_ctx) { ::glHint(target, mode); } }
    void glBlendFunc(GLenum sfactor, GLenum dfactor) { if (!_ctx) { ::glBlendFunc(sfactor, dfactor); } }
    void glLineWidth(GLfloat width)
    {
        if (_ctx) {
            _state.lineWidth = width;
        } else {
            ::glLineWidth(width);
        }
    }

    void glPointSize(GLfloat size) { if (_ctx) { _state.pointSize = size; } else { ::glPointSize(size); } }
    void glLineStipple(GLint factor, GLushort pattern)
    {
        if (_ctx) {
            _state.stippleFactor = factor;
            _state.stipplePattern = pattern;
        } else {
            ::glLineStipple(factor, pattern);
        }
    }

    void glPushAttrib(GLbitfield mask) { if (_ctx) { _attribStack.push_back(_state); } else { ::glPushAttrib(mask); } }
    void glPopAttrib()
    {
        if (_ctx) {
            if ( !_attribStack.empty() ) {
                _state = _attribStack.back();
                _attribStack.pop_back();
            }
        } else {
            ::glPopAttrib();
        }
    }

    // colour
    void glColor3f(GLfloat r, GLfloat g, GLfloat b) { if (_ctx) { setColourState(r, g, b, 1.f); } else { ::glColor3f(r, g, b); } }
    void glColor3d(GLdouble r, GLdouble g, GLdouble b) { if (_ctx) { setColourState( (float)r, (float)g, (float)b, 1.f ); } else { ::glColor3d(r, g, b); } }
    void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { if (_ctx) { setColourState(r, g, b, a); } else { ::glColor4f(r, g, b, a); } }

    // matrices
    void glMatrixMode(GLenum mode) { if (_ctx) { _matrixMode = mode; } else { ::glMatrixMode(mode); } }
    void glPushMatrix()
    {
        if (_ctx) {
            if (_matrixMode == GL_MODELVIEW) {
                _stack.push_back( _stack.back() );
            }
        } else {
            ::glPushMatrix();
        }
    }

    void glPopMatrix()
    {
        if (_ctx) {
            if ( (_matrixMode == GL_MODELVIEW) && (_stack.size() > 1) ) {
                _stack.pop_back();
            }
        } else {
            ::glPopMatrix();
        }
    }

    void glTranslated(GLdouble x, GLdouble y, GLdouble z) { if (_ctx) { translateDS(x, y); } else { ::glTranslated(x, y, z); } }
    void glTranslatef(GLfloat x, GLfloat y, GLfloat z) { if (_ctx) { translateDS(x, y); } else { ::glTranslatef(x, y, z); } }
    void glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
    {
        if (_ctx) {
            if ( (_matrixMode == GL_MODELVIEW) && (x == 0.) && (y == 0.) && (z != 0.) ) {
                double a = angle * 3.14159265358979323846264338327950288 / 180.;
                if (z < 0.) {
                    a = -a;
                }
                Mat4 r;
                r.m[0] = std::cos(a); r.m[1] = std::sin(a);
                r.m[4] = -std::sin(a); r.m[5] = std::cos(a);
                _stack.back() = _stack.back() * r;
            }
        } else {
            ::glRotated(angle, x, y, z);
        }
    }

    void glMultMatrixd(const GLdouble *m)
    {
        if (_ctx) {
            if (_matrixMode == GL_MODELVIEW) {
                Mat4 r;
                std::memcpy( r.m, m, sizeof(r.m) );
                _stack.back() = _stack.back() * r;
            }
        } else {
            ::glMultMatrixd(m);
        }
    }

    void glGetDoublev(GLenum pname, GLdouble *params)
    {
        if (_ctx) {
            // a projection for which one viewport pixel (see glGetIntegerv) is pixelScale
            if (pname == GL_PROJECTION_MATRIX) {
                for (int i = 0; i < 16; ++i) {
                    params[i] = (i % 5 == 0) ? 1. : 0.;
                }
                params[0] = (_pixelScale.x != 0.) ? 2. / _pixelScale.x : 1.;
                params[5] = (_pixelScale.y != 0.) ? 2. / _pixelScale.y : 1.;
            }
        } else {
            ::glGetDoublev(pname, params);
        }
    }

    void glGetIntegerv(GLenum pname, GLint *params)
    {
        if (_ctx) {
            if (pname == GL_VIEWPORT) {
                params[0] = params[1] = 0;
                params[2] = params[3] = 1;
            }
        } else {
            ::glGetIntegerv(pname, params);
        }
    }

    // primitives
    void glBegin(GLenum mode)
    {
        if (_ctx) {
            _mode = mode;
            _vertices.clear();
        } else {
            ::glBegin(mode);
        }
    }

    void glVertex2d(GLdouble x, GLdouble y)
    {
        if (_ctx) {
            Vertex v;
            transformDS(x, y, &v.p);
            v.c = _state.colour;
            _vertices.push_back(v);
        } else {
            ::glVertex2d(x, y);
        }
    }

    void glVertex2f(GLfloat x, GLfloat y) { if (_ctx) { glVertex2d(x, y); } else { ::glVertex2f(x, y); } }

    void glEnd()
    {
        if (_ctx) {
            flushDS();
        } else {
            ::glEnd();
        }
    }

    /// TextRenderer::bitmapString(x, y, string, font): the DrawSuite draws the
    /// text with the host's font, left-aligned on the baseline at (x, y).
    void bitmapString(double x,
                      double y,
                      const char* string,
                      TextRenderer::Font font = TextRenderer::FONT_HELVETICA_12)
    {
#if OFXS_OVERLAYDRAW_DS
        if (_ctx) {
            OfxPointD p;
            transformDS(x, y, &p);
            applyColourDS(_state.colour);
            _ctx->drawText(string, p, kOfxDrawTextAlignmentLeft | kOfxDrawTextAlignmentBaseline);

            return;
        }
#endif
        TextRenderer::bitmapString(x, y, string, font);
    }

private:
    struct Colour
    {
        float r, g, b, a;
        Colour() : r(1.f), g(1.f), b(1.f), a(1.f) {}

        bool operator==(const Colour &o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }

        bool operator!=(const Colour &o) const { return !(*this == o); }
    };

    struct State
    {
        Colour colour;
        float lineWidth;
        float pointSize;
        bool stipple;
        bool pointSmooth;
        GLint stippleFactor;
        GLushort stipplePattern;
        State() : colour(), lineWidth(1.f), pointSize(1.f), stipple(false), pointSmooth(false), stippleFactor(1), stipplePattern(0xFFFF) {}
    };

    struct Mat4 // column-major, like OpenGL
    {
        double m[16];
        Mat4() { for (int i = 0; i < 16; ++i) { m[i] = (i % 5 == 0) ? 1. : 0.; } }

        Mat4 operator*(const Mat4 &o) const
        {
            Mat4 r;

            for (int c = 0; c < 4; ++c) {
                for (int l = 0; l < 4; ++l) {
                    double s = 0.;
                    for (int k = 0; k < 4; ++k) {
                        s += m[k * 4 + l] * o.m[c * 4 + k];
                    }
                    r.m[c * 4 + l] = s;
                }
            }

            return r;
        }
    };

    struct Vertex
    {
        OfxPointD p; // canonical coordinates
        Colour c;
    };

    void setColourState(float r,
                        float g,
                        float b,
                        float a)
    {
        _state.colour.r = r;
        _state.colour.g = g;
        _state.colour.b = b;
        _state.colour.a = a;
    }

    void enableDS(GLenum cap,
                  bool on)
    {
        if (cap == GL_LINE_STIPPLE) {
            _state.stipple = on;
        } else if (cap == GL_POINT_SMOOTH) {
            _state.pointSmooth = on;
        }
    }

    void translateDS(double x,
                     double y)
    {
        if (_matrixMode == GL_PROJECTION) {
            // applied after the modelview matrix, in canonical coordinates
            _projOffsetX += x;
            _projOffsetY += y;
        } else if (_matrixMode == GL_MODELVIEW) {
            Mat4 t;
            t.m[12] = x;
            t.m[13] = y;
            _stack.back() = _stack.back() * t;
        }
    }

    void transformDS(double x,
                     double y,
                     OfxPointD* p) const
    {
        const double* m = _stack.back().m;
        double w = m[3] * x + m[7] * y + m[15];

        if (w == 0.) {
            w = 1.;
        }
        p->x = (m[0] * x + m[4] * y + m[12]) / w + _projOffsetX;
        p->y = (m[1] * x + m[5] * y + m[13]) / w + _projOffsetY;
    }

    // send the line width and stipple to the host if they changed since the last
    // primitive (they are DrawSuite state, and the initial host state is unknown)
    void syncLineStateDS()
    {
#if OFXS_OVERLAYDRAW_DS
        // OpenGL widths are in device pixels; the host adjusts the DrawSuite width for screen density
        float width = (float)(_state.lineWidth / _screenPixelRatio);
        if (width != _ctxLineWidth) {
            _ctx->setLineWidth(width);
            _ctxLineWidth = width;
        }
        OfxDrawLineStipplePattern p = kOfxDrawLineStipplePatternSolid;
        if ( _state.stipple && (_state.stipplePattern != 0xFFFF) ) {
            // longest "on" run, in pixels
            int longest = 0, run = 0;
            for (int i = 0; i < 32; ++i) {
                if ( (_state.stipplePattern >> (i % 16)) & 1 ) {
                    ++run;
                    longest = (std::max)(longest, run);
                } else {
                    run = 0;
                }
            }
            longest = (std::min)(longest, 16) * (std::max)(1, (int)_state.stippleFactor);
            p = (longest <= 2) ? kOfxDrawLineStipplePatternDot : kOfxDrawLineStipplePatternDash;
        }
        if ( (int)p != _ctxStipple ) {
            _ctx->setLineStipple(p);
            _ctxStipple = (int)p;
        }
#else
        (void)_ctxLineWidth;
        (void)_ctxStipple;
#endif
    }

    void applyColourDS(const Colour &c)
    {
#if OFXS_OVERLAYDRAW_DS
        if ( !_ctxColourSet || (c != _ctxColour) ) {
            _ctx->setColour(c.r, c.g, c.b, c.a);
            _ctxColour = c;
            _ctxColourSet = true;
        }
#else
        (void)c;
        (void)_ctxColour;
        (void)_ctxColourSet;
        (void)_screenPixelRatio;
#endif
    }

#if OFXS_OVERLAYDRAW_DS
    void drawDS(OfxDrawPrimitive prim,
                const std::vector<OfxPointD> &pts)
    {
        if ( !pts.empty() ) {
            syncLineStateDS();
            _ctx->draw( prim, &pts.front(), (int)pts.size() );
        }
    }

    // point size in canonical units (half extent), from the OpenGL size in device pixels
    void drawPointDS(const Vertex &v)
    {
        double half = (_state.pointSize / _screenPixelRatio) / 2.;
        double hx = half * _pixelScale.x;
        double hy = half * _pixelScale.y;

        applyColourDS(v.c);
        std::vector<OfxPointD> pts;
        if (_state.pointSmooth) {
            const int n = 12;
            pts.resize(n);
            for (int i = 0; i < n; ++i) {
                double a = i * 2. * 3.14159265358979323846264338327950288 / n;
                pts[i].x = v.p.x + hx * std::cos(a);
                pts[i].y = v.p.y + hy * std::sin(a);
            }
            drawDS(kOfxDrawPrimitivePolygon, pts);
        } else {
            pts.resize(2);
            pts[0].x = v.p.x - hx; pts[0].y = v.p.y - hy;
            pts[1].x = v.p.x + hx; pts[1].y = v.p.y + hy;
            drawDS(kOfxDrawPrimitiveRectangle, pts);
        }
    }

    // draw segments (a[i], b[i]) with the colour of b[i] (OpenGL flat shading uses
    // the last vertex of a line), grouping consecutive segments of the same colour
    void drawSegmentsDS(const std::vector<const Vertex*> &ends)
    {
        std::vector<OfxPointD> pts;
        Colour current;

        for (size_t i = 0; i + 1 < ends.size(); i += 2) {
            const Colour &c = ends[i + 1]->c;
            if ( !pts.empty() && (c != current) ) {
                applyColourDS(current);
                drawDS(kOfxDrawPrimitiveLines, pts);
                pts.clear();
            }
            current = c;
            pts.push_back(ends[i]->p);
            pts.push_back(ends[i + 1]->p);
        }
        if ( !pts.empty() ) {
            applyColourDS(current);
            drawDS(kOfxDrawPrimitiveLines, pts);
        }
    }

    bool uniformColour() const
    {
        for (size_t i = 1; i < _vertices.size(); ++i) {
            if (_vertices[i].c != _vertices[0].c) {
                return false;
            }
        }

        return true;
    }

    std::vector<OfxPointD> points(size_t first,
                                  size_t count) const
    {
        std::vector<OfxPointD> pts(count);

        for (size_t i = 0; i < count; ++i) {
            pts[i] = _vertices[first + i].p;
        }

        return pts;
    }

#endif // if OFXS_OVERLAYDRAW_DS

    void flushDS()
    {
#if OFXS_OVERLAYDRAW_DS
        const size_t n = _vertices.size();
        std::vector<const Vertex*> ends;

        switch (_mode) {
        case GL_POINTS:
            for (size_t i = 0; i < n; ++i) {
                drawPointDS(_vertices[i]);
            }
            break;
        case GL_LINES:
            for (size_t i = 0; i + 1 < n; i += 2) {
                ends.push_back(&_vertices[i]);
                ends.push_back(&_vertices[i + 1]);
            }
            drawSegmentsDS(ends);
            break;
        case GL_LINE_STRIP:
        case GL_LINE_LOOP:
            if (n < 2) {
                break;
            }
            if ( uniformColour() ) {
                applyColourDS(_vertices[0].c);
                drawDS(_mode == GL_LINE_STRIP ? kOfxDrawPrimitiveLineStrip : kOfxDrawPrimitiveLineLoop, points(0, n) );
            } else {
                for (size_t i = 0; i + 1 < n; ++i) {
                    ends.push_back(&_vertices[i]);
                    ends.push_back(&_vertices[i + 1]);
                }
                if (_mode == GL_LINE_LOOP) {
                    ends.push_back(&_vertices[n - 1]);
                    ends.push_back(&_vertices[0]);
                }
                drawSegmentsDS(ends);
            }
            break;
        case GL_POLYGON:
            if (n >= 3) {
                // flat shading: the colour of the first vertex
                applyColourDS(_vertices[0].c);
                drawDS( kOfxDrawPrimitivePolygon, points(0, n) );
            }
            break;
        case GL_TRIANGLES:
            for (size_t i = 0; i + 2 < n; i += 3) {
                applyColourDS(_vertices[i + 2].c);
                drawDS( kOfxDrawPrimitivePolygon, points(i, 3) );
            }
            break;
        case GL_QUADS:
            for (size_t i = 0; i + 3 < n; i += 4) {
                applyColourDS(_vertices[i + 3].c);
                drawDS( kOfxDrawPrimitivePolygon, points(i, 4) );
            }
            break;
        default:
            assert(false); // not used by the SupportExt overlays
            break;
        }
#endif // if OFXS_OVERLAYDRAW_DS
        _vertices.clear();
        _mode = 0;
    }

#if OFXS_OVERLAYDRAW_DS
    const DrawContext* _ctx; // args.drawContext (V2 draws), NULL for OpenGL
#else
    const void* _ctx; // always NULL
#endif
    bool _canDraw;
    OfxPointD _pixelScale;
    double _screenPixelRatio;
    GLenum _mode;
    GLenum _matrixMode;
    std::vector<Mat4> _stack;
    double _projOffsetX, _projOffsetY;
    std::vector<Vertex> _vertices;
    State _state;
    std::vector<State> _attribStack;
    Colour _ctxColour;
    bool _ctxColourSet;
    float _ctxLineWidth; // line width sent to the host, -1 if none yet
    int _ctxStipple; // OfxDrawLineStipplePattern sent to the host, -1 if none yet
};
#ifdef OFXS_OVERLAYDRAW_INLINE_NAMESPACE
} // inline namespace
#endif
} // namespace OFX

#endif // openfx_supportext_ofxsOverlayDraw_h
