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
 * OFX utilities for tracking.
 */

#include "ofxsTracking.h"
#include <cmath>

#include "ofxsOGLTextRenderer.h"

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#define kSupportsTiles 1
#define kSupportsMultiResolution 1
#define kSupportsRenderScale 0 // we need full-res images
#define kRenderThreadSafety eRenderFullySafe

#define POINT_SIZE 5
#define POINT_TOLERANCE 6
#define HANDLE_SIZE 6

using namespace OFX;
using std::string;

namespace OFX {
GenericTrackerPlugin::GenericTrackerPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
    , _dstClip(NULL)
    , _srcClip(NULL)
    , _backwardButton(NULL)
    , _prevButton(NULL)
    , _nextButton(NULL)
    , _forwardButton(NULL)
    , _instanceName(NULL)
{
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    assert(_dstClip->getPixelComponents() == ePixelComponentAlpha ||
           _dstClip->getPixelComponents() == ePixelComponentRGB ||
           _dstClip->getPixelComponents() == ePixelComponentRGBA);
    _srcClip = getContext() == eContextGenerator ? NULL : fetchClip(kOfxImageEffectSimpleSourceClipName);
    assert( (!_srcClip && getContext() == eContextGenerator) ||
            (_srcClip->getPixelComponents() == ePixelComponentAlpha ||
             _srcClip->getPixelComponents() == ePixelComponentRGB ||
             _srcClip->getPixelComponents() == ePixelComponentRGBA) );


    _backwardButton = fetchPushButtonParam(kParamTrackingBackward);
    _prevButton = fetchPushButtonParam(kParamTrackingPrevious);
    _nextButton = fetchPushButtonParam(kParamTrackingNext);
    _forwardButton = fetchPushButtonParam(kParamTrackingForward);
    _instanceName = fetchStringParam(kNatronOfxParamStringSublabelName);
    assert(_backwardButton && _prevButton && _nextButton && _forwardButton && _instanceName);
}

bool
GenericTrackerPlugin::isIdentity(const IsIdentityArguments &args,
                                 Clip * &identityClip,
                                 double &identityTime, int& /*view*/, std::string& /*plane*/)
{
    if ( !kSupportsRenderScale && ( (args.renderScale.x != 1.) || (args.renderScale.y != 1.) ) ) {
        throwSuiteStatusException(kOfxStatFailed);

        return false;
    }

    identityClip = _srcClip;
    identityTime = args.time;

    return true;
}

void
GenericTrackerPlugin::changedParam(const InstanceChangedArgs &args,
                                   const string &paramName)
{
    if ( !kSupportsRenderScale && ( (args.renderScale.x != 1.) || (args.renderScale.y != 1.) ) ) {
        throwSuiteStatusException(kOfxStatFailed);

        return;
    }

    TrackArguments trackArgs;
    trackArgs.renderScale = args.renderScale;
    if ( (paramName == kParamTrackingBackward) && _srcClip && _srcClip->isConnected() ) {
        trackArgs.first = args.time;
        //double first,last; timeLineGetBounds(first, last); trackArgs.last = first + 1;// wrong: we want the srcClip range
        OfxRangeD range = _srcClip->getFrameRange();
        trackArgs.last = range.min + 1;
        if (trackArgs.last <= trackArgs.first) {
            trackArgs.forward = false;
            trackArgs.reason = args.reason;
            trackRange(trackArgs);
        }
    } else if ( (paramName == kParamTrackingPrevious) && _srcClip && _srcClip->isConnected() ) {
        trackArgs.first = args.time;
        trackArgs.last = trackArgs.first;
        trackArgs.forward = false;
        trackArgs.reason = args.reason;
        trackRange(trackArgs);
    } else if ( (paramName == kParamTrackingNext) && _srcClip && _srcClip->isConnected() ) {
        trackArgs.first = args.time;
        trackArgs.last = trackArgs.first;
        trackArgs.forward = true;
        trackArgs.reason = args.reason;
        trackRange(trackArgs);
    } else if ( (paramName == kParamTrackingForward) && _srcClip && _srcClip->isConnected() ) {
        trackArgs.first = args.time;
        //double first,last; timeLineGetBounds(first, last); trackArgs.last = last - 1; // wrong: we want the srcClip range
        OfxRangeD range = _srcClip->getFrameRange();
        trackArgs.last = range.max - 1;
        if (trackArgs.last >= trackArgs.first) {
            trackArgs.forward = true;
            trackArgs.reason = args.reason;
            trackRange(trackArgs);
        }
    }
}

bool
GenericTrackerPlugin::getRegionOfDefinition(const RegionOfDefinitionArguments &args,
                                            OfxRectD & /*rod*/)
{
    if ( !kSupportsRenderScale && ( (args.renderScale.x != 1.) || (args.renderScale.y != 1.) ) ) {
        throwSuiteStatusException(kOfxStatFailed);
    }

    return false;
}

void
genericTrackerDescribe(ImageEffectDescriptor &desc)
{
    desc.addSupportedContext(eContextGeneral);
    desc.addSupportedContext(eContextFilter);
    desc.addSupportedContext(eContextTracker);
    // supported bit depths depend on the tracking algorithm.
    //desc.addSupportedBitDepth(eBitDepthUByte);
    //desc.addSupportedBitDepth(eBitDepthUShort);
    //desc.addSupportedBitDepth(eBitDepthFloat);

    // single instance depends on the algorithm
    //desc.setSingleInstance(false);

    // no host frame threading (anyway, the tracker always returns identity)
    desc.setHostFrameThreading(false);

    ///We do temporal clip access
    desc.setTemporalClipAccess(true);

    // rendertwicealways must be set to true if the tracker cannot handle interlaced content (most don't)
    //desc.setRenderTwiceAlways(true);

    desc.setSupportsMultipleClipPARs(false);

    // support multithread (anyway, the tracker always returns identity)
    desc.setRenderThreadSafety(kRenderThreadSafety);

    // support tiles (anyway, the tracker always returns identity)
    desc.setSupportsTiles(kSupportsTiles);

    // in order to support render scale, render() must take into account the pixelaspectratio and the renderscale
    // and scale the transform appropriately.
    // All other functions are usually in canonical coordinates.

    ///We support multi-resolution (which does not mean we support render scale)
    desc.setSupportsMultiResolution(kSupportsMultiResolution);
#ifdef OFX_EXTENSIONS_NATRON
    desc.setChannelSelector(ePixelComponentNone);
#endif
}

PageParamDescriptor*
genericTrackerDescribeInContextBegin(ImageEffectDescriptor &desc,
                                     ContextEnum /*context*/)
{
    // Source clip only in the filter context
    // create the mandated source clip
    // always declare the source clip first, because some hosts may consider
    // it as the default input clip (e.g. Nuke)
    ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);

    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentAlpha);

    ///we do temporal clip access
    srcClip->setTemporalClipAccess(true);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setIsMask(false);
    srcClip->setOptional(false);

    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setSupportsTiles(kSupportsTiles);


    // make some pages and to things in
    PageParamDescriptor *page = desc.definePageParam("Controls");

    return page;
}

void
genericTrackerDescribePointParameters(ImageEffectDescriptor &desc,
                                      PageParamDescriptor* page)
{
    ///Declare the name first so that in Natron it appears as the first column in the multi instance
    // name
    {
        StringParamDescriptor* param = desc.defineStringParam(kParamTrackingLabel);
        param->setLabel(kParamTrackingLabelLabel);
        param->setHint(kParamTrackingLabelHint);
        param->setDefault(kParamTrackingLabelDefault);
        param->setInstanceSpecific(true);
        ////param->setIsSecret(false); // it has to be user-editable
        ////param->setEnabled(true); // it has to be user-editable
        ////param->setIsPersistent(true); // it has to be saved with the instance parameters
        param->setEvaluateOnChange(false); // it is meaningless
        if (page) {
            page->addChild(*param);
        }
    }


    // backward
    {
        PushButtonParamDescriptor* param = desc.definePushButtonParam(kParamTrackingBackward);
        param->setLabel(kParamTrackingBackwardLabel);
        param->setHint(kParamTrackingBackwardHint);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }

    // prev
    {
        PushButtonParamDescriptor* param = desc.definePushButtonParam(kParamTrackingPrevious);
        param->setLabel(kParamTrackingPreviousLabel);
        param->setHint(kParamTrackingPreviousHint);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }

    // next
    {
        PushButtonParamDescriptor* param = desc.definePushButtonParam(kParamTrackingNext);
        param->setLabel(kParamTrackingNextLabel);
        param->setHint(kParamTrackingNextHint);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }

    // forward
    {
        PushButtonParamDescriptor* param = desc.definePushButtonParam(kParamTrackingForward);
        param->setLabel(kParamTrackingForwardLabel);
        param->setHint(kParamTrackingForwardHint);
        if (page) {
            page->addChild(*param);
        }
    }
} // genericTrackerDescribePointParameters

//////////////////// INTERACT ////////////////////

static bool
isNearby(const OfxPointD & p,
         double x,
         double y,
         double tolerance,
         const OfxPointD & pscale)
{
    return std::fabs(p.x - x) <= tolerance * pscale.x &&  std::fabs(p.y - y) <= tolerance * pscale.y;
}

bool
TrackerRegionInteract::draw(const DrawArgs &args)
{
    OfxRGBColourD color = { 0.8, 0.8, 0.8 };

    getSuggestedColour(color);
    const OfxPointD& pscale = args.pixelScale;
    // all drawing goes through d: OpenGL, or the DrawSuite for V2 overlays
    OverlayDraw d(args, _drawMode);
    if ( !d.canDraw() ) {
        return false; // registered as V2 but no draw context: OpenGL is not allowed
    }
    GLdouble projection[16];
    d.glGetDoublev( GL_PROJECTION_MATRIX, projection);
    GLint viewport[4];
    d.glGetIntegerv(GL_VIEWPORT, viewport);
    OfxPointD shadow; // how much to translate GL_PROJECTION to get exactly one pixel on screen
    // one pixel in the projection; 0 for a degenerate viewport or projection (inf, then
    // inf - inf = NaN in the shadow translations)
    {
        const double denx = projection[0] * viewport[2];
        const double deny = projection[5] * viewport[3];
        shadow.x = (denx != 0.) ? 2. / denx : 0.;
        shadow.y = (deny != 0.) ? 2. / deny : 0.;
    }

    double xi1, xi2, yi1, yi2, xo1, xo2, yo1, yo2, xc, yc, xoff, yoff;

    if (_ms != eMouseStateIdle) {
        xi1 = _innerBtmLeftDragPos.x;
        yi1 = _innerBtmLeftDragPos.y;
        xi2 = _innerTopRightDragPos.x;
        yi2 = _innerTopRightDragPos.y;
        xo1 = _outerBtmLeftDragPos.x;
        yo1 = _outerBtmLeftDragPos.y;
        xo2 = _outerTopRightDragPos.x;
        yo2 = _outerTopRightDragPos.y;
        xc = _centerDragPos.x;
        yc = _centerDragPos.y;
        xoff = _offsetDragPos.x;
        yoff = _offsetDragPos.y;
    } else {
        _innerBtmLeft->getValueAtTime( args.time, xi1, yi1);
        _innerTopRight->getValueAtTime(args.time, xi2, yi2);
        _outerBtmLeft->getValueAtTime( args.time, xo1, yo1);
        _outerTopRight->getValueAtTime(args.time, xo2, yo2);
        _center->getValueAtTime(args.time, xc, yc);
        _offset->getValueAtTime(args.time, xoff, yoff);
        ///innerBtmLeft and outerBtmLeft are relative to the center, make them absolute
        xi1 += (xc + xoff);
        yi1 += (yc + yoff);
        xi2 += (xc + xoff);
        yi2 += (yc + yoff);
        xo1 += (xc + xoff);
        yo1 += (yc + yoff);
        xo2 += (xc + xoff);
        yo2 += (yc + yoff);
    }

    bool hiDPI = _hiDPI ? _hiDPI->getValue() : false;
    double screenPixelRatio = hiDPI ? 2 : 1;
#ifdef OFX_EXTENSIONS_NATRON
    screenPixelRatio *= OverlayDraw::hostScreenPixelRatio(args); // 1 if the host does not set it
    hiDPI |= args.screenPixelRatio > 1;
#endif
    screenPixelRatio = d.overlayScreenPixelRatio(screenPixelRatio); // DrawSuite: 1 if <= 0
    TextRenderer::Font font = hiDPI ? TextRenderer::FONT_TIMES_ROMAN_24 : TextRenderer::FONT_HELVETICA_12;

    //glPushAttrib(GL_ALL_ATTRIB_BITS); // caller is responsible for protecting attribs

    d.glDisable(GL_LINE_STIPPLE);
    d.glEnable(GL_LINE_SMOOTH);
    d.glDisable(GL_POINT_SMOOTH);
    d.glEnable(GL_BLEND);
    d.glHint(GL_LINE_SMOOTH_HINT, GL_DONT_CARE);
    d.glLineWidth(static_cast<GLfloat>(1.5 * screenPixelRatio));
    d.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw everything twice
    // l = 0: shadow
    // l = 1: drawing
    for (int l = 0; l < 2; ++l) {
        // shadow (uses GL_PROJECTION)
        d.glMatrixMode(GL_PROJECTION);
        int direction = (l == 0) ? 1 : -1;
        // translate (1,-1) pixels
        d.glTranslated(direction * shadow.x, -direction * shadow.y, 0);
        d.glMatrixMode(GL_MODELVIEW); // Modelview should be used on Nuke

        d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
        d.glBegin(GL_LINE_LOOP);
        d.glVertex2d(xi1, yi1);
        d.glVertex2d(xi1, yi2);
        d.glVertex2d(xi2, yi2);
        d.glVertex2d(xi2, yi1);
        d.glEnd();

        d.glBegin(GL_LINE_LOOP);
        d.glVertex2d(xo1, yo1);
        d.glVertex2d(xo1, yo2);
        d.glVertex2d(xo2, yo2);
        d.glVertex2d(xo2, yo1);
        d.glEnd();

        d.glPointSize(static_cast<GLfloat>(POINT_SIZE * screenPixelRatio));
        d.glBegin(GL_POINTS);

        ///draw center
        if ( (_ds == eDrawStateHoveringCenter) || (_ms == eMouseStateDraggingCenter) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
        }
        d.glVertex2d(xc, yc);

        if ( (xoff != 0) || (yoff != 0) ) {
            d.glVertex2d(xc + xoff, yc + yoff);
        }

        //////DRAWING INNER POINTS
        if ( (_ds == eDrawStateHoveringInnerBtmLeft) || (_ms == eMouseStateDraggingInnerBtmLeft) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xi1, yi1);
        }
        if ( (_ds == eDrawStateHoveringInnerBtmMid) || (_ms == eMouseStateDraggingInnerBtmMid) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xc + xoff, yi1);
        }
        if ( (_ds == eDrawStateHoveringInnerBtmRight) || (_ms == eMouseStateDraggingInnerBtmRight) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xi2, yi1);
        }
        if ( (_ds == eDrawStateHoveringInnerMidLeft) || (_ms == eMouseStateDraggingInnerMidLeft) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xi1, yc + yoff);
        }
        if ( (_ds == eDrawStateHoveringInnerMidRight) || (_ms == eMouseStateDraggingInnerMidRight) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xi2, yc + yoff);
        }
        if ( (_ds == eDrawStateHoveringInnerTopLeft) || (_ms == eMouseStateDraggingInnerTopLeft) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xi1, yi2);
        }

        if ( (_ds == eDrawStateHoveringInnerTopMid) || (_ms == eMouseStateDraggingInnerTopMid) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xc + xoff, yi2);
        }

        if ( (_ds == eDrawStateHoveringInnerTopRight) || (_ms == eMouseStateDraggingInnerTopRight) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xi2, yi2);
        }


        //////DRAWING OUTTER POINTS

        if ( (_ds == eDrawStateHoveringOuterBtmLeft) || (_ms == eMouseStateDraggingOuterBtmLeft) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xo1, yo1);
        }
        if ( (_ds == eDrawStateHoveringOuterBtmMid) || (_ms == eMouseStateDraggingOuterBtmMid) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xc + xoff, yo1);
        }
        if ( (_ds == eDrawStateHoveringOuterBtmRight) || (_ms == eMouseStateDraggingOuterBtmRight) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xo2, yo1);
        }
        if ( (_ds == eDrawStateHoveringOuterMidLeft) || (_ms == eMouseStateDraggingOuterMidLeft) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xo1, yc + yoff);
        }
        if ( (_ds == eDrawStateHoveringOuterMidRight) || (_ms == eMouseStateDraggingOuterMidRight) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xo2, yc + yoff);
        }

        if ( (_ds == eDrawStateHoveringOuterTopLeft) || (_ms == eMouseStateDraggingOuterTopLeft) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xo1, yo2);
        }
        if ( (_ds == eDrawStateHoveringOuterTopMid) || (_ms == eMouseStateDraggingOuterTopMid) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xc + xoff, yo2);
        }
        if ( (_ds == eDrawStateHoveringOuterTopRight) || (_ms == eMouseStateDraggingOuterTopRight) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
            d.glVertex2d(xo2, yo2);
        }

        d.glEnd();

        if ( (xoff != 0) || (yoff != 0) ) {
            d.glBegin(GL_LINES);
            d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
            d.glVertex2d(xc, yc);
            d.glVertex2d(xc + xoff, yc + yoff);
            d.glEnd();
        }

        double handleSizeX = HANDLE_SIZE * pscale.x;
        double handleSizeY = HANDLE_SIZE * pscale.y;

        ///now show small lines at handle positions
        d.glBegin(GL_LINES);

        if ( (_ds == eDrawStateHoveringInnerMidLeft) || (_ms == eMouseStateDraggingInnerMidLeft) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
        }
        d.glVertex2d(xi1, yc + yoff);
        d.glVertex2d(xi1 - handleSizeX, yc + yoff);

        if ( (_ds == eDrawStateHoveringInnerTopMid) || (_ms == eMouseStateDraggingInnerTopMid) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
        }
        d.glVertex2d(xc + xoff, yi2);
        d.glVertex2d(xc + xoff, yi2 + handleSizeY);

        if ( (_ds == eDrawStateHoveringInnerMidRight) || (_ms == eMouseStateDraggingInnerMidRight) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
        }
        d.glVertex2d(xi2, yc + yoff);
        d.glVertex2d(xi2 + handleSizeX, yc + yoff);

        if ( (_ds == eDrawStateHoveringInnerBtmMid) || (_ms == eMouseStateDraggingInnerBtmMid) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
        }
        d.glVertex2d(xc + xoff, yi1);
        d.glVertex2d(xc + xoff, yi1 - handleSizeY);

        //////DRAWING OUTTER HANDLES

        if ( (_ds == eDrawStateHoveringOuterMidLeft) || (_ms == eMouseStateDraggingOuterMidLeft) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
        }
        d.glVertex2d(xo1, yc + yoff);
        d.glVertex2d(xo1 - handleSizeX, yc + yoff);

        if ( (_ds == eDrawStateHoveringOuterTopMid) || (_ms == eMouseStateDraggingOuterTopMid) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
        }
        d.glVertex2d(xc + xoff, yo2);
        d.glVertex2d(xc + xoff, yo2 + handleSizeY);

        if ( (_ds == eDrawStateHoveringOuterMidRight) || (_ms == eMouseStateDraggingOuterMidRight) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
        }
        d.glVertex2d(xo2 + handleSizeX, yc + yoff);
        d.glVertex2d(xo2, yc + yoff);

        if ( (_ds == eDrawStateHoveringOuterBtmMid) || (_ms == eMouseStateDraggingOuterBtmMid) ) {
            d.glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
        }
        d.glVertex2d(xc + xoff, yo1);
        d.glVertex2d(xc + xoff, yo1 - handleSizeY);
        d.glEnd();


        d.glColor3f( (float)color.r * l, (float)color.g * l, (float)color.b * l );
        string name;
        _name->getValue(name);
        d.bitmapString(xc, yc + POINT_SIZE * screenPixelRatio, name.c_str(), font);
    }

    //glPopAttrib();

    return true;
} // draw

void
TrackerRegionInteract::updateCursor()
{
    const char* name = NULL;

    switch (_ms) {
    case eMouseStateIdle:
        switch (_ds) {
        case eDrawStateInactive:
            break;
        case eDrawStateHoveringCenter:
            name = kOfxsCursorMove;
            break;
        case eDrawStateHoveringInnerTopLeft:
        case eDrawStateHoveringInnerBtmRight:
        case eDrawStateHoveringOuterTopLeft:
        case eDrawStateHoveringOuterBtmRight:
            name = kOfxsCursorNWSEResize;
            break;
        case eDrawStateHoveringInnerTopRight:
        case eDrawStateHoveringInnerBtmLeft:
        case eDrawStateHoveringOuterTopRight:
        case eDrawStateHoveringOuterBtmLeft:
            name = kOfxsCursorNESWResize;
            break;
        case eDrawStateHoveringInnerTopMid:
        case eDrawStateHoveringInnerBtmMid:
        case eDrawStateHoveringOuterTopMid:
        case eDrawStateHoveringOuterBtmMid:
            name = kOfxsCursorNSResize;
            break;
        case eDrawStateHoveringInnerMidLeft:
        case eDrawStateHoveringInnerMidRight:
        case eDrawStateHoveringOuterMidLeft:
        case eDrawStateHoveringOuterMidRight:
            name = kOfxsCursorEWResize;
            break;
        }
        break;
    case eMouseStateDraggingCenter:
    case eMouseStateDraggingOffset:
        name = kOfxsCursorMove;
        break;
    case eMouseStateDraggingInnerTopLeft:
    case eMouseStateDraggingInnerBtmRight:
    case eMouseStateDraggingOuterTopLeft:
    case eMouseStateDraggingOuterBtmRight:
        name = kOfxsCursorNWSEResize;
        break;
    case eMouseStateDraggingInnerTopRight:
    case eMouseStateDraggingInnerBtmLeft:
    case eMouseStateDraggingOuterTopRight:
    case eMouseStateDraggingOuterBtmLeft:
        name = kOfxsCursorNESWResize;
        break;
    case eMouseStateDraggingInnerTopMid:
    case eMouseStateDraggingInnerBtmMid:
    case eMouseStateDraggingOuterTopMid:
    case eMouseStateDraggingOuterBtmMid:
        name = kOfxsCursorNSResize;
        break;
    case eMouseStateDraggingInnerMidLeft:
    case eMouseStateDraggingInnerMidRight:
    case eMouseStateDraggingOuterMidLeft:
    case eMouseStateDraggingOuterMidRight:
        name = kOfxsCursorEWResize;
        break;
    }
    if (!name) {
        // not over a handle: give the cursor back to the host
        _cursor.release(this);

        return;
    }
    _cursor.set(this, name);
}

bool
TrackerRegionInteract::penMotion(const PenArgs &args)
{
    const OfxPointD& pscale = args.pixelScale;
    bool valuesChanged = false;
    OfxPointD delta;

    delta.x = args.penPosition.x - _lastMousePos.x;
    delta.y = args.penPosition.y - _lastMousePos.y;

    double xi1, xi2, yi1, yi2, xo1, xo2, yo1, yo2, xc, yc, xoff, yoff;
    if (_ms == eMouseStateIdle) {
        _innerBtmLeft->getValueAtTime( args.time, xi1, yi1);
        _innerTopRight->getValueAtTime(args.time, xi2, yi2);
        _outerBtmLeft->getValueAtTime( args.time, xo1, yo1);
        _outerTopRight->getValueAtTime(args.time, xo2, yo2);
        _center->getValueAtTime(args.time, xc, yc);
        _offset->getValueAtTime(args.time, xoff, yoff);

        ///innerBtmLeft and outerBtmLeft are relative to the center, make them absolute
        xi1 += (xc + xoff);
        yi1 += (yc + yoff);
        xi2 += (xc + xoff);
        yi2 += (yc + yoff);
        xo1 += (xc + xoff);
        yo1 += (yc + yoff);
        xo2 += (xc + xoff);
        yo2 += (yc + yoff);
    } else {
        xi1 = _innerBtmLeftDragPos.x;
        yi1 = _innerBtmLeftDragPos.y;
        xi2 = _innerTopRightDragPos.x;
        yi2 = _innerTopRightDragPos.y;
        xo1 = _outerBtmLeftDragPos.x;
        yo1 = _outerBtmLeftDragPos.y;
        xo2 = _outerTopRightDragPos.x;
        yo2 = _outerTopRightDragPos.y;
        xc = _centerDragPos.x;
        yc = _centerDragPos.y;
        xoff = _offsetDragPos.x;
        yoff = _offsetDragPos.y;
    }


    DrawStateEnum newState = _ds;
    if (_ms == eMouseStateIdle) {
        // test center first
        if ( isNearby(args.penPosition, xc,  yc,  POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringCenter;
        } else if ( isNearby(args.penPosition, xi1, yi1, POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringInnerBtmLeft;
        } else if ( isNearby(args.penPosition, xi2, yi1, POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringInnerBtmRight;
        } else if ( isNearby(args.penPosition, xi1, yi2, POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringInnerTopLeft;
        } else if ( isNearby(args.penPosition, xi2, yi2, POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringInnerTopRight;
        } else if ( isNearby(args.penPosition, xc + xoff,  yi1, POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringInnerBtmMid;
        } else if ( isNearby(args.penPosition, xi1, yc + yoff,  POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringInnerMidLeft;
        } else if ( isNearby(args.penPosition, xc + xoff,  yi2, POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringInnerTopMid;
        } else if ( isNearby(args.penPosition, xi2, yc + yoff,  POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringInnerMidRight;
        } else if ( isNearby(args.penPosition, xo1, yo1, POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringOuterBtmLeft;
        } else if ( isNearby(args.penPosition, xo2, yo1, POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringOuterBtmRight;
        } else if ( isNearby(args.penPosition, xo1, yo2, POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringOuterTopLeft;
        } else if ( isNearby(args.penPosition, xo2, yo2, POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringOuterTopRight;
        } else if ( isNearby(args.penPosition, xc + xoff,  yo1, POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringOuterBtmMid;
        } else if ( isNearby(args.penPosition, xo1, yc + yoff,  POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringOuterMidLeft;
        } else if ( isNearby(args.penPosition, xc + xoff,  yo2, POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringOuterTopMid;
        } else if ( isNearby(args.penPosition, xo2, yc + yoff,  POINT_TOLERANCE, pscale) ) {
            newState = eDrawStateHoveringOuterMidRight;
        } else {
            newState = eDrawStateInactive;
        }
    }
    bool redraw = _ds != newState;
    _ds = newState;

    double multiplier = ctrlDown() ? 0 : 1;
    if (_ms == eMouseStateDraggingInnerBtmLeft) {
        xi1 += delta.x;
        yi1 += delta.y;

        xi2 -= delta.x;
        yi2 -= delta.y;

        ///also move the outer rect
        xo1 += delta.x;
        yo1 += delta.y;

        xo2 -= delta.x;
        yo2 -= delta.y;

        valuesChanged = true;
    } else if (_ms == eMouseStateDraggingInnerTopLeft) {
        xi1 += delta.x;
        yi1 -= delta.y;

        yi2 += delta.y;
        xi2 -= delta.x;

        xo1 += delta.x;
        yo1 -= delta.y;

        yo2 += delta.y;
        xo2 -= delta.x;

        valuesChanged = true;
    } else if (_ms == eMouseStateDraggingInnerTopRight) {
        xi1 -= delta.x;
        yi1 -= delta.y;

        yi2 += delta.y;
        xi2 += delta.x;

        xo1 -= delta.x;
        yo1 -= delta.y;

        yo2 += delta.y;
        xo2 += delta.x;

        valuesChanged = true;
    } else if (_ms == eMouseStateDraggingInnerBtmRight) {
        yi1 += delta.y;
        xi1 -= delta.x;

        yi2 -= delta.y;
        xi2 += delta.x;

        yo1 += delta.y;
        xo1 -= delta.x;

        yo2 -= delta.y;
        xo2 += delta.x;

        valuesChanged = true;
    } else if (_ms == eMouseStateDraggingInnerTopMid) {
        yi1 -= delta.y;

        yi2 += delta.y;

        yo1 -= delta.y;

        yo2 += delta.y;

        valuesChanged = true;
    } else if (_ms == eMouseStateDraggingInnerMidRight) {
        xi1 -= delta.x;

        xi2 += delta.x;

        xo1 -= delta.x;

        xo2 += delta.x;

        valuesChanged = true;
    } else if (_ms == eMouseStateDraggingInnerBtmMid) {
        yi1 += delta.y;

        yi2 -= delta.y;

        yo1 += delta.y;

        yo2 -= delta.y;

        valuesChanged = true;
    } else if (_ms == eMouseStateDraggingInnerMidLeft) {
        xi1 += delta.x;

        xi2 -= delta.x;

        xo1 += delta.x;

        xo2 -= delta.x;

        valuesChanged = true;
    } else if (_ms == eMouseStateDraggingOuterBtmLeft) {
        xo1 += delta.x;
        yo1 += delta.y;

        xo2 -= multiplier * delta.x;
        yo2 -= multiplier * delta.y;

        valuesChanged = true;
    } else if (_ms == eMouseStateDraggingOuterTopLeft) {
        xo1 += delta.x;
        if ( !ctrlDown() ) {
            yo1 -= delta.y;
        }

        yo2 += delta.y;
        xo2 -= multiplier * delta.x;

        valuesChanged = true;
    } else if (_ms == eMouseStateDraggingOuterTopRight) {
        if ( !ctrlDown() ) {
            xo1 -= delta.x;
            yo1 -= delta.y;
        }

        yo2 +=  delta.y;
        xo2 +=  delta.x;

        valuesChanged = true;
    } else if (_ms == eMouseStateDraggingOuterBtmRight) {
        yo1 += delta.y;
        if ( !ctrlDown() ) {
            xo1 -= delta.x;
        }

        yo2 -= multiplier * delta.y;
        xo2 +=  delta.x;

        valuesChanged = true;
    } else if (_ms == eMouseStateDraggingOuterTopMid) {
        if ( !ctrlDown() ) {
            yo1 -= delta.y;
        }

        yo2 += delta.y;

        valuesChanged = true;
    } else if (_ms == eMouseStateDraggingOuterMidRight) {
        if ( !ctrlDown() ) {
            xo1 -= delta.x;
        }

        xo2 +=  delta.x;

        valuesChanged = true;
    } else if (_ms == eMouseStateDraggingOuterBtmMid) {
        yo1 += delta.y;

        yo2 -= multiplier * delta.y;

        valuesChanged = true;
    } else if (_ms == eMouseStateDraggingOuterMidLeft) {
        xo1 += delta.x;

        xo2 -= multiplier * delta.x;

        valuesChanged = true;
    } else if ( (_ms == eMouseStateDraggingCenter) || (_ms == eMouseStateDraggingOffset) ) {
        xi1 += delta.x;
        yi1 += delta.y;

        xi2 += delta.x;
        yi2 += delta.y;

        xo1 += delta.x;
        yo1 += delta.y;

        xo2 += delta.x;
        yo2 += delta.y;

        if (_ms == eMouseStateDraggingCenter) {
            xc += delta.x;
            yc += delta.y;
        } else {
            assert(_ms == eMouseStateDraggingOffset);
            xoff += delta.x;
            yoff += delta.y;
        }

        valuesChanged = true;
    }


    if ( isDraggingOuterPoint() ) {
        /// outer rect must at least contain the inner rect

        if (xo1 > xi1) {
            xo1 = xi1;
            valuesChanged = true;
        }

        if (yo1 > yi1) {
            yo1 = yi1;
            valuesChanged = true;
        }

        if (xo2 < xi2) {
            xo2 = xi2;
            valuesChanged = true;
        }
        if (yo2 < yi2) {
            yo2 = yi2;
            valuesChanged = true;
        }
    }

    if ( isDraggingInnerPoint() ) {
        /// inner rect must contain center point
        if ( xi1 > (xc + xoff) ) {
            double diffX = xi1 - xc - xoff;
            xi1 = xc + xoff;
            xo1 -= diffX;
            xo2 += multiplier * diffX;
            xi2 += multiplier * diffX;
            valuesChanged = true;
        }
        if ( yi1 > (yc + yoff) ) {
            double diffY = yi1 - yc - yoff;
            yi1 = yc + yoff;
            yo1 -= diffY;
            yo2 += multiplier * diffY;
            yi2 += multiplier * diffY;
            valuesChanged = true;
        }
        if ( xi2 <= (xc + xoff) ) {
            double diffX = xi2 - xc - xoff;
            xi2 = xc + xoff;
            xo2 += diffX;
            xo1 -= multiplier * diffX;
            xi1 -= multiplier * diffX;
            valuesChanged = true;
        }
        if ( yi2 <= (yc + yoff) ) {
            double diffY = yi2 - yc - yoff;
            yi2 = yc + yoff;
            yo2 -= diffY;
            yo1 -= multiplier * diffY;
            yi1 -= multiplier * diffY;
            valuesChanged = true;
        }
    }

    // Only while dragging: when idle, the rectangles come from the params, and
    // fixing degenerate ones here would mark every hover as handled (trapped).
    if (_ms != eMouseStateIdle) {
        ///forbid 0 pixels wide rectangles
        if (xi2 <= xi1) {
            xi1 = (xi2 + xi1) / 2;
            xi2 = xi1 + 1;
            valuesChanged = true;
        }
        if (yi2 <= yi1) {
            yi1 = (yi2 + yi1) / 2;
            yi2 = yi1 + 1;
            valuesChanged = true;
        }
        if (xo2 <= xo1) {
            xo1 = (xo2 + xo1) / 2;
            xo2 = xo1 + 1;
            valuesChanged = true;
        }
        if (yo2 <= yo1) {
            yo1 = (yo2 + yo1) / 2;
            yo2 = yo1 + 1;
            valuesChanged = true;
        }
    }

    if (valuesChanged) {
        ///Keep the points in absolute coordinates
        _innerBtmLeftDragPos.x  = xi1;
        _innerBtmLeftDragPos.y  = yi1;
        _innerTopRightDragPos.x = xi2;
        _innerTopRightDragPos.y = yi2;
        _outerBtmLeftDragPos.x  = xo1;
        _outerBtmLeftDragPos.y  = yo1;
        _outerTopRightDragPos.x = xo2;
        _outerTopRightDragPos.y = yo2;
        _centerDragPos.x        = xc;
        _centerDragPos.y        = yc;
        _offsetDragPos.x        = xoff;
        _offsetDragPos.y        = yoff;
    }

    if (redraw || valuesChanged) {
        requestRedraw();
    }

    _lastMousePos = args.penPosition;

    // report the cursor for the hovered or dragged handle; the host reads it
    // whether or not the event is trapped, so the return value is unchanged
    updateCursor();

    return valuesChanged;
} // penMotion

bool
TrackerRegionInteract::penDown(const PenArgs &args)
{
    const OfxPointD& pscale = args.pixelScale;
    bool didSomething = false;
    double xi1, xi2, yi1, yi2, xo1, xo2, yo1, yo2, xc, yc, xoff, yoff;

    _innerBtmLeft->getValueAtTime( args.time, xi1, yi1);
    _innerTopRight->getValueAtTime(args.time, xi2, yi2);
    _outerBtmLeft->getValueAtTime( args.time, xo1, yo1);
    _outerTopRight->getValueAtTime(args.time, xo2, yo2);
    _center->getValueAtTime(args.time, xc, yc);
    _offset->getValueAtTime(args.time, xoff, yoff);

    ///innerBtmLeft and outerBtmLeft are relative to the center, make them absolute
    xi1 += (xc + xoff);
    yi1 += (yc + yoff);
    xi2 += (xc + xoff);
    yi2 += (yc + yoff);
    xo1 += (xc + xoff);
    yo1 += (yc + yoff);
    xo2 += (xc + xoff);
    yo2 += (yc + yoff);

    MouseStateEnum newState = _ms;
    // test center first
    if ( isNearby(args.penPosition, xc,  yc,  POINT_TOLERANCE, pscale) ) {
        if ( ctrlDown() ) {
            newState = eMouseStateDraggingOffset;
        } else {
            newState = eMouseStateDraggingCenter;
        }
    } else if ( (xoff != 0) && (yoff != 0) && isNearby(args.penPosition, xc + xoff, yc + yoff, POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingOffset;
    } else if ( isNearby(args.penPosition, xi1, yi1, POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingInnerBtmLeft;
    } else if ( isNearby(args.penPosition, xi2, yi1, POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingInnerBtmRight;
    } else if ( isNearby(args.penPosition, xi1, yi2, POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingInnerTopLeft;
    } else if ( isNearby(args.penPosition, xi2, yi2, POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingInnerTopRight;
    } else if ( isNearby(args.penPosition, xc + xoff,  yi1, POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingInnerBtmMid;
    } else if ( isNearby(args.penPosition, xi1, yc + yoff,  POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingInnerMidLeft;
    } else if ( isNearby(args.penPosition, xc + xoff,  yi2, POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingInnerTopMid;
    } else if ( isNearby(args.penPosition, xi2, yc + yoff,  POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingInnerMidRight;
    } else if ( isNearby(args.penPosition, xo1, yo1, POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingOuterBtmLeft;
    } else if ( isNearby(args.penPosition, xo2, yo1, POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingOuterBtmRight;
    } else if ( isNearby(args.penPosition, xo1, yo2, POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingOuterTopLeft;
    } else if ( isNearby(args.penPosition, xo2, yo2, POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingOuterTopRight;
    } else if ( isNearby(args.penPosition, xc + xoff,  yo1, POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingOuterBtmMid;
    } else if ( isNearby(args.penPosition, xo1, yc + yoff,  POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingOuterMidLeft;
    } else if ( isNearby(args.penPosition, xc + xoff,  yo2, POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingOuterTopMid;
    } else if ( isNearby(args.penPosition, xo2, yc + yoff,  POINT_TOLERANCE, pscale) ) {
        newState = eMouseStateDraggingOuterMidRight;
    } else {
        newState = eMouseStateIdle;
    }
    didSomething = _ms != newState;
    _ms = newState;


    ///Keep the points in absolute coordinates
    _innerBtmLeftDragPos.x  = xi1;
    _innerBtmLeftDragPos.y  = yi1;
    _innerTopRightDragPos.x = xi2;
    _innerTopRightDragPos.y = yi2;
    _outerBtmLeftDragPos.x  = xo1;
    _outerBtmLeftDragPos.y  = yo1;
    _outerTopRightDragPos.x = xo2;
    _outerTopRightDragPos.y = yo2;
    _centerDragPos.x        = xc;
    _centerDragPos.y        = yc;
    _offsetDragPos.x        = xoff;
    _offsetDragPos.y        = yoff;

    _lastMousePos = args.penPosition;

    if (didSomething) {
        requestRedraw();
    }

    return didSomething;
} // penDown

bool
TrackerRegionInteract::isDraggingInnerPoint() const
{
    return (_ms == eMouseStateDraggingInnerTopLeft  ||
            _ms == eMouseStateDraggingInnerTopRight ||
            _ms == eMouseStateDraggingInnerBtmLeft  ||
            _ms == eMouseStateDraggingInnerBtmRight ||
            _ms == eMouseStateDraggingInnerTopMid   ||
            _ms == eMouseStateDraggingInnerMidRight ||
            _ms == eMouseStateDraggingInnerBtmMid   ||
            _ms == eMouseStateDraggingInnerMidLeft);
}

bool
TrackerRegionInteract::isDraggingOuterPoint() const
{
    return (_ms == eMouseStateDraggingOuterTopLeft  ||
            _ms == eMouseStateDraggingOuterTopRight ||
            _ms == eMouseStateDraggingOuterBtmLeft  ||
            _ms == eMouseStateDraggingOuterBtmRight ||
            _ms == eMouseStateDraggingOuterTopMid   ||
            _ms == eMouseStateDraggingOuterMidRight ||
            _ms == eMouseStateDraggingOuterBtmMid   ||
            _ms == eMouseStateDraggingOuterMidLeft);
}

bool
TrackerRegionInteract::penUp(const PenArgs &args)
{
    if (_ms == eMouseStateIdle) {
        return false;
    }

    const OfxPointD &center = _centerDragPos;
    const OfxPointD &offset = _offsetDragPos;
    ImageEffect::EditBlock eb(*_effect, "setTrackerRegion");
    {
        OfxPointD btmLeft;
        btmLeft.x = _innerBtmLeftDragPos.x - center.x - offset.x;
        btmLeft.y = _innerBtmLeftDragPos.y - center.y - offset.y;

        _innerBtmLeft->setValue(btmLeft.x, btmLeft.y);

        OfxPointD topRight;
        topRight.x = _innerTopRightDragPos.x - center.x - offset.x;
        topRight.y = _innerTopRightDragPos.y - center.y - offset.y;

        _innerTopRight->setValue(topRight.x, topRight.y);
    }
    {
        OfxPointD btmLeft;
        btmLeft.x = _outerBtmLeftDragPos.x - center.x - offset.x;
        btmLeft.y = _outerBtmLeftDragPos.y - center.y - offset.y;
        _outerBtmLeft->setValue(btmLeft.x, btmLeft.y);

        OfxPointD topRight;
        topRight.x = _outerTopRightDragPos.x - center.x - offset.x;
        topRight.y = _outerTopRightDragPos.y - center.y - offset.y;
        _outerTopRight->setValue(topRight.x, topRight.y);
    }

    if (_ms == eMouseStateDraggingCenter) {
        _center->setValueAtTime(args.time, _centerDragPos.x, _centerDragPos.y);
    } else if (_ms == eMouseStateDraggingOffset) {
        _offset->setValueAtTime(args.time, _offsetDragPos.x, _offsetDragPos.y);
    }

    _ms = eMouseStateIdle;

    requestRedraw();

    return true;
}

bool
TrackerRegionInteract::ctrlDown() const
{
#ifdef OFXS_HAVE_KEY_MODIFIERS
    return (getKeyModifiers() & kNatronOfxKeyModifierControl) != 0;
#else
    return _controlDown > 0;
#endif
}

bool
TrackerRegionInteract::keyDown(const KeyArgs &args)
{
    if ( (args.keySymbol == kOfxKey_Control_L) || (args.keySymbol == kOfxKey_Control_R) ) {
        ++_controlDown;
    } else if ( (args.keySymbol == kOfxKey_Alt_L) || (args.keySymbol == kOfxKey_Alt_R) ) {
        ++_altDown;
    }

    return false;
}

bool
TrackerRegionInteract::keyUp(const KeyArgs &args)
{
    if ( (args.keySymbol == kOfxKey_Control_L) || (args.keySymbol == kOfxKey_Control_R) ) {
        if (_controlDown > 0) {
            --_controlDown;
        }
    } else if ( (args.keySymbol == kOfxKey_Alt_L) || (args.keySymbol == kOfxKey_Alt_R) ) {
        if (_altDown > 0) {
            --_altDown;
        }
    }

    return false;
}

/** @brief Called when the interact is loses input focus */
void
TrackerRegionInteract::loseFocus(const FocusArgs & /*args*/)
{
    // reset the modifiers state
    _controlDown = 0;
    _altDown = 0;
    _ds = eDrawStateInactive;
    _ms = eMouseStateIdle;
    // the host restores its own cursor
    _cursor.release(this);
}
} // namespace OFX
