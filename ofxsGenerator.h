/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of openfx-supportext <https://github.com/devernay/openfx-supportext>,
 * Copyright (C) 2015 INRIA
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
 * OFX Generator plug-in helper
 */

#ifndef openfx_supportext_ofxsGenerator_h
#define openfx_supportext_ofxsGenerator_h

#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include "ofxsRectangleInteract.h"
#include "ofxNatron.h"

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

#define kParamGeneratorRange "frameRange"
#define kParamGeneratorRangeLabel "Frame Range"
#define kParamGeneratorRangeHint "Time domain."

enum GeneratorExtentEnum
{
    eGeneratorExtentFormat = 0,
    eGeneratorExtentSize,
    eGeneratorExtentProject,
    eGeneratorExtentDefault,
};

#define kParamGeneratorFormat kNatronParamFormatChoice
#define kParamGeneratorFormatLabel "Format"
#define kParamGeneratorFormatHint "The output format"

#define kParamGeneratorSize kNatronParamFormatSize
#define kParamGeneratorSizeLabel "Size"
#define kParamGeneratorSizeHint "The output dimensions of the image in pixels."

#define kParamGeneratorPAR kNatronParamFormatPar
#define kParamGeneratorPARLabel "Pixel Aspect Ratio"
#define kParamGeneratorPARHint "Output pixel aspect ratio."

#define kParamGeneratorCenter "recenter"
#define kParamGeneratorCenterLabel "Center"
#define kParamGeneratorCenterHint "Centers the region of definition to the input region of definition. " \
    "If there is no input, then the region of definition is centered to the project window."


class GeneratorPlugin
    : public OFX::ImageEffect
{
protected:
    // do not need to delete these, the ImageEffect is managing them for us
    OFX::Clip *_dstClip;
    OFX::ChoiceParam* _extent;
    OFX::ChoiceParam* _format;
    OFX::Int2DParam* _formatSize;
    OFX::DoubleParam* _formatPar;
    OFX::Double2DParam* _btmLeft;
    OFX::Double2DParam* _size;
    OFX::BooleanParam* _interactive;
    OFX::ChoiceParam *_outputComponents;
    OFX::ChoiceParam *_outputBitDepth;
    OFX::Int2DParam  *_range;
    OFX::PushButtonParam *_recenter;
    bool _useOutputComponentsAndDepth;

public:

    GeneratorPlugin(OfxImageEffectHandle handle, bool useOutputComponentsAndDepth);

protected:

    // Override to return the source clip if there's any.
    virtual OFX::Clip* getSrcClip() const { return 0; }

    void checkComponents(OFX::BitDepthEnum dstBitDepth, OFX::PixelComponentEnum dstComponents);
    bool getRegionOfDefinition(OfxRectD &rod);
    virtual void getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences) OVERRIDE;
    // must be called by derived class if it is reimplemented (see Radial.cpp for example)
    virtual bool isIdentity(const OFX::IsIdentityArguments &args,
                            OFX::Clip * &identityClip,
                            double &identityTime) OVERRIDE;

private:

    virtual void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) OVERRIDE;
    virtual bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments & /*args*/,
                                       OfxRectD &rod) OVERRIDE
    {
        return getRegionOfDefinition(rod);
    }

    /* override the time domain action, only for the general context */
    virtual bool getTimeDomain(OfxRangeD &range) OVERRIDE FINAL;


    void updateParamsVisibility();

    virtual bool paramsNotAnimated() = 0;

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
    OFX::ChoiceParam* _extent;
    GeneratorExtentEnum _extentValue;
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
                                GeneratorExtentEnum defaultType,
                                bool useOutputComponentsAndDepth,
                                ContextEnum context);
} // OFX

#endif // ifndef openfx_supportext_ofxsGenerator_h
