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

#include <cmath>
#include <cassert>
#include <limits>

#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include "ofxsRectangleInteract.h"
#include "ofxsFormatResolution.h"
#include "ofxsSupportPrivate.h"

#define kParamExtent "extent"
#define kParamExtentLabel "Extent"
#define kParamExtentHint "Extent (size and offset) of the output."
#define kParamExtentOptionFormat "Format"
#define kParamExtentOptionFormatHint "Use a pre-defined image format."
#define kParamExtentOptionSize "Size"
#define kParamExtentOptionSizeHint "Use a specific extent (size and offset)."
#define kParamExtentOptionProject "Project"
#define kParamExtentOptionProjectHint "Use the project extent (size and offset)."
#define kParamExtentOptionDefault "Default"
#define kParamExtentOptionDefaultHint "Use the default extent (e.g. the source clip extent, if connected)."

#define kParamOutputComponents "outputComponents"
#define kParamOutputComponentsLabel "Output Components"
#define kParamOutputComponentsHint "Components in the output"
#define kParamOutputComponentsOptionRGBA "RGBA"
#define kParamOutputComponentsOptionRGB "RGB"
#define kParamOutputComponentsOptionAlpha "Alpha"

#define kParamOutputBitDepth "outputBitDepth"
#define kParamOutputBitDepthLabel "Output Bit Depth"
#define kParamOutputBitDepthHint "Bit depth of the output.\n8 bits uses the sRGB colorspace, 16-bits uses Rec.709."
#define kParamOutputBitDepthOptionByte "Byte (8 bits)"
#define kParamOutputBitDepthOptionShort "Short (16 bits)"
#define kParamOutputBitDepthOptionFloat "Float (32 bits)"


enum GeneratorTypeEnum
{
    eGeneratorTypeFormat = 0,
    eGeneratorTypeSize,
    eGeneratorTypeProject,
    eGeneratorTypeDefault,
};

#define kParamFormat "format"
#define kParamFormatLabel "Format"
#define kParamFormatHint "The output format"


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

    GeneratorPlugin(OfxImageEffectHandle handle)
        : OFX::ImageEffect(handle)
          , _dstClip(0)
          , _type(0)
          , _format(0)
          , _btmLeft(0)
          , _size(0)
          , _interactive(0)
          , _outputComponents(0)
          , _outputBitDepth(0)
          , _supportsBytes(0)
          , _supportsShorts(0)
          , _supportsFloats(0)
          , _supportsRGBA(0)
          , _supportsRGB(0)
          , _supportsAlpha(0)
    {
        _dstClip = fetchClip(kOfxImageEffectOutputClipName);
        assert( _dstClip && (_dstClip->getPixelComponents() == OFX::ePixelComponentRGB ||
                             _dstClip->getPixelComponents() == OFX::ePixelComponentRGBA ||
                             _dstClip->getPixelComponents() == OFX::ePixelComponentAlpha) );

        _type = fetchChoiceParam(kParamExtent);
        _format = fetchChoiceParam(kParamFormat);
        _btmLeft = fetchDouble2DParam(kParamRectangleInteractBtmLeft);
        _size = fetchDouble2DParam(kParamRectangleInteractSize);
        _interactive = fetchBooleanParam(kParamRectangleInteractInteractive);
        assert(_type && _format && _btmLeft && _size && _interactive);
        _outputComponents = fetchChoiceParam(kParamOutputComponents);
        if (OFX::getImageEffectHostDescription()->supportsMultipleClipDepths) {
            _outputBitDepth = fetchChoiceParam(kParamOutputBitDepth);
        }
        updateParamsVisibility();

        const OFX::PropertySet &effectProps = getPropertySet();

        int numPixelDepths = effectProps.propGetDimension(kOfxImageEffectPropSupportedPixelDepths);
        for(int i = 0; i < numPixelDepths; ++i) {
            OFX::BitDepthEnum pixelDepth = OFX::mapStrToBitDepthEnum(effectProps.propGetString(kOfxImageEffectPropSupportedPixelDepths, i));
            bool supported = OFX::getImageEffectHostDescription()->supportsBitDepth(pixelDepth);
            switch (pixelDepth) {
                case OFX::eBitDepthUByte:
                    _supportsBytes  = supported;
                    break;
                case OFX::eBitDepthUShort:
                    _supportsShorts = supported;
                    break;
                case OFX::eBitDepthFloat:
                    _supportsFloats = supported;
                    break;
                default:
                    // other bitdepths are not supported by this plugin
                    break;
            }
        }
        {
            int i = 0;
            if (_supportsFloats) {
                _outputBitDepthMap[i] = OFX::eBitDepthFloat;
                ++i;
            }
            if (_supportsShorts) {
                _outputBitDepthMap[i] = OFX::eBitDepthUShort;
                ++i;
            }
            if (_supportsBytes) {
                _outputBitDepthMap[i] = OFX::eBitDepthUByte;
                ++i;
            }
            _outputBitDepthMap[i] = OFX::eBitDepthNone;
        }

        const OFX::PropertySet &dstClipProps = _dstClip->getPropertySet();

        int numComponents = dstClipProps.propGetDimension(kOfxImageEffectPropSupportedComponents);
        for(int i = 0; i < numComponents; ++i) {
            OFX::PixelComponentEnum pixelComponents = OFX::mapStrToPixelComponentEnum(dstClipProps.propGetString(kOfxImageEffectPropSupportedComponents, i));
            bool supported = OFX::getImageEffectHostDescription()->supportsPixelComponent(pixelComponents);
            switch (pixelComponents) {
                case OFX::ePixelComponentRGBA:
                    _supportsRGBA  = supported;
                    break;
                case OFX::ePixelComponentRGB:
                    _supportsRGB = supported;
                    break;
                case OFX::ePixelComponentAlpha:
                    _supportsAlpha = supported;
                    break;
                default:
                    // other components are not supported by this plugin
                    break;
            }
        }
        {
            int i = 0;
            if (_supportsRGBA) {
                _outputComponentsMap[i] = OFX::ePixelComponentRGBA;
                ++i;
            }
            if (_supportsRGB) {
                _outputComponentsMap[i] = OFX::ePixelComponentRGB;
                ++i;
            }
            if (_supportsAlpha) {
                _outputComponentsMap[i] = OFX::ePixelComponentAlpha;
                ++i;
            }
            _outputComponentsMap[i] = OFX::ePixelComponentNone;
        }
    }

protected:
    void
    checkComponents(OFX::BitDepthEnum dstBitDepth, OFX::PixelComponentEnum dstComponents)
    {
        // get the components of _dstClip
        int outputComponents_i;
        _outputComponents->getValue(outputComponents_i);
        OFX::PixelComponentEnum outputComponents = _outputComponentsMap[outputComponents_i];
        if (dstComponents != outputComponents) {
            setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host dit not take into account output components");
            OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
        }

        if (OFX::getImageEffectHostDescription()->supportsMultipleClipDepths) {
            // get the bitDepth of _dstClip
            int outputBitDepth_i;
            _outputBitDepth->getValue(outputBitDepth_i);
            OFX::BitDepthEnum outputBitDepth = _outputBitDepthMap[outputBitDepth_i];
            if (dstBitDepth != outputBitDepth) {
                setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host dit not take into account output bit depth");
                OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
            }
        }
    }

private:

    virtual void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;
    virtual bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;
    virtual void getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;
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

void
GeneratorPlugin::updateParamsVisibility()
{
        int type_i;
        _type->getValue(type_i);
        GeneratorTypeEnum type = (GeneratorTypeEnum)type_i;

    bool hasFormat = (type == eGeneratorTypeFormat);
    bool hasSize = (type == eGeneratorTypeSize);

    _format->setEnabled(hasFormat);
    _format->setIsSecret(!hasFormat);
    _size->setEnabled(hasSize);
    _size->setIsSecret(!hasSize);
    _btmLeft->setEnabled(hasSize);
    _btmLeft->setIsSecret(!hasSize);
    _interactive->setEnabled(hasSize);
    _interactive->setIsSecret(!hasSize);
}

void
GeneratorPlugin::changedParam(const OFX::InstanceChangedArgs &args,
                              const std::string &paramName)
{
    if (paramName == kParamExtent && args.reason == OFX::eChangeUserEdit) {
        updateParamsVisibility();
    }
}

bool
GeneratorPlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &/*args*/,
                                       OfxRectD &rod)
{
    int type_i;
    _type->getValue(type_i);
    GeneratorTypeEnum type = (GeneratorTypeEnum)type_i;
    switch (type) {
    case eGeneratorTypeFormat: {
        int format_i;
        _format->getValue(format_i);
        double par;
        size_t w,h;
        getFormatResolution( (OFX::EParamFormat)format_i, &w, &h, &par );
        rod.x1 = rod.y1 = 0;
        rod.x2 = w;
        rod.y2 = h;

        return true;
    }
    case eGeneratorTypeSize: {
        _size->getValue(rod.x2, rod.y2);
        _btmLeft->getValue(rod.x1, rod.y1);
        rod.x2 += rod.x1;
        rod.y2 += rod.y1;

        return true;
    }
    case eGeneratorTypeProject: {
        OfxPointD ext = getProjectExtent();
        OfxPointD off = getProjectOffset();
        rod.x1 = off.x;
        rod.x2 = ext.x;
        rod.y1 = off.y;
        rod.y2 = ext.y;

        return true;
    }
    case eGeneratorTypeDefault:
      
        return false;
    }
    return false;
}

void
GeneratorPlugin::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    double par = 0.;
    int type_i;

    _type->getValue(type_i);
    GeneratorTypeEnum type = (GeneratorTypeEnum)type_i;
    switch (type) {
    case eGeneratorTypeFormat: {
        //specific output format
        int index;
        _format->getValue(index);
        size_t w,h;
        getFormatResolution( (OFX::EParamFormat)index, &w, &h, &par );
        break;
    }
    case eGeneratorTypeProject:
    case eGeneratorTypeDefault: {
        par = getProjectPixelAspectRatio();
        break;
    }
    case eGeneratorTypeSize:
        break;
    }

    if (par != 0.) {
      clipPreferences.setPixelAspectRatio(*_dstClip, par);
    }

    // set the components of _dstClip
    int outputComponents_i;
    _outputComponents->getValue(outputComponents_i);
    OFX::PixelComponentEnum outputComponents = _outputComponentsMap[outputComponents_i];
    clipPreferences.setClipComponents(*_dstClip, outputComponents);

    if (OFX::getImageEffectHostDescription()->supportsMultipleClipDepths) {
        // set the bitDepth of _dstClip
        int outputBitDepth_i;
        _outputBitDepth->getValue(outputBitDepth_i);
        OFX::BitDepthEnum outputBitDepth = _outputBitDepthMap[outputBitDepth_i];
        clipPreferences.setClipBitDepth(*_dstClip, outputBitDepth);
    }
}

class GeneratorInteract
    : public OFX::RectangleInteract
{
public:

    GeneratorInteract(OfxInteractHandle handle,
                      OFX::ImageEffect* effect)
        : RectangleInteract(handle,effect)
          , _type(0)
          , _generatorType(eGeneratorTypeDefault)
    {
        _type = effect->fetchChoiceParam(kParamExtent);
        assert(_type);
    }

    virtual bool draw(const OFX::DrawArgs &args) OVERRIDE FINAL;
    virtual bool penMotion(const OFX::PenArgs &args) OVERRIDE FINAL;
    virtual bool penDown(const OFX::PenArgs &args) OVERRIDE FINAL;
    virtual bool penUp(const OFX::PenArgs &args) OVERRIDE FINAL;
    virtual bool keyDown(const OFX::KeyArgs &args) OVERRIDE FINAL;
    virtual bool keyUp(const OFX::KeyArgs & args) OVERRIDE FINAL;

private:

    virtual void aboutToCheckInteractivity(OfxTime /*time*/)  OVERRIDE FINAL
    {
        int type_i;
        _type->getValue(type_i);
        _generatorType = (GeneratorTypeEnum)type_i;
    }

    virtual bool allowTopLeftInteraction() const OVERRIDE FINAL
    {
        return _generatorType == eGeneratorTypeSize;
    }

    virtual bool allowBtmRightInteraction() const OVERRIDE FINAL
    {
        return _generatorType == eGeneratorTypeSize;
    }

    virtual bool allowBtmLeftInteraction() const OVERRIDE FINAL
    {
        return _generatorType == eGeneratorTypeSize;
    }

    virtual bool allowBtmMidInteraction() const OVERRIDE FINAL
    {
        return _generatorType == eGeneratorTypeSize;
    }

    virtual bool allowMidLeftInteraction() const OVERRIDE FINAL
    {
        return _generatorType == eGeneratorTypeSize;
    }

    virtual bool allowCenterInteraction() const OVERRIDE FINAL
    {
        return _generatorType == eGeneratorTypeSize;
    }

    OFX::ChoiceParam* _type;
    GeneratorTypeEnum _generatorType;
};

bool
GeneratorInteract::draw(const OFX::DrawArgs &args)
{
    int type_i;
    _type->getValue(type_i);
    GeneratorTypeEnum type = (GeneratorTypeEnum)type_i;

    if (type != eGeneratorTypeSize) {
        return false;
    }

    return RectangleInteract::draw(args);
}

bool
GeneratorInteract::penMotion(const OFX::PenArgs &args)
{
    int type_i;
    _type->getValue(type_i);
    GeneratorTypeEnum type = (GeneratorTypeEnum)type_i;

    if (type != eGeneratorTypeSize) {
        return false;
    }

    return RectangleInteract::penMotion(args);
}

bool
GeneratorInteract::penDown(const OFX::PenArgs &args)
{
    int type_i;
    _type->getValue(type_i);
    GeneratorTypeEnum type = (GeneratorTypeEnum)type_i;

    if (type != eGeneratorTypeSize) {
        return false;
    }

    return RectangleInteract::penDown(args);
}

bool
GeneratorInteract::penUp(const OFX::PenArgs &args)
{
    int type_i;
    _type->getValue(type_i);
    GeneratorTypeEnum type = (GeneratorTypeEnum)type_i;

    if (type != eGeneratorTypeSize) {
        return false;
    }

    return RectangleInteract::penUp(args);
}


bool
GeneratorInteract::keyDown(const OFX::KeyArgs &args)
{
    int type_i;
    _type->getValue(type_i);
    GeneratorTypeEnum type = (GeneratorTypeEnum)type_i;

    if (type != eGeneratorTypeSize) {
        return false;
    }
    
    return RectangleInteract::keyDown(args);
}

bool
GeneratorInteract::keyUp(const OFX::KeyArgs & args)
{
    int type_i;
    _type->getValue(type_i);
    GeneratorTypeEnum type = (GeneratorTypeEnum)type_i;

    if (type != eGeneratorTypeSize) {
        return false;
    }

    return RectangleInteract::keyUp(args);
}

namespace OFX {
class GeneratorOverlayDescriptor
    : public DefaultEffectOverlayDescriptor<GeneratorOverlayDescriptor, GeneratorInteract>
{
};

inline void
generatorDescribeInteract(OFX::ImageEffectDescriptor &desc)
{
    desc.setOverlayInteractDescriptor(new GeneratorOverlayDescriptor);
}

inline void
generatorDescribeInContext(PageParamDescriptor *page,
                           OFX::ImageEffectDescriptor &desc,
                           OFX::ClipDescriptor &dstClip,
                           ContextEnum /*context*/)
{
    {
        ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamExtent);
        param->setLabels(kParamExtentLabel, kParamExtentLabel, kParamExtentLabel);
        param->setHint(kParamExtentHint);
        assert(param->getNOptions() == eGeneratorTypeFormat);
        param->appendOption(kParamExtentOptionFormat, kParamExtentOptionFormatHint);
        assert(param->getNOptions() == eGeneratorTypeSize);
        param->appendOption(kParamExtentOptionSize, kParamExtentOptionSizeHint);
        assert(param->getNOptions() == eGeneratorTypeProject);
        param->appendOption(kParamExtentOptionProject, kParamExtentOptionProjectHint);
        assert(param->getNOptions() == eGeneratorTypeDefault);
        param->appendOption(kParamExtentOptionDefault, kParamExtentOptionDefaultHint);
        param->setAnimates(false);
        param->setDefault(3);
        page->addChild(*param);
    }
    {
        ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamFormat);
        param->setLabels(kParamFormatLabel, kParamFormatLabel, kParamFormatLabel);
        param->setAnimates(false);
        assert(param->getNOptions() == eParamFormatPCVideo);
        param->appendOption(kParamFormatPCVideoLabel);
        assert(param->getNOptions() == eParamFormatNTSC);
        param->appendOption(kParamFormatNTSCLabel);
        assert(param->getNOptions() == eParamFormatPAL);
        param->appendOption(kParamFormatPALLabel);
        assert(param->getNOptions() == eParamFormatHD);
        param->appendOption(kParamFormatHDLabel);
        assert(param->getNOptions() == eParamFormatNTSC169);
        param->appendOption(kParamFormatNTSC169Label);
        assert(param->getNOptions() == eParamFormatPAL169);
        param->appendOption(kParamFormatPAL169Label);
        assert(param->getNOptions() == eParamFormat1kSuper35);
        param->appendOption(kParamFormat1kSuper35Label);
        assert(param->getNOptions() == eParamFormat1kCinemascope);
        param->appendOption(kParamFormat1kCinemascopeLabel);
        assert(param->getNOptions() == eParamFormat2kSuper35);
        param->appendOption(kParamFormat2kSuper35Label);
        assert(param->getNOptions() == eParamFormat2kCinemascope);
        param->appendOption(kParamFormat2kCinemascopeLabel);
        assert(param->getNOptions() == eParamFormat4kSuper35);
        param->appendOption(kParamFormat4kSuper35Label);
        assert(param->getNOptions() == eParamFormat4kCinemascope);
        param->appendOption(kParamFormat4kCinemascopeLabel);
        assert(param->getNOptions() == eParamFormatSquare256);
        param->appendOption(kParamFormatSquare256Label);
        assert(param->getNOptions() == eParamFormatSquare512);
        param->appendOption(kParamFormatSquare512Label);
        assert(param->getNOptions() == eParamFormatSquare1k);
        param->appendOption(kParamFormatSquare1kLabel);
        assert(param->getNOptions() == eParamFormatSquare2k);
        param->appendOption(kParamFormatSquare2kLabel);
        param->setDefault(0);
        param->setHint(kParamFormatHint);
        page->addChild(*param);
    }

    // btmLeft
    {
        Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamRectangleInteractBtmLeft);
        param->setLabels(kParamRectangleInteractBtmLeftLabel,kParamRectangleInteractBtmLeftLabel,kParamRectangleInteractBtmLeftLabel);
        param->setDoubleType(OFX::eDoubleTypeXYAbsolute);
        param->setDefaultCoordinateSystem(OFX::eCoordinatesNormalised);
        param->setDefault(0., 0.);
        param->setIncrement(1.);
        param->setHint("Coordinates of the bottom left corner of the size rectangle.");
        param->setDigits(0);
        page->addChild(*param);
    }

    // size
    {
        Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamRectangleInteractSize);
        param->setLabels(kParamRectangleInteractSizeLabel, kParamRectangleInteractSizeLabel, kParamRectangleInteractSizeLabel);
        param->setDoubleType(OFX::eDoubleTypeXYAbsolute);
        param->setDefaultCoordinateSystem(OFX::eCoordinatesNormalised);
        param->setDefault(1., 1.);
        param->setIncrement(1.);
        param->setDimensionLabels(kParamRectangleInteractSizeDim1, kParamRectangleInteractSizeDim1);
        param->setHint("Width and height of the size rectangle.");
        param->setIncrement(1.);
        param->setDigits(0);
        page->addChild(*param);
    }

    // interactive
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamRectangleInteractInteractive);
        param->setLabels(kParamRectangleInteractInteractiveLabel, kParamRectangleInteractInteractiveLabel, kParamRectangleInteractInteractiveLabel);
        param->setHint(kParamRectangleInteractInteractiveHint);
        param->setEvaluateOnChange(false);
        page->addChild(*param);
    }

    bool supportsBytes  = false;
    bool supportsShorts = false;
    bool supportsFloats = false;
    OFX::BitDepthEnum outputBitDepthMap[4];

    const OFX::PropertySet &effectProps = desc.getPropertySet();

    int numPixelDepths = effectProps.propGetDimension(kOfxImageEffectPropSupportedPixelDepths);
    for(int i = 0; i < numPixelDepths; ++i) {
        OFX::BitDepthEnum pixelDepth = OFX::mapStrToBitDepthEnum(effectProps.propGetString(kOfxImageEffectPropSupportedPixelDepths, i));
        bool supported = OFX::getImageEffectHostDescription()->supportsBitDepth(pixelDepth);
        switch (pixelDepth) {
            case OFX::eBitDepthUByte:
                supportsBytes  = supported;
                break;
            case OFX::eBitDepthUShort:
                supportsShorts = supported;
                break;
            case OFX::eBitDepthFloat:
                supportsFloats = supported;
                break;
            default:
                // other bitdepths are not supported by this plugin
                break;
        }
    }
    {
        int i = 0;
        if (supportsFloats) {
            outputBitDepthMap[i] = OFX::eBitDepthFloat;
            ++i;
        }
        if (supportsShorts) {
            outputBitDepthMap[i] = OFX::eBitDepthUShort;
            ++i;
        }
        if (supportsBytes) {
            outputBitDepthMap[i] = OFX::eBitDepthUByte;
            ++i;
        }
        outputBitDepthMap[i] = OFX::eBitDepthNone;
    }

    bool supportsRGBA   = false;
    bool supportsRGB    = false;
    bool supportsAlpha  = false;

    OFX::PixelComponentEnum outputComponentsMap[4];

    const OFX::PropertySet &dstClipProps = dstClip.getPropertySet();
    int numComponents = dstClipProps.propGetDimension(kOfxImageEffectPropSupportedComponents);
    for(int i = 0; i < numComponents; ++i) {
        OFX::PixelComponentEnum pixelComponents = OFX::mapStrToPixelComponentEnum(dstClipProps.propGetString(kOfxImageEffectPropSupportedComponents, i));
        bool supported = OFX::getImageEffectHostDescription()->supportsPixelComponent(pixelComponents);
        switch (pixelComponents) {
            case OFX::ePixelComponentRGBA:
                supportsRGBA  = supported;
                break;
            case OFX::ePixelComponentRGB:
                supportsRGB = supported;
                break;
            case OFX::ePixelComponentAlpha:
                supportsAlpha = supported;
                break;
            default:
                // other components are not supported by this plugin
                break;
        }
    }
    {
        int i = 0;
        if (supportsRGBA) {
            outputComponentsMap[i] = OFX::ePixelComponentRGBA;
            ++i;
        }
        if (supportsRGB) {
            outputComponentsMap[i] = OFX::ePixelComponentRGB;
            ++i;
        }
        if (supportsAlpha) {
            outputComponentsMap[i] = OFX::ePixelComponentAlpha;
            ++i;
        }
        outputComponentsMap[i] = OFX::ePixelComponentNone;
    }

    // outputComponents
    {
        ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamOutputComponents);
        param->setLabels(kParamOutputComponentsLabel, kParamOutputComponentsLabel, kParamOutputComponentsLabel);
        param->setHint(kParamOutputComponentsHint);
        // the following must be in the same order as in describe(), so that the map works
        if (supportsRGBA) {
            assert(outputComponentsMap[param->getNOptions()] == ePixelComponentRGBA);
            param->appendOption(kParamOutputComponentsOptionRGBA);
        }
        if (supportsRGB) {
            assert(outputComponentsMap[param->getNOptions()] == ePixelComponentRGB);
            param->appendOption(kParamOutputComponentsOptionRGB);
        }
        if (supportsAlpha) {
            assert(outputComponentsMap[param->getNOptions()] == ePixelComponentAlpha);
            param->appendOption(kParamOutputComponentsOptionAlpha);
        }
        param->setDefault(0);
        param->setAnimates(false);
        desc.addClipPreferencesSlaveParam(*param);
        page->addChild(*param);
    }

    // ouputBitDepth
    if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
        ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamOutputBitDepth);
        param->setLabels(kParamOutputBitDepthLabel, kParamOutputBitDepthLabel, kParamOutputBitDepthLabel);
        param->setHint(kParamOutputBitDepthHint);
        // the following must be in the same order as in describe(), so that the map works
        if (supportsFloats) {
            // coverity[check_return]
            assert(0 <= param->getNOptions() && param->getNOptions() < 4 && outputBitDepthMap[param->getNOptions()] == eBitDepthFloat);
            param->appendOption(kParamOutputBitDepthOptionFloat);
        }
        if (supportsShorts) {
            // coverity[check_return]
            assert(0 <= param->getNOptions() && param->getNOptions() < 4 && outputBitDepthMap[param->getNOptions()] == eBitDepthUShort);
            param->appendOption(kParamOutputBitDepthOptionShort);
        }
        if (supportsBytes) {
            // coverity[check_return]
            assert(0 <= param->getNOptions() && param->getNOptions() < 4 && outputBitDepthMap[param->getNOptions()] == eBitDepthUByte);
            param->appendOption(kParamOutputBitDepthOptionByte);
        }
        param->setDefault(0);
        param->setAnimates(false);
#ifndef DEBUG
        // Shuffle only does linear conversion, which is useless for 8-bits and 16-bits formats.
        // Disable it for now (in the future, there may be colorspace conversion options)
        param->setIsSecret(true); // always secret
#endif
        desc.addClipPreferencesSlaveParam(*param);
        page->addChild(*param);
    }

} // generatorDescribeInContext
} // OFX

#endif // ifndef _OFXS_GENERATOR_H_
