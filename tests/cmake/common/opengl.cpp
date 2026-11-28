// loads OpenGL through SupportExt (glad, and EGL on Linux)
#include "ofxsOGLUtilities.h"
bool sxTestLoadGL() { return OFX::ofxsLoadOpenGLOnce(); }
