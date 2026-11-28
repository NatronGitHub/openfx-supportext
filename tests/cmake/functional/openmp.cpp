// calls OFX::ofxsPinOpenMPRuntime() (the OPENMP component, ofxsOpenMP.cpp), so that the module
// links only if that source was added (#222)
#include "ofxsOpenMP.h"
bool sxTestOpenMP() { return OFX::ofxsPinOpenMPRuntime(); }
