/*
   OFX Generator plug-in helper

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

#ifndef _OFXS_GENERATOR_H_
#define _OFXS_GENERATOR_H_

#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include "ofxsRectangleInteract.h"

#define kParamGeneratorExtent "extent"
#define kParamGeneratorExtentLabel "Extent"
#define kParamGeneratorExtentHint "Extent (size and offset) of the output."
#define kParamGeneratorExtentOptionFormat "Format"
#define kParamGeneratorExtentOptionFormatHint "Use a pre-defined image format."
#define kParamGeneratorExtentOptionSize "Size"
#define kParamGeneratorExtentOptionSizeHint "Use a specific extent (size and offset)."
#define kParamGeneratorExtentOptionProject "Project"
#define kParamGeneratorExtentOptionProjectHint "Use the project extent (size and offset)."
#define kParamGeneratorExtentOptionDefault "Default"
#define kParamGeneratorExtentOptionDefaultHint "Use the default extent (e.g. the source clip extent, if connected)."

#define kParamGeneratorOutputComponents "outputComponents"
#define kParamGeneratorOutputComponentsLabel "Output Components"
#define kParamGeneratorOutputComponentsHint "Components in the output"
#define kParamGeneratorOutputComponentsOptionRGBA "RGBA"
#define kParamGeneratorOutputComponentsOptionRGB "RGB"
#define kParamGeneratorOutputComponentsOptionAlpha "Alpha"

#define kParamGeneratorOutputBitDepth "outputBitDepth"
#define kParamGeneratorOutputBitDepthLabel "Output Bit Depth"
#define kParamGeneratorOutputBitDepthHint "Bit depth of the output.\n8 bits uses the sRGB colorspace, 16-bits uses Rec.709."
#define kParamGeneratorOutputBitDepthOptionByte "Byte (8 bits)"
#define kParamGeneratorOutputBitDepthOptionShort "Short (16 bits)"
#define kParamGeneratorOutputBitDepthOptionFloat "Float (32 bits)"


enum GeneratorTypeEnum
{
    eGeneratorTypeFormat = 0,
    eGeneratorTypeSize,
    eGeneratorTypeProject,
    eGeneratorTypeDefault,
};

#define kParamGeneratorFormat "format"
#define kParamGeneratorFormatLabel "Format"
#define kParamGeneratorFormatHint "The output format"


class GeneratorPlugin
    : public OFX::ImageEffect
{
protected:
    // do not need to delete these, the ImageEffect is managing them for us
    OFX::Clip *_dstClip;
    OFX::ChoiceParam* _type;
    OFX::ChoiceParam* _format;
    OFX::Double2DParam* _btmLeft;
    OFX::Double2DParam* _size;
    OFX::BooleanParam* _interactive;
    OFX::ChoiceParam *_outputComponents;
    OFX::ChoiceParam *_outputBitDepth;

public:

    GeneratorPlugin(OfxImageEffectHandle handle);

protected:
    void checkComponents(OFX::BitDepthEnum dstBitDepth, OFX::PixelComponentEnum dstComponents);
    bool getRegionOfDefinition(OfxRectD &rod);
    virtual void getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences) OVERRIDE;

private:

    virtual void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;
    virtual bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &/*args*/, OfxRectD &rod) OVERRIDE FINAL {
        return getRegionOfDefinition(rod);
    }
    void updateParamsVisibility();

private:
    OFX::PixelComponentEnum _outputComponentsMap[4];
    OFX::BitDepthEnum _outputBitDepthMap[4];
    bool _supportsBytes;
    bool _supportsShorts;
    bool _supportsFloats;
    bool _supportsRGBA;
    bool _supportsRGB;
    bool _supportsAlpha;
};


class GeneratorInteract
    : public OFX::RectangleInteract
{
public:

    GeneratorInteract(OfxInteractHandle handle,
                      OFX::ImageEffect* effect);

    virtual bool draw(const OFX::DrawArgs &args) OVERRIDE FINAL;
    virtual bool penMotion(const OFX::PenArgs &args) OVERRIDE FINAL;
    virtual bool penDown(const OFX::PenArgs &args) OVERRIDE FINAL;
    virtual bool penUp(const OFX::PenArgs &args) OVERRIDE FINAL;
    virtual bool keyDown(const OFX::KeyArgs &args) OVERRIDE FINAL;
    virtual bool keyUp(const OFX::KeyArgs & args) OVERRIDE FINAL;
    virtual void loseFocus(const OFX::FocusArgs &args) OVERRIDE FINAL;

private:

    virtual void aboutToCheckInteractivity(OfxTime time) OVERRIDE FINAL;
    virtual bool allowTopLeftInteraction() const OVERRIDE FINAL;
    virtual bool allowBtmRightInteraction() const OVERRIDE FINAL;
    virtual bool allowBtmLeftInteraction() const OVERRIDE FINAL;
    virtual bool allowBtmMidInteraction() const OVERRIDE FINAL;
    virtual bool allowMidLeftInteraction() const OVERRIDE FINAL;
    virtual bool allowCenterInteraction() const OVERRIDE FINAL;

    OFX::ChoiceParam* _type;
    GeneratorTypeEnum _generatorType;
};


namespace OFX {
class GeneratorOverlayDescriptor
    : public DefaultEffectOverlayDescriptor<GeneratorOverlayDescriptor, GeneratorInteract>
{
};

void generatorDescribe(OFX::ImageEffectDescriptor &desc);

void generatorDescribeInContext(PageParamDescriptor *page,
                                OFX::ImageEffectDescriptor &desc,
                                OFX::ClipDescriptor &dstClip,
                                ContextEnum context);
} // OFX

#endif // ifndef _OFXS_GENERATOR_H_
