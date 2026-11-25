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
 * in the host's render threads, may still be in its code: the host crashes. Call
 * ofxsPinOpenMPRuntime() before the first parallel region (at render entry, or right before a
 * parallel region); it is cheap and idempotent, and pins the runtime the first time.
 * - ELF (Linux, BSD): dlopen(RTLD_NOLOAD | RTLD_NODELETE) of the runtime that holds
 *   omp_get_max_threads.
 * - Windows: GetModuleHandleExW(FROM_ADDRESS | PIN) of the module that holds omp_get_max_threads
 *   (with MSVC's import thunk, that is the plug-in itself, which is then never unloaded: also safe).
 * - macOS: nothing (libomp joins its threads when it is unloaded; pinning would move that teardown
 *   to exit, see https://reviews.llvm.org/D159324).
 * Without OpenMP (_OPENMP undefined), it does nothing. From openfx-gmic #213.
 */

#ifndef openfx_supportext_ofxsOpenMP_h
#define openfx_supportext_ofxsOpenMP_h

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
/// Keep the OpenMP runtime loaded once the plug-in is unloaded (see above). Returns false if it
/// could not be pinned (a warning is printed once), true otherwise, including when there is
/// nothing to do.
inline bool
ofxsPinOpenMPRuntime()
{
#if defined(_OPENMP) && !defined(__APPLE__)
    static const bool pinned = [] {
#ifdef _WIN32
        HMODULE h = NULL;
        const bool ok = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                                           reinterpret_cast<LPCWSTR>(&omp_get_max_threads), &h) != 0;
#else
        Dl_info info;
        const bool ok = ( dladdr(reinterpret_cast<void*>(&omp_get_max_threads), &info) != 0 ) && info.dli_fname &&
                        *info.dli_fname &&
                        ( dlopen(info.dli_fname, RTLD_LAZY | RTLD_NOLOAD | RTLD_NODELETE) != nullptr );
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

#endif // openfx_supportext_ofxsOpenMP_h
