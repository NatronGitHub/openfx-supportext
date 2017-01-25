/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of openfx-supportext <https://github.com/devernay/openfx-supportext>,
 * Copyright (C) 2013-2017 INRIA
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
 * Helper functions to implement plug-ins that support kFnOfxImageEffectPlaneSuite v2
 * In order to use these functions the following condition must be met:
 *#if defined(OFX_EXTENSIONS_NUKE) && defined(OFX_EXTENSIONS_NATRON)

   if (fetchSuite(kFnOfxImageEffectPlaneSuite, 2) &&  // for clipGetImagePlane
   getImageEffectHostDescription()->supportsDynamicChoices && // for dynamic layer choices
   getImageEffectHostDescription()->isMultiPlanar) // for clipGetImagePlane
   ... this is ok...
 *#endif
 */
#include "ofxsMultiPlane.h"

#include <algorithm>


using namespace OFX;

using std::vector;
using std::string;
using std::map;

static bool gHostRequiresStringParamForDynamicChoice = false;
static bool gHostSupportsMultiPlaneV1 = false;
static bool gHostSupportsMultiPlaneV2 = false;
static bool gHostSupportsDynamicChoices = false;

static const char* rgbaComps[4] = {"R", "G", "B", "A"};
static const char* rgbComps[3] = {"R", "G", "B"};
static const char* alphaComps[1] = {"A"};
static const char* motionComps[2] = {"U", "V"};
static const char* disparityComps[2] = {"X", "Y"};
static const char* xyComps[2] = {"X", "Y"};

namespace OFX {
namespace MultiPlane {


ImagePlaneDesc::ImagePlaneDesc()
: _planeID("none")
, _planeLabel("none")
, _channels()
, _channelsLabel("none")
{
}

ImagePlaneDesc::ImagePlaneDesc(const std::string& planeID,
                               const std::string& planeLabel,
                               const std::string& channelsLabel,
                               const std::vector<std::string>& channels)
: _planeID(planeID)
, _planeLabel(planeLabel)
, _channels(channels)
, _channelsLabel(channelsLabel)
{
    if (planeLabel.empty()) {
        // Plane label is the ID if empty
        _planeLabel = _planeID;
    }
    if ( channelsLabel.empty() ) {
        // Channels label is the concatenation of all channels
        for (std::size_t i = 0; i < channels.size(); ++i) {
            _channelsLabel.append(channels[i]);
        }
    }
}

ImagePlaneDesc::ImagePlaneDesc(const std::string& planeName,
                               const std::string& planeLabel,
                               const std::string& channelsLabel,
                               const char** channels,
                               int count)
: _planeID(planeName)
, _planeLabel(planeLabel)
, _channels()
, _channelsLabel(channelsLabel)
{
    _channels.resize(count);
    for (int i = 0; i < count; ++i) {
        _channels[i] = channels[i];
    }

    if (planeLabel.empty()) {
        // Plane label is the ID if empty
        _planeLabel = _planeID;
    }
    if ( channelsLabel.empty() ) {
        // Channels label is the concatenation of all channels
        for (std::size_t i = 0; i < _channels.size(); ++i) {
            _channelsLabel.append(channels[i]);
        }
    }
}

ImagePlaneDesc::ImagePlaneDesc(const ImagePlaneDesc& other)
{
    *this = other;
}

ImagePlaneDesc&
ImagePlaneDesc::operator=(const ImagePlaneDesc& other)
{
    _planeID = other._planeID;
    _planeLabel = other._planeLabel;
    _channels = other._channels;
    _channelsLabel = other._channelsLabel;
    return *this;
}

ImagePlaneDesc::~ImagePlaneDesc()
{
}

bool
ImagePlaneDesc::isColorPlane(const std::string& planeID)
{
    return planeID == kOfxMultiplaneColorPlaneID;
}

bool
ImagePlaneDesc::isColorPlane() const
{
    return ImagePlaneDesc::isColorPlane(_planeID);
}



bool
ImagePlaneDesc::operator==(const ImagePlaneDesc& other) const
{
    if ( _channels.size() != other._channels.size() ) {
        return false;
    }
    return _planeID == other._planeID;
}

bool
ImagePlaneDesc::operator<(const ImagePlaneDesc& other) const
{
    return _planeID < other._planeID;
}

int
ImagePlaneDesc::getNumComponents() const
{
    return (int)_channels.size();
}

const std::string&
ImagePlaneDesc::getPlaneID() const
{
    return _planeID;
}

const std::string&
ImagePlaneDesc::getPlaneLabel() const
{
    return _planeLabel;
}

const std::string&
ImagePlaneDesc::getChannelsLabel() const
{
    return _channelsLabel;
}

const std::vector<std::string>&
ImagePlaneDesc::getChannels() const
{
    return _channels;
}

const ImagePlaneDesc&
ImagePlaneDesc::getNoneComponents()
{
    static const ImagePlaneDesc comp;
    return comp;
}

const ImagePlaneDesc&
ImagePlaneDesc::getRGBAComponents()
{
    static const ImagePlaneDesc comp(kOfxMultiplaneColorPlaneID, kOfxMultiplaneColorPlaneLabel, "", rgbaComps, 4);

    return comp;
}

const ImagePlaneDesc&
ImagePlaneDesc::getRGBComponents()
{
    static const ImagePlaneDesc comp(kOfxMultiplaneColorPlaneID, kOfxMultiplaneColorPlaneLabel, "", rgbComps, 3);

    return comp;
}


const ImagePlaneDesc&
ImagePlaneDesc::getXYComponents()
{
    static const ImagePlaneDesc comp(kOfxMultiplaneColorPlaneID, kOfxMultiplaneColorPlaneLabel, "XY", xyComps, 2);

    return comp;
}

const ImagePlaneDesc&
ImagePlaneDesc::getAlphaComponents()
{
    static const ImagePlaneDesc comp(kOfxMultiplaneColorPlaneID, kOfxMultiplaneColorPlaneLabel, "Alpha", alphaComps, 1);

    return comp;
}

const ImagePlaneDesc&
ImagePlaneDesc::getBackwardMotionComponents()
{
    static const ImagePlaneDesc comp(kOfxMultiplaneBackwardMotionVectorsPlaneID, kOfxMultiplaneBackwardMotionVectorsPlaneLabel, kOfxMultiplaneMotionComponentsLabel, motionComps, 2);

    return comp;
}

const ImagePlaneDesc&
ImagePlaneDesc::getForwardMotionComponents()
{
    static const ImagePlaneDesc comp(kOfxMultiplaneForwardMotionVectorsPlaneID, kOfxMultiplaneForwardMotionVectorsPlaneLabel, kOfxMultiplaneMotionComponentsLabel, motionComps, 2);

    return comp;
}

const ImagePlaneDesc&
ImagePlaneDesc::getDisparityLeftComponents()
{
    static const ImagePlaneDesc comp(kOfxMultiplaneDisparityLeftPlaneID, kOfxMultiplaneDisparityLeftPlaneLabel, kOfxMultiplaneDisparityComponentsLabel, disparityComps, 2);

    return comp;
}

const ImagePlaneDesc&
ImagePlaneDesc::getDisparityRightComponents()
{
    static const ImagePlaneDesc comp(kOfxMultiplaneDisparityRightPlaneID, kOfxMultiplaneDisparityRightPlaneLabel, kOfxMultiplaneDisparityComponentsLabel, disparityComps, 2);

    return comp;
}


void
ImagePlaneDesc::getChannelOption(int channelIndex, std::string* optionID, std::string* optionLabel) const
{
    if (channelIndex < 0 || channelIndex >= (int)_channels.size()) {
        assert(false);
        return;
    }

    *optionLabel += _planeLabel;
    *optionID += _planeID;
    if ( !optionLabel->empty() ) {
        *optionLabel += '.';
    }
    if (!optionID->empty()) {
        *optionID += '.';
    }

    // For the option label, append the name of the channel
    *optionLabel += _channels[channelIndex];
    *optionID += _channels[channelIndex];
}

void
ImagePlaneDesc::getPlaneOption(std::string* optionID, std::string* optionLabel) const
{
    // The option ID is always the name of the layer, this ensures for the Color plane that even if the components type changes, the choice stays
    // the same in the parameter.
    *optionLabel = _planeLabel + "." + _channelsLabel;
    *optionID = _planeID;
}

const ImagePlaneDesc&
ImagePlaneDesc::mapNCompsToColorPlane(int nComps)
{
    switch (nComps) {
        case 1:
            return ImagePlaneDesc::getAlphaComponents();
        case 2:
            return ImagePlaneDesc::getXYComponents();
        case 3:
            return ImagePlaneDesc::getRGBComponents();
        case 4:
            return ImagePlaneDesc::getRGBAComponents();
        default:
            return ImagePlaneDesc::getNoneComponents();
    }
}

static ImagePlaneDesc
ofxCustomCompToNatronComp(const std::string& comp)
{
    std::string planeID, planeLabel, channelsLabel;
    std::vector<std::string> channels;
    if (!extractCustomPlane(comp, &planeID, &planeLabel, &channelsLabel, &channels)) {
        return ImagePlaneDesc::getNoneComponents();
    }

    return ImagePlaneDesc(planeID, planeLabel, channelsLabel, channels);
}

ImagePlaneDesc
ImagePlaneDesc::mapOFXPlaneStringToPlane(const std::string& ofxPlane)
{
    assert(ofxPlane != kFnOfxImagePlaneColour);
    if (ofxPlane == kFnOfxImagePlaneBackwardMotionVector) {
        return ImagePlaneDesc::getBackwardMotionComponents();
    } else if (ofxPlane == kFnOfxImagePlaneForwardMotionVector) {
        return ImagePlaneDesc::getForwardMotionComponents();
    } else if (ofxPlane == kFnOfxImagePlaneStereoDisparityLeft) {
        return ImagePlaneDesc::getDisparityLeftComponents();
    } else if (ofxPlane == kFnOfxImagePlaneStereoDisparityRight) {
        return ImagePlaneDesc::getDisparityRightComponents();
    } else {
        return ofxCustomCompToNatronComp(ofxPlane);
    }
}

void
ImagePlaneDesc::mapOFXComponentsTypeStringToPlanes(const std::string& ofxComponents, ImagePlaneDesc* plane, ImagePlaneDesc* pairedPlane)
{
    if (ofxComponents ==  kOfxImageComponentRGBA) {
        *plane = ImagePlaneDesc::getRGBAComponents();
    } else if (ofxComponents == kOfxImageComponentAlpha) {
        *plane = ImagePlaneDesc::getAlphaComponents();
    } else if (ofxComponents == kOfxImageComponentRGB) {
        *plane = ImagePlaneDesc::getRGBComponents();
    }else if (ofxComponents == kNatronOfxImageComponentXY) {
        *plane = ImagePlaneDesc::getXYComponents();
    } else if (ofxComponents == kOfxImageComponentNone) {
        *plane = ImagePlaneDesc::getNoneComponents();
    } else if (ofxComponents == kFnOfxImageComponentMotionVectors) {
        *plane = ImagePlaneDesc::getBackwardMotionComponents();
        *pairedPlane = ImagePlaneDesc::getForwardMotionComponents();
    } else if (ofxComponents == kFnOfxImageComponentStereoDisparity) {
        *plane = ImagePlaneDesc::getDisparityLeftComponents();
        *pairedPlane = ImagePlaneDesc::getDisparityRightComponents();
    } else {
        *plane = ofxCustomCompToNatronComp(ofxComponents);
    }

} // mapOFXComponentsTypeStringToPlanes


static std::string
natronCustomCompToOfxComp(const ImagePlaneDesc &comp)
{
    std::stringstream ss;
    const std::vector<std::string>& channels = comp.getChannels();
    const std::string& planeID = comp.getPlaneID();
    const std::string& planeLabel = comp.getPlaneLabel();
    const std::string& channelsLabel = comp.getChannelsLabel();
    ss << kNatronOfxImageComponentsPlaneName << planeID;
    if (!planeLabel.empty()) {
        ss << kNatronOfxImageComponentsPlaneLabel << planeLabel;
    }
    if (!channelsLabel.empty()) {
        ss << kNatronOfxImageComponentsPlaneChannelsLabel << channelsLabel;
    }
    for (std::size_t i = 0; i < channels.size(); ++i) {
        ss << kNatronOfxImageComponentsPlaneChannel << channels[i];
    }

    return ss.str();
} // natronCustomCompToOfxComp


std::string
ImagePlaneDesc::mapPlaneToOFXPlaneString(const ImagePlaneDesc& plane)
{
    if (plane.isColorPlane()) {
        return kFnOfxImagePlaneColour;
    } else if ( plane == ImagePlaneDesc::getBackwardMotionComponents() ) {
        return kFnOfxImagePlaneBackwardMotionVector;
    } else if ( plane == ImagePlaneDesc::getForwardMotionComponents()) {
        return kFnOfxImagePlaneForwardMotionVector;
    } else if ( plane == ImagePlaneDesc::getDisparityLeftComponents()) {
        return kFnOfxImagePlaneStereoDisparityLeft;
    } else if ( plane == ImagePlaneDesc::getDisparityRightComponents() ) {
        return kFnOfxImagePlaneStereoDisparityRight;
    } else {
        return natronCustomCompToOfxComp(plane);
    }

}

std::string
ImagePlaneDesc::mapPlaneToOFXComponentsTypeString(const ImagePlaneDesc& plane)
{
    if ( plane == ImagePlaneDesc::getNoneComponents() ) {
        return kOfxImageComponentNone;
    } else if ( plane == ImagePlaneDesc::getAlphaComponents() ) {
        return kOfxImageComponentAlpha;
    } else if ( plane == ImagePlaneDesc::getRGBComponents() ) {
        return kOfxImageComponentRGB;
    } else if ( plane == ImagePlaneDesc::getRGBAComponents() ) {
        return kOfxImageComponentRGBA;
    } else if ( plane == ImagePlaneDesc::getXYComponents() ) {
        return kNatronOfxImageComponentXY;
    } else if ( plane == ImagePlaneDesc::getBackwardMotionComponents() ||
               plane == ImagePlaneDesc::getForwardMotionComponents()) {
        return kFnOfxImageComponentMotionVectors;
    } else if ( plane == ImagePlaneDesc::getDisparityLeftComponents() ||
               plane == ImagePlaneDesc::getDisparityRightComponents()) {
        return kFnOfxImageComponentStereoDisparity;
    } else {
        return natronCustomCompToOfxComp(plane);
    }
}

} // namespace MultiPlane
} // namespace OFX

namespace  {
template <typename T>
void
addInputChannelOptionsRGBAInternal(T* param,
                                   const vector<string>& clips,
                                   bool addConstants,
                                   bool onlyColorPlane,
                                   vector<string>* options,
                                   vector<string>* optionLabels)
{

    const MultiPlane::ImagePlaneDesc& rgbaPlane = MultiPlane::ImagePlaneDesc::getRGBAComponents();
    const MultiPlane::ImagePlaneDesc& disparityLeftPlane = MultiPlane::ImagePlaneDesc::getDisparityLeftComponents();
    const MultiPlane::ImagePlaneDesc& disparityRightPlane = MultiPlane::ImagePlaneDesc::getDisparityRightComponents();
    const MultiPlane::ImagePlaneDesc& motionBwPlane = MultiPlane::ImagePlaneDesc::getBackwardMotionComponents();
    const MultiPlane::ImagePlaneDesc& motionFwPlane = MultiPlane::ImagePlaneDesc::getForwardMotionComponents();

    std::vector<const MultiPlane::ImagePlaneDesc*> planesToAdd;
    planesToAdd.push_back(&rgbaPlane);
    if (!onlyColorPlane) {
        planesToAdd.push_back(&disparityLeftPlane);
        planesToAdd.push_back(&disparityRightPlane);
        planesToAdd.push_back(&motionBwPlane);
        planesToAdd.push_back(&motionFwPlane);
    }

    for (std::size_t c = 0; c < clips.size(); ++c) {
        const string& clipName = clips[c];

        for (std::size_t p = 0; p < planesToAdd.size(); ++p) {
            const std::string& planeLabel = planesToAdd[p]->getPlaneLabel();

            const std::vector<std::string>& planeChannels = planesToAdd[p]->getChannels();

            for (std::size_t i = 0; i < planeChannels.size(); ++i) {
                string opt, hint;
                opt.append(clipName);
                opt.push_back('.');
                if (planesToAdd[i] != &rgbaPlane) {
                    opt.append(planeLabel);
                    opt.push_back('.');
                }
                opt.append(planeChannels[i]);
                hint.append(planeChannels[i]);
                hint.append(" channel from input ");
                hint.append(clipName);
                if (param) {
                    param->appendOption(opt, hint);
                }
                if (options) {
                    options->push_back(opt);
                }
                if (optionLabels) {
                    optionLabels->push_back(hint);
                }

            }
        }

        if ( addConstants && (c == 0) ) {
            {
                string opt, hint;
                opt.append(kMultiPlaneChannelParamOption0);
                hint.append(kMultiPlaneChannelParamOption0Hint);
                if (param) {
                    param->appendOption(opt, hint);
                }
                if (options) {
                    options->push_back(opt);
                }
                if (optionLabels) {
                    optionLabels->push_back(hint);
                }
            }
            {
                string opt, hint;
                opt.append(kMultiPlaneChannelParamOption1);
                hint.append(kMultiPlaneChannelParamOption1Hint);
                if (param) {
                    param->appendOption(opt, hint);
                }
                if (options) {
                    options->push_back(opt);
                }
                if (optionLabels) {
                    optionLabels->push_back(hint);
                }
            }
        }
    }
} // addInputChannelOptionsRGBAInternal

} // anonymous namespace

namespace OFX {
namespace MultiPlane {

namespace Factory {
void
addInputChannelOptionsRGBA(ChoiceParamDescriptor* param,
                           const vector<string>& clips,
                           bool addConstants,
                           bool onlyColorPlane)
{
    addInputChannelOptionsRGBAInternal<ChoiceParamDescriptor>(param, clips, addConstants, onlyColorPlane, 0, 0);
}

void
addInputChannelOptionsRGBA(const vector<string>& clips,
                           bool addConstants,
                           bool onlyColorPlane,
                           vector<string>* options,
                           vector<string>* optionsLabel)
{
    addInputChannelOptionsRGBAInternal<ChoiceParam>(0, clips, addConstants, onlyColorPlane, options, optionsLabel);
}
}         // factory

/**
 * @brief For each choice param, the list of clips it "depends on" (that is the clip available planes that will be visible in the choice)
 **/
struct ChoiceParamClips
{
    // The choice parameter containing the planes or channels.
    ChoiceParam* param;

    // A string parameter used to serialize the channel choice string. Note that this is only needed for Natron 2, other hosts should not need it.
    StringParam* stringparam;

    // True if the menu should contain any entry for each channel of each plane
    bool splitPlanesIntoChannels;

    // True if we should add a "None" option
    bool addNoneOption;

    bool isOutput;

    vector<Clip*> clips;
    vector<string> clipsName;

    ChoiceParamClips()
    : param(0)
    , stringparam(0)
    , splitPlanesIntoChannels(false)
    , addNoneOption(false)
    , isOutput(false)
    , clips()
    , clipsName()
    {
    }
};



struct MultiPlaneEffectPrivate
{
    // Pointer to the public interface
    MultiPlaneEffect* _publicInterface;

    // A map of each dynamic choice parameters containing planes/channels
    map<string, ChoiceParamClips> params;

    // The output clip
    Clip* dstClip;

    // If true, all planes have to be processed
    BooleanParam* allPlanesCheckbox;


    MultiPlaneEffectPrivate(MultiPlaneEffect* publicInterface)
    : _publicInterface(publicInterface)
    , params()
    , dstClip( publicInterface->fetchClip(kOfxImageEffectOutputClipName) )
    , allPlanesCheckbox(0)
    {
    }

    /**
     * @brief This is called inside buildChannelMenus, but needs to be called in the constructor of the plug-in (in createInstanceAction)
     * because getClipPreferences may not be called at that time if not all mandatory inputs are connected.
     **/
    void setChannelsFromStringParams();

    void setChannelsFromStringParamInternal(ChoiceParam* param, StringParam* stringParam, const vector<string>& options);

    MultiPlaneEffect::ChangedParamRetCode checkIfChangedParamCalledOnDynamicChoiceInternal(const string& paramName, const ChoiceParamClips& param, InstanceChangeReason reason);
};

MultiPlaneEffect::MultiPlaneEffect(OfxImageEffectHandle handle)
    : ImageEffect(handle)
    , _imp( new MultiPlaneEffectPrivate(this) )
{
}

MultiPlaneEffect::~MultiPlaneEffect()
{
}

void
MultiPlaneEffect::fetchDynamicMultiplaneChoiceParameter(const string& paramName,
                                                        bool splitPlanesIntoChannelOptions,
                                                        bool canAddNoneOption,
                                                        bool isOutputPlaneChoice,
                                                        const vector<Clip*>& dependsClips)
{
    ChoiceParamClips& paramData = _imp->params[paramName];

    paramData.param = fetchChoiceParam(paramName);
    paramData.stringparam = fetchStringParam(paramName + "Choice");
    assert(paramData.param && paramData.stringparam);
    paramData.splitPlanesIntoChannels = splitPlanesIntoChannelOptions;
    paramData.addNoneOption = canAddNoneOption;
    paramData.clips = dependsClips;

    for (std::size_t i = 0; i < dependsClips.size(); ++i) {
        paramData.clipsName.push_back( dependsClips[i]->name() );
    }

    paramData.isOutput = isOutputPlaneChoice;

    if (isOutputPlaneChoice && !_imp->allPlanesCheckbox && paramExists(kMultiPlaneProcessAllPlanesParam)) {
        _imp->allPlanesCheckbox = fetchBooleanParam(kMultiPlaneProcessAllPlanesParam);
    }

    if (_imp->allPlanesCheckbox) {
        bool allPlanesSelected = _imp->allPlanesCheckbox->getValue();
        paramData.param->setIsSecretAndDisabled(allPlanesSelected);
    }

    _imp->setChannelsFromStringParams();
}

void
MultiPlaneEffectPrivate::setChannelsFromStringParams()
{
    if (gHostRequiresStringParamForDynamicChoice) {
        return;
    }
    for (map<string, ChoiceParamClips>::iterator it = params.begin(); it != params.end(); ++it) {
        vector<string> options;
        it->second.param->getOptions(&options);
        setChannelsFromStringParamInternal(it->second.param, it->second.stringparam, options);
    }
}

void
MultiPlaneEffectPrivate::setChannelsFromStringParamInternal(ChoiceParam* param,
                                                            StringParam* stringParam,
                                                            const vector<string>& options)
{
    string valueStr;

    stringParam->getValue(valueStr);

    if ( valueStr.empty() ) {
        int cur_i;
        param->getValue(cur_i);
        if ( ( cur_i >= 0) && ( cur_i < (int)options.size() ) ) {
            valueStr = options[cur_i];
        }
        param->getOption(cur_i, valueStr);
        stringParam->setValue(valueStr);
    } else {
        int foundOption = -1;
        for (int i = 0; i < (int)options.size(); ++i) {
            if (options[i] == valueStr) {
                foundOption = i;
                break;
            }
        }
        if (foundOption != -1) {
            param->setValue(foundOption);
        }
    }
}

static void getPlanesAvailableForParam(ChoiceParamClips* param, std::vector<ImagePlaneDesc>* planes, map<Clip*, vector<string> >* clipAvailablePlanes)
{
    for (std::size_t c = 0; c < param->clips.size(); ++c) {

        // Did we fetch the clip available planes already ?
        vector<string>* availableClipPlanes = 0;
        map<Clip*, vector<string> >::iterator foundClip = clipAvailablePlanes->find(param->clips[c]);
        if (foundClip != clipAvailablePlanes->end()) {
            availableClipPlanes = &foundClip->second;
        } else {
            availableClipPlanes = &(*clipAvailablePlanes)[param->clips[c]];
            param->clips[c]->getComponentsPresent(availableClipPlanes);
        }

        for (std::size_t i = 0; i < availableClipPlanes->size(); ++i) {

            ImagePlaneDesc plane = ImagePlaneDesc::mapOFXPlaneStringToPlane((*availableClipPlanes)[i]);
            planes->push_back(plane);
        }
    }
} // getPlanesAvailableForParam

void
MultiPlaneEffect::buildChannelMenus(const string& paramName)
{
    if (!gHostSupportsDynamicChoices) {
        return;
    }
    // Store for each clip used the available planes, to avoid inspecting the clip available planes
    // multiple times.
    map<Clip*, vector<string> > perClipAvailablePlanes;

    for (map<string, ChoiceParamClips>::iterator it = _imp->params.begin(); it != _imp->params.end(); ++it) {
        if (!paramName.empty() && paramName != it->first) {
            continue;
        }


        vector<string> optionIDs, optionLabels, optionHints;

        if (it->second.splitPlanesIntoChannels) {
            Factory::addInputChannelOptionsRGBA(it->second.clipsName, true /*addConstants*/, true /*onlyColorPlane*/, &optionIDs, &optionHints);
        } else {
            if (it->second.addNoneOption) {
                optionIDs.push_back(kMultiPlanePlaneParamOptionNone);
                optionLabels.push_back(kMultiPlanePlaneParamOptionNoneLabel);
                optionHints.push_back("");
            }
        }

        std::vector<ImagePlaneDesc> planes;
        getPlanesAvailableForParam(&it->second, &planes, &perClipAvailablePlanes);

        for (std::size_t i = 0; i < planes.size(); ++i) {

            if (it->second.splitPlanesIntoChannels) {
                // User wants per-channel options
                int nChannels = planes[i].getNumComponents();
                for (int k = 0; k < nChannels; ++k) {
                    optionIDs.resize(optionIDs.size() + 1);
                    optionLabels.resize(optionLabels.size() + 1);
                    optionHints.push_back("");
                    planes[i].getChannelOption(k, &optionIDs[optionIDs.size() - 1], &optionLabels[optionLabels.size() - 1]);
                }
            } else {
                    // User wants planes in options
                    optionIDs.resize(optionIDs.size() + 1);
                    optionLabels.resize(optionLabels.size() + 1);
                    optionHints.push_back("");
                    planes[i].getPlaneOption(&optionIDs[optionIDs.size() - 1], &optionLabels[optionLabels.size() - 1]);
            }

        } //for each plane

        // Set the new choice menu
        it->second.param->resetOptions(optionLabels, optionHints, optionIDs);

    } // for all choice parameters
} // buildChannelMenus


MultiPlaneEffect::ChangedParamRetCode
MultiPlaneEffectPrivate::checkIfChangedParamCalledOnDynamicChoiceInternal(const string& paramName,
                                                                          const ChoiceParamClips& param,
                                                                          InstanceChangeReason reason)
{

    if ( param.param && ( paramName == param.param->getName() ) && (reason == eChangeUserEdit) ) {
        // The choice parameter changed
        int choice_i;
        param.param->getValue(choice_i);
        string optionName;
        param.param->getOption(choice_i, optionName);
        param.stringparam->setValue(optionName);

        return MultiPlaneEffect::eChangedParamRetCodeChoiceParamChanged;
    } else if ( param.stringparam && paramName == param.stringparam->getName() ) {
        // The string parameter changed
        vector<string> options;
        param.param->getOptions(&options);
        setChannelsFromStringParamInternal(param.param, param.stringparam, options);

        return MultiPlaneEffect::eChangedParamRetCodeStringParamChanged;
    } else if ( allPlanesCheckbox && paramName == allPlanesCheckbox->getName() ) {
        bool allPlanesSelected = allPlanesCheckbox->getValue();
        for (map<string, ChoiceParamClips>::const_iterator it = params.begin(); it != params.end(); ++it) {
            it->second.param->setIsSecretAndDisabled(allPlanesSelected);
        }
        return MultiPlaneEffect::eChangedParamRetCodeAllPlanesParamChanged;
    }
    return MultiPlaneEffect::eChangedParamRetCodeNoChange;
}

MultiPlaneEffect::ChangedParamRetCode
MultiPlaneEffect::checkIfChangedParamCalledOnDynamicChoice(const string& paramName,
                                                           const string& paramToCheck,
                                                           InstanceChangeReason reason)
{
    map<string, ChoiceParamClips>::iterator found = _imp->params.find(paramToCheck);

    if ( found == _imp->params.end() ) {
        return eChangedParamRetCodeNoChange;
    }

    return _imp->checkIfChangedParamCalledOnDynamicChoiceInternal(paramName, found->second, reason);
}

bool
MultiPlaneEffect::handleChangedParamForAllDynamicChoices(const string& paramName,
                                                         InstanceChangeReason reason)
{
    for (map<string, ChoiceParamClips>::iterator it = _imp->params.begin(); it != _imp->params.end(); ++it) {
        if ( _imp->checkIfChangedParamCalledOnDynamicChoiceInternal(paramName, it->second, reason) ) {
            return true;
        }
    }

    return false;
}

MultiPlaneEffect::GetPlaneNeededRetCodeEnum
MultiPlaneEffect::getPlaneNeeded(const std::string& paramName,
                                 OFX::Clip** clip,
                                 ImagePlaneDesc* plane,
                                 int* channelIndexInPlane)
{


    map<string, ChoiceParamClips>::iterator found = _imp->params.find(paramName);
    assert( found != _imp->params.end() );
    if ( found == _imp->params.end() ) {
        return eGetPlaneNeededRetCodeFailed;
    }


    if (found->second.isOutput && _imp->allPlanesCheckbox) {
        bool processAll = _imp->allPlanesCheckbox->getValue();
        if (processAll) {
            return eGetPlaneNeededRetCodeReturnedAllPlanes;
        }
    }


    *clip = 0;

    int choice_i;
    found->second.param->getValue(choice_i);
    
    string selectedOptionID;
    if ( (0 <= choice_i) && ( choice_i < found->second.param->getNOptions() ) ) {
        found->second.param->getOptionName(choice_i, selectedOptionID);
    } else {
        return eGetPlaneNeededRetCodeFailed;
    }

    if ( selectedOptionID.empty() ) {
        return eGetPlaneNeededRetCodeFailed;
    }

    if (selectedOptionID == kMultiPlaneChannelParamOption0) {
        return eGetPlaneNeededRetCodeReturnedConstant0;
    }

    if (selectedOptionID == kMultiPlaneChannelParamOption1) {
        return eGetPlaneNeededRetCodeReturnedConstant1;
    }

    if (selectedOptionID == kMultiPlanePlaneParamOptionNone) {
        *plane = ImagePlaneDesc::getNoneComponents();
        return eGetPlaneNeededRetCodeReturnedPlane;
    }

    // Find in the clip available components a matching optionID
    map<Clip*, vector<string> > perClipAvailablePlanes;

    std::vector<ImagePlaneDesc> planes;
    getPlanesAvailableForParam(&found->second, &planes, &perClipAvailablePlanes);

    for (std::size_t i = 0; i < planes.size(); ++i) {

        if (found->second.splitPlanesIntoChannels) {
            // User wants per-channel options
            int nChannels = planes[i].getNumComponents();
            for (int k = 0; k < nChannels; ++k) {
                std::string optionID, optionLabel;
                planes[i].getChannelOption(k, &optionID, &optionLabel);
                if (selectedOptionID == optionID) {
                    *plane = planes[i];
                    *channelIndexInPlane = k;
                    return eGetPlaneNeededRetCodeReturnedChannelInPlane;
                }
            }
        } else {
            // User wants planes in options
            std::string optionID, optionLabel;
            planes[i].getPlaneOption(&optionID, &optionLabel);
            if (selectedOptionID == optionID) {
                *plane = planes[i];
                return eGetPlaneNeededRetCodeReturnedPlane;
            }
        }

    } //for each plane
    return eGetPlaneNeededRetCodeFailed;
} // MultiPlaneEffect::getPlaneNeededForParam


static void refreshHostFlags()
{
#ifdef OFX_EXTENSIONS_NATRON
    if (getImageEffectHostDescription()->supportsDynamicChoices) {
        gHostSupportsDynamicChoices = true;
    }
    if (getImageEffectHostDescription()->isNatron && getImageEffectHostDescription()->versionMajor < 3) {
        gHostRequiresStringParamForDynamicChoice = true;
    }
#endif
#ifdef OFX_EXTENSIONS_NUKE
    if (fetchSuite(kFnOfxImageEffectPlaneSuite, 1)) {
        gHostSupportsMultiPlaneV1 = true;
    }
    if (fetchSuite(kFnOfxImageEffectPlaneSuite, 2)) {
        gHostSupportsMultiPlaneV2 = true;
    }
#endif
}

namespace Factory {
ChoiceParamDescriptor*
describeInContextAddPlaneChoice(ImageEffectDescriptor &desc,
                                PageParamDescriptor* page,
                                const std::string& name,
                                const std::string& label,
                                const std::string& hint)
{

    refreshHostFlags();
    if (!gHostSupportsMultiPlaneV2 && !gHostSupportsMultiPlaneV1) {
        throw std::runtime_error("Hosts does not meet requirements");
    }
    ChoiceParamDescriptor *ret;
    {
        ChoiceParamDescriptor *param = desc.defineChoiceParam(name);
        param->setLabel(label);
        param->setHint(hint);
#ifdef OFX_EXTENSIONS_NATRON
        param->setHostCanAddOptions(true);             //< the host can allow the user to add custom entries
#endif
        if (!gHostSupportsMultiPlaneV2) {
            // Add hard-coded planes
            const MultiPlane::ImagePlaneDesc& rgbaPlane = MultiPlane::ImagePlaneDesc::getRGBAComponents();
            const MultiPlane::ImagePlaneDesc& disparityLeftPlane = MultiPlane::ImagePlaneDesc::getDisparityLeftComponents();
            const MultiPlane::ImagePlaneDesc& disparityRightPlane = MultiPlane::ImagePlaneDesc::getDisparityRightComponents();
            const MultiPlane::ImagePlaneDesc& motionBwPlane = MultiPlane::ImagePlaneDesc::getBackwardMotionComponents();
            const MultiPlane::ImagePlaneDesc& motionFwPlane = MultiPlane::ImagePlaneDesc::getForwardMotionComponents();

            std::vector<const MultiPlane::ImagePlaneDesc*> planesToAdd;
            planesToAdd.push_back(&rgbaPlane);
            planesToAdd.push_back(&disparityLeftPlane);
            planesToAdd.push_back(&disparityRightPlane);
            planesToAdd.push_back(&motionBwPlane);
            planesToAdd.push_back(&motionFwPlane);

            for (std::size_t i = 0; i < planesToAdd.size(); ++i) {
                std::string optionID, optionLabel;
                planesToAdd[i]->getPlaneOption(&optionID, &optionLabel);
                param->appendOption(optionLabel, "", optionID);
            }

        }
        if (gHostRequiresStringParamForDynamicChoice) {
            param->setEvaluateOnChange(false);
            param->setIsPersistent(true);
        }
        param->setDefault(0);
        param->setAnimates(false);
        desc.addClipPreferencesSlaveParam(*param);             // < the menu is built in getClipPreferences
        if (page) {
            page->addChild(*param);
        }
        ret = param;
    }
    if (gHostRequiresStringParamForDynamicChoice) {
        //Add a hidden string param that will remember the value of the choice
        StringParamDescriptor* param = desc.defineStringParam(name + "Choice");
        param->setLabel(label + "Choice");
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }

    return ret;
}

OFX::BooleanParamDescriptor*
describeInContextAddAllPlanesOutputCheckbox(OFX::ImageEffectDescriptor &desc, OFX::PageParamDescriptor* page)
{
    refreshHostFlags();
    if (!gHostSupportsMultiPlaneV2 && !gHostSupportsMultiPlaneV1) {
        throw std::runtime_error("Hosts does not meet requirements");
    }
    BooleanParamDescriptor* param = desc.defineBooleanParam(kMultiPlaneProcessAllPlanesParam);
    param->setLabel(kMultiPlaneProcessAllPlanesParamLabel);
    param->setHint(kMultiPlaneProcessAllPlanesParamHint);
    param->setAnimates(false);
    if (page) {
        page->addChild(*param);
    }
    return param;
}

ChoiceParamDescriptor*
describeInContextAddPlaneChannelChoice(ImageEffectDescriptor &desc,
                                       PageParamDescriptor* page,
                                       const vector<string>& clips,
                                       const string& name,
                                       const string& label,
                                       const string& hint)
    
{

    refreshHostFlags();
    if (!gHostSupportsMultiPlaneV2 && !gHostSupportsMultiPlaneV1) {
        throw std::runtime_error("Hosts does not meet requirements");
    }
    
    ChoiceParamDescriptor *ret;
    {
        ChoiceParamDescriptor *param = desc.defineChoiceParam(name);
        param->setLabel(label);
        param->setHint(hint);
        param->setAnimates(false);
        addInputChannelOptionsRGBA(param, clips, true /*addContants*/, gHostSupportsMultiPlaneV2 /*onlyColorPlane*/);
        if (gHostRequiresStringParamForDynamicChoice) {
            param->setEvaluateOnChange(false);
            param->setIsPersistent(false);
        }
        if (page) {
            page->addChild(*param);
        }
        ret = param;
    }
    if (gHostRequiresStringParamForDynamicChoice && gHostSupportsDynamicChoices && gHostSupportsMultiPlaneV2) {
        string strName = name + "Choice";
        //Add a hidden string param that will remember the value of the choice
        StringParamDescriptor* param = desc.defineStringParam(strName);
        param->setLabel(label + "Choice");
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }

    return ret;
}
}         // Factory
}     // namespace MultiPlane
} // namespace OFX
