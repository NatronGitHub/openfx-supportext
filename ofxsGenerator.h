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
 * OFX Generator plug-in helper
 */

#ifndef openfx_supportext_ofxsGenerator_h
#define openfx_supportext_ofxsGenerator_h

#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include "ofxsRectangleInteract.h"
#ifdef OFX_EXTENSIONS_NATRON
#include "ofxNatron.h"
#endif

#define kParamGeneratorExtent "extent"
#define kParamGeneratorExtentLabel "Extent"
#define kParamGeneratorExtentHint "Extent (size and offset) of the output."
#define kParamGeneratorExtentOptionFormat "Format", "Use a pre-defined image format.", "format"
#define kParamGeneratorExtentOptionSize "Size", "Use a specific extent (size and offset).", "size"
#define kParamGeneratorExtentOptionProject "Project", "Use the project extent (size and offset).", "project"
#define kParamGeneratorExtentOptionDefault "Default", "Use the default extent (e.g. the source clip extent, if connected).", "default"

#define kParamGeneratorOutputComponents "outputComponents"
#define kParamGeneratorOutputComponentsLabel "Output Components"
#define kParamGeneratorOutputComponentsHint "Components in the output"
#define kParamGeneratorOutputComponentsOptionRGBA "RGBA"
#define kParamGeneratorOutputComponentsOptionRGB "RGB"
#define kParamGeneratorOutputComponentsOptionXY "XY"
#define kParamGeneratorOutputComponentsOptionAlpha "Alpha"

#define kParamGeneratorOutputBitDepth "outputBitDepth"
#define kParamGeneratorOutputBitDepthLabel "Output Bit Depth"
#define kParamGeneratorOutputBitDepthHint "Bit depth of the output.\n8 bits uses the sRGB colorspace, 16-bits uses Rec.709."
#define kParamGeneratorOutputBitDepthOptionByte "Byte (8 bits)", "", "8u"
#define kParamGeneratorOutputBitDepthOptionShort "Short (16 bits)", "", "16u"
#define kParamGeneratorOutputBitDepthOptionHalf "Half (16 bits)", "", "16f"
#define kParamGeneratorOutputBitDepthOptionFloat "Float (32 bits)", "", "32f"

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

#define kParamGeneratorReformat "reformat"
#define kParamGeneratorReformatLabel "Reformat"
#define kParamGeneratorReformatHint "Set the output format to the given extent, except if the Bottom Left or Size parameters is animated."


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
    OFX::BooleanParam* _reformat;
    OFX::Double2DParam* _btmLeft;
    OFX::Double2DParam* _size;
    OFX::BooleanParam* _interactive;
    OFX::BooleanParam* _hiDPI;
    OFX::ChoiceParam *_outputComponents;
    OFX::ChoiceParam *_outputBitDepth;
    OFX::Int2DParam  *_range;
    OFX::PushButtonParam *_recenter;
    bool _useOutputComponentsAndDepth;

public:

    GeneratorPlugin(OfxImageEffectHandle handle,
                    bool useOutputComponentsAndDepth,
                    bool supportsBitDepthByte,
                    bool supportsBitDepthUShort,
                    bool supportsBitDepthHalf,
                    bool supportsBitDepthFloat);

protected:

    // Override to return the source clip if there's any.
    virtual OFX::Clip* getSrcClip() const { return 0; }

    void checkComponents(OFX::BitDepthEnum dstBitDepth, OFX::PixelComponentEnum dstComponents);
    bool getRegionOfDefinition(double time, OfxRectD &rod);
    virtual void getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences) OVERRIDE;


private:

    virtual void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) OVERRIDE;

    /** @brief The sync private data action, called when the effect needs to sync any private data to persistent parameters */
    virtual void syncPrivateData(void) OVERRIDE
    {
        updateParamsVisibility();
    }

    virtual bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments & args,
                                       OfxRectD &rod) OVERRIDE
    {
        return getRegionOfDefinition(args.time, rod);
    }

    /* override the time domain action, only for the general context */
    virtual bool getTimeDomain(OfxRangeD &range) OVERRIDE FINAL;


    void updateParamsVisibility();

private:
    OFX::PixelComponentEnum _outputComponentsMap[10];
    OFX::BitDepthEnum _outputBitDepthMap[10];
    bool _supportsByte;
    bool _supportsUShort;
    bool _supportsHalf;
    bool _supportsFloat;
    bool _supportsRGBA;
    bool _supportsRGB;
    bool _supportsXY;
    bool _supportsAlpha;
};


class GeneratorInteract
    : public OFX::RectangleInteract
{
public:

    GeneratorInteract(OfxInteractHandle handle,
                      OFX::ImageEffect* effect,
                      const char* angleParamName = NULL);

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

class GeneratorOverlayDescriptorV2
    : public DefaultEffectOverlayDescriptorV2<GeneratorOverlayDescriptorV2, GeneratorInteract>
{
};

// Set the generator overlay. If hasAngle is true, the effect must also have a
// kParamRectangleInteractAngle parameter, and the overlay is rotated by that angle.
void generatorDescribe(OFX::ImageEffectDescriptor &desc, bool hasAngle = false);

/// How the generator overlay draws, for generatorDescribe(desc, [hasAngle,] overlayDraw).
enum GeneratorOverlayDrawEnum
{
    eGeneratorOverlayDrawOpenGL = 0, //< OpenGL (kOfxImageEffectPluginPropOverlayInteractV1), like generatorDescribe(desc, hasAngle)
    eGeneratorOverlayDrawSuite //< OFX 1.5 DrawSuite on hosts that provide it (V2), else OpenGL
};

/// Set the generator overlay, optionally rotated (hasAngle, as above). With
/// eGeneratorOverlayDrawSuite, it is registered with
/// ofxsSetOverlayInteractDescriptorV2, and draws with the DrawSuite on hosts that
/// provide it: only use it if the plugin adds no OpenGL drawing of its own to the
/// generator overlay.
/// generatorDescribe(desc, overlayDraw) is generatorDescribe(desc, false, overlayDraw).
/// These overloads have no default argument, so that generatorDescribe(desc) and
/// generatorDescribe(desc, bool) always select the OpenGL overlay above.
void generatorDescribe(OFX::ImageEffectDescriptor &desc, bool hasAngle, GeneratorOverlayDrawEnum overlayDraw);
void generatorDescribe(OFX::ImageEffectDescriptor &desc, GeneratorOverlayDrawEnum overlayDraw);

/// The generator parameters, in this order. A plug-in that needs another order (e.g. to follow
/// the Nuke node's knob order) calls the parts below instead, in any order, with these rules:
/// - generatorDescribeFormat(), generatorDescribeArea() and generatorDescribeFrameRange() are each
///   called exactly once (GeneratorPlugin fetches their parameters);
/// - generatorDescribeOutputComponents() is called exactly once if and only if the plug-in
///   constructs GeneratorPlugin with useOutputComponentsAndDepth = true;
/// - generatorDescribeArea() comes before generatorDescribeEnd() (it detects whether the host
///   supports normalised defaults), and generatorDescribeEnd() is called once, last.
/// Hosts identify parameters by name, so the order only changes the GUI.
void generatorDescribeInContext(PageParamDescriptor *page,
                                OFX::ImageEffectDescriptor &desc,
                                OFX::ClipDescriptor &dstClip,
                                GeneratorExtentEnum defaultType,
                                PixelComponentEnum defaultComponents, // either RGBA, RGB, XY or Alpha
                                bool useOutputComponentsAndDepth,
                                ContextEnum context,
                                bool reformat = true);

/// Extent, Center, Reformat (Natron), Format and the custom format size and PAR.
void generatorDescribeFormat(PageParamDescriptor *page, OFX::ImageEffectDescriptor &desc,
                             GeneratorExtentEnum defaultType, bool reformat = true);
/// Bottom Left, Size, Interactive Update and the hidden HiDPI parameter (the area overlay).
void generatorDescribeArea(PageParamDescriptor *page, OFX::ImageEffectDescriptor &desc);
/// Frame Range (general context only).
void generatorDescribeFrameRange(PageParamDescriptor *page, OFX::ImageEffectDescriptor &desc,
                                 ContextEnum context);
/// Output Components and Output Bit Depth (with the components and depths the host supports).
void generatorDescribeOutputComponents(PageParamDescriptor *page, OFX::ImageEffectDescriptor &desc,
                                       OFX::ClipDescriptor &dstClip,
                                       PixelComponentEnum defaultComponents); // either RGBA, RGB, XY or Alpha
/// The hidden parameters: call it once, after all the other generator parts.
void generatorDescribeEnd(PageParamDescriptor *page, OFX::ImageEffectDescriptor &desc);
} // OFX

#endif // ifndef openfx_supportext_ofxsGenerator_h
