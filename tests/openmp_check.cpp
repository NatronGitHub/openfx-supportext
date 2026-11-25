// Compile check of ofxsOpenMP.h with OpenMP enabled (the top-level CMakeLists.txt builds it when
// CMake finds OpenMP): the ELF and Windows paths of ofxsPinOpenMPRuntime().
#include "ofxsOpenMP.h"

bool
supportextOpenMPCheck()
{
    const bool pinned = OFX::ofxsPinOpenMPRuntime();
    int n = 0;
#pragma omp parallel reduction(+:n)
    n += 1;

    return pinned && (n > 0);
}
