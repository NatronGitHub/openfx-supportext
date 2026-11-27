// compiles and instantiates the helpers shared with openfx-misc (#231)
#include "ofxsCoords.h"
#include "ofxsMultiView.h"
#include "ofxsNonFinite.h"
#include "ofxsTransformInteract.h"

float sxTestSamples(unsigned char c, float f)
{
    return OFX::sampleToFloat<unsigned char, 255>(c) + OFX::finiteSampleToFloat<float, 1>(f);
}

OfxRectD sxTestFormat(const OFX::Clip* clip, const OFX::ImageEffect& effect)
{
    return OFX::Coords::clipFormatCanonical(clip, effect);
}

OFX::Image* sxTestFetchView(OFX::Clip* clip, double time, int view)
{
    return OFX::fetchViewImage(clip, time, view);
}

void sxTestDenormalise(OFX::ImageEffect& effect, OFX::BooleanParam* centerChanged)
{
    OFX::ofxsTransformDenormaliseDefaults(effect, true, centerChanged);
}

#include "ofxsSoftEdge.h"

double sxTestSoftEdge(double t)
{
    return OFX::ofxsSmoothstep(t) + OFX::ofxsFalloff(OFX::OfxsFalloffEnum::eSmooth0, t)
           + OFX::ofxsSoftEdgeWeight(OFX::OfxsFalloffEnum::eQuadratic, t, 1., 0.5);
}
