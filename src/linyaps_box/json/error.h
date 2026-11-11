// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <stdexcept>

namespace linyaps_box::json {

class decode_error : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

} // namespace linyaps_box::json
