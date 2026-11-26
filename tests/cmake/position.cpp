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
        , _showPoint1( effect->fetchBooleanParam("showPoint1") )
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

    // point1 is hidden while the showPoint1 checkbox is off
    bool isPointShown(int i, double time) const OVERRIDE { return i != 1 || _showPoint1->getValueAtTime(time); }

    // an edit block that never throws (before openfx 45578b69, the Support library's
    // EditBlock throws from its destructor if the host fails paramEditEnd)
    class NoThrowEditBlock
    {
    public:
        NoThrowEditBlock(OFX::ParamSet& params, const char* name)
            : _params(&params)
        {
            try {
                _params->beginEditBlock(name);
            } catch (...) {
                _params = NULL; // no block to end
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
        // every change gets an edit block, so the host can merge a drag into
        // one undo step; the bookkeeping shares the block of the first and the last change
        NoThrowEditBlock block(*_effect, "Move point");
        OFX::PointsInteract::setPointValue(i, time, p, firstChange, dragEnd);
        // ... the plug-in's bookkeeping, if firstChange || dragEnd
    }

    OFX::ImageEffect* _effect;
    OFX::BooleanParam* _showPoint1;
};

void
sxTestDescribePoints(OFX::ImageEffectDescriptor& desc)
{
    OFX::ofxsSetOverlayInteractDescriptorV2(desc, new OFX::PointsOverlayDescriptorV2<SxTestPointsInteract>);
    OFX::ofxsSetOverlayInteractDescriptorV2(desc, new OFX::PositionOverlayDescriptorV2<SxTestPositionParam>);
}
