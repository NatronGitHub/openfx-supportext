// test 4 of CMakeLists.txt: uses the STATIC library (functional_static), so that its archive
// members are pulled in and their OpenGL references must be resolved by the propagated links
void sxTestDrawText();
void sxTestEmpty() { sxTestDrawText(); }
