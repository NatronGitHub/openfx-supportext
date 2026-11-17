// a plug-in using the header-only PositionInteract overlay
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
