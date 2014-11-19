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

#include "ofxsMipMap.h"

namespace OFX {
    
    // update the window of dst defined by nextRenderWindow by halving the corresponding area in src.
    // proofread and fixed by F. Devernay on 3/10/2014
    template <typename PIX,int nComponents>
    static void
    halveWindow(const OfxRectI& nextRenderWindow,
                const PIX* srcPixels,
                const OfxRectI& srcBounds,
                int srcRowBytes,
                PIX* dstPixels,
                const OfxRectI& dstBounds,
                int dstRowBytes)
    {
        int srcRowSize = srcRowBytes / sizeof(PIX);
        int dstRowSize = dstRowBytes / sizeof(PIX);
        
        const PIX* srcData =  srcPixels - (srcBounds.x1 * nComponents + srcRowSize * srcBounds.y1);
        PIX* dstData = dstPixels - (dstBounds.x1 * nComponents + dstRowSize * dstBounds.y1);
        
        assert(nextRenderWindow.x1 * 2 >= (srcBounds.x1 - 1) && (nextRenderWindow.x2-1) * 2 < srcBounds.x2 &&
               nextRenderWindow.y1 * 2 >= (srcBounds.y1 - 1) && (nextRenderWindow.y2-1) * 2 < srcBounds.y2);
        for (int y = nextRenderWindow.y1; y < nextRenderWindow.y2;++y) {
            
            const PIX* srcLineStart = srcData + y * 2 * srcRowSize;
            PIX* dstLineStart = dstData + y * dstRowSize;
            
            bool pickNextRow = (y * 2) < (srcBounds.y2 - 1);
            bool pickThisRow = (y * 2) >= (srcBounds.y1);
            int sumH = (int)pickNextRow + (int)pickThisRow;
            assert(sumH == 1 || sumH == 2);
            for (int x = nextRenderWindow.x1; x < nextRenderWindow.x2;++x) {
                
                bool pickNextCol = (x * 2) < (srcBounds.x2 - 1);
                bool pickThisCol = (x * 2) >= (srcBounds.x1);
                int sumW = (int)pickThisCol + (int)pickNextCol;
                assert(sumW == 1 || sumW == 2);
                for (int k = 0; k < nComponents; ++k) {
                    ///a b
                    ///c d
                    
                    PIX a = (pickThisCol && pickThisRow) ? srcLineStart[x * 2 * nComponents + k] : 0;
                    PIX b = (pickNextCol && pickThisRow) ? srcLineStart[(x * 2 + 1) * nComponents + k] : 0;
                    PIX c = (pickThisCol && pickNextRow) ? srcLineStart[(x * 2 * nComponents) + srcRowSize + k]: 0;
                    PIX d = (pickNextCol && pickNextRow) ? srcLineStart[(x * 2 + 1) * nComponents + srcRowSize + k] : 0;
                    
                    assert(sumW == 2 || (sumW == 1 && ((a == 0 && c == 0) || (b == 0 && d == 0))));
                    assert(sumH == 2 || (sumH == 1 && ((a == 0 && b == 0) || (c == 0 && d == 0))));
                    dstLineStart[x * nComponents + k] = (a + b + c + d) / (sumH * sumW);
                }
            }
        }
    }
    
    // update the window of dst defined by originalRenderWindow by mipmapping the windows of src defined by renderWindowFullRes
    // proofread and fixed by F. Devernay on 3/10/2014
    template <typename PIX,int nComponents>
    static void
    buildMipMapLevel(OFX::ImageEffect* instance,
                     const OfxRectI& originalRenderWindow,
                     const OfxRectI& renderWindowFullRes,
                     unsigned int level,
                     const PIX* srcPixels,
                     const OfxRectI& srcBounds,
                     int srcRowBytes,
                     PIX* dstPixels,
                     const OfxRectI& dstBounds,
                     int dstRowBytes)
    {
        assert(level > 0);
        
        std::auto_ptr<OFX::ImageMemory> mem;
        size_t memSize = 0;
        std::auto_ptr<OFX::ImageMemory> tmpMem;
        size_t tmpMemSize = 0;
        PIX* nextImg = NULL;
        
        const PIX* previousImg = srcPixels;
        OfxRectI previousBounds = srcBounds;
        int previousRowBytes = srcRowBytes;
        
        OfxRectI nextRenderWindow = renderWindowFullRes;
        
        ///Build all the mipmap levels until we reach the one we are interested in
        for (unsigned int i = 1; i < level; ++i) {
            // loop invariant:
            // - previousImg, previousBounds, previousRowBytes describe the data ate the level before i
            // - nextRenderWindow contains the renderWindow at the level before i
            //
            ///Halve the smallest enclosing po2 rect as we need to render a minimum of the renderWindow
            nextRenderWindow = downscalePowerOfTwoSmallestEnclosing(nextRenderWindow, 1);
#     ifdef DEBUG
            {
                // check that doing i times 1 level is the same as doing i levels
                OfxRectI nrw = downscalePowerOfTwoSmallestEnclosing(renderWindowFullRes, i);
                assert(nrw.x1 == nextRenderWindow.x1 && nrw.x2 == nextRenderWindow.x2 && nrw.y1 == nextRenderWindow.y1 && nrw.y2 == nextRenderWindow.y2);
            }
#     endif
            ///Allocate a temporary image if necessary, or reuse the previously allocated buffer
            int nextRowBytes =  (nextRenderWindow.x2 - nextRenderWindow.x1)  * nComponents * sizeof(PIX);
            size_t newMemSize =  (nextRenderWindow.y2 - nextRenderWindow.y1) * nextRowBytes;
            if (tmpMem.get()) {
                // there should be enough memory: no need to reallocate
                assert(tmpMemSize >= memSize);
            } else {
                tmpMem.reset(new OFX::ImageMemory(newMemSize, instance));
                tmpMemSize = newMemSize;
            }
            nextImg = (float*)tmpMem->lock();
            
            halveWindow<PIX, nComponents>(nextRenderWindow, previousImg, previousBounds, previousRowBytes, nextImg, nextRenderWindow, nextRowBytes);
            
            ///Switch for next pass
            previousBounds = nextRenderWindow;
            previousRowBytes = nextRowBytes;
            previousImg = nextImg;
            mem = tmpMem;
            memSize = tmpMemSize;
        }
        // here:
        // - previousImg, previousBounds, previousRowBytes describe the data ate the level before 'level'
        // - nextRenderWindow contains the renderWindow at the level before 'level'
        
        ///On the last iteration halve directly into the dstPixels
        ///The nextRenderWindow should be equal to the original render window.
        nextRenderWindow = downscalePowerOfTwoSmallestEnclosing(nextRenderWindow, 1);
        assert(originalRenderWindow.x1 == nextRenderWindow.x1 && originalRenderWindow.x2 == nextRenderWindow.x2 &&
               originalRenderWindow.y1 == nextRenderWindow.y1 && originalRenderWindow.y2 == nextRenderWindow.y2);
        
        halveWindow<PIX, nComponents>(nextRenderWindow, previousImg, previousBounds, previousRowBytes, dstPixels, dstBounds, dstRowBytes);
        // mem and tmpMem are freed at destruction
    }
    
    
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
                       int dstRowBytes)
    {
        assert(srcPixelData && dstPixelData);
        
        // do the rendering
        if (dstPixelDepth != OFX::eBitDepthFloat ||
            (dstPixelComponents != OFX::ePixelComponentRGBA &&
             dstPixelComponents != OFX::ePixelComponentRGB &&
             dstPixelComponents != OFX::ePixelComponentAlpha) ||
            dstPixelDepth != srcPixelDepth ||
            dstPixelComponents != srcPixelComponents) {
            OFX::throwSuiteStatusException(kOfxStatErrFormat);
        }
        
        if (dstPixelComponents == OFX::ePixelComponentRGBA) {
            buildMipMapLevel<float, 4>(instance, originalRenderWindow, renderWindow, levels, (const float*)srcPixelData,
                                       srcBounds, srcRowBytes, (float*)dstPixelData, dstBounds, dstRowBytes);
        } else if (dstPixelComponents == OFX::ePixelComponentRGB) {
            buildMipMapLevel<float, 3>(instance, originalRenderWindow, renderWindow, levels, (const float*)srcPixelData,
                                       srcBounds, srcRowBytes, (float*)dstPixelData, dstBounds, dstRowBytes);
        }  else if (dstPixelComponents == OFX::ePixelComponentAlpha) {
            buildMipMapLevel<float, 1>(instance, originalRenderWindow, renderWindow,levels, (const float*)srcPixelData,
                                       srcBounds, srcRowBytes, (float*)dstPixelData, dstBounds, dstRowBytes);
        } // switch
    }
    
    template <typename PIX,int nComponents>
    static void
    ofxsBuildMipMapsForComponents(OFX::ImageEffect* instance,
                                  const OfxRectI& renderWindow,
                                  const PIX* srcPixelData,
                                  const OfxRectI& srcBounds,
                                  int srcRowBytes,
                                  unsigned int maxLevel,
                                  MipMapsVector& mipmaps)
    {
        
        const PIX* previousImg = srcPixels;
        OfxRectI previousBounds = srcBounds;
        int previousRowBytes = srcRowBytes;
        
        OfxRectI nextRenderWindow = renderWindow;
        
        ///Build all the mipmap levels until we reach the one we are interested in
        for (unsigned int i = 1; i <= maxLevel; ++i) {
            // loop invariant:
            // - previousImg, previousBounds, previousRowBytes describe the data ate the level before i
            // - nextRenderWindow contains the renderWindow at the level before i
            //
            ///Halve the smallest enclosing po2 rect as we need to render a minimum of the renderWindow
            nextRenderWindow = downscalePowerOfTwoSmallestEnclosing(nextRenderWindow, 1);
#     ifdef DEBUG
            {
                // check that doing i times 1 level is the same as doing i levels
                OfxRectI nrw = downscalePowerOfTwoSmallestEnclosing(renderWindowFullRes, i);
                assert(nrw.x1 == nextRenderWindow.x1 && nrw.x2 == nextRenderWindow.x2 && nrw.y1 == nextRenderWindow.y1 && nrw.y2 == nextRenderWindow.y2);
            }
#     endif
            assert(i - 1 >= 0);

            ///Allocate a temporary image if necessary, or reuse the previously allocated buffer
            int nextRowBytes = (nextRenderWindow.x2 - nextRenderWindow.x1)  * nComponents * sizeof(PIX);
            mipmaps[i - 1].memSize = (nextRenderWindow.y2 - nextRenderWindow.y1) * nextRowBytes;
            mipmaps[i - 1].bounds = nextRenderWindow;
            
            mipmaps[i - 1].data = new OFX::ImageMemory(mipmaps[i - 1].memSize, instance);
            tmpMemSize = newMemSize;
        
            float* nextImg = (float*)tmpMem->lock();
            
            halveWindow<PIX, nComponents>(nextRenderWindow, previousImg, previousBounds, previousRowBytes, nextImg, nextRenderWindow, nextRowBytes);
            
            ///Switch for next pass
            previousBounds = nextRenderWindow;
            previousRowBytes = nextRowBytes;
            previousImg = nextImg;
        }
    }
    
    
    void
    ofxsBuildMipMaps(OFX::ImageEffect* instance,
                     const OfxRectI& renderWindow,
                     const void* srcPixelData,
                     OFX::PixelComponentEnum srcPixelComponents,
                     OFX::BitDepthEnum srcPixelDepth,
                     const OfxRectI& srcBounds,
                     int srcRowBytes,
                     unsigned int maxLevel,
                     MipMapsVector& mipmaps)
    {
        assert(srcPixelData && mipmaps->size() == maxLevel);
        
        // do the rendering
        if (srcPixelDepth != OFX::eBitDepthFloat ||
            (srcPixelComponents != OFX::ePixelComponentRGBA &&
             srcPixelComponents != OFX::ePixelComponentRGB &&
             srcPixelComponents != OFX::ePixelComponentAlpha)) {
            OFX::throwSuiteStatusException(kOfxStatErrFormat);
        }
        
        if (dstPixelComponents == OFX::ePixelComponentRGBA) {
            ofxsBuildMipMapsForComponents<float,4>(instance,renderWindow,srcPixelData,srcBounds,
                                          srcRowBytes,maxLevel,mipmaps);
        } else if (dstPixelComponents == OFX::ePixelComponentRGB) {
            ofxsBuildMipMapsForComponents<float,3>(instance,renderWindow,srcPixelData,srcBounds,
                                                   srcRowBytes,maxLevel,mipmaps);

        }  else if (dstPixelComponents == OFX::ePixelComponentAlpha) {
            ofxsBuildMipMapsForComponents<float,1>(instance,renderWindow,srcPixelData,srcBounds,
                                                   srcRowBytes,maxLevel,mipmaps);
        }

    }
    
} // OFX
