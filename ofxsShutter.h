/*
   OFX Shutter parameter support

   Copyright (C) 2015 INRIA

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
#ifndef __SupportExt__Shutter__
#define __SupportExt__Shutter__

#include <memory>

#include "ofxsImageEffect.h"
#include "ofxsMacros.h"


#define kParamShutter "shutter"
#define kParamShutterLabel "Shutter"
#define kParamShutterHint "Controls how long (in frames) the shutter should remain open."

#define kParamShutterOffset "shutterOffset"
#define kParamShutterOffsetLabel "Shutter Offset"
#define kParamShutterOffsetHint "Controls when the shutter should be open/closed. Ignored if there is no motion blur (i.e. shutter=0 or motionBlur=0)."
#define kParamShutterOffsetOptionCentered "Centred"
#define kParamShutterOffsetOptionCenteredHint "Centers the shutter around the frame (from t-shutter/2 to t+shutter/2)"
#define kParamShutterOffsetOptionStart "Start"
#define kParamShutterOffsetOptionStartHint "Open the shutter at the frame (from t to t+shutter)"
#define kParamShutterOffsetOptionEnd "End"
#define kParamShutterOffsetOptionEndHint "Close the shutter at the frame (from t-shutter to t)"
#define kParamShutterOffsetOptionCustom "Custom"
#define kParamShutterOffsetOptionCustomHint "Open the shutter at t+shuttercustomoffset (from t+shuttercustomoffset to t+shuttercustomoffset+shutter)"

enum ShutterOffsetEnum
{
    eShutterOffsetCentered,
    eShutterOffsetStart,
    eShutterOffsetEnd,
    eShutterOffsetCustom
};


#define kParamShutterCustomOffset "shutterCustomOffset"
#define kParamShutterCustomOffsetLabel "Custom Offset"
#define kParamShutterCustomOffsetHint "When custom is selected, the shutter is open at current time plus this offset (in frames). Ignored if there is no motion blur (i.e. shutter=0 or motionBlur=0)."

namespace OFX {
    void shutterDescribeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context, OFX::PageParamDescriptor* page);
    void shutterRange(double time, double shutter, ShutterOffsetEnum shutteroffset, double shuttercustomoffset, OfxRangeD* range);
}
#endif /* defined(__SupportExt__Shutter__) */
