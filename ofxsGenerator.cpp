/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of openfx-supportext <https://github.com/devernay/openfx-supportext>,
 * Copyright (C) 2013-2016 INRIA
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

#include "ofxsGenerator.h"

#include <cmath>
#include <cassert>
#include <limits>

#include "ofxsFormatResolution.h"
#include "ofxsCoords.h"

GeneratorPlugin::GeneratorPlugin(OfxImageEffectHandle handle,
                                 bool useOutputComponentsAndDepth,
                                 bool supportsBitDepthByte,
                                 bool supportsBitDepthUShort,
                                 bool supportsBitDepthFloat)
    : OFX::ImageEffect(handle)
    , _dstClip(0)
    , _extent(0)
    , _format(0)
    , _formatSize(0)
    , _formatPar(0)
    , _btmLeft(0)
    , _size(0)
    , _interactive(0)
    , _outputComponents(0)
    , _outputBitDepth(0)
    , _range(0)
    , _recenter(0)
    , _useOutputComponentsAndDepth(useOutputComponentsAndDepth)
    , _supportsBytes(supportsBitDepthByte)
    , _supportsShorts(supportsBitDepthUShort)
    , _supportsFloats(supportsBitDepthFloat)
    , _supportsRGBA(0)
    , _supportsRGB(0)
    , _supportsAlpha(0)
{
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    assert( _dstClip && (!_dstClip->isConnected() || _dstClip->getPixelComponents() == OFX::ePixelComponentRGBA ||
                         _dstClip->getPixelComponents() == OFX::ePixelComponentRGB ||
                         _dstClip->getPixelComponents() == OFX::ePixelComponentXY ||
                         _dstClip->getPixelComponents() == OFX::ePixelComponentAlpha) );

    _extent = fetchChoiceParam(kParamGeneratorExtent);
    _format = fetchChoiceParam(kParamGeneratorFormat);
    _formatSize = fetchInt2DParam(kParamGeneratorSize);
    _formatPar = fetchDoubleParam(kParamGeneratorPAR);
    _btmLeft = fetchDouble2DParam(kParamRectangleInteractBtmLeft);
    _size = fetchDouble2DParam(kParamRectangleInteractSize);
    _recenter = fetchPushButtonParam(kParamGeneratorCenter);
    _interactive = fetchBooleanParam(kParamRectangleInteractInteractive);
    assert(_extent && _format && _formatSize && _formatPar && _btmLeft && _size && _interactive && _recenter);

    if (_useOutputComponentsAndDepth) {
        _outputComponents = fetchChoiceParam(kParamGeneratorOutputComponents);

        if (OFX::getImageEffectHostDescription()->supportsMultipleClipDepths) {
            _outputBitDepth = fetchChoiceParam(kParamGeneratorOutputBitDepth);
        }
    }
    if (getContext() == OFX::eContextGeneral) {
        _range   = fetchInt2DParam(kParamGeneratorRange);
        assert(_range);
    }

    updateParamsVisibility();

    // kOfxImageEffectPropSupportedPixelDepths is not a property of the effect instance
    // (only the host and the plugin descriptor)
    {
        int i = 0;
        if (_supportsFloats && OFX::getImageEffectHostDescription()->supportsBitDepth(OFX::eBitDepthFloat)) {
            _outputBitDepthMap[i] = OFX::eBitDepthFloat;
            ++i;
        }
        if (_supportsShorts && OFX::getImageEffectHostDescription()->supportsBitDepth(OFX::eBitDepthUShort)) {
            _outputBitDepthMap[i] = OFX::eBitDepthUShort;
            ++i;
        }
        if (_supportsBytes && OFX::getImageEffectHostDescription()->supportsBitDepth(OFX::eBitDepthUByte)) {
            _outputBitDepthMap[i] = OFX::eBitDepthUByte;
            ++i;
        }
        _outputBitDepthMap[i] = OFX::eBitDepthNone;
    }

    const OFX::PropertySet &dstClipProps = _dstClip->getPropertySet();
    int numComponents = dstClipProps.propGetDimension(kOfxImageEffectPropSupportedComponents);
    for (int i = 0; i < numComponents; ++i) {
        OFX::PixelComponentEnum pixelComponents = OFX::mapStrToPixelComponentEnum( dstClipProps.propGetString(kOfxImageEffectPropSupportedComponents, i) );
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

void
GeneratorPlugin::checkComponents(OFX::BitDepthEnum dstBitDepth,
                                 OFX::PixelComponentEnum dstComponents)
{
    if (!_useOutputComponentsAndDepth) {
        return;
    }

    // get the components of _dstClip
    OFX::PixelComponentEnum outputComponents = _outputComponentsMap[_outputComponents->getValue()];
    if (dstComponents != outputComponents) {
        setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host dit not take into account output components");
        OFX::throwSuiteStatusException(kOfxStatErrImageFormat);

        return;
    }

    if (OFX::getImageEffectHostDescription()->supportsMultipleClipDepths) {
        // get the bitDepth of _dstClip
        OFX::BitDepthEnum outputBitDepth = _outputBitDepthMap[_outputBitDepth->getValue()];
        if (dstBitDepth != outputBitDepth) {
            setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host dit not take into account output bit depth");
            OFX::throwSuiteStatusException(kOfxStatErrImageFormat);

            return;
        }
    }
}

/* override the time domain action, only for the general context */
bool
GeneratorPlugin::getTimeDomain(OfxRangeD &range)
{
    // this should only be called in the general context, ever!
    if (getContext() == OFX::eContextGeneral) {
        assert(_range);
        // how many frames on the input clip
        //OfxRangeD srcRange = _srcClip->getFrameRange();

        int min, max;
        _range->getValue(min, max);
        range.min = min;
        range.max = max;

        return true;
    }

    return false;
}

bool
GeneratorPlugin::isIdentity(const OFX::IsIdentityArguments &args,
                            OFX::Clip * &identityClip,
                            double &identityTime)
{
    if ( OFX::getImageEffectHostDescription()->isNatron && (getContext() == OFX::eContextGeneral) ) {
        // only Natron supports setting the identityClip to the output clip

        OFX::Clip* srcClip = getSrcClip();
        if ( srcClip && srcClip->isConnected() ) {
            // If the source clip is connected, then the output is likely not to be identity
            return false;
        }

        int min, max;
        _range->getValue(min, max);

        GeneratorExtentEnum extent = (GeneratorExtentEnum)_extent->getValue();
        if (extent == eGeneratorExtentSize) {
            ///If not animated and different than 'min' time, return identity on the min time.
            ///We need to check more parameters
            if ( paramsNotAnimated() && (_size->getNumKeys() == 0) && (_btmLeft->getNumKeys() == 0) && (args.time != min) ) {
                identityClip = _dstClip;
                identityTime = min;

                return true;
            }
        } else {
            ///If not animated and different than 'min' time, return identity on the min time.
            if ( paramsNotAnimated() && (args.time != min) ) {
                identityClip = _dstClip;
                identityTime = min;

                return true;
            }
        }
    }

    return false;
}

void
GeneratorPlugin::updateParamsVisibility()
{
    GeneratorExtentEnum extent = (GeneratorExtentEnum)_extent->getValue();
    bool hasFormat = (extent == eGeneratorExtentFormat);
    bool hasSize = (extent == eGeneratorExtentSize);

    _format->setEnabled(hasFormat);
    _format->setIsSecret(!hasFormat);
    _size->setEnabled(hasSize);
    _size->setIsSecret(!hasSize);
    _recenter->setEnabled(hasSize);
    _recenter->setIsSecret(!hasSize);
    _btmLeft->setEnabled(hasSize);
    _btmLeft->setIsSecret(!hasSize);
    _interactive->setEnabled(hasSize);
    _interactive->setIsSecret(!hasSize);
}

void
GeneratorPlugin::changedParam(const OFX::InstanceChangedArgs &args,
                              const std::string &paramName)
{
    if ( (paramName == kParamGeneratorExtent) && (args.reason == OFX::eChangeUserEdit) ) {
        updateParamsVisibility();
    } else if (paramName == kParamGeneratorFormat) {
        //the host does not handle the format itself, do it ourselves
        OFX::EParamFormat format = (OFX::EParamFormat)_format->getValue();
        int w = 0, h = 0;
        double par = -1;
        getFormatResolution(format, &w, &h, &par);
        assert(par != -1);
        _formatPar->setValue(par);
        _formatSize->setValue(w, h);
    } else if (paramName == kParamGeneratorCenter) {
        OFX::Clip* srcClip = getSrcClip();
        OfxRectD srcRoD;
        if ( srcClip && srcClip->isConnected() ) {
            srcRoD = srcClip->getRegionOfDefinition(args.time);
        } else {
            OfxPointD siz = getProjectSize();
            OfxPointD off = getProjectOffset();
            srcRoD.x1 = off.x;
            srcRoD.x2 = off.x + siz.x;
            srcRoD.y1 = off.y;
            srcRoD.y2 = off.y + siz.y;
        }
        OfxPointD center;
        center.x = (srcRoD.x2 + srcRoD.x1) / 2.;
        center.y = (srcRoD.y2 + srcRoD.y1) / 2.;

        OfxRectD rectangle;
        _size->getValue(rectangle.x2, rectangle.y2);
        _btmLeft->getValue(rectangle.x1, rectangle.y1);
        rectangle.x2 += rectangle.x1;
        rectangle.y2 += rectangle.y1;

        OfxRectD newRectangle;
        newRectangle.x1 = center.x - (rectangle.x2 - rectangle.x1) / 2.;
        newRectangle.y1 = center.y - (rectangle.y2 - rectangle.y1) / 2.;
        newRectangle.x2 = newRectangle.x1 + (rectangle.x2 - rectangle.x1);
        newRectangle.y2 = newRectangle.y1 + (rectangle.y2 - rectangle.y1);

        _size->setValue(newRectangle.x2 - newRectangle.x1, newRectangle.y2 - newRectangle.y1);
        _btmLeft->setValue(newRectangle.x1, newRectangle.y1);
    }
}

bool
GeneratorPlugin::getRegionOfDefinition(OfxRectD &rod)
{
    GeneratorExtentEnum extent = (GeneratorExtentEnum)_extent->getValue();

    switch (extent) {
    case eGeneratorExtentFormat: {
        int w, h;
        _formatSize->getValue(w, h);
        double par;
        _formatPar->getValue(par);
        OfxRectI pixelFormat;
        pixelFormat.x1 = pixelFormat.y1 = 0;
        pixelFormat.x2 = w;
        pixelFormat.y2 = h;
        OfxPointD renderScale = {1., 1.};
        OFX::Coords::toCanonical(pixelFormat, renderScale, par, &rod);

        return true;
    }
    case eGeneratorExtentSize: {
        _size->getValue(rod.x2, rod.y2);
        _btmLeft->getValue(rod.x1, rod.y1);
        rod.x2 += rod.x1;
        rod.y2 += rod.y1;

        return true;
    }
    case eGeneratorExtentProject: {
        OfxPointD siz = getProjectSize();
        OfxPointD off = getProjectOffset();
        rod.x1 = off.x;
        rod.x2 = off.x + siz.x;
        rod.y1 = off.y;
        rod.y2 = off.y + siz.y;

        return true;
    }
    case eGeneratorExtentDefault:

        return false;
    }

    return false;
}

void
GeneratorPlugin::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    double par = 0.;
    GeneratorExtentEnum extent = (GeneratorExtentEnum)_extent->getValue();

    switch (extent) {
    case eGeneratorExtentFormat: {
        //specific output format
        _formatPar->getValue(par);
        break;
    }
    case eGeneratorExtentProject:
    case eGeneratorExtentDefault: {
        /// this should be the defalut value given by the host, no need to set it.
        /// @see Instance::setupClipPreferencesArgs() in HostSupport, it should have
        /// the line:
        /// double inputPar = getProjectPixelAspectRatio();

        //par = getProjectPixelAspectRatio();
        break;
    }
    case eGeneratorExtentSize:
        break;
    }

    if (par != 0.) {
        clipPreferences.setPixelAspectRatio(*_dstClip, par);
    }

    if (_useOutputComponentsAndDepth) {
        // set the components of _dstClip
        OFX::PixelComponentEnum outputComponents = _outputComponentsMap[_outputComponents->getValue()];
        clipPreferences.setClipComponents(*_dstClip, outputComponents);

        if (OFX::getImageEffectHostDescription()->supportsMultipleClipDepths) {
            // set the bitDepth of _dstClip
            OFX::BitDepthEnum outputBitDepth = _outputBitDepthMap[_outputBitDepth->getValue()];
            clipPreferences.setClipBitDepth(*_dstClip, outputBitDepth);
        }
    }
}

GeneratorInteract::GeneratorInteract(OfxInteractHandle handle,
                                     OFX::ImageEffect* effect)
    : RectangleInteract(handle, effect)
    , _extent(0)
    , _extentValue(eGeneratorExtentDefault)
{
    _extent = effect->fetchChoiceParam(kParamGeneratorExtent);
    assert(_extent);
}

void
GeneratorInteract::aboutToCheckInteractivity(OfxTime /*time*/)
{
    _extentValue = (GeneratorExtentEnum)_extent->getValue();
}

bool
GeneratorInteract::allowTopLeftInteraction() const
{
    return _extentValue == eGeneratorExtentSize;
}

bool
GeneratorInteract::allowBtmRightInteraction() const
{
    return _extentValue == eGeneratorExtentSize;
}

bool
GeneratorInteract::allowBtmLeftInteraction() const
{
    return _extentValue == eGeneratorExtentSize;
}

bool
GeneratorInteract::allowBtmMidInteraction() const
{
    return _extentValue == eGeneratorExtentSize;
}

bool
GeneratorInteract::allowMidLeftInteraction() const
{
    return _extentValue == eGeneratorExtentSize;
}

bool
GeneratorInteract::allowCenterInteraction() const
{
    return _extentValue == eGeneratorExtentSize;
}

bool
GeneratorInteract::draw(const OFX::DrawArgs &args)
{
    GeneratorExtentEnum extent = (GeneratorExtentEnum)_extent->getValue();

    if (extent != eGeneratorExtentSize) {
        return false;
    }

    return RectangleInteract::draw(args);
}

bool
GeneratorInteract::penMotion(const OFX::PenArgs &args)
{
    GeneratorExtentEnum extent = (GeneratorExtentEnum)_extent->getValue();

    if (extent != eGeneratorExtentSize) {
        return false;
    }

    return RectangleInteract::penMotion(args);
}

bool
GeneratorInteract::penDown(const OFX::PenArgs &args)
{
    GeneratorExtentEnum extent = (GeneratorExtentEnum)_extent->getValue();

    if (extent != eGeneratorExtentSize) {
        return false;
    }

    return RectangleInteract::penDown(args);
}

bool
GeneratorInteract::penUp(const OFX::PenArgs &args)
{
    GeneratorExtentEnum extent = (GeneratorExtentEnum)_extent->getValue();

    if (extent != eGeneratorExtentSize) {
        return false;
    }

    return RectangleInteract::penUp(args);
}

void
GeneratorInteract::loseFocus(const OFX::FocusArgs &args)
{
    return RectangleInteract::loseFocus(args);
}

bool
GeneratorInteract::keyDown(const OFX::KeyArgs &args)
{
    GeneratorExtentEnum extent = (GeneratorExtentEnum)_extent->getValue();

    if (extent != eGeneratorExtentSize) {
        return false;
    }

    return RectangleInteract::keyDown(args);
}

bool
GeneratorInteract::keyUp(const OFX::KeyArgs & args)
{
    GeneratorExtentEnum extent = (GeneratorExtentEnum)_extent->getValue();

    if (extent != eGeneratorExtentSize) {
        return false;
    }

    return RectangleInteract::keyUp(args);
}

namespace OFX {
void
generatorDescribe(OFX::ImageEffectDescriptor &desc)
{
    desc.setOverlayInteractDescriptor(new GeneratorOverlayDescriptor);
}

void
generatorDescribeInContext(PageParamDescriptor *page,
                           OFX::ImageEffectDescriptor &desc,
                           OFX::ClipDescriptor &dstClip,
                           GeneratorExtentEnum defaultType,
                           bool useOutputComponentsAndDepth,
                           ContextEnum context)
{
    // extent
    {
        ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamGeneratorExtent);
        param->setLabel(kParamGeneratorExtentLabel);
        param->setHint(kParamGeneratorExtentHint);
        assert(param->getNOptions() == eGeneratorExtentFormat);
        param->appendOption(kParamGeneratorExtentOptionFormat, kParamGeneratorExtentOptionFormatHint);
        assert(param->getNOptions() == eGeneratorExtentSize);
        param->appendOption(kParamGeneratorExtentOptionSize, kParamGeneratorExtentOptionSizeHint);
        assert(param->getNOptions() == eGeneratorExtentProject);
        param->appendOption(kParamGeneratorExtentOptionProject, kParamGeneratorExtentOptionProjectHint);
        assert(param->getNOptions() == eGeneratorExtentDefault);
        param->appendOption(kParamGeneratorExtentOptionDefault, kParamGeneratorExtentOptionDefaultHint);
        param->setDefault( (int)defaultType );
        param->setLayoutHint(OFX::eLayoutHintNoNewLine);
        param->setAnimates(false);
        desc.addClipPreferencesSlaveParam(*param);
        if (page) {
            page->addChild(*param);
        }
    }

    // recenter
    {
        PushButtonParamDescriptor* param = desc.definePushButtonParam(kParamGeneratorCenter);
        param->setLabel(kParamGeneratorCenterLabel);
        param->setHint(kParamGeneratorCenterHint);
        param->setLayoutHint(OFX::eLayoutHintNoNewLine);
        if (page) {
            page->addChild(*param);
        }
    }

    // format
    {
        ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamGeneratorFormat);
        param->setLabel(kParamGeneratorFormatLabel);
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
        param->setDefault(eParamFormatPCVideo);
        param->setHint(kParamGeneratorFormatHint);
        param->setAnimates(false);
        desc.addClipPreferencesSlaveParam(*param);
        if (page) {
            page->addChild(*param);
        }
    }

    {
        int w = 0, h = 0;
        double par = -1.;
        getFormatResolution(eParamFormatPCVideo, &w, &h, &par);
        assert(par != -1);
        {
            Int2DParamDescriptor* param = desc.defineInt2DParam(kParamGeneratorSize);
            param->setLabel(kParamGeneratorSizeLabel);
            param->setHint(kParamGeneratorSizeHint);
            param->setIsSecret(true);
            param->setDefault(w, h);
            if (page) {
                page->addChild(*param);
            }
        }

        {
            DoubleParamDescriptor* param = desc.defineDoubleParam(kParamGeneratorPAR);
            param->setLabel(kParamGeneratorPARLabel);
            param->setHint(kParamGeneratorPARHint);
            param->setIsSecret(true);
            param->setRange(0., DBL_MAX);
            param->setDisplayRange(0.5, 2.);
            param->setDefault(par);
            if (page) {
                page->addChild(*param);
            }
        }
    }

    // btmLeft
    {
        Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamRectangleInteractBtmLeft);
        param->setLabel(kParamRectangleInteractBtmLeftLabel);
        param->setDoubleType(eDoubleTypeXYAbsolute);
        param->setDefaultCoordinateSystem(eCoordinatesNormalised);
        param->setDefault(0., 0.);
        param->setRange(-DBL_MAX, -DBL_MAX, DBL_MAX, DBL_MAX); // Resolve requires range and display range or values are clamped to (-1,1)
        param->setDisplayRange(-10000, -10000, 10000, 10000); // Resolve requires display range or values are clamped to (-1,1)
        param->setIncrement(1.);
        param->setLayoutHint(OFX::eLayoutHintNoNewLine);
        param->setHint("Coordinates of the bottom left corner of the size rectangle.");
        param->setDigits(0);
        if (page) {
            page->addChild(*param);
        }
    }

    // size
    {
        Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamRectangleInteractSize);
        param->setLabel(kParamRectangleInteractSizeLabel);
        param->setDoubleType(eDoubleTypeXY);
        param->setDefaultCoordinateSystem(eCoordinatesNormalised);
        param->setDefault(1., 1.);
        param->setRange(0., 0., DBL_MAX, DBL_MAX); // Resolve requires range and display range or values are clamped to (-1,1)
        param->setDisplayRange(0, 0, 10000, 10000); // Resolve requires display range or values are clamped to (-1,1)
        param->setIncrement(1.);
        param->setDimensionLabels(kParamRectangleInteractSizeDim1, kParamRectangleInteractSizeDim2);
        param->setHint("Width and height of the size rectangle.");
        param->setIncrement(1.);
        param->setDigits(0);
        if (page) {
            page->addChild(*param);
        }
    }


    // interactive
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamRectangleInteractInteractive);
        param->setLabel(kParamRectangleInteractInteractiveLabel);
        param->setHint(kParamRectangleInteractInteractiveHint);
        param->setEvaluateOnChange(false);
        if (page) {
            page->addChild(*param);
        }
    }

    // range
    if (context == OFX::eContextGeneral) {
        Int2DParamDescriptor *param = desc.defineInt2DParam(kParamGeneratorRange);
        param->setLabel(kParamGeneratorRangeLabel);
        param->setHint(kParamGeneratorRangeHint);
        param->setDefault(1, 1);
        param->setDimensionLabels("min", "max");
        param->setAnimates(false); // can not animate, because it defines the time domain
        if (page) {
            page->addChild(*param);
        }
    }

    if (useOutputComponentsAndDepth) {
        bool supportsBytes  = false;
        bool supportsShorts = false;
        bool supportsFloats = false;
        OFX::BitDepthEnum outputBitDepthMap[4];
        const OFX::PropertySet &effectProps = desc.getPropertySet();
        int numPixelDepths = effectProps.propGetDimension(kOfxImageEffectPropSupportedPixelDepths);
        for (int i = 0; i < numPixelDepths; ++i) {
            OFX::BitDepthEnum pixelDepth = OFX::mapStrToBitDepthEnum( effectProps.propGetString(kOfxImageEffectPropSupportedPixelDepths, i) );
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

        {
            bool supportsRGBA   = false;
            bool supportsRGB    = false;
            bool supportsAlpha  = false;
            OFX::PixelComponentEnum outputComponentsMap[4];
            const OFX::PropertySet &dstClipProps = dstClip.getPropertySet();
            int numComponents = dstClipProps.propGetDimension(kOfxImageEffectPropSupportedComponents);
            for (int i = 0; i < numComponents; ++i) {
                OFX::PixelComponentEnum pixelComponents = OFX::mapStrToPixelComponentEnum( dstClipProps.propGetString(kOfxImageEffectPropSupportedComponents, i) );
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
                ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamGeneratorOutputComponents);
                param->setLabel(kParamGeneratorOutputComponentsLabel);
                param->setHint(kParamGeneratorOutputComponentsHint);
                // the following must be in the same order as in describe(), so that the map works
                if (supportsRGBA) {
                    assert(outputComponentsMap[param->getNOptions()] == ePixelComponentRGBA);
                    param->appendOption(kParamGeneratorOutputComponentsOptionRGBA);
                }
                if (supportsRGB) {
                    assert(outputComponentsMap[param->getNOptions()] == ePixelComponentRGB);
                    param->appendOption(kParamGeneratorOutputComponentsOptionRGB);
                }
                if (supportsAlpha) {
                    assert(outputComponentsMap[param->getNOptions()] == ePixelComponentAlpha);
                    param->appendOption(kParamGeneratorOutputComponentsOptionAlpha);
                }
                param->setDefault(0);
                param->setAnimates(false);
                desc.addClipPreferencesSlaveParam(*param);
                if (page) {
                    page->addChild(*param);
                }
            }
        }

        // ouputBitDepth
        if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
            ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamGeneratorOutputBitDepth);
            param->setLabel(kParamGeneratorOutputBitDepthLabel);
            param->setHint(kParamGeneratorOutputBitDepthHint);
            // the following must be in the same order as in describe(), so that the map works
            if (supportsFloats) {
                // coverity[check_return]
                assert(0 <= param->getNOptions() && param->getNOptions() < 4 && outputBitDepthMap[param->getNOptions()] == eBitDepthFloat);
                param->appendOption(kParamGeneratorOutputBitDepthOptionFloat);
            }
            if (supportsShorts) {
                // coverity[check_return]
                assert(0 <= param->getNOptions() && param->getNOptions() < 4 && outputBitDepthMap[param->getNOptions()] == eBitDepthUShort);
                param->appendOption(kParamGeneratorOutputBitDepthOptionShort);
            }
            if (supportsBytes) {
                // coverity[check_return]
                assert(0 <= param->getNOptions() && param->getNOptions() < 4 && outputBitDepthMap[param->getNOptions()] == eBitDepthUByte);
                param->appendOption(kParamGeneratorOutputBitDepthOptionByte);
            }
            param->setDefault(0);
#ifndef DEBUG
            // Shuffle only does linear conversion, which is useless for 8-bits and 16-bits formats.
            // Disable it for now (in the future, there may be colorspace conversion options)
            param->setIsSecret(true); // always secret
#endif
            param->setAnimates(false);
            desc.addClipPreferencesSlaveParam(*param);
            if (page) {
                page->addChild(*param);
            }
        }
    } // useOutputComponentsAndDepth
} // generatorDescribeInContext
} // OFX
