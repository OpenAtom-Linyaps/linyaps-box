/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "macro.h"
#include "common.h"
#include "logger.h"
#include "semaphore.h"
#include "filesystem.h"
#include "json.h"

#include <fstream>

namespace linglong {
namespace util {
namespace json {

inline nlohmann::json fromByteArray(const std::string &content)
{
    return nlohmann::json::parse(content);
}

inline nlohmann::json fromFile(const std::string &filepath)
{
    std::ifstream f(filepath);
    std::string str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto j = fromByteArray(str);
    return j;
}

} // namespace json
} // namespace util
} // namespace linglong