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

#ifndef openfx_supportext_ofxsMultiPlane_h
#define openfx_supportext_ofxsMultiPlane_h

#include <cmath>
#include <map>
#include <string>
#include <list>
#include <vector>

#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#ifdef OFX_EXTENSIONS_NATRON
#include "ofxNatron.h"
#endif

#define kPlaneLabelAll "All"
#define kPlaneLabelColorAlpha "Color.Alpha"
#define kPlaneLabelColorRGB "Color.RGB"
#define kPlaneLabelColorRGBA "Color.RGBA"
#define kPlaneLabelMotionBackwardPlaneName "Backward.Motion"
#define kPlaneLabelMotionForwardPlaneName "Forward.Motion"
#define kPlaneLabelDisparityLeftPlaneName "DisparityLeft.Disparity"
#define kPlaneLabelDisparityRightPlaneName "DisparityRight.Disparity"

#define kMultiPlaneParamOutputChannels kNatronOfxParamOutputChannels
#define kMultiPlaneParamOutputChannelsChoice kMultiPlaneParamOutputChannels "Choice"
#define kMultiPlaneParamOutputChannelsLabel "Output Layer"
#define kMultiPlaneParamOutputChannelsHint "The layer that will be written to in output"

#define kMultiPlaneParamOutputOption0 "0"
#define kMultiPlaneParamOutputOption0Hint "0 constant channel"
#define kMultiPlaneParamOutputOption1 "1"
#define kMultiPlaneParamOutputOption1Hint "1 constant channel"

namespace OFX {
    namespace MultiPlane {
        
        struct ClipsComponentsInfoBase
        {
            //A pointer to the clip
            OFX::Clip* clip;
            
            //The value returned by clip->getComponentsPresent()
            std::list<std::string> componentsPresent;
            
            ClipsComponentsInfoBase() : clip(0), componentsPresent() {}
            
            virtual ~ClipsComponentsInfoBase() {}
        };
    
        struct ClipComponentsInfo : public ClipsComponentsInfoBase
        {
            
            //A pointer to a components present cache held as a member of the plug-in (no need to lock it as accessed always on the same thread)
            //This is to speed-up buildChannelMenus to avoid re-building menus and make complex API calls if they did not change.
            std::list<std::string>* componentsPresentCache;
            
            //When hasListChanged has been called; this is set to true, indicating that the value of isCacheValid is correct
            mutable bool comparisonToCacheDone;
            mutable bool isCacheValid;
            
            ClipComponentsInfo() : ClipsComponentsInfoBase(), componentsPresentCache(0), comparisonToCacheDone(false), isCacheValid(false) {}
            
            virtual ~ClipComponentsInfo() {}
        };
        
        // map <ClipName, ClipComponentsInfo>
        typedef std::map<std::string, ClipsComponentsInfoBase> PerClipComponentsMap;
        
        /**
         * @brief Encode the given layer and channel names into a string following the specification in ofxNatron.h
         **/
        std::string makeNatronCustomChannel(const std::string& layer,const std::vector<std::string>& channels);
        
        /**
         * @brief Given the string "comp" in the format described in ofxNatron.h, extract the layer name, the paired layer (if any)
         * and the channels
         **/
        void extractChannelsFromComponentString(const std::string& comp,
                                                std::string* layer,
                                                std::string* pairedLayer, //< if disparity or motion vectors
                                                std::vector<std::string>* channels);
        
        /**
         * @brief Returns the layer and channel index selected by the user in the dynamic choice param.
         * @param ofxPlane Contains the plane name defined in nuke/fnOfxExtensions.h or the custom plane name defined in ofxNatron.h
         * @param ofxComponents Generally the same as ofxPlane except in the following cases:
         - for the motion vectors planes (e.g: kFnOfxImagePlaneBackwardMotionVector)
         where the compoonents are in that case kFnOfxImageComponentMotionVectors.
         - for the color plane (i.e: kFnOfxImagePlaneColour) where in that case the ofxComponents may be kOfxImageComponentAlpha or
         kOfxImageComponentRGBA or kOfxImageComponentRGB or any other "default" ofx components supported on the clip.
         *
         * If ofxPlane is empty but the function returned true that is because the choice is either kMultiPlaneParamOutputOption0 or kMultiPlaneParamOutputOption1
         * ofxComponents will have been set correctly to one of these values.
         *
         * @param channelIndexInPlane Contains the selected channel index in the layer set to ofxPlane
         * @param isCreatingAlpha If Selected layer is the colour plane (i.e: kPlaneLabelColorAlpha or kPlaneLabelColorRGB or kPlaneLabelColorRGBA)
         * and if the user selected the alpha channel (e.g: RGBA.a), but the clip pixel components are RGB, then this value will be set to true.
         **/

        bool getPlaneNeededForParam(double time,
                                    const PerClipComponentsMap& perClipComponents,
                                    OFX::ChoiceParam* param,
                                    OFX::Clip** clip,
                                    std::string* ofxPlane,
                                    std::string* ofxComponents,
                                    int* channelIndexInPlane,
                                    bool* isCreatingAlpha);
        
        /**
         * @brief Returns the layer selected by the user in the dynamic output choice.
         * @param ofxPlane Contains the plane name defined in nuke/fnOfxExtensions.h or the custom plane name defined in ofxNatron.h
         * @param ofxComponents Generally the same as ofxPlane except in the following cases:
            - for the motion vectors planes (e.g: kFnOfxImagePlaneBackwardMotionVector) 
         where the compoonents are in that case kFnOfxImageComponentMotionVectors. 
            - for the color plane (i.e: kFnOfxImagePlaneColour) where in that case the ofxComponents may be kOfxImageComponentAlpha or 
             kOfxImageComponentRGBA or kOfxImageComponentRGB or any other "default" ofx components supported on the clip.
         * 
         * If ofxPlane is empty but the function returned true that is because the choice is either kMultiPlaneParamOutputOption0 or kMultiPlaneParamOutputOption1
         * ofxComponents will have been set correctly to one of these values.
         **/
        bool getPlaneNeededInOutput(const std::list<std::string>& outputComponents,
                                    const OFX::Clip* dstClip,
                                    OFX::ChoiceParam* param,
                                    std::string* ofxPlane,
                                    std::string* ofxComponents);
        
        
        /**
         * @brief For each choice param, the list of clips it "depends on" (that is the clip layers that will be visible in the choice)
         * If the clips vector contains a single clip and this is the output clip then it is expected that param points to the kMultiPlaneParamOutputChannels
         * parameter.
         * We pass a pointer to a vector of clips because when calling buildChannelMenus(), there might be multiple choice params using the same clips.
         * This lets a chance to optimize the data copying since ClipComponentsInfo is quite a large struct.
         **/
        struct ChoiceParamClips
        {
            OFX::ChoiceParam* param;
            OFX::StringParam* stringparam;
            std::vector<ClipComponentsInfo>* clips;
        };
        
        /**
         * @brief Rebuild the given choice parameter depending on the clips components present. 
         * The only way to properly refresh the dynamic choice is when getClipPreferences is called.
         * If the componentsPresentCache member of each ClipComponentsInfo is set to a list stored in the plug-in, then
         * this function will first check if any change happened before rebuilding the menu, thus avoiding many complex API calls.
         **/
        void buildChannelMenus(const std::vector<ChoiceParamClips>& params);
        
        struct ChoiceStringParam
        {
            OFX::ChoiceParam* param;
            OFX::StringParam* stringParam;
        };
        
        /**
         * @brief This is called inside buildChannelMenus, but needs to be called in the constructor of the plug-in (in createInstanceAction)
         * because getClipPreferences may not be called at that time if not all mandatory inputs are connected.
         **/
        void setChannelsFromStringParams(const std::list<ChoiceStringParam>& params, bool allowReset);
        
        /**
         * @brief To be called in the changedParam action for each dynamic choice holding channels/layers info. This will synchronize the hidden string
         * parameter to reflect the value of the choice parameter (only if the reason is a user change).
         * @return Returns true if the param change was caught, false otherwise
         **/
        enum ChangedParamRetCode
        {
            eChangedParamRetCodeNoChange,
            eChangedParamRetCodeChoiceParamChanged,
            eChangedParamRetCodeStringParamChanged
        };
        ChangedParamRetCode checkIfChangedParamCalledOnDynamicChoice(const std::string& paramName, OFX::InstanceChangeReason reason, OFX::ChoiceParam* param, OFX::StringParam* stringparam);
        
        /**
         * @brief Add a dynamic choice parameter to select the output layer (in which the plug-in will render)
         **/
        OFX::ChoiceParamDescriptor* describeInContextAddOutputLayerChoice(bool addAllChoice, OFX::ImageEffectDescriptor &desc,OFX::PageParamDescriptor* page);
        
        /**
         * @brief Add a dynamic choice parameter to select a channel among possibly different source clips
         **/
        OFX::ChoiceParamDescriptor* describeInContextAddChannelChoice(OFX::ImageEffectDescriptor &desc,
                                                                      OFX::PageParamDescriptor* page,
                                                                      const std::vector<std::string>& clips,
                                                                      const std::string& name,
                                                                      const std::string& label,
                                                                      const std::string& hint);
        
        /**
         * @brief Add the standard R,G,B,A choices for the given clips. 
         * @param addConstants If true, it will also add the "0" and "1" choice to the list
         * @param options[out] If non-null fills the vector with the options that were appended to the param
         **/
        void addInputChannelOptionsRGBA(OFX::ChoiceParamDescriptor* param,
                                        const std::vector<std::string>& clips,
                                        bool addConstants,
                                        std::vector<std::string>* options);
        
        /**
         * @brief Same as above, but for a choice param instance
         **/
        void addInputChannelOptionsRGBA(OFX::ChoiceParam* param,
                                        const std::vector<std::string>& clips,
                                        bool addConstants,
                                        std::vector<std::string>* options);
        
    } // namespace MultiPlane
} // namespace OFX



#endif /* defined(openfx_supportext_ofxsMultiPlane_h) */
