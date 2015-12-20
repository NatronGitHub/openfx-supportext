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
 * Helper functions to implement plug-ins that support kFnOfxImageEffectPlaneSuite v2
 * In order to use these functions the following condition must be met:
 #if defined(OFX_EXTENSIONS_NUKE) && defined(OFX_EXTENSIONS_NATRON)
 
 if (OFX::fetchSuite(kFnOfxImageEffectPlaneSuite, 2) &&  // for clipGetImagePlane
 OFX::getImageEffectHostDescription()->supportsDynamicChoices && // for dynamic layer choices
 OFX::getImageEffectHostDescription()->isMultiPlanar) // for clipGetImagePlane
 ... this is ok...
 #endif
 */
#include "ofxsMultiPlane.h"

namespace  {
template <typename T>
void addInputChannelOptionsRGBA(T* param,
                                const std::vector<std::string>& clips,
                                bool addConstants,
                                std::vector<std::string>* options) {
    
    static const char* optionsBits[4][2] = { {"r", "Red"}, {"g", "Green"}, {"b", "Blue"}, {"a", "Alpha"} };
    
    for (std::size_t c = 0; c < clips.size(); ++c) {
        const std::string& clipName = clips[c];
        
        for (int i = 0; i < 4; ++i) {
            std::string opt,hint;
            opt.append(clipName);
            opt.push_back('.');
            opt.append(optionsBits[i][0]);
            hint.append(optionsBits[i][1]);
            hint.append(" channel from input ");
            hint.append(clipName);
            param->appendOption(opt,hint);
            if (options) {
                options->push_back(opt);
            }
        }
        
        if (addConstants && c == 0) {
            {
                std::string opt,hint;
                opt.append(kMultiPlaneParamOutputOption0);
                hint.append(kMultiPlaneParamOutputOption0Hint);
                param->appendOption(opt,hint);
                if (options) {
                    options->push_back(opt);
                }
                
            }
            {
                std::string opt,hint;
                opt.append(kMultiPlaneParamOutputOption1);
                hint.append(kMultiPlaneParamOutputOption1Hint);
                param->appendOption(opt,hint);
                if (options) {
                    options->push_back(opt);
                }
                
            }
        }
    }
    
}

static bool hasListChanged(const std::list<std::string>& oldList, const std::list<std::string>& newList)
{
    if (oldList.size() != newList.size()) {
        return true;
    }
    
    std::list<std::string>::const_iterator itNew = newList.begin();
    for (std::list<std::string>::const_iterator it = oldList.begin(); it!=oldList.end(); ++it,++itNew) {
        if (*it != *itNew) {
            return true;
        }
    }
    return false;
}

static void extractChannelsFromComponentString(const std::string& comp,
                                               std::string* layer,
                                               std::string* pairedLayer, //< if disparity or motion vectors
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
            channels->assign(layerChannels.begin() + 1, layerChannels.end());
        }
#endif
    }
}

static void appendComponents(const std::string& clipName,
                             const std::list<std::string>& components,
                             OFX::ChoiceParam* param,
                             std::vector<std::string>* channelChoices)
{
    
    std::list<std::string> usedComps;
    for (std::list<std::string>::const_iterator it = components.begin(); it!=components.end(); ++it) {
        std::string layer, secondLayer;
        std::vector<std::string> channels;
        extractChannelsFromComponentString(*it, &layer, &secondLayer, &channels);
        if (channels.empty()) {
            continue;
        }
        if (layer.empty()) {
            continue;
        }
        for (std::size_t i = 0; i < channels.size(); ++i) {
            std::string opt = clipName + ".";
            if (!layer.empty()) {
                opt.append(layer);
                opt.push_back('.');
            }
            opt.append(channels[i]);
            
            if (std::find(usedComps.begin(), usedComps.end(), opt) == usedComps.end()) {
                usedComps.push_back(opt);
                param->appendOption(opt, channels[i] + " channel from " + ((layer.empty())? std::string() : std::string("layer/view ") + layer + " of ") + "input " + clipName);
                if (channelChoices) {
                    channelChoices->push_back(opt);
                }
                
            }
            
        }
        
        if (!secondLayer.empty()) {
            for (std::size_t i = 0; i < channels.size(); ++i) {
                std::string opt = clipName + ".";
                if (!secondLayer.empty()) {
                    opt.append(secondLayer);
                    opt.push_back('.');
                }
                opt.append(channels[i]);
                if (std::find(usedComps.begin(), usedComps.end(), opt) == usedComps.end()) {
                    usedComps.push_back(opt);
                    param->appendOption(opt, channels[i] + " channel from layer " + secondLayer + " of input " + clipName);
                    if (channelChoices) {
                        channelChoices->push_back(opt);
                    }
                    
                }
            }
        }
    }
}
    

struct SetChannelsFromStringData
{
    OFX::ChoiceParam* param;
    OFX::StringParam* stringParam;
    
    std::vector<std::string> options;
};

static void setChannelsFromStringParamsInternal(const std::vector<SetChannelsFromStringData>& data, bool allowReset)
{
    for (std::vector<SetChannelsFromStringData>::const_iterator it = data.begin(); it!=data.end(); ++it) {
        
        std::string valueStr;
        it->stringParam->getValue(valueStr);
        
        if (valueStr.empty()) {
            int cur_i;
            it->param->getValue(cur_i);
            if (cur_i >= 0 && cur_i < (int)it->options.size()) {
                valueStr = it->options[cur_i];
            }
            it->param->getOption(cur_i, valueStr);
            it->stringParam->setValue(valueStr);
        } else {
            int foundOption = -1;
            for (int i = 0; i < (int)it->options.size(); ++i) {
                if (it->options[i] == valueStr) {
                    foundOption = i;
                    break;
                }
            }
            if (foundOption != -1) {
                it->param->setValue(foundOption);
            } else {
                if (allowReset) {
                    int defValue;
                    it->param->getDefault(defValue);
                    if (defValue >= 0 && defValue < (int)it->options.size()) {
                        it->param->setValue(defValue);
                        it->stringParam->setValue(it->options[defValue]);
                    }
                }
            }
        }

    }
}

} // anonymous namespace

namespace OFX {
    namespace MultiPlane {
        
        
        bool getPlaneNeededForParam(double time,
                                    const PerClipComponentsMap& perClipComponents,
                                    OFX::ChoiceParam* param,
                                    OFX::Clip** clip,
                                    std::string* ofxPlane,
                                    std::string* ofxComponents,
                                    int* channelIndexInPlane,
                                    bool* isCreatingAlpha)
        {
            assert(clip);
            assert(!perClipComponents.empty());
            *clip = 0;
            
            *isCreatingAlpha = false;
            
            int channelIndex;
            param->getValueAtTime(time, channelIndex);
            std::string channelEncoded;
            param->getOption(channelIndex, channelEncoded);
            if (channelEncoded.empty()) {
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
            
            PerClipComponentsMap::const_iterator foundClip = perClipComponents.end();
            for (PerClipComponentsMap::const_iterator it = perClipComponents.begin(); it!=perClipComponents.end(); ++it) {
                
                // Must be at least something like "A."
                if (channelEncoded.size() < it->first.size() + 1) {
                    return false;
                }
                
                if (channelEncoded.substr(0,it->first.size()) == it->first) {
                    *clip = it->second.clip;
                    foundClip = it;
                    break;
                }
                
            }
            
            if (!*clip) {
                return false;
            }
            
            std::size_t lastDotPos = channelEncoded.find_last_of('.');
            if (lastDotPos == std::string::npos || lastDotPos == channelEncoded.size() - 1) {
                *clip = 0;
                return false;
            }
            
            std::string chanName = channelEncoded.substr(lastDotPos + 1,std::string::npos);
            std::string layerName;
            for (std::size_t i = foundClip->first.size() + 1; i < lastDotPos; ++i) {
                layerName.push_back(channelEncoded[i]);
            }
            
            if (layerName.empty() ||
                layerName == kPlaneLabelColorAlpha ||
                layerName == kPlaneLabelColorRGB ||
                layerName == kPlaneLabelColorRGBA) {
                std::string comp = (*clip)->getPixelComponentsProperty();
                if (chanName == "r" || chanName == "R" || chanName == "x" || chanName == "X") {
                    *channelIndexInPlane = 0;
                } else if (chanName == "g" || chanName == "G" || chanName == "y" || chanName == "Y") {
                    *channelIndexInPlane = 1;
                } else if (chanName == "b" || chanName == "B" || chanName == "z" || chanName == "Z") {
                    *channelIndexInPlane = 2;
                } else if (chanName == "a" || chanName == "A" || chanName == "w" || chanName == "W") {
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
                if (chanName == "x" || chanName == "X") {
                    *channelIndexInPlane = 0;
                } else if (chanName == "y" || chanName == "Y") {
                    *channelIndexInPlane = 1;
                } else {
                    assert(false);
                }
                *ofxComponents = kFnOfxImageComponentStereoDisparity;
                *ofxPlane = kFnOfxImagePlaneStereoDisparityLeft;
                return true;
            } else if (layerName == kPlaneLabelDisparityRightPlaneName) {
                if (chanName == "x" || chanName == "X") {
                    *channelIndexInPlane = 0;
                } else if (chanName == "y" || chanName == "Y") {
                    *channelIndexInPlane = 1;
                } else {
                    assert(false);
                }
                *ofxComponents = kFnOfxImageComponentStereoDisparity;
                *ofxPlane =  kFnOfxImagePlaneStereoDisparityRight;
                return true;
            } else if (layerName == kPlaneLabelMotionBackwardPlaneName) {
                if (chanName == "u" || chanName == "U") {
                    *channelIndexInPlane = 0;
                } else if (chanName == "v" || chanName == "V") {
                    *channelIndexInPlane = 1;
                } else {
                    assert(false);
                }
                *ofxComponents = kFnOfxImageComponentMotionVectors;
                *ofxPlane = kFnOfxImagePlaneBackwardMotionVector;
                return true;
            } else if (layerName == kPlaneLabelMotionForwardPlaneName) {
                if (chanName == "u" || chanName == "U") {
                    *channelIndexInPlane = 0;
                } else if (chanName == "v" || chanName == "V") {
                    *channelIndexInPlane = 1;
                } else {
                    assert(false);
                }
                *ofxComponents = kFnOfxImageComponentMotionVectors;
                *ofxPlane = kFnOfxImagePlaneForwardMotionVector;
                return true;
#ifdef OFX_EXTENSIONS_NATRON
            } else {
                //Find in clip components the layerName
                for (std::list<std::string>::const_iterator it = foundClip->second.componentsPresent.begin(); it!=foundClip->second.componentsPresent.end(); ++it) {
                    //We found a matching layer
                    std::string realLayerName;
                    std::vector<std::string> channels;
                    std::vector<std::string> layerChannels = mapPixelComponentCustomToLayerChannels(*it);
                    if (layerChannels.empty() || layerName != layerChannels[0]) {
                        // ignore it
                        continue;
                    }
                    channels.assign(layerChannels.begin() + 1, layerChannels.end());
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
        }
        
        bool getPlaneNeededInOutput(const std::list<std::string>& outputComponents,
                                    const OFX::Clip* dstClip,
                                    OFX::ChoiceParam* param,
                                    std::string* ofxPlane,
                                    std::string* ofxComponents)
        {
            int layer_i;
            param->getValue(layer_i);
            std::string layerName;
            param->getOption(layer_i, layerName);
            
            if (layerName.empty() ||
                layerName == kPlaneLabelColorRGBA ||
                layerName == kPlaneLabelColorRGB ||
                layerName == kPlaneLabelColorAlpha) {
                std::string comp = dstClip->getPixelComponentsProperty();
                *ofxComponents = comp;
                *ofxPlane = kFnOfxImagePlaneColour;
                return true;
            } else if (layerName == kPlaneLabelDisparityLeftPlaneName) {
                *ofxComponents = kFnOfxImageComponentStereoDisparity;
                *ofxPlane = kFnOfxImagePlaneStereoDisparityLeft;
                return true;
            } else if (layerName == kPlaneLabelDisparityRightPlaneName) {
                *ofxComponents = kFnOfxImageComponentStereoDisparity;
                *ofxPlane =  kFnOfxImagePlaneStereoDisparityRight;
                return true;
            } else if (layerName == kPlaneLabelMotionBackwardPlaneName) {
                *ofxComponents = kFnOfxImageComponentMotionVectors;
                *ofxPlane = kFnOfxImagePlaneBackwardMotionVector;
                return true;
            } else if (layerName == kPlaneLabelMotionForwardPlaneName) {
                *ofxComponents = kFnOfxImageComponentMotionVectors;
                *ofxPlane = kFnOfxImagePlaneForwardMotionVector;
                return true;
#ifdef OFX_EXTENSIONS_NATRON
            } else {
                //Find in aComponents or bComponents a layer matching the name of the layer
                for (std::list<std::string>::const_iterator it = outputComponents.begin(); it!=outputComponents.end(); ++it) {
                    if (it->find(layerName) != std::string::npos) {
                        //We found a matching layer
                        std::string realLayerName;
                        std::vector<std::string> layerChannels = mapPixelComponentCustomToLayerChannels(*it);
                        if (layerChannels.empty()) {
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

        }
        
        void buildChannelMenus(const std::vector<ChoiceParamClips>& params)
        {
            
            std::map<OFX::Clip*, const ClipComponentsInfo*> cacheInfos;
            
            std::vector<SetChannelsFromStringData> data(params.size());

            for (std::size_t k = 0; k < params.size(); ++k) {
                
                bool hasChanged = false;
                for (std::size_t c = 0; c < params[k].clips.size(); ++c) {
                    if (!params[k].clips[c].componentsPresentCache) {
                        hasChanged = true;
                        break;
                    }
                    
                    ///Try to call hasListChanged as few as possible
                    bool thisListChanged;
                    if (params[k].clips[c].comparisonToCacheDone) {
                        thisListChanged = !params[k].clips[c].isCacheValid;
                    } else {
                        thisListChanged = hasListChanged(params[k].clips[c].componentsPresent, *params[k].clips[c].componentsPresentCache);
                        params[k].clips[c].isCacheValid = !thisListChanged;
                        params[k].clips[c].comparisonToCacheDone = true;
                    }
                    if (thisListChanged) {
                        hasChanged = true;
                        break;
                    }
                    
                    std::map<OFX::Clip*, const ClipComponentsInfo*>::iterator foundCacheInfoForClip = cacheInfos.find(params[k].clips[c].clip);
                    if (foundCacheInfoForClip == cacheInfos.end()) {
                        cacheInfos.insert(std::make_pair(params[k].clips[c].clip, &params[k].clips[c]));
                    }
                }
                if (!hasChanged) {
                    break;
                }
                
                //We must rebuild the choice
                params[k].param->resetOptions();
                std::vector<std::string> clipNames;
                bool isOutput = false;
                for (std::size_t c = 0; c < params[k].clips.size(); ++c) {
                    const std::string& name = params[k].clips[c].clip->name();
                    clipNames.push_back(name);
                    if (name == kOfxImageEffectOutputClipName) {
                        assert(params[k].clips.size() == 1);
                        isOutput = true;
                    }
                }
                
                data[k].param = params[k].param;
                data[k].stringParam = params[k].stringparam;
                addInputChannelOptionsRGBA<ChoiceParam>(params[k].param, clipNames, !isOutput, &data[k].options);
                for (std::size_t c = 0; c < params[k].clips.size(); ++c) {
                    appendComponents(params[k].clips[c].clip->name(), params[k].clips[c].componentsPresent, params[k].param, &data[k].options);
                }
                
               
            }
            
            //Update the cache for each clip
            for (std::map<OFX::Clip*, const ClipComponentsInfo*>::iterator it = cacheInfos.begin(); it!=cacheInfos.end(); ++it) {
                assert(it->second->componentsPresentCache);
                *it->second->componentsPresentCache = it->second->componentsPresent;
            }
            
            
            setChannelsFromStringParamsInternal(data, true);
        }
        
        void setChannelsFromStringParams(const std::list<ChoiceStringParam>& params, bool allowReset)
        {
            std::vector<SetChannelsFromStringData> data(params.size());
            int c = 0;
            for (std::list<ChoiceStringParam>::const_iterator it = params.begin(); it!=params.end(); ++it, ++c) {
                SetChannelsFromStringData& d = data[c];
                d.param = it->param;
                d.stringParam = it->stringParam;
                int nOptions = it->param->getNOptions();
                d.options.resize(nOptions);
                for (int i = 0; i < nOptions; ++i) {
                    d.param->getOption(i, d.options[i]);
                }
            }
            setChannelsFromStringParamsInternal(data, allowReset);
        }
        
        void checkIfChangedParamCalledOnDynamicChoice(const std::string& paramName, OFX::InstanceChangeReason reason, OFX::ChoiceParam* param, OFX::StringParam* stringparam)
        {
            
            if (paramName == param->getName() && reason == OFX::eChangeUserEdit && stringparam) {
                int choice_i;
                param->getValue(choice_i);
                std::string optionName;
                param->getOption(choice_i, optionName);
                stringparam->setValue(optionName);
            }
        }
        
        OFX::ChoiceParamDescriptor* describeInContextAddOutputLayerChoice(OFX::ImageEffectDescriptor &desc, OFX::PageParamDescriptor* page)
        {
            
            OFX::ChoiceParamDescriptor *ret;
            {
                OFX::ChoiceParamDescriptor *param = desc.defineChoiceParam(kMultiPlaneParamOutputChannels);
                param->setLabel(kMultiPlaneParamOutputChannelsLabel);
                param->setHint(kMultiPlaneParamOutputChannelsHint);
#ifdef OFX_EXTENSIONS_NATRON
                param->setHostCanAddOptions(true); //< the host can allow the user to add custom entries
#endif
                param->appendOption(kPlaneLabelColorRGBA);
                param->appendOption(kPlaneLabelMotionForwardPlaneName);
                param->appendOption(kPlaneLabelMotionBackwardPlaneName);
                param->appendOption(kPlaneLabelDisparityLeftPlaneName);
                param->appendOption(kPlaneLabelDisparityRightPlaneName);
                param->setEvaluateOnChange(false);
                param->setIsPersistant(false);
                param->setAnimates(false);
                desc.addClipPreferencesSlaveParam(*param); // < the menu is built in getClipPreferences
                if (page) {
                    page->addChild(*param);
                }
                ret = param;
            }
            {
                //Add a hidden string param that will remember the value of the choice
                OFX::StringParamDescriptor* param = desc.defineStringParam(kMultiPlaneParamOutputChannelsChoice);
                param->setLabel(kMultiPlaneParamOutputChannelsLabel "Choice");
                param->setIsSecret(true);
                page->addChild(*param);
                
            }
            return ret;
        }
        
        
        OFX::ChoiceParamDescriptor* describeInContextAddChannelChoice(OFX::ImageEffectDescriptor &desc,
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
                addInputChannelOptionsRGBA<ChoiceParamDescriptor>(param, clips, true, 0);
                param->setEvaluateOnChange(false);
                param->setIsPersistant(false);
                
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
                param->setIsSecret(true);
                page->addChild(*param);
            }
            return ret;
        }
        
    } // namespace MultiPlane

} // namespace OFX
