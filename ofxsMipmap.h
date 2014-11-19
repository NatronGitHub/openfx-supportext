/*
 OFX mipmapping help functions
 
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

#ifndef _OFXS_MIPMAP_H
#define _OFXS_MIPMAP_H

#include <cmath>
#include <cassert>
#include <vector>

#include "ofxsImageEffect.h"

namespace OFX {
    
    void
    ofxsScalePixelData(OFX::ImageEffect* instance,
                       const OfxRectI& originalRenderWindow,
                       const OfxRectI& renderWindow,
                       unsigned int levels,
                       const void* srcPixelData,
                       OFX::PixelComponentEnum srcPixelComponents,
                       OFX::BitDepthEnum srcPixelDepth,
                       const OfxRectI& srcBounds,
                       int srcRowBytes,
                       void* dstPixelData,
                       OFX::PixelComponentEnum dstPixelComponents,
                       OFX::BitDepthEnum dstPixelDepth,
                       const OfxRectI& dstBounds,
                       int dstRowBytes);
    
    struct MipMap {
        std::size_t memSize;
        OFX::ImageMemory* data;
        OfxRectI bounds;
        
        MipMap()
        : memSize(0)
        , data(0)
        , bounds()
        {
            
        }
        
        ~MipMap()
        {
            delete data;
            data = 0;
        }
    };
    
    //Contains all levels of details > 0, sort by decreasing LoD
    typedef std::vector<MipMap> MipMapsVector;
    
    /**
      @brief Given the original image, this function builds all mipmap levels
      up to maxLevel and stores them in the mipmaps vector, in decreasing LoD.
      The original image will not be stored in the mipmaps vector.
      @param mipmaps[out] The mipmaps vector should contains at least maxLevel
      entries
     **/
    void
    ofxsBuildMipMaps(OFX::ImageEffect* instance,
                     const OfxRectI& renderWindow,
                     const void* srcPixelData,
                     OFX::PixelComponentEnum srcPixelComponents,
                     OFX::BitDepthEnum srcPixelDepth,
                     const OfxRectI& srcBounds,
                     int srcRowBytes,
                     unsigned int maxLevel,
                     MipMapsVector& mipmaps);

} // OFX

#endif
