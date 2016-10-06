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
 * Helper functions to implement plug-ins that support kFnOfxImageEffectPlaneSuite v2
 * In order to use these functions the following condition must be met:
 *#if defined(OFX_EXTENSIONS_NUKE) && defined(OFX_EXTENSIONS_NATRON)

   if (OFX::fetchSuite(kFnOfxImageEffectPlaneSuite, 2) &&  // for clipGetImagePlane
   OFX::getImageEffectHostDescription()->supportsDynamicChoices && // for dynamic layer choices
   OFX::getImageEffectHostDescription()->isMultiPlanar) // for clipGetImagePlane
   ... this is ok...
 *#endif
 */
#include "ofxsMultiPlane.h"

#include <algorithm>

namespace OFX {
namespace MultiPlane {
namespace Utils {
void
extractChannelsFromComponentString(const std::string& comp,
                                   std::string* layer,
                                   std::string* pairedLayer,                  //< if disparity or motion vectors
                                   std::vector<std::string>* channels)
{
    if (comp == kOfxImageComponentAlpha) {
        //*layer = kShuffleColorPlaneName;
        channels->push_back("A");
    } else if (comp == kOfxImageComponentRGB) {
        //*layer = kShuffleColorPlaneName;
        channels->push_back("R");
        channels->push_back("G");
        channels->push_back("B");
    } else if (comp == kOfxImageComponentRGBA) {
        //*layer = kShuffleColorPlaneName;
        channels->push_back("R");
        channels->push_back("G");
        channels->push_back("B");
        channels->push_back("A");
    } else if (comp == kFnOfxImageComponentMotionVectors) {
        *layer = kPlaneLabelMotionBackwardPlaneName;
        *pairedLayer = kPlaneLabelMotionForwardPlaneName;
        channels->push_back("U");
        channels->push_back("V");
    } else if (comp == kFnOfxImageComponentStereoDisparity) {
        *layer = kPlaneLabelDisparityLeftPlaneName;
        *pairedLayer = kPlaneLabelDisparityRightPlaneName;
        channels->push_back("X");
        channels->push_back("Y");
#ifdef OFX_EXTENSIONS_NATRON
    } else if (comp == kNatronOfxImageComponentXY) {
        channels->push_back("X");
        channels->push_back("Y");
    } else {
        std::vector<std::string> layerChannels = OFX::mapPixelComponentCustomToLayerChannels(comp);
        if (layerChannels.size() >= 1) {
            *layer = layerChannels[0];
            channels->assign( layerChannels.begin() + 1, layerChannels.end() );
        }
#endif
    }
}
}         // Utils
}
}

namespace  {
template <typename T>
void
addInputChannelOptionsRGBAInternal(T* param,
                                   const std::vector<std::string>& clips,
                                   bool addConstants,
                                   std::vector<std::string>* options,
                                   std::vector<std::string>* optionLabels)
{
    static const char* optionsBits[4][2] = {
        {"r", "Red"}, {"g", "Green"}, {"b", "Blue"}, {"a", "Alpha"}
    };

    for (std::size_t c = 0; c < clips.size(); ++c) {
        const std::string& clipName = clips[c];

        for (int i = 0; i < 4; ++i) {
            std::string opt, hint;
            opt.append(clipName);
            opt.push_back('.');
            opt.append(optionsBits[i][0]);
            hint.append(optionsBits[i][1]);
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

        if ( addConstants && (c == 0) ) {
            {
                std::string opt, hint;
                opt.append(kMultiPlaneParamOutputOption0);
                hint.append(kMultiPlaneParamOutputOption0Hint);
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
                std::string opt, hint;
                opt.append(kMultiPlaneParamOutputOption1);
                hint.append(kMultiPlaneParamOutputOption1Hint);
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

static bool
hasListChanged(const std::vector<std::string>& oldList,
               const std::vector<std::string>& newList)
{
    if ( oldList.size() != newList.size() ) {
        return true;
    }

    std::vector<std::string> oldListSorted = oldList;
    std::sort( oldListSorted.begin(), oldListSorted.end() );
    std::vector<std::string> newListSorted = newList;
    std::sort( newListSorted.begin(), newListSorted.end() );
    std::vector<std::string>::const_iterator itNew = newListSorted.begin();
    for (std::vector<std::string>::const_iterator it = oldListSorted.begin(); it != oldListSorted.end(); ++it, ++itNew) {
        if (*it != *itNew) {
            return true;
        }
    }

    return false;
}

static void
appendComponents(const std::string& clipName,
                 const std::vector<std::string>& components,
                 const bool isOutputChannelsParam,
                 std::vector<std::string>* channelChoices,
                 std::vector<std::string>* channelChoicesLabel)
{
    if (isOutputChannelsParam) {
        //Pre-process to add color comps first
        std::list<std::string> compsToAdd;
        bool foundColor = false;
        bool hasAll = false;
        for (std::vector<std::string>::const_iterator it = components.begin(); it != components.end(); ++it) {
            if ( !hasAll && (*it == kPlaneLabelAll) ) {
                hasAll = true;
                continue;
            }
            std::string layer, secondLayer;
            std::vector<std::string> channels;
            OFX::MultiPlane::Utils::extractChannelsFromComponentString(*it, &layer, &secondLayer, &channels);
            if ( channels.empty() ) {
                continue;
            }
            if ( layer.empty() ) {
                if (*it == kOfxImageComponentRGBA) {
                    channelChoices->push_back(kPlaneLabelColorRGBA);
                    foundColor = true;
                } else if (*it == kOfxImageComponentRGB) {
                    channelChoices->push_back(kPlaneLabelColorRGB);
                    foundColor = true;
                } else if (*it == kOfxImageComponentAlpha) {
                    channelChoices->push_back(kPlaneLabelColorAlpha);
                    foundColor = true;
                }

                continue;
            } else {
                /* if (layer == kPlaneLabelMotionForwardPlaneName ||
                     layer == kPlaneLabelMotionBackwardPlaneName ||
                     layer == kPlaneLabelDisparityLeftPlaneName ||
                     layer == kPlaneLabelDisparityRightPlaneName) {
                     continue;
                   }*/
            }

            //Append the channel names to the layer
            //Edit: Uncommented to match what is done in other softwares
            //Note that uncommenting will break compatibility with projects using multi-plane features
            /*for (std::size_t i = 0; i < channels.size(); ++i) {
                std::string opt;
                if (!layer.empty()) {
                    opt.append(layer);
                    opt.push_back('.');
                }
                opt.append(channels[i]);
               }*/

            compsToAdd.push_back(layer);
        }
        if (hasAll) {
            channelChoices->push_back(kPlaneLabelAll);
        }
        if (!foundColor) {
            channelChoices->push_back(kPlaneLabelColorRGBA);
        }
        /*channelChoices->push_back(kPlaneLabelMotionForwardPlaneName);
           channelChoices->push_back(kPlaneLabelMotionBackwardPlaneName);
           channelChoices->push_back(kPlaneLabelDisparityLeftPlaneName);
           channelChoices->push_back(kPlaneLabelDisparityRightPlaneName);*/
        channelChoices->insert( channelChoices->end(), compsToAdd.begin(), compsToAdd.end() );
    } else { // !isOutputChannelsParam
        std::vector<std::string> usedComps;
        for (std::vector<std::string>::const_iterator it = components.begin(); it != components.end(); ++it) {
            std::string layer, secondLayer;
            std::vector<std::string> channels;
            OFX::MultiPlane::Utils::extractChannelsFromComponentString(*it, &layer, &secondLayer, &channels);
            if ( channels.empty() ) {
                continue;
            }
            if ( layer.empty() ) {
                continue;
            }
            for (std::size_t i = 0; i < channels.size(); ++i) {
                std::string opt;
                if ( !clipName.empty() ) {
                    opt += clipName;
                    opt += '.';
                }

                if ( !layer.empty() ) {
                    opt.append(layer);
                    opt.push_back('.');
                }
                opt.append(channels[i]);

                if ( std::find(usedComps.begin(), usedComps.end(), opt) == usedComps.end() ) {
                    usedComps.push_back(opt);
                    channelChoices->push_back(opt);
                    channelChoicesLabel->push_back(channels[i] + " channel from " + ( ( layer.empty() ) ? std::string() : std::string("layer/view ") + layer + " of " ) + "input " + clipName);
                }
            }

            if ( !secondLayer.empty() ) {
                for (std::size_t i = 0; i < channels.size(); ++i) {
                    std::string opt;
                    if ( !clipName.empty() ) {
                        opt += clipName;
                        opt += '.';
                    }
                    if ( !secondLayer.empty() ) {
                        opt.append(secondLayer);
                        opt.push_back('.');
                    }
                    opt.append(channels[i]);
                    if ( std::find(usedComps.begin(), usedComps.end(), opt) == usedComps.end() ) {
                        usedComps.push_back(opt);
                        channelChoices->push_back(opt);
                        channelChoicesLabel->push_back(channels[i] + " channel from layer " + secondLayer + " of input " + clipName);
                    }
                }
            }
        }
    } // isOutputChannelsParam
} // appendComponents

static void
parseLayerString(const std::string& encoded,
                 bool* isColor)
{
    if ( (encoded == kPlaneLabelColorRGBA) ||
         ( encoded == kPlaneLabelColorRGB) ||
         ( encoded == kPlaneLabelColorAlpha) ) {
        *isColor = true;
    } else {
        *isColor = false;
    }
}

static bool
parseChannelString(const std::string& encodedChannel,
                   std::string* clipName,
                   std::string* layerName,
                   std::string* channelName,
                   bool *isColor)
{
    std::size_t foundLastDot = encodedChannel.find_last_of('.');

    if (foundLastDot == std::string::npos) {
        *isColor = false;
        if (encodedChannel == kMultiPlaneParamOutputOption0) {
            *layerName = kMultiPlaneParamOutputOption0;

            return true;
        } else if (encodedChannel == kMultiPlaneParamOutputOption1) {
            *layerName = kMultiPlaneParamOutputOption1;

            return true;
        }

        return false;
    }
    *channelName = encodedChannel.substr(foundLastDot + 1);

    std::string baseName = encodedChannel.substr(0, foundLastDot);
    std::size_t foundPrevDot = baseName.find_first_of('.');
    if (foundPrevDot != std::string::npos) {
        //Remove the node name
        *layerName = baseName.substr(foundPrevDot + 1);
        *clipName = baseName.substr(0, foundPrevDot);
    } else {
        *layerName = baseName;
        clipName->clear();
    }
    *isColor = *layerName == kPlaneLabelColorRGBA || *layerName == kPlaneLabelColorRGB || *layerName == kPlaneLabelColorAlpha;

    return true;
}

class ChoiceMergeEntriesData
{
public:

    ChoiceMergeEntriesData()
    {
    }

    virtual void clear() = 0;

    virtual ~ChoiceMergeEntriesData()
    {
    }
};


class MergeChannelData
    : public ChoiceMergeEntriesData
{
public:

    std::string bNode, bLayer, bChannel;
    bool isColor;
    bool dataSet;

    MergeChannelData()
        : ChoiceMergeEntriesData()
        , isColor(false)
        , dataSet(false)
    {
    }

    virtual void clear()
    {
        dataSet = false;
        bNode.clear();
        bLayer.clear();
        bChannel.clear();
    }

    virtual ~MergeChannelData()
    {
    }
};

static bool
channelEqualityFunctorInternal(const std::string& aLayer,
                               const std::string& aChannel,
                               const std::string& bLayer,
                               const std::string& bChannel,
                               bool aIsColor,
                               bool bIsColor)
{
    if ( aChannel.empty() && bChannel.empty() ) {
        // kMultiPlaneParamOutputOption0 and kMultiPlaneParamOutputOption1 choice
        return aLayer == bLayer;
    } else if (aChannel != bChannel) {
        return false;
    } else {
        // Same channel, check layer
        if (aLayer == bLayer) {
            return true;
        } else if (aIsColor && bIsColor) {
            return true;
        }
    }

    return false;
}

static bool
channelEqualityFunctor(const std::string& a,
                       const std::string& b,
                       ChoiceMergeEntriesData* data)
{
    MergeChannelData* mergeData = dynamic_cast<MergeChannelData*>(data);

    assert(mergeData);
    std::string aNode, aLayer, aChannel;
    bool aIsColor;
    parseChannelString(a, &aNode, &aLayer, &aChannel, &aIsColor);
    if (!mergeData->dataSet) {
        parseChannelString(b, &mergeData->bNode, &mergeData->bLayer, &mergeData->bChannel, &mergeData->isColor);
        mergeData->dataSet = true;
    }

    return channelEqualityFunctorInternal(aLayer, aChannel, mergeData->bLayer, mergeData->bChannel, aIsColor, mergeData->isColor);
}

class MergeOutputLayerEntriesData
    : public ChoiceMergeEntriesData
{
public:

    bool isColor;
    bool dataSet;

    MergeOutputLayerEntriesData()
        : ChoiceMergeEntriesData()
        , isColor(false)
        , dataSet(false)
    {
    }

    virtual void clear()
    {
        dataSet = false;
    }

    virtual ~MergeOutputLayerEntriesData()
    {
    }
};

static bool
layerEqualityFunctor(const std::string& a,
                     const std::string& b,
                     ChoiceMergeEntriesData* data)
{
    MergeOutputLayerEntriesData* mergeData = dynamic_cast<MergeOutputLayerEntriesData*>(data);

    assert(mergeData);
    bool aIsColor;
    parseLayerString(a, &aIsColor);
    if (!mergeData->dataSet) {
        parseLayerString(b, &mergeData->isColor);
        mergeData->dataSet = true;
    }
    if (aIsColor && mergeData->isColor) {
        return true;
    } else if (a == b) {
        return true;
    }

    return false;
}

typedef bool (*MergeMenuEqualityFunctor)(const std::string& a, const std::string& b, ChoiceMergeEntriesData* userData);


void
mergeChannelEntries(const std::vector<std::string>& newEntries,
                    const std::vector<std::string>& newEntriesLabel,
                    std::vector<std::string>* mergedEntries,
                    std::vector<std::string>* mergedEntriesLabel,
                    MergeMenuEqualityFunctor mergingFunctor,
                    ChoiceMergeEntriesData* mergingData)
{
    for (std::size_t i = 0; i < newEntries.size(); ++i) {
        mergingData->clear();
        bool found = false;
        for (std::size_t j = 0; j < mergedEntries->size(); ++j) {
            if ( mergingFunctor( (*mergedEntries)[j], newEntries[i], mergingData ) ) {
                if ( (*mergedEntries)[j] != newEntries[i] ) {
                    (*mergedEntries)[j] = newEntries[i];
                }
                found = true;
                break;
            }
        }
        if (!found) {
            if ( i < newEntriesLabel.size() ) {
                mergedEntriesLabel->push_back(newEntriesLabel[i]);
            }
            mergedEntries->push_back(newEntries[i]);
        }
    }
}
} // anonymous namespace

namespace OFX {
namespace MultiPlane {
namespace Utils {
std::string
makeNatronCustomChannel(const std::string& layer,
                        const std::vector<std::string>& channels)
{
    std::string ret(kNatronOfxImageComponentsPlane);

    ret.append(layer);
    for (std::size_t i = 0; i < channels.size(); ++i) {
        ret.append(kNatronOfxImageComponentsPlaneChannel);
        ret.append(channels[i]);
    }

    return ret;
}
}         // Utils

namespace Factory {
void
addInputChannelOptionsRGBA(OFX::ChoiceParamDescriptor* param,
                           const std::vector<std::string>& clips,
                           bool addConstants)
{
    addInputChannelOptionsRGBAInternal<OFX::ChoiceParamDescriptor>(param, clips, addConstants, 0, 0);
}

void
addInputChannelOptionsRGBA(const std::vector<std::string>& clips,
                           bool addConstants,
                           std::vector<std::string>* options,
                           std::vector<std::string>* optionsLabel)
{
    addInputChannelOptionsRGBAInternal<OFX::ChoiceParam>(0, clips, addConstants, options, optionsLabel);
}
}         // factory

/**
 * @brief For each choice param, the list of clips it "depends on" (that is the clip layers that will be visible in the choice)
 * If the clips vector contains a single clip and this is the output clip then it is expected that param points to the kMultiPlaneParamOutputChannels
 * parameter.
 **/
struct ChoiceParamClips
{
    OFX::ChoiceParam* param;
    OFX::StringParam* stringparam;
    OFX::PushButtonParam* buttonparam;
    bool isOutput;
    std::vector<OFX::Clip*> clips;
    std::vector<std::string> clipsName;

    ChoiceParamClips()
        : param(0)
        , stringparam(0)
        , buttonparam(0)
        , isOutput(false)
        , clips()
        , clipsName()
    {
    }
};


struct ClipsComponentsInfoBase
{
    //A pointer to the clip
    OFX::Clip* clip;

    //The value returned by clip->getComponentsPresent()
    std::vector<std::string> componentsPresent;

    ClipsComponentsInfoBase() : clip(0), componentsPresent() {}

    virtual ~ClipsComponentsInfoBase() {}
};

struct ClipComponentsInfo
    : public ClipsComponentsInfoBase
{
    //A pointer to a components present cache held as a member of the plug-in (no need to lock it as accessed always on the same thread)
    //This is to speed-up buildChannelMenus to avoid re-building menus and make complex API calls if they did not change.
    std::vector<std::string>* componentsPresentCache;

    //When hasListChanged has been called; this is set to true, indicating that the value of isCacheValid is correct
    mutable bool comparisonToCacheDone;
    mutable bool isCacheUpToDate;

    ClipComponentsInfo() : ClipsComponentsInfoBase(), componentsPresentCache(0), comparisonToCacheDone(false), isCacheUpToDate(false) {}

    virtual ~ClipComponentsInfo() {}
};

class BuildChannelMenusData
{
    bool mergeMenus;
    std::map<OFX::Clip*, ClipComponentsInfo> cacheInfos;
    struct ChoiceParamData
    {
        bool hasChanged;
        std::vector<const ClipComponentsInfo*> clipsInfos;
        std::vector<std::string> options;

        ChoiceParamData()
            : hasChanged(false)
            , clipsInfos()
        {
        }
    };

    std::map<const ChoiceParamClips*, ChoiceParamData> params;

public:

    /** @param mergeMenus If true, the existing entries in the choice menu will be merged with the new components present on the clips.*/
    BuildChannelMenusData(bool mergeMenus = true)
        : mergeMenus(mergeMenus)
        , cacheInfos()
        , params()
    {
    }

    void addParamToRebuild(const ChoiceParamClips* paramData,
                           bool addChoiceAllToOutput,
                           std::map<OFX::Clip*, std::vector<std::string> >& componentsCache);

    void buildChannelsMenus();

    const std::vector<std::string>& getParamOptions(const ChoiceParamClips*) const;
};


struct MultiPlaneEffectPrivate
{
    MultiPlaneEffect* _publicInterface;
    std::map<OFX::Clip*, std::vector<std::string> > clipComponentsCache;
    std::map<std::string, ChoiceParamClips> params;

    // Used in the checkIfChangedParamCalledOnDynamicChoiceInternal function when refreshing the choice menu.
    // We need to know if it had the all choice in the last call made to buildChannelMenus()
    bool lastBuildChannelMenusHadAllChoice;

    MultiPlaneEffectPrivate(MultiPlaneEffect* publicInterface)
        : _publicInterface(publicInterface)
        , lastBuildChannelMenusHadAllChoice(false)
    {
    }

    /**
     * @brief This is called inside buildChannelMenus, but needs to be called in the constructor of the plug-in (in createInstanceAction)
     * because getClipPreferences may not be called at that time if not all mandatory inputs are connected.
     **/
    void setChannelsFromStringParams(bool allowReset);

    void setChannelsFromStringParamInternal(OFX::ChoiceParam* param, OFX::StringParam* stringParam, const std::vector<std::string>& options, bool allowReset);

    MultiPlaneEffect::ChangedParamRetCode checkIfChangedParamCalledOnDynamicChoiceInternal(const std::string& paramName, const ChoiceParamClips& param, OFX::InstanceChangeReason reason);
};

MultiPlaneEffect::MultiPlaneEffect(OfxImageEffectHandle handle)
    : OFX::ImageEffect(handle)
    , _imp( new MultiPlaneEffectPrivate(this) )
{
}

MultiPlaneEffect::~MultiPlaneEffect()
{
}

void
MultiPlaneEffect::fetchDynamicMultiplaneChoiceParameter(const std::string& paramName,
                                                        const std::vector<OFX::Clip*>& dependsClips)
{
    ChoiceParamClips& paramData = _imp->params[paramName];

    paramData.param = fetchChoiceParam(paramName);
    paramData.stringparam = fetchStringParam(paramName + "Choice");
    paramData.buttonparam = fetchPushButtonParam(paramName + "RefreshButton");
    assert(paramData.param && paramData.stringparam && paramData.buttonparam);
    paramData.isOutput = paramName == kMultiPlaneParamOutputChannels;
    assert( !paramData.isOutput || (dependsClips.size() == 1 && dependsClips[0]) );
    paramData.clips = dependsClips;
    for (std::size_t i = 0; i < dependsClips.size(); ++i) {
        paramData.clipsName.push_back( dependsClips[i]->name() );
        _imp->clipComponentsCache[dependsClips[i]].clear();
    }

    _imp->setChannelsFromStringParams(false);
}

void
MultiPlaneEffectPrivate::setChannelsFromStringParams(bool allowReset)
{
    for (std::map<std::string, ChoiceParamClips>::iterator it = params.begin(); it != params.end(); ++it) {
        std::vector<std::string> options;
        it->second.param->getOptions(&options);
        setChannelsFromStringParamInternal(it->second.param, it->second.stringparam, options, allowReset);
    }
}

void
MultiPlaneEffectPrivate::setChannelsFromStringParamInternal(OFX::ChoiceParam* param,
                                                            OFX::StringParam* stringParam,
                                                            const std::vector<std::string>& options,
                                                            bool /*allowReset*/)
{
    std::string valueStr;

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
        } else {
            /*if (allowReset) {
               int defValue;
               it->param->getDefault(defValue);
               if (defValue >= 0 && defValue < (int)it->options.size()) {
               it->param->setValue(defValue);
               it->stringParam->setValue(it->options[defValue]);
               }
               }*/
        }
    }
}

void
MultiPlaneEffect::buildChannelMenus(const std::string& paramName,
                                    bool mergeEntries,
                                    bool addChoiceAllToOutput)
{
    _imp->lastBuildChannelMenusHadAllChoice = addChoiceAllToOutput;

    BuildChannelMenusData data(mergeEntries);
    if ( paramName.empty() ) {
        // build all
        for (std::map<std::string, ChoiceParamClips>::iterator it = _imp->params.begin(); it != _imp->params.end(); ++it) {
            data.addParamToRebuild(&it->second, addChoiceAllToOutput, _imp->clipComponentsCache);
        }
        data.buildChannelsMenus();

        // Reset the choice current value from the string parameters
        for (std::map<std::string, ChoiceParamClips>::iterator it = _imp->params.begin(); it != _imp->params.end(); ++it) {
            try {
                _imp->setChannelsFromStringParamInternal(it->second.param, it->second.stringparam, data.getParamOptions(&it->second), true);
            } catch (...) {
                throwSuiteStatusException(kOfxStatFailed);
            }
        }
    } else {
        std::map<std::string, ChoiceParamClips>::iterator found = _imp->params.find(paramName);
        if ( found != _imp->params.end() ) {
            data.addParamToRebuild(&found->second, addChoiceAllToOutput, _imp->clipComponentsCache);
        }
        data.buildChannelsMenus();
        if ( found != _imp->params.end() ) {
            try {
                _imp->setChannelsFromStringParamInternal(found->second.param, found->second.stringparam, data.getParamOptions(&found->second), true);
            } catch (...) {
                throwSuiteStatusException(kOfxStatFailed);
            }
        }
    }
}

const std::vector<std::string>&
MultiPlaneEffect::getCachedComponentsPresent(OFX::Clip* clip) const
{
    std::map<OFX::Clip*, std::vector<std::string> >::const_iterator foundCompsCache = _imp->clipComponentsCache.find(clip);

    if ( foundCompsCache != _imp->clipComponentsCache.end() ) {
        return foundCompsCache->second;
    } else {
        assert(false);
        throwSuiteStatusException(kOfxStatFailed);
    }
}

const std::vector<std::string>&
BuildChannelMenusData::getParamOptions(const ChoiceParamClips* param) const
{
    std::map<const ChoiceParamClips*, ChoiceParamData>::const_iterator found = params.find(param);

    assert( found != params.end() );
    if ( found == params.end() ) {
        throw std::runtime_error("buildChannelsMenus() must be called before getParamOptions() and only works for params that have been registered with addParamToRebuild()");
    }

    return found->second.options;
}

void
BuildChannelMenusData::buildChannelsMenus()
{
    for (std::map<const ChoiceParamClips*, ChoiceParamData>::iterator it = params.begin(); it != params.end(); ++it) {
        if (!it->second.hasChanged) {
            continue;
        }


        std::string oldComponent;
        it->first->stringparam->getValue(oldComponent);

        // Get the current choice menu state
        std::vector<std::string> oldOptions, oldOptionLabels, newOptionLabels;
        it->first->param->getOptions(&oldOptions, &oldOptionLabels);
        assert( oldOptionLabels.empty() || oldOptionLabels.size() == oldOptions.size() );

        // Extract the new list from the components present
        if (!it->first->isOutput) {
            Factory::addInputChannelOptionsRGBA(it->first->clipsName, !it->first->isOutput, &it->second.options, &newOptionLabels);
        }
        for (std::size_t c = 0; c < it->second.clipsInfos.size(); ++c) {
            appendComponents(it->second.clipsInfos[c]->clip->name(), it->second.clipsInfos[c]->componentsPresent, it->first->isOutput, &it->second.options, &newOptionLabels);
        }

        if (mergeMenus) {
            // Merge the 2 list together
            if (it->first->isOutput) {
                MergeOutputLayerEntriesData tmpData;
                mergeChannelEntries(oldOptions, oldOptionLabels, &it->second.options, &newOptionLabels, layerEqualityFunctor, &tmpData);
            } else {
                MergeChannelData tmpData;
                mergeChannelEntries(oldOptions, oldOptionLabels, &it->second.options, &newOptionLabels, channelEqualityFunctor, &tmpData);
            }
        }

        // Set the new choice menu
        it->first->param->resetOptions(it->second.options, newOptionLabels);
    }         // for (std::size_t k = 0; k < params.size(); ++k) {
}

void
BuildChannelMenusData::addParamToRebuild(const OFX::MultiPlane::ChoiceParamClips *paramData,
                                         bool addChoiceAllToOutput,
                                         std::map<OFX::Clip*, std::vector<std::string> >& componentsCache)
{
    ChoiceParamData& data = params[paramData];


    //data.clipsData.resize(paramData->clips.size());
    for (std::size_t i = 0; i < paramData->clips.size(); ++i) {
        std::map<OFX::Clip*, ClipComponentsInfo>::iterator foundCacheInfoForClip = cacheInfos.find(paramData->clips[i]);
        if ( foundCacheInfoForClip != cacheInfos.end() ) {
            // We already processed components for that clip
            data.clipsInfos.push_back(&foundCacheInfoForClip->second);
            if (!mergeMenus || !foundCacheInfoForClip->second.isCacheUpToDate) {
                data.hasChanged = true;
            }
        } else {
            ClipComponentsInfo& clipInfo = cacheInfos[paramData->clips[i]];
            data.clipsInfos.push_back(&clipInfo);
            clipInfo.clip = paramData->clips[i];

            // Create the clip info
            paramData->clips[i]->getComponentsPresent(&clipInfo.componentsPresent);

            if (paramData->isOutput && addChoiceAllToOutput) {
                clipInfo.componentsPresent.push_back(kPlaneLabelAll);
            }

            std::map<OFX::Clip*, std::vector<std::string> >::iterator foundCompsCache = componentsCache.find(paramData->clips[i]);
            if ( foundCompsCache != componentsCache.end() ) {
                if (!mergeMenus) {
                    foundCompsCache->second.clear();
                    data.hasChanged = true;
                } else {
                    clipInfo.componentsPresentCache = &foundCompsCache->second;

                    bool thisListChanged = hasListChanged(clipInfo.componentsPresent, *clipInfo.componentsPresentCache);
                    clipInfo.isCacheUpToDate = !thisListChanged;
                    clipInfo.comparisonToCacheDone = true;
                    if (thisListChanged) {
                        data.hasChanged = true;
                        *clipInfo.componentsPresentCache = clipInfo.componentsPresent;
                    }
                }
            } else {
                data.hasChanged = true;
            }
        }
    }
}

MultiPlaneEffect::ChangedParamRetCode
MultiPlaneEffectPrivate::checkIfChangedParamCalledOnDynamicChoiceInternal(const std::string& paramName,
                                                                          const ChoiceParamClips& param,
                                                                          OFX::InstanceChangeReason /*reason*/)
{
    if (param.stringparam) {
        if ( paramName == param.param->getName() ) {
            int choice_i;
            param.param->getValue(choice_i);
            std::string optionName;
            param.param->getOption(choice_i, optionName);
            param.stringparam->setValue(optionName);

            return MultiPlaneEffect::eChangedParamRetCodeChoiceParamChanged;
        } else if ( paramName == param.stringparam->getName() ) {
            std::vector<std::string> options;
            param.param->getOptions(&options);
            setChannelsFromStringParamInternal(param.param, param.stringparam, options, true);

            return MultiPlaneEffect::eChangedParamRetCodeStringParamChanged;
        } else if ( paramName == param.buttonparam->getName() ) {
            _publicInterface->buildChannelMenus(param.param->getName(), false, lastBuildChannelMenusHadAllChoice);

            return MultiPlaneEffect::eChangedParamRetCodeButtonParamChanged;
        }
    }

    return MultiPlaneEffect::eChangedParamRetCodeNoChange;
}

MultiPlaneEffect::ChangedParamRetCode
MultiPlaneEffect::checkIfChangedParamCalledOnDynamicChoice(const std::string& paramName,
                                                           const std::string& paramToCheck,
                                                           OFX::InstanceChangeReason reason)
{
    std::map<std::string, ChoiceParamClips>::iterator found = _imp->params.find(paramToCheck);

    if ( found == _imp->params.end() ) {
        return eChangedParamRetCodeNoChange;
    }

    return _imp->checkIfChangedParamCalledOnDynamicChoiceInternal(paramName, found->second, reason);
}

bool
MultiPlaneEffect::handleChangedParamForAllDynamicChoices(const std::string& paramName,
                                                         OFX::InstanceChangeReason reason)
{
    for (std::map<std::string, ChoiceParamClips>::iterator it = _imp->params.begin(); it != _imp->params.end(); ++it) {
        if ( _imp->checkIfChangedParamCalledOnDynamicChoiceInternal(paramName, it->second, reason) ) {
            return true;
        }
    }

    return false;
}

bool
MultiPlaneEffect::getPlaneNeededForParam(double time,
                                         const std::string& paramName,
                                         OFX::Clip** clip,
                                         std::string* ofxPlane,
                                         std::string* ofxComponents,
                                         int* channelIndexInPlane,
                                         bool* isCreatingAlpha)
{
    std::map<std::string, ChoiceParamClips>::iterator found = _imp->params.find(paramName);

    assert( found != _imp->params.end() );
    if ( found == _imp->params.end() ) {
        return false;
    }

    // clipname, components
    typedef std::map<std::string, ClipsComponentsInfoBase> PerClipComponents;
    PerClipComponents clipsComponents;
    for (std::size_t i = 0; i < found->second.clips.size(); ++i) {
        ClipsComponentsInfoBase& components = clipsComponents[found->second.clips[i]->name()];
        components.clip = found->second.clips[i];
    }

    assert(clip);
    assert( !clipsComponents.empty() );
    *clip = 0;

    *isCreatingAlpha = false;

    int channelIndex;
    found->second.param->getValueAtTime(time, channelIndex);
    std::string channelEncoded;
    if ( 0 <= channelIndex && channelIndex < found->second.param->getNOptions() ) {
        found->second.param->getOption(channelIndex, channelEncoded);
    } else {
        return false;
    }

    if ( channelEncoded.empty() ) {
        return false;
    }

    if (channelEncoded == kMultiPlaneParamOutputOption0) {
        *ofxComponents =  kMultiPlaneParamOutputOption0;

        return true;
    }

    if (channelEncoded == kMultiPlaneParamOutputOption1) {
        *ofxComponents = kMultiPlaneParamOutputOption1;

        return true;
    }

    PerClipComponents::iterator foundClip = clipsComponents.end();
    for (PerClipComponents::iterator it = clipsComponents.begin(); it != clipsComponents.end(); ++it) {
        // Must be at least something like "A."
        if (channelEncoded.size() < it->first.size() + 1) {
            return false;
        }

        if (channelEncoded.substr( 0, it->first.size() ) == it->first) {
            *clip = it->second.clip;
            foundClip = it;
            break;
        }
    }

    if (!*clip) {
        return false;
    }

    std::size_t lastDotPos = channelEncoded.find_last_of('.');
    if ( ( lastDotPos == std::string::npos) || ( lastDotPos == channelEncoded.size() - 1) ) {
        *clip = 0;

        return false;
    }

    std::string chanName = channelEncoded.substr(lastDotPos + 1, std::string::npos);
    std::string layerName;
    for (std::size_t i = foundClip->first.size() + 1; i < lastDotPos; ++i) {
        layerName.push_back(channelEncoded[i]);
    }

    if ( layerName.empty() ||
         ( layerName == kPlaneLabelColorAlpha) ||
         ( layerName == kPlaneLabelColorRGB) ||
         ( layerName == kPlaneLabelColorRGBA) ) {
        std::string comp = (*clip)->getPixelComponentsProperty();
        if ( ( chanName == "r") || ( chanName == "R") || ( chanName == "x") || ( chanName == "X") ) {
            *channelIndexInPlane = 0;
        } else if ( ( chanName == "g") || ( chanName == "G") || ( chanName == "y") || ( chanName == "Y") ) {
            *channelIndexInPlane = 1;
        } else if ( ( chanName == "b") || ( chanName == "B") || ( chanName == "z") || ( chanName == "Z") ) {
            *channelIndexInPlane = 2;
        } else if ( ( chanName == "a") || ( chanName == "A") || ( chanName == "w") || ( chanName == "W") ) {
            if (comp == kOfxImageComponentAlpha) {
                *channelIndexInPlane = 0;
            } else if (comp == kOfxImageComponentRGBA) {
                *channelIndexInPlane = 3;
            } else {
                *isCreatingAlpha = true;
                *ofxComponents = kMultiPlaneParamOutputOption1;

                return true;
            }
        } else {
            assert(false);
        }
        *ofxComponents = comp;
        *ofxPlane = kFnOfxImagePlaneColour;

        return true;
    } else if (layerName == kPlaneLabelDisparityLeftPlaneName) {
        if ( ( chanName == "x") || ( chanName == "X") ) {
            *channelIndexInPlane = 0;
        } else if ( ( chanName == "y") || ( chanName == "Y") ) {
            *channelIndexInPlane = 1;
        } else {
            assert(false);
        }
        *ofxComponents = kPlaneLabelDisparityLeftPlaneName;
        *ofxPlane = kPlaneLabelDisparityLeftPlaneName;

        return true;
    } else if (layerName == kPlaneLabelDisparityRightPlaneName) {
        if ( ( chanName == "x") || ( chanName == "X") ) {
            *channelIndexInPlane = 0;
        } else if ( ( chanName == "y") || ( chanName == "Y") ) {
            *channelIndexInPlane = 1;
        } else {
            assert(false);
        }
        *ofxComponents = kPlaneLabelDisparityRightPlaneName;
        *ofxPlane =  kPlaneLabelDisparityRightPlaneName;

        return true;
    } else if (layerName == kPlaneLabelMotionBackwardPlaneName) {
        if ( ( chanName == "u") || ( chanName == "U") ) {
            *channelIndexInPlane = 0;
        } else if ( ( chanName == "v") || ( chanName == "V") ) {
            *channelIndexInPlane = 1;
        } else {
            assert(false);
        }
        *ofxComponents = kPlaneLabelMotionBackwardPlaneName;
        *ofxPlane = kPlaneLabelMotionBackwardPlaneName;

        return true;
    } else if (layerName == kPlaneLabelMotionForwardPlaneName) {
        if ( ( chanName == "u") || ( chanName == "U") ) {
            *channelIndexInPlane = 0;
        } else if ( ( chanName == "v") || ( chanName == "V") ) {
            *channelIndexInPlane = 1;
        } else {
            assert(false);
        }
        *ofxComponents = kPlaneLabelMotionForwardPlaneName;
        *ofxPlane = kPlaneLabelMotionForwardPlaneName;

        return true;
#ifdef OFX_EXTENSIONS_NATRON
    } else {
        //Find in clip components the layerName
        foundClip->second.componentsPresent = getCachedComponentsPresent(foundClip->second.clip);
        for (std::vector<std::string>::const_iterator it = foundClip->second.componentsPresent.begin(); it != foundClip->second.componentsPresent.end(); ++it) {
            //We found a matching layer
            std::string realLayerName;
            std::vector<std::string> channels;
            std::vector<std::string> layerChannels = mapPixelComponentCustomToLayerChannels(*it);
            if ( layerChannels.empty() || ( layerName != layerChannels[0]) ) {
                // ignore it
                continue;
            }
            channels.assign( layerChannels.begin() + 1, layerChannels.end() );
            int foundChannel = -1;
            for (std::size_t i = 0; i < channels.size(); ++i) {
                if (channels[i] == chanName) {
                    foundChannel = i;
                    break;
                }
            }
            assert(foundChannel != -1);
            *ofxPlane = *it;
            *channelIndexInPlane = foundChannel;
            *ofxComponents = *it;

            return true;
        }

#endif // OFX_EXTENSIONS_NATRON
    }

    return false;
} // MultiPlaneEffect::getPlaneNeededForParam

bool
MultiPlaneEffect::getPlaneNeededInOutput(std::string* ofxPlane,
                                         std::string* ofxComponents)
{
    std::map<std::string, ChoiceParamClips>::iterator found = _imp->params.find(kMultiPlaneParamOutputChannels);

    assert( found != _imp->params.end() );
    if ( found == _imp->params.end() ) {
        return false;
    }


    int layer_i;
    found->second.param->getValue(layer_i);
    std::string layerName;
    try {
        found->second.param->getOption(layer_i, layerName);
    } catch (...) {
    }

    if ( layerName.empty() ||
         ( layerName == kPlaneLabelColorRGBA) ||
         ( layerName == kPlaneLabelColorRGB) ||
         ( layerName == kPlaneLabelColorAlpha) || found->second.param->getIsSecret() ) {
        assert(found->second.clips[0]);
        std::string comp = found->second.clips[0]->getPixelComponentsProperty();
        *ofxComponents = comp;
        *ofxPlane = kFnOfxImagePlaneColour;

        return true;
    } else if (layerName == kPlaneLabelAll) {
        *ofxPlane = kPlaneLabelAll;
        *ofxComponents = kPlaneLabelAll;
    } else if (layerName == kPlaneLabelDisparityLeftPlaneName) {
        std::vector<std::string> channels(2);
        channels[0] = "X";
        channels[1] = "Y";
        *ofxComponents = OFX::MultiPlane::Utils::makeNatronCustomChannel(kPlaneLabelDisparityLeftPlaneName, channels);
        *ofxPlane = *ofxComponents;

        return true;
    } else if (layerName == kPlaneLabelDisparityRightPlaneName) {
        std::vector<std::string> channels(2);
        channels[0] = "X";
        channels[1] = "Y";
        *ofxComponents = OFX::MultiPlane::Utils::makeNatronCustomChannel(kPlaneLabelDisparityRightPlaneName, channels);
        *ofxPlane = *ofxComponents;

        return true;
    } else if (layerName == kPlaneLabelMotionBackwardPlaneName) {
        std::vector<std::string> channels(2);
        channels[0] = "U";
        channels[1] = "V";
        *ofxComponents = OFX::MultiPlane::Utils::makeNatronCustomChannel(kPlaneLabelMotionBackwardPlaneName, channels);
        *ofxPlane = *ofxComponents;

        return true;
    } else if (layerName == kPlaneLabelMotionForwardPlaneName) {
        std::vector<std::string> channels(2);
        channels[0] = "U";
        channels[1] = "V";
        *ofxComponents = OFX::MultiPlane::Utils::makeNatronCustomChannel(kPlaneLabelMotionForwardPlaneName, channels);
        *ofxPlane = *ofxComponents;

        return true;
#ifdef OFX_EXTENSIONS_NATRON
    } else {
        std::vector<std::string> components;
        components = getCachedComponentsPresent(found->second.clips[0]);

        //Find in aComponents or bComponents a layer matching the name of the layer
        for (std::vector<std::string>::const_iterator it = components.begin(); it != components.end(); ++it) {
            if (it->find(layerName) != std::string::npos) {
                //We found a matching layer
                std::string realLayerName;
                std::vector<std::string> layerChannels = mapPixelComponentCustomToLayerChannels(*it);
                if ( layerChannels.empty() ) {
                    // ignore it
                    continue;
                }
                *ofxPlane = *it;
                *ofxComponents = *it;

                return true;
            }
        }
#endif // OFX_EXTENSIONS_NATRON
    }

    return false;
} // MultiPlaneEffect::getPlaneNeededInOutput

namespace Factory {
OFX::ChoiceParamDescriptor*
describeInContextAddOutputLayerChoice(bool addAllChoice,
                                      OFX::ImageEffectDescriptor &desc,
                                      OFX::PageParamDescriptor* page)
{
    OFX::ChoiceParamDescriptor *ret;
    {
        OFX::ChoiceParamDescriptor *param = desc.defineChoiceParam(kMultiPlaneParamOutputChannels);
        param->setLabel(kMultiPlaneParamOutputChannelsLabel);
        param->setHint(kMultiPlaneParamOutputChannelsHint);
#ifdef OFX_EXTENSIONS_NATRON
        param->setHostCanAddOptions(true);             //< the host can allow the user to add custom entries
#endif

        param->appendOption(kPlaneLabelColorRGBA);
        /*param->appendOption(kPlaneLabelMotionForwardPlaneName);
           param->appendOption(kPlaneLabelMotionBackwardPlaneName);
           param->appendOption(kPlaneLabelDisparityLeftPlaneName);
           param->appendOption(kPlaneLabelDisparityRightPlaneName);*/
        if (addAllChoice) {
            param->appendOption(kPlaneLabelAll);
        }
        param->setEvaluateOnChange(false);
        param->setIsPersistent(true);
        param->setDefault(0);
        param->setAnimates(false);
        desc.addClipPreferencesSlaveParam(*param);             // < the menu is built in getClipPreferences
        if (page) {
            page->addChild(*param);
        }
        ret = param;
    }
    {
        //Add a hidden string param that will remember the value of the choice
        OFX::StringParamDescriptor* param = desc.defineStringParam(kMultiPlaneParamOutputChannelsChoice);
        param->setLabel(kMultiPlaneParamOutputChannelsLabel "Choice");
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        OFX::PushButtonParamDescriptor* param = desc.definePushButtonParam(kMultiPlaneParamOutputChannelsRefreshButton);
        param->setLabel(kMultiPlaneParamOutputChannels "RefreshButton");
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }

    return ret;
}

OFX::ChoiceParamDescriptor*
describeInContextAddChannelChoice(OFX::ImageEffectDescriptor &desc,
                                  OFX::PageParamDescriptor* page,
                                  const std::vector<std::string>& clips,
                                  const std::string& name,
                                  const std::string& label,
                                  const std::string& hint)

{
    ChoiceParamDescriptor *ret;
    {
        ChoiceParamDescriptor *param = desc.defineChoiceParam(name);
        param->setLabel(label);
        param->setHint(hint);
        param->setAnimates(false);
        addInputChannelOptionsRGBA(param, clips, true);
        param->setEvaluateOnChange(false);
        param->setIsPersistent(false);
        if (page) {
            page->addChild(*param);
        }
        ret = param;
    }
    {
        std::string strName = name + "Choice";
        //Add a hidden string param that will remember the value of the choice
        OFX::StringParamDescriptor* param = desc.defineStringParam(strName);
        param->setLabel(label + "Choice");
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        std::string strName = name + "RefreshButton";
        OFX::PushButtonParamDescriptor* param = desc.definePushButtonParam(strName);
        param->setLabel(label + "RefreshButton");
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
