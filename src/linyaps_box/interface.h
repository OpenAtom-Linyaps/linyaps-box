// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

namespace linyaps_box {

// prevent object slicing
class interface
{
public:
    interface() = default;
    virtual ~interface() = default;
    interface(const interface &) = delete;
    interface &operator=(const interface &) = delete;
    interface(interface &&) = delete;
    interface &operator=(interface &&) = delete;
};
} // namespace linyaps_box
