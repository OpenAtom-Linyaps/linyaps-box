// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/utils.h"

#include <fmt/format.h>

namespace linyaps_box::log {

class sink
{
public:
    sink() = default;
    virtual ~sink() noexcept = default;
    sink(const sink &) = delete;
    sink(sink &&) noexcept = default;
    sink &operator=(const sink &) = delete;
    sink &operator=(sink &&) noexcept = default;

    virtual auto log(fmt::memory_buffer &buf, const log_context &ctx) const noexcept -> void = 0;
};

class forwarder
{
public:
    forwarder() = default;
    virtual ~forwarder() noexcept = default;
    forwarder(const forwarder &) = delete;
    forwarder(forwarder &&) noexcept = default;
    forwarder &operator=(const forwarder &) = delete;
    forwarder &operator=(forwarder &&) noexcept = default;

    virtual auto forward(const log_context &ctx) noexcept -> void = 0;
};

} // namespace linyaps_box::log
