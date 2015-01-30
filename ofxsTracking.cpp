/*
 OFX utilities for tracking.
 
 Copyright (C) 2014 INRIA
 
 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:
 
 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 
 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.
 
 Neither the name of the {organization} nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 INRIA
 Domaine de Voluceau
 Rocquencourt - B.P. 105
 78153 Le Chesnay Cedex - France
 */

#include "ofxsTracking.h"
#include <cmath>

#include "ofxsOGLTextRenderer.h"

#ifdef __APPLE__
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

GenericTrackerPlugin::GenericTrackerPlugin(OfxImageEffectHandle handle)
: ImageEffect(handle)
, dstClip_(0)
, srcClip_(0)
, _center(0)
, _offset(0)
, _innerBtmLeft(0)
, _innerTopRight(0)
, _outerBtmLeft(0)
, _outerTopRight(0)
, _backwardButton(0)
, _prevButton(0)
, _nextButton(0)
, _forwardButton(0)
, _instanceName(0)
{
    dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
    assert(dstClip_->getPixelComponents() == ePixelComponentAlpha || dstClip_->getPixelComponents() == ePixelComponentRGB || dstClip_->getPixelComponents() == ePixelComponentRGBA);
    srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
    assert(srcClip_->getPixelComponents() == ePixelComponentAlpha || srcClip_->getPixelComponents() == ePixelComponentRGB || srcClip_->getPixelComponents() == ePixelComponentRGBA);
    
    
    _center = fetchDouble2DParam(kParamTrackingCenterPoint);
    _offset = fetchDouble2DParam(kParamTrackingOffset);
    _innerBtmLeft = fetchDouble2DParam(kParamTrackingPatternBoxBtmLeft);
    _innerTopRight = fetchDouble2DParam(kParamTrackingPatternBoxTopRight);
    _outerBtmLeft = fetchDouble2DParam(kParamTrackingSearchBoxBtmLeft);
    _outerTopRight = fetchDouble2DParam(kParamTrackingSearchBoxTopRight);
    _backwardButton = fetchPushButtonParam(kParamTrackingBackward);
    _prevButton = fetchPushButtonParam(kParamTrackingPrevious);
    _nextButton = fetchPushButtonParam(kParamTrackingNext);
    _forwardButton = fetchPushButtonParam(kParamTrackingForward);
    _instanceName = fetchStringParam(kOfxParamStringSublabelName);
    assert(_center && _offset &&  _innerTopRight && _innerBtmLeft && _outerTopRight && _outerBtmLeft && _backwardButton && _prevButton && _nextButton && _forwardButton && _instanceName);
}

bool
GenericTrackerPlugin::isIdentity(const IsIdentityArguments &args,
                                 Clip * &identityClip,
                                 double &identityTime)
{
    if ( !kSupportsRenderScale && ( (args.renderScale.x != 1.) || (args.renderScale.y != 1.) ) ) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    
    identityClip = srcClip_;
    identityTime = args.time;
    
    return true;
}

void
GenericTrackerPlugin::changedParam(const OFX::InstanceChangedArgs &args,
                                   const std::string &paramName)
{
    if ( !kSupportsRenderScale && ( (args.renderScale.x != 1.) || (args.renderScale.y != 1.) ) ) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    
    OFX::TrackArguments trackArgs;
    trackArgs.renderScale = args.renderScale;
    if (paramName == kParamTrackingBackward) {
        trackArgs.first = args.time;
        double first,last;
        timeLineGetBounds(first, last);
        trackArgs.last = first + 1;
        if (trackArgs.last <= trackArgs.first) {
            trackArgs.forward = false;
            trackArgs.reason = args.reason;
            trackRange(trackArgs);
        }
    } else if (paramName == kParamTrackingPrevious) {
        trackArgs.first = args.time;
        trackArgs.last = trackArgs.first;
        trackArgs.forward = false;
        trackArgs.reason = args.reason;
        trackRange(trackArgs);
    } else if (paramName == kParamTrackingNext) {
        trackArgs.first = args.time;
        trackArgs.last = trackArgs.first;
        trackArgs.forward = true;
        trackArgs.reason = args.reason;
        trackRange(trackArgs);
    } else if (paramName == kParamTrackingForward) {
        trackArgs.first = args.time;
        double first,last;
        timeLineGetBounds(first, last);
        trackArgs.last = last - 1;
        if (trackArgs.last >= trackArgs.first) {
            trackArgs.forward = true;
            trackArgs.reason = args.reason;
            trackRange(trackArgs);
        }
    }
}

bool
GenericTrackerPlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args,
                                            OfxRectD & /*rod*/)
{
    if ( !kSupportsRenderScale && ( (args.renderScale.x != 1.) || (args.renderScale.y != 1.) ) ) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    
    return false;
}

void
OFX::genericTrackerDescribe(OFX::ImageEffectDescriptor &desc)
{
    desc.addSupportedContext(eContextGeneral);
    desc.addSupportedContext(eContextFilter);
    
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
}

OFX::PageParamDescriptor*
OFX::genericTrackerDescribeInContextBegin(OFX::ImageEffectDescriptor &desc,
                                          OFX::ContextEnum /*context*/)
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
OFX::genericTrackerDescribePointParameters(OFX::ImageEffectDescriptor &desc,
                                           OFX::PageParamDescriptor* page)
{
    ///Declare the name first so that in Natron it appears as the first column in the multi instance
    // name
    {
        OFX::StringParamDescriptor* param = desc.defineStringParam(kParamTrackingLabel);
        param->setLabels(kParamTrackingLabelLabel, kParamTrackingLabelLabel, kParamTrackingLabelLabel);
        param->setHint(kParamTrackingLabelHint);
        param->setDefault(kParamTrackingLabelDefault);
        ////param->setIsSecret(false); // it has to be user-editable
        ////param->setEnabled(true); // it has to be user-editable
        ////param->setIsPersistant(true); // it has to be saved with the instance parameters
        param->setEvaluateOnChange(false); // it is meaningless
        page->addChild(*param);
    }
    
    // center
    {
        OFX::Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamTrackingCenterPoint);
        param->setLabels(kParamTrackingCenterPointLabel, kParamTrackingCenterPointLabel, kParamTrackingCenterPointLabel);
        param->setHint(kParamTrackingCenterPointHint);
        param->setDoubleType(eDoubleTypeXYAbsolute);
        param->setDefaultCoordinateSystem(eCoordinatesNormalised);
        param->setDefault(0.5, 0.5);
        param->setIncrement(1.);
        param->getPropertySet().propSetInt(kOfxParamPropPluginMayWrite, 1);
        page->addChild(*param);
    }
    
    // offset
    {
        OFX::Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamTrackingOffset);
        param->setLabels(kParamTrackingOffsetLabel, kParamTrackingOffsetLabel, kParamTrackingOffsetLabel);
        param->setHint(kParamTrackingOffsetHint);
        param->setDoubleType(eDoubleTypeXYAbsolute);
        param->setDefaultCoordinateSystem(eCoordinatesCanonical);
        param->setDefault(0, 0);
        param->setIncrement(1.);
        page->addChild(*param);
    }
    
    // innerBtmLeft
    {
        OFX::Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamTrackingPatternBoxBtmLeft);
        param->setLabels(kParamTrackingPatternBoxBtmLeftLabel, kParamTrackingPatternBoxBtmLeftLabel, kParamTrackingPatternBoxBtmLeftLabel);
        param->setHint(kParamTrackingPatternBoxBtmLeftHint);
        param->setDoubleType(eDoubleTypeXY);
        param->setDefaultCoordinateSystem(eCoordinatesCanonical);
        param->setDefault(-15,-15);
        param->setDisplayRange(-50., -50., 50., 50.);
        param->setIncrement(1.);
        //param->setIsSecret(true);
        param->getPropertySet().propSetInt(kOfxParamPropPluginMayWrite, 1);
        page->addChild(*param);
    }
    
    // innerTopRight
    {
        OFX::Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamTrackingPatternBoxTopRight);
        param->setLabels(kParamTrackingPatternBoxTopRightLabel, kParamTrackingPatternBoxTopRightLabel, kParamTrackingPatternBoxTopRightLabel);
        param->setHint(kParamTrackingPatternBoxTopRightHint);
        param->setDoubleType(eDoubleTypeXY);
        param->setDefaultCoordinateSystem(eCoordinatesCanonical);
        param->setDefault(15, 15);
        param->setDisplayRange(-50., -50., 50., 50.);
        param->setIncrement(1.);
        //innerTopRight->setIsSecret(true);
        param->getPropertySet().propSetInt(kOfxParamPropPluginMayWrite, 1);
        page->addChild(*param);
    }
    
    // outerBtmLeft
    {
        OFX::Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamTrackingSearchBoxBtmLeft);
        param->setLabels(kParamTrackingSearchBoxBtmLeftLabel, kParamTrackingSearchBoxBtmLeftLabel, kParamTrackingSearchBoxBtmLeftLabel);
        param->setHint(kParamTrackingSearchBoxBtmLeftHint);
        param->setDoubleType(eDoubleTypeXY);
        param->setDefaultCoordinateSystem(eCoordinatesCanonical);
        param->setDefault(-25,-25);
        param->setDisplayRange(-100., -100., 100., 100.);
        param->setIncrement(1.);
        //param->setIsSecret(true);
        param->getPropertySet().propSetInt(kOfxParamPropPluginMayWrite, 1);
        page->addChild(*param);
    }
    
    // outerTopRight
    {
        OFX::Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamTrackingSearchBoxTopRight);
        param->setLabels(kParamTrackingSearchBoxTopRightLabel, kParamTrackingSearchBoxTopRightLabel, kParamTrackingSearchBoxTopRightLabel);
        param->setHint(kParamTrackingSearchBoxBtmLeftHint);
        param->setDoubleType(eDoubleTypeXY);
        param->setDefaultCoordinateSystem(eCoordinatesCanonical);
        param->setDefault(25, 25);
        param->setDisplayRange(-100., -100., 100., 100.);
        param->setIncrement(1.);
        //param->setIsSecret(true);
        param->getPropertySet().propSetInt(kOfxParamPropPluginMayWrite, 1);
        page->addChild(*param);
    }
    
    // backward
    {
        OFX::PushButtonParamDescriptor* param = desc.definePushButtonParam(kParamTrackingBackward);
        param->setLabels(kParamTrackingBackwardLabel, kParamTrackingBackwardLabel,kParamTrackingBackwardLabel);
        param->setHint(kParamTrackingBackwardHint);
        param->setLayoutHint(eLayoutHintNoNewLine);
        page->addChild(*param);
    }
    
    // prev
    {
        OFX::PushButtonParamDescriptor* param = desc.definePushButtonParam(kParamTrackingPrevious);
        param->setLabels(kParamTrackingPreviousLabel, kParamTrackingPreviousLabel, kParamTrackingPreviousLabel);
        param->setHint(kParamTrackingPreviousHint);
        param->setLayoutHint(eLayoutHintNoNewLine);
        page->addChild(*param);
    }
    
    // next
    {
        OFX::PushButtonParamDescriptor* param = desc.definePushButtonParam(kParamTrackingNext);
        param->setLabels(kParamTrackingNextLabel, kParamTrackingNextLabel, kParamTrackingNextLabel);
        param->setHint(kParamTrackingNextHint);
        param->setLayoutHint(eLayoutHintNoNewLine);
        page->addChild(*param);
    }
    
    // forward
    {
        OFX::PushButtonParamDescriptor* param = desc.definePushButtonParam(kParamTrackingForward);
        param->setLabels(kParamTrackingForwardLabel, kParamTrackingForwardLabel, kParamTrackingForwardLabel);
        param->setHint(kParamTrackingForwardHint);
        page->addChild(*param);
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
TrackerRegionInteract::draw(const OFX::DrawArgs &args)
{
    OfxRGBColourD color = { 0.8, 0.8, 0.8 };
    getSuggestedColour(color);
    const OfxPointD& pscale = args.pixelScale;
    
    double xi1, xi2, yi1, yi2, xo1, xo2, yo1, yo2, xc, yc,xoff,yoff;
    
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
    
    
    glPushAttrib(GL_ALL_ATTRIB_BITS);

    //glDisable(GL_LINE_STIPPLE);
    glEnable(GL_LINE_SMOOTH);
    //glEnable(GL_POINT_SMOOTH);
    glEnable(GL_BLEND);
    glHint(GL_LINE_SMOOTH_HINT,GL_DONT_CARE);
    glLineWidth(1.5f);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    
    // Draw everything twice
    // l = 0: shadow
    // l = 1: drawing
    for (int l = 0; l < 2; ++l) {
        // shadow (uses GL_PROJECTION)
        glMatrixMode(GL_PROJECTION);
        int direction = (l == 0) ? 1 : -1;
        // translate (1,-1) pixels
        glTranslated(direction * pscale.x / 256, -direction * pscale.y / 256, 0);
        glMatrixMode(GL_MODELVIEW); // Modelview should be used on Nuke
        
        glColor3f((float)color.r * l, (float)color.g * l, (float)color.b * l);
        glBegin(GL_LINE_LOOP);
        glVertex2d(xi1, yi1);
        glVertex2d(xi1, yi2);
        glVertex2d(xi2, yi2);
        glVertex2d(xi2, yi1);
        glEnd();
        
        glBegin(GL_LINE_LOOP);
        glVertex2d(xo1, yo1);
        glVertex2d(xo1, yo2);
        glVertex2d(xo2, yo2);
        glVertex2d(xo2, yo1);
        glEnd();
        
        glPointSize(POINT_SIZE);
        glBegin(GL_POINTS);
        
        ///draw center
        if ( (_ds == eDrawStateHoveringCenter) || (_ms == eMouseStateDraggingCenter)) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            glColor3f((float)color.r * l, (float)color.g * l, (float)color.b * l);
        }
        glVertex2d(xc, yc);
        
        if (xoff != 0 || yoff != 0) {
            glVertex2d(xc + xoff, yc + yoff);
        }
        
        //////DRAWING INNER POINTS
        if ( (_ds == eDrawStateHoveringInnerBtmLeft) || (_ms == eMouseStateDraggingInnerBtmLeft) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xi1, yi1);
        }
        if ( (_ds == eDrawStateHoveringInnerBtmMid) || (_ms == eMouseStateDraggingInnerBtmMid) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xc + xoff, yi1);
        }
        if ( (_ds == eDrawStateHoveringInnerBtmRight) || (_ms == eMouseStateDraggingInnerBtmRight) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xi2, yi1);
        }
        if ( (_ds == eDrawStateHoveringInnerMidLeft) || (_ms == eMouseStateDraggingInnerMidLeft) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xi1, yc + yoff);
        }
        if ( (_ds == eDrawStateHoveringInnerMidRight) || (_ms == eMouseStateDraggingInnerMidRight) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xi2, yc + yoff);
        }
        if ( (_ds == eDrawStateHoveringInnerTopLeft) || (_ms == eMouseStateDraggingInnerTopLeft) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xi1, yi2);
        }
        
        if ( (_ds == eDrawStateHoveringInnerTopMid) || (_ms == eMouseStateDraggingInnerTopMid) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xc + xoff, yi2);
        }
        
        if ( (_ds == eDrawStateHoveringInnerTopRight) || (_ms == eMouseStateDraggingInnerTopRight) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xi2, yi2);
        }
        
        
        //////DRAWING OUTTER POINTS
        
        if ( (_ds == eDrawStateHoveringOuterBtmLeft) || (_ms == eMouseStateDraggingOuterBtmLeft) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xo1, yo1);
        }
        if ( (_ds == eDrawStateHoveringOuterBtmMid) || (_ms == eMouseStateDraggingOuterBtmMid) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xc + xoff, yo1);
        }
        if ( (_ds == eDrawStateHoveringOuterBtmRight) || (_ms == eMouseStateDraggingOuterBtmRight) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xo2, yo1);
        }
        if ( (_ds == eDrawStateHoveringOuterMidLeft) || (_ms == eMouseStateDraggingOuterMidLeft) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xo1, yc + yoff);
        }
        if ( (_ds == eDrawStateHoveringOuterMidRight) || (_ms == eMouseStateDraggingOuterMidRight) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xo2, yc + yoff);
        }
        
        if ( (_ds == eDrawStateHoveringOuterTopLeft) || (_ms == eMouseStateDraggingOuterTopLeft) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xo1, yo2);
        }
        if ( (_ds == eDrawStateHoveringOuterTopMid) || (_ms == eMouseStateDraggingOuterTopMid) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xc + xoff, yo2);
        }
        if ( (_ds == eDrawStateHoveringOuterTopRight) || (_ms == eMouseStateDraggingOuterTopRight) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
            glVertex2d(xo2, yo2);
        }
        
        glEnd();
        
        if (xoff != 0 || yoff != 0) {
            glBegin(GL_LINES);
            glColor3f((float)color.r * l, (float)color.g * l, (float)color.b * l);
            glVertex2d(xc, yc);
            glVertex2d(xc + xoff, yc + yoff);
            glEnd();
        }
        
        double handleSizeX = HANDLE_SIZE * pscale.x;
        double handleSizeY = HANDLE_SIZE * pscale.y;
        
        ///now show small lines at handle positions
        glBegin(GL_LINES);
        
        if ( (_ds == eDrawStateHoveringInnerMidLeft) || (_ms == eMouseStateDraggingInnerMidLeft) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            glColor3f((float)color.r * l, (float)color.g * l, (float)color.b * l);
        }
        glVertex2d(xi1, yc + yoff);
        glVertex2d(xi1 - handleSizeX, yc + yoff);
        
        if ( (_ds == eDrawStateHoveringInnerTopMid) || (_ms == eMouseStateDraggingInnerTopMid) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            glColor3f((float)color.r * l, (float)color.g * l, (float)color.b * l);
        }
        glVertex2d(xc + xoff, yi2);
        glVertex2d(xc + xoff, yi2 + handleSizeY);
        
        if ( (_ds == eDrawStateHoveringInnerMidRight) || (_ms == eMouseStateDraggingInnerMidRight) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            glColor3f((float)color.r * l, (float)color.g * l, (float)color.b * l);
        }
        glVertex2d(xi2, yc + yoff);
        glVertex2d(xi2 + handleSizeX, yc + yoff);
        
        if ( (_ds == eDrawStateHoveringInnerBtmMid) || (_ms == eMouseStateDraggingInnerBtmMid) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            glColor3f((float)color.r * l, (float)color.g * l, (float)color.b * l);
        }
        glVertex2d(xc + xoff, yi1);
        glVertex2d(xc + xoff, yi1 - handleSizeY);
        
        //////DRAWING OUTTER HANDLES
        
        if ( (_ds == eDrawStateHoveringOuterMidLeft) || (_ms == eMouseStateDraggingOuterMidLeft) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            glColor3f((float)color.r * l, (float)color.g * l, (float)color.b * l);
        }
        glVertex2d(xo1, yc + yoff);
        glVertex2d(xo1 - handleSizeX, yc + yoff);
        
        if ( (_ds == eDrawStateHoveringOuterTopMid) || (_ms == eMouseStateDraggingOuterTopMid) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            glColor3f((float)color.r * l, (float)color.g * l, (float)color.b * l);
        }
        glVertex2d(xc + xoff, yo2);
        glVertex2d(xc + xoff, yo2 + handleSizeY);
        
        if ( (_ds == eDrawStateHoveringOuterMidRight) || (_ms == eMouseStateDraggingOuterMidRight) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            glColor3f((float)color.r * l, (float)color.g * l, (float)color.b * l);
        }
        glVertex2d(xo2 + handleSizeX, yc + yoff);
        glVertex2d(xo2, yc + yoff);
        
        if ( (_ds == eDrawStateHoveringOuterBtmMid) || (_ms == eMouseStateDraggingOuterBtmMid) ) {
            glColor3f(0.f * l, 1.f * l, 0.f * l);
        } else {
            glColor3f((float)color.r * l, (float)color.g * l, (float)color.b * l);
        }
        glVertex2d(xc + xoff, yo1);
        glVertex2d(xc + xoff, yo1 - handleSizeY);
        glEnd();
        
        
        glColor3f((float)color.r * l, (float)color.g * l, (float)color.b * l);
        std::string name;
        _name->getValue(name);
        TextRenderer::bitmapString( xc, yc, name.c_str() );
    }
    
    glPopAttrib();
    
    return true;
} // draw

bool
TrackerRegionInteract::penMotion(const OFX::PenArgs &args)
{
    const OfxPointD& pscale = args.pixelScale;
    
    bool didSomething = false;
    OfxPointD delta;
    delta.x = args.penPosition.x - _lastMousePos.x;
    delta.y = args.penPosition.y - _lastMousePos.y;
    
    double xi1, xi2, yi1, yi2, xo1, xo2, yo1, yo2, xc, yc,xoff,yoff;
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
    
    bool lastStateWasHovered = _ds != eDrawStateInactive;
    
    if (_ms == eMouseStateIdle) {
        // test center first
        if ( isNearby(args.penPosition, xc,  yc,  POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringCenter;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xi1, yi1, POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringInnerBtmLeft;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xi2, yi1, POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringInnerBtmRight;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xi1, yi2, POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringInnerTopLeft;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xi2, yi2, POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringInnerTopRight;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xc + xoff,  yi1, POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringInnerBtmMid;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xi1, yc + yoff,  POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringInnerMidLeft;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xc + xoff,  yi2, POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringInnerTopMid;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xi2, yc + yoff,  POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringInnerMidRight;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xo1, yo1, POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringOuterBtmLeft;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xo2, yo1, POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringOuterBtmRight;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xo1, yo2, POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringOuterTopLeft;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xo2, yo2, POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringOuterTopRight;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xc + xoff,  yo1, POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringOuterBtmMid;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xo1, yc + yoff,  POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringOuterMidLeft;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xc + xoff,  yo2, POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringOuterTopMid;
            didSomething = true;
        } else if ( isNearby(args.penPosition, xo2, yc + yoff,  POINT_TOLERANCE, pscale) ) {
            _ds = eDrawStateHoveringOuterMidRight;
            didSomething = true;
        } else {
            _ds = eDrawStateInactive;
        }
    }
    
    double multiplier = _controlDown ? 0 : 1;
    if (_ms == eMouseStateDraggingInnerBtmLeft) {
        _innerBtmLeftDragPos.x += delta.x;
        _innerBtmLeftDragPos.y += delta.y;
        _innerTopRightDragPos.x -= delta.x;
        _innerTopRightDragPos.y -= delta.y;
        ///also move the outer rect
        _outerBtmLeftDragPos.x += delta.x;
        _outerBtmLeftDragPos.y += delta.y;
        _outerTopRightDragPos.x -= delta.x;
        _outerTopRightDragPos.y -= delta.y;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingInnerTopLeft) {
        _innerBtmLeftDragPos.x += delta.x;
        _innerBtmLeftDragPos.y -= delta.y;
        
        _innerTopRightDragPos.y += delta.y;
        _innerTopRightDragPos.x -= delta.x;
        
        _outerBtmLeftDragPos.x += delta.x;
        _outerBtmLeftDragPos.y -= delta.y;
        
        _outerTopRightDragPos.y += delta.y;
        _outerTopRightDragPos.x -= delta.x;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingInnerTopRight) {
        _innerBtmLeftDragPos.x -= delta.x;
        _innerBtmLeftDragPos.y -= delta.y;
        
        _innerTopRightDragPos.y += delta.y;
        _innerTopRightDragPos.x += delta.x;
        
        _outerBtmLeftDragPos.x -= delta.x;
        _outerBtmLeftDragPos.y -= delta.y;
        
        _outerTopRightDragPos.y += delta.y;
        _outerTopRightDragPos.x += delta.x;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingInnerBtmRight) {
        _innerTopRightDragPos.y -= delta.y;
        _innerTopRightDragPos.x += delta.x;
        _innerBtmLeftDragPos.y += delta.y;
        _innerBtmLeftDragPos.x -= delta.x;
        
        
        _outerTopRightDragPos.y -= delta.y;
        _outerTopRightDragPos.x += delta.x;
        _outerBtmLeftDragPos.y += delta.y;
        _outerBtmLeftDragPos.x -= delta.x;
        
        
        didSomething = true;
    } else if (_ms == eMouseStateDraggingInnerTopMid) {
        _innerBtmLeftDragPos.y -= delta.y;
        _outerBtmLeftDragPos.y -= delta.y;
        
        _innerTopRightDragPos.y += delta.y;
        _outerTopRightDragPos.y += delta.y;
        
        didSomething = true;
    } else if (_ms == eMouseStateDraggingInnerMidRight) {
        _innerTopRightDragPos.x += delta.x;
        _outerTopRightDragPos.x += delta.x;
        _innerBtmLeftDragPos.x -= delta.x;
        _outerBtmLeftDragPos.x -= delta.x;
        
        
        didSomething = true;
    } else if (_ms == eMouseStateDraggingInnerBtmMid) {
        _innerBtmLeftDragPos.y += delta.y;
        _innerTopRightDragPos.y -= delta.y;
        
        _outerBtmLeftDragPos.y += delta.y;
        _outerTopRightDragPos.y -= delta.y;
        
        didSomething = true;
    } else if (_ms == eMouseStateDraggingInnerMidLeft) {
        _innerBtmLeftDragPos.x += delta.x;
        _innerTopRightDragPos.x -= delta.x;
        
        _outerBtmLeftDragPos.x += delta.x;
        _outerTopRightDragPos.x -= delta.x;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingOuterBtmLeft) {
        _outerBtmLeftDragPos.x += delta.x;
        _outerBtmLeftDragPos.y += delta.y;
        _outerTopRightDragPos.x -= multiplier * delta.x;
        _outerTopRightDragPos.y -= multiplier * delta.y;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingOuterTopLeft) {
        _outerBtmLeftDragPos.x += delta.x;
        if (!_controlDown) {
            _outerBtmLeftDragPos.y -= delta.y;
        }
        _outerTopRightDragPos.y += delta.y;
        _outerTopRightDragPos.x -= multiplier * delta.x;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingOuterTopRight) {
        if (!_controlDown) {
            _outerBtmLeftDragPos.x -= delta.x;
            _outerBtmLeftDragPos.y -= delta.y;
        }
        _outerTopRightDragPos.y +=  delta.y;
        _outerTopRightDragPos.x +=  delta.x;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingOuterBtmRight) {
        _outerTopRightDragPos.y -= multiplier * delta.y;
        _outerTopRightDragPos.x +=  delta.x;
        _outerBtmLeftDragPos.y += delta.y;
        if (!_controlDown) {
            _outerBtmLeftDragPos.x -= delta.x;
        }
        didSomething = true;
    } else if (_ms == eMouseStateDraggingOuterTopMid) {
        if (!_controlDown) {
            _outerBtmLeftDragPos.y -= delta.y;
        }
        _outerTopRightDragPos.y += delta.y;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingOuterMidRight) {
        _outerTopRightDragPos.x +=  delta.x;
        if (!_controlDown) {
            _outerBtmLeftDragPos.x -= delta.x;
        }
        didSomething = true;
    } else if (_ms == eMouseStateDraggingOuterBtmMid) {
        _outerBtmLeftDragPos.y += delta.y;
        _outerTopRightDragPos.y -= multiplier * delta.y;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingOuterMidLeft) {
        _outerBtmLeftDragPos.x += delta.x;
        _outerTopRightDragPos.x -= multiplier * delta.x;
        didSomething = true;
    } else if (_ms == eMouseStateDraggingCenter || _ms == eMouseStateDraggingOffset) {
        if (_ms == eMouseStateDraggingCenter) {
            _centerDragPos.x += delta.x;
            _centerDragPos.y += delta.y;
        } else {
            assert(_ms == eMouseStateDraggingOffset);
            _offsetDragPos.x += delta.x;
            _offsetDragPos.y += delta.y;
        }
        _innerBtmLeftDragPos.x += delta.x;
        _innerBtmLeftDragPos.y += delta.y;
        _innerTopRightDragPos.x += delta.x;
        _innerTopRightDragPos.y += delta.y;
        _outerBtmLeftDragPos.x += delta.x;
        _outerBtmLeftDragPos.y += delta.y;
        _outerTopRightDragPos.x += delta.x;
        _outerTopRightDragPos.y += delta.y;
        didSomething = true;
    }
    
    
    if ( isDraggingOuterPoint() ) {
        /// outer rect must at least contain the inner rect
        
        if (_outerBtmLeftDragPos.x > xi1) {
            _outerBtmLeftDragPos.x = xi1;
        }
        
        if (_outerBtmLeftDragPos.y > yi1) {
            _outerBtmLeftDragPos.y = yi1;
        }
        
        if (_outerTopRightDragPos.x < xi2) {
            _outerTopRightDragPos.x = xi2;
        }
        if (_outerTopRightDragPos.y < yi2) {
            _outerTopRightDragPos.y = yi2;
        }
    }
    
    if ( isDraggingInnerPoint() ) {
        /// inner rect must contain center point
        if (_innerBtmLeftDragPos.x > (xc + xoff) ) {
            double diffX = _innerBtmLeftDragPos.x - xc - xoff;
            _innerBtmLeftDragPos.x = xc + xoff;
            _outerBtmLeftDragPos.x -= diffX;
            _outerTopRightDragPos.x += multiplier * diffX;
            _innerTopRightDragPos.x += multiplier * diffX;
        }
        if (_innerBtmLeftDragPos.y > (yc + yoff)) {
            double diffY = _innerBtmLeftDragPos.y - yc - yoff;
            _innerBtmLeftDragPos.y = yc + yoff;
            _outerBtmLeftDragPos.y -= diffY;
            _outerTopRightDragPos.y += multiplier * diffY;
            _innerTopRightDragPos.y += multiplier * diffY;
        }
        if (_innerTopRightDragPos.x <= (xc + xoff)) {
            double diffX = _innerTopRightDragPos.x - xc - xoff;
            _innerTopRightDragPos.x = xc + xoff;
            _outerTopRightDragPos.x += diffX;
            _outerBtmLeftDragPos.x -= multiplier * diffX;
            _innerBtmLeftDragPos.x -= multiplier * diffX;
        }
        if (_innerTopRightDragPos.y <= (yc + yoff)) {
            double diffY = _innerTopRightDragPos.y - yc - yoff;
            _innerTopRightDragPos.y = yc + yoff;
            _outerTopRightDragPos.y -= diffY;
            _outerBtmLeftDragPos.y -= multiplier * diffY;
            _innerBtmLeftDragPos.y -= multiplier * diffY;
        }
    }
    
    ///forbid 0 pixels wide rectangles
    if (_innerTopRightDragPos.x <= _innerBtmLeftDragPos.x) {
        _innerBtmLeftDragPos.x = (_innerTopRightDragPos.x + _innerBtmLeftDragPos.x) / 2;
        _innerTopRightDragPos.x = _innerBtmLeftDragPos.x + 1;
    }
    if (_innerTopRightDragPos.y <= _innerBtmLeftDragPos.y) {
        _innerBtmLeftDragPos.y = (_innerTopRightDragPos.y + _innerBtmLeftDragPos.y) / 2;
        _innerTopRightDragPos.y = _innerBtmLeftDragPos.y + 1;
    }
    if (_outerTopRightDragPos.x <= _outerBtmLeftDragPos.x) {
        _outerBtmLeftDragPos.x = (_outerTopRightDragPos.x + _outerBtmLeftDragPos.x) / 2;
        _outerTopRightDragPos.x = _outerBtmLeftDragPos.x + 1;
    }
    if (_outerTopRightDragPos.y <= _outerBtmLeftDragPos.y) {
        _outerBtmLeftDragPos.y = (_outerTopRightDragPos.y + _outerBtmLeftDragPos.y) / 2;
        _outerTopRightDragPos.y = _outerBtmLeftDragPos.y + 1;
    }
    
    
    ///repaint if we toggled off a hovered handle
    if (lastStateWasHovered && !didSomething) {
        didSomething = true;
    }
    
    _lastMousePos = args.penPosition;
    
    return didSomething;
} // penMotion

bool
TrackerRegionInteract::penDown(const OFX::PenArgs &args)
{
    const OfxPointD& pscale = args.pixelScale;
    
    bool didSomething = false;
    double xi1, xi2, yi1, yi2, xo1, xo2, yo1, yo2, xc, yc,xoff,yoff;
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
    
    // test center first
    if ( isNearby(args.penPosition, xc,  yc,  POINT_TOLERANCE, pscale) ) {
        if (_controlDown > 0) {
            _ms = eMouseStateDraggingOffset;
        } else {
            _ms = eMouseStateDraggingCenter;
        }
        didSomething = true;
    } else if (xoff != 0 && yoff != 0 && isNearby(args.penPosition, xc + xoff, yc + yoff, POINT_TOLERANCE, pscale)) {
        _ms = eMouseStateDraggingOffset;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xi1, yi1, POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingInnerBtmLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xi2, yi1, POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingInnerBtmRight;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xi1, yi2, POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingInnerTopLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xi2, yi2, POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingInnerTopRight;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xc + xoff,  yi1, POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingInnerBtmMid;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xi1, yc + yoff,  POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingInnerMidLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xc + xoff,  yi2, POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingInnerTopMid;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xi2, yc + yoff,  POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingInnerMidRight;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xo1, yo1, POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingOuterBtmLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xo2, yo1, POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingOuterBtmRight;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xo1, yo2, POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingOuterTopLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xo2, yo2, POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingOuterTopRight;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xc + xoff,  yo1, POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingOuterBtmMid;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xo1, yc + yoff,  POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingOuterMidLeft;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xc + xoff,  yo2, POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingOuterTopMid;
        didSomething = true;
    } else if ( isNearby(args.penPosition, xo2, yc + yoff,  POINT_TOLERANCE, pscale) ) {
        _ms = eMouseStateDraggingOuterMidRight;
        didSomething = true;
    } else {
        _ms = eMouseStateIdle;
    }
    
    
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
    
    return didSomething;
} // penDown

bool
TrackerRegionInteract::isDraggingInnerPoint() const
{
    return _ms == eMouseStateDraggingInnerTopLeft ||
    _ms == eMouseStateDraggingInnerTopRight ||
    _ms == eMouseStateDraggingInnerBtmLeft ||
    _ms == eMouseStateDraggingInnerBtmRight ||
    _ms == eMouseStateDraggingInnerTopMid ||
    _ms == eMouseStateDraggingInnerMidRight ||
    _ms == eMouseStateDraggingInnerBtmMid ||
    _ms == eMouseStateDraggingInnerMidLeft;
}

bool
TrackerRegionInteract::isDraggingOuterPoint() const
{
    return _ms == eMouseStateDraggingOuterTopLeft ||
    _ms == eMouseStateDraggingOuterTopRight ||
    _ms == eMouseStateDraggingOuterBtmLeft ||
    _ms == eMouseStateDraggingOuterBtmRight ||
    _ms == eMouseStateDraggingOuterTopMid ||
    _ms == eMouseStateDraggingOuterMidRight ||
    _ms == eMouseStateDraggingOuterBtmMid ||
    _ms == eMouseStateDraggingOuterMidLeft;
}

bool
TrackerRegionInteract::penUp(const OFX::PenArgs &args)
{
    if (_ms == eMouseStateIdle) {
        return false;
    }
    
    OfxPointD center,offset;
    if (_ms == eMouseStateDraggingCenter) {
        center = _centerDragPos;
    } else {
        _center->getValue(center.x, center.y);
    }
    if (_ms == eMouseStateDraggingOffset) {
        offset = _offsetDragPos;
    } else {
        _offset->getValue(offset.x, offset.y);
    }
    _effect->beginEditBlock("setTrackerRegion");
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
        _center->setValueAtTime(args.time,_centerDragPos.x, _centerDragPos.y);
    } else if (_ms == eMouseStateDraggingOffset) {
        _offset->setValueAtTime(args.time, _offsetDragPos.x, _offsetDragPos.y);
    }
    _effect->endEditBlock();

    _ms = eMouseStateIdle;
    
    return true;
}

bool
TrackerRegionInteract::keyDown(const OFX::KeyArgs &args)
{
    if ( (args.keySymbol == kOfxKey_Control_L) || (args.keySymbol == kOfxKey_Control_R) ) {
        ++_controlDown;
    } else if ( (args.keySymbol == kOfxKey_Alt_L) || (args.keySymbol == kOfxKey_Alt_R) ) {
        ++_altDown;
    }
    
    return false;
}

bool
TrackerRegionInteract::keyUp(const OFX::KeyArgs &args)
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
}

