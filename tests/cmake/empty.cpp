// uses the STATIC library (sx_static), so that its archive members are pulled in and
// their OpenGL references must be resolved by the propagated links
void sxTestDrawText();
void sxTestEmpty() { sxTestDrawText(); }
