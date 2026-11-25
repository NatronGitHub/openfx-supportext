/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of openfx-supportext <https://github.com/NatronGitHub/openfx-supportext>,
 * (C) 2018-2021 The Natron Developers
 * (C) 2013-2018 INRIA
 *
 * openfx-supportext is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * openfx-supportext is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with openfx-supportext.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

/*
 * Keep the OpenMP runtime loaded after the plug-in is unloaded.
 *
 * When the host unloads (dlcloses) a plug-in binary, an OpenMP runtime whose only user is that
 * plug-in (e.g. libgomp) is unmapped too, while its pool threads, created by parallel regions run
 * in the host's render threads, may still be in its code: the host crashes (openfx-gmic #213).
 * Call ofxsPinOpenMPRuntime() before the first parallel region (at render entry, or right before
 * a parallel region): it is cheap and idempotent, and pins the runtime the first time.
 * - ELF (Linux, the BSDs): dlopen(RTLD_NOLOAD | RTLD_NODELETE) of the object that holds
 *   omp_get_max_threads: libgomp, or the plug-in itself if the runtime is linked statically.
 * - Windows: GetModuleHandleExW(FROM_ADDRESS | PIN) of the module that holds
 *   omp_get_max_threads: vcomp with MSVC (dllimport); with MinGW's libgomp (no dllimport) the
 *   address is an import thunk in the plug-in, which is then never unloaded (also safe).
 * - macOS: nothing (libomp joins its threads when it is unloaded; pinning would move that teardown
 *   to exit, see https://reviews.llvm.org/D159324).
 * - Without OpenMP (_OPENMP undefined): nothing.
 * The code is in ofxsOpenMP.cpp (cmake/SupportExt.cmake component OPENMP), compiled with the
 * plug-in's OpenMP flags, so that <windows.h> and <dlfcn.h> don't leak into the includer.
 */

#ifndef openfx_supportext_ofxsOpenMP_h
#define openfx_supportext_ofxsOpenMP_h

namespace OFX {
/// Keep the OpenMP runtime loaded once the plug-in is unloaded (see above). Returns false if it
/// could not be pinned (a warning is printed once), true otherwise, including when there is
/// nothing to do. Once per plug-in binary. Needs ofxsOpenMP.cpp in the plug-in (the OPENMP
/// component of cmake/SupportExt.cmake), or the link fails.
bool ofxsPinOpenMPRuntime();
} // namespace OFX

#endif // openfx_supportext_ofxsOpenMP_h
