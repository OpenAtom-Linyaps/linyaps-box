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

    std::string s;
    for (auto iterator = vec.begin(); iterator != std::prev(vec.end()); ++iterator) {
        s += *iterator + sep;
    }
    s += vec.back();
    return s;
}

strVec strSpilt(const std::string &s, const std::string &sep)
{
    strVec vec;
    size_t pos_begin = 0;
    size_t pos_end = 0;
    while ((pos_end = s.find(sep, pos_begin)) != std::string::npos) {
        auto t = s.substr(pos_begin, pos_end - pos_begin);
        if (!t.empty()) {
            vec.push_back(t);
        }
        pos_begin = pos_end + sep.size();
    }
    auto t = s.substr(pos_begin, s.size() - pos_begin);
    if (!t.empty()) {
        vec.push_back(t);
    }
    return vec;
}

} // namespace util
} // namespace linglong