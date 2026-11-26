// a plug-in using the header-only PositionInteract and PointsInteract overlays
#include "ofxsPositionInteract.h"
struct SxTestPositionParam
{
    static const char* name() { return "position"; }
    static const char* interactiveName() { return NULL; }
};
OFX::Interact*
sxTestMakePositionInteract(OfxInteractHandle handle, OFX::ImageEffect* effect)
{
    return new OFX::PositionInteract<SxTestPositionParam>(handle, effect);
}

// two points configured at run time, one updating while dragged, one hidden by a condition
class SxTestPointsInteract
    : public OFX::PointsInteract
{
public:
    SxTestPointsInteract(OfxInteractHandle handle, OFX::ImageEffect* effect)
        : OFX::PointsInteract( handle, effect, points() )
        , _effect(effect)
    {
    }

private:
    static std::vector<OFX::PointsInteractPoint> points()
    {
        std::vector<OFX::PointsInteractPoint> p;
        p.push_back( OFX::PointsInteractPoint("point0") );
        p.back().colour.r = 1.;
        p.back().updateWhileDragging = true;
        p.push_back( OFX::PointsInteractPoint("point1") );
        p.back().label = "point1";
        p.back().size = 7.;

        return p;
    }

    bool isPointShown(int i, double /*time*/) const OVERRIDE { return i != 1 || pointCount() > 1; }

    void setPointValue(int i, double time, const OfxPointD& p, bool firstChange, bool dragEnd) OVERRIDE
    {
        // the bookkeeping shares the value change's edit block
        OFX::ParamSet::EditBlock block(*_effect, "Move point", firstChange || dragEnd);
        OFX::PointsInteract::setPointValue(i, time, p, firstChange, dragEnd);
    }

    OFX::ImageEffect* _effect;
};

void
sxTestDescribePoints(OFX::ImageEffectDescriptor& desc)
{
    OFX::ofxsSetOverlayInteractDescriptorV2(desc, new OFX::PointsOverlayDescriptorV2<SxTestPointsInteract>);
    OFX::ofxsSetOverlayInteractDescriptorV2(desc, new OFX::PositionOverlayDescriptorV2<SxTestPositionParam>);
}
