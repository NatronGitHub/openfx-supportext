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
 * OFX::ofxsPinOpenMPRuntime(), see ofxsOpenMP.h. In a source file, so that <windows.h> and
 * <dlfcn.h> don't leak into the plug-in's own code. Compiled with the plug-in's OpenMP flags
 * (cmake/SupportExt.cmake component OPENMP).
 */

#include "ofxsOpenMP.h"

#if defined(_OPENMP) && !defined(__APPLE__)
#include <omp.h>
#include <cstdio>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#endif

namespace OFX {
bool
ofxsPinOpenMPRuntime()
{
#if defined(_OPENMP) && !defined(__APPLE__)
    // a function-local static: once per plug-in binary, thread-safe initialisation
    static const bool pinned = [] {
#ifdef _WIN32
        // MSVC: omp_get_max_threads is dllimport, so this pins vcomp; MinGW libgomp (no dllimport)
        // or a static runtime: the address is in the plug-in, which is pinned instead (also safe)
        HMODULE h = NULL;
        const bool ok = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                                           reinterpret_cast<LPCWSTR>(&omp_get_max_threads), &h) != 0;
#elif defined(RTLD_NOLOAD) && defined(RTLD_NODELETE)
        // the object that holds the runtime (the plug-in itself if the runtime is static): one
        // more reference, deliberately never closed, and never unloaded
        Dl_info info;
        const bool ok = ( dladdr(reinterpret_cast<void*>(&omp_get_max_threads), &info) != 0 ) && info.dli_fname &&
                        *info.dli_fname &&
                        ( dlopen(info.dli_fname, RTLD_LAZY | RTLD_NOLOAD | RTLD_NODELETE) != nullptr );
#else
        const bool ok = false; // no RTLD_NODELETE (e.g. OpenBSD)
#endif
        if (!ok) {
            std::fprintf(stderr, "openfx-supportext: warning: could not keep the OpenMP runtime loaded: the host may crash when it unloads the plug-in\n");
        }

        return ok;
    }();

    return pinned;
#else

    return true;
#endif
}
} // namespace OFX
