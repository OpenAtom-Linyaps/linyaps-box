/*
 * Copyright (c) 2021-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common.h"

namespace linglong {
namespace util {

std::string strVecJoin(const strVec &vec, char sep)
{
    if (vec.empty()) {
        return "";
    }

    std::string str;
    for (auto iterator = vec.begin(); iterator != std::prev(vec.end()); ++iterator) {
        str += *iterator + sep;
    }
    str += vec.back();
    return str;
}

strVec strSpilt(const std::string &str, const std::string &sep)
{
    strVec vec;
    size_t pos_begin = 0;
    size_t pos_end = 0;
    while ((pos_end = str.find(sep, pos_begin)) != std::string::npos) {
        auto t = str.substr(pos_begin, pos_end - pos_begin);
        if (!t.empty()) {
            vec.push_back(t);
        }
        pos_begin = pos_end + sep.size();
    }
    auto t = str.substr(pos_begin, str.size() - pos_begin);
    if (!t.empty()) {
        vec.push_back(t);
    }
    return vec;
}

} // namespace util
} // namespace linglong