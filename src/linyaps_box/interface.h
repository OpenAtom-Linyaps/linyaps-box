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
    virtual ~interface();
    interface(const interface &) = delete;
    auto operator=(const interface &) -> interface & = delete;
    interface(interface &&) = delete;
    auto operator=(interface &&) -> interface & = delete;
};
} // namespace linyaps_box
