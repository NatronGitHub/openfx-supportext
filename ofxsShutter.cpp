/*
   OFX Transform3x3 plugin: a base plugin for 2D homographic transform,
   represented by a 3x3 matrix.

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

/*
 Although the indications from nuke/fnOfxExtensions.h were followed, and the
 kFnOfxImageEffectActionGetTransform action was implemented in the Support
 library, that action is never called by the Nuke host.

 The extension was implemented as specified in Natron and in the Support library.
 
 @see gHostDescription.canTransform, ImageEffectDescriptor::setCanTransform(),
 and ImageEffect::getTransform().

 There is also an open question about how the last plugin in a transform chain
 may get the concatenated transform from upstream, the untransformed source image,
 concatenate its own transform and apply the resulting transform in its render
 action.
 
 Our solution is to have kFnOfxImageEffectCanTransform set on source clips for which
 a transform can be attached to fetched images.
 @see ClipDescriptor::setCanTransform().

 In this case, images fetched from the host may have a kFnOfxPropMatrix2D attached,
 which must be combined with the transformation applied by the effect (which
 may be any deformation function, not only a homography).
 @see ImageBase::getTransform() and ImageBase::getTransformIsIdentity
 */
// Uncomment the following to enable the experimental host transform code.
#define ENABLE_HOST_TRANSFORM

//include <memory>
//#include <algorithm>

#include "ofxsShutter.h"

using namespace OFX;


void
OFX::shutterDescribeInContext(OFX::ImageEffectDescriptor &desc,
                              OFX::ContextEnum /*context*/,
                              OFX::PageParamDescriptor* page)
{

    // shutter
    {
        DoubleParamDescriptor* param = desc.defineDoubleParam(kParamShutter);
        param->setLabel(kParamShutterLabel);
        param->setHint(kParamShutterHint);
        param->setDefault(0.5);
        param->setRange(0., 2.);
        param->setIncrement(0.01);
        param->setDisplayRange(0., 2.);
        if (page) {
            page->addChild(*param);
        }
    }

    // shutteroffset
    {
        ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamShutterOffset);
        param->setLabel(kParamShutterOffsetLabel);
        param->setHint(kParamShutterOffsetHint);
        assert(param->getNOptions() == eShutterOffsetCentered);
        param->appendOption(kParamShutterOffsetOptionCentered, kParamShutterOffsetOptionCenteredHint);
        assert(param->getNOptions() == eShutterOffsetStart);
        param->appendOption(kParamShutterOffsetOptionStart, kParamShutterOffsetOptionStartHint);
        assert(param->getNOptions() == eShutterOffsetEnd);
        param->appendOption(kParamShutterOffsetOptionEnd, kParamShutterOffsetOptionEndHint);
        assert(param->getNOptions() == eShutterOffsetCustom);
        param->appendOption(kParamShutterOffsetOptionCustom, kParamShutterOffsetOptionCustomHint);
        param->setAnimates(true);
        param->setDefault(eShutterOffsetStart);
        if (page) {
            page->addChild(*param);
        }
    }

    // shuttercustomoffset
    {
        DoubleParamDescriptor* param = desc.defineDoubleParam(kParamShutterCustomOffset);
        param->setLabel(kParamShutterCustomOffsetLabel);
        param->setHint(kParamShutterCustomOffsetHint);
        param->setDefault(0.);
        param->setRange(-1., 1.);
        param->setIncrement(0.1);
        param->setDisplayRange(-1., 1.);
        if (page) {
            page->addChild(*param);
        }
    }
} // shutterDescribeInContext

void
OFX::shutterRange(double time,
                  double shutter,
                  ShutterOffsetEnum shutteroffset,
                  double shuttercustomoffset,
                  OfxRangeD* range)
{
    switch (shutteroffset) {
        case eShutterOffsetCentered:
            range->min = time - shutter / 2;
            range->max = time + shutter / 2;
            break;
        case eShutterOffsetStart:
            range->min = time;
            range->max = time + shutter;
            break;
        case eShutterOffsetEnd:
            range->min = time - shutter;
            range->max = time;
            break;
        case eShutterOffsetCustom:
            range->min = time + shuttercustomoffset;
            range->max = time + shuttercustomoffset + shutter;
            break;
        default:
            range->min = time;
            range->max = time;
            break;
    }
}
