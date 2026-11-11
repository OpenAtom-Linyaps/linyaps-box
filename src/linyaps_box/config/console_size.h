// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/json/dsl_fwd.h"

namespace linyaps_box::config {

struct console_size
{
    unsigned short height;
    unsigned short width;
};

auto ops_of(json::object_tag<console_size>) -> const json::frame_ops &;
} // namespace linyaps_box::config
