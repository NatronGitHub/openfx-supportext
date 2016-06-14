/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of openfx-supportext <https://github.com/devernay/openfx-supportext>,
 * Copyright (C) 2013-2016 INRIA
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
 * OFX utiliy functions to open a file safely with UTF-8 encoded strings.
 */

#include "ofxsFileOpen.h"

#if defined(_WIN32)
#include <windows.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif


namespace OFX {

#ifdef _WIN32
std::wstring utf8_to_utf16 (const std::string& str)
{
    std::wstring native;

    native.resize(MultiByteToWideChar (CP_UTF8, 0, str.c_str(), -1, NULL, 0));
    MultiByteToWideChar (CP_UTF8, 0, str.c_str(), -1, &native[0], (int)native.size());

    return native;
}

std::string utf16_to_utf8 (const std::wstring& str)
{
    std::string utf8;

    utf8.resize(WideCharToMultiByte (CP_UTF8, 0, str.c_str(), -1, NULL, 0, NULL, NULL));
    WideCharToMultiByte (CP_UTF8, 0, str.c_str(), -1, &utf8[0], (int)utf8.size(), NULL, NULL);
        
    return utf8;
}
#endif

std::FILE* open_file(const std::string& path, const std::string& mode)
{
#ifdef _WIN32
    // on Windows fopen does not accept UTF-8 paths, so we convert to wide char
    std::wstring wpath = utf8_to_utf16 (path);
    std::wstring wmode = utf8_to_utf16 (mode);

    return ::_wfopen (wpath.c_str(), wmode.c_str());
#else
    // on Unix platforms passing in UTF-8 works
    return std::fopen (path.c_str(), mode.c_str());
#endif
}


} // namespace OFX
