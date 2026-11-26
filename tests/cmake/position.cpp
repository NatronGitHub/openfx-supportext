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

    // an edit block that never throws (the Support library's EditBlock throws from its
    // destructor if the host fails paramEditEnd)
    class NoThrowEditBlock
    {
    public:
        NoThrowEditBlock(OFX::ParamSet& params, const char* name, bool cond)
            : _params(cond ? &params : NULL)
        {
            if (_params) {
                try {
                    _params->beginEditBlock(name);
                } catch (...) {
                    _params = NULL; // no block to end
                }
            }
        }

        ~NoThrowEditBlock()
        {
            if (_params) {
                try {
                    _params->endEditBlock();
                } catch (...) {
                }
            }
        }

        NoThrowEditBlock(const NoThrowEditBlock&) = delete;
        NoThrowEditBlock& operator=(const NoThrowEditBlock&) = delete;

    private:
        OFX::ParamSet* _params;
    };

    void setPointValue(int i, double time, const OfxPointD& p, bool firstChange, bool dragEnd) OVERRIDE
    {
        // the bookkeeping shares the value change's edit block
        NoThrowEditBlock block(*_effect, "Move point", firstChange || dragEnd);
        OFX::PointsInteract::setPointValue(i, time, p, firstChange, dragEnd);
        // ... the plug-in's bookkeeping, if firstChange || dragEnd
    }

    OFX::ImageEffect* _effect;
};

void
sxTestDescribePoints(OFX::ImageEffectDescriptor& desc)
{
    OFX::ofxsSetOverlayInteractDescriptorV2(desc, new OFX::PointsOverlayDescriptorV2<SxTestPointsInteract>);
    OFX::ofxsSetOverlayInteractDescriptorV2(desc, new OFX::PositionOverlayDescriptorV2<SxTestPositionParam>);
}
