// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/utils.h"
#include "linyaps_box/utils/file_describer.h"

namespace linyaps_box::log {

class stderr_sink;

struct stderr_spec
{
};

class stderr_sink
{
public:
    explicit stderr_sink(stderr_spec spec) noexcept;
    stderr_sink(const stderr_sink &) = delete;
    stderr_sink(stderr_sink &&) noexcept = default;
    stderr_sink &operator=(const stderr_sink &) = delete;
    stderr_sink &operator=(stderr_sink &&) noexcept = default;
    ~stderr_sink() = default;

    auto log(const log_context &ctx) const -> void;

private:
    utils::file_descriptor stderr_fd;
    bool color;
};
} // namespace linyaps_box::log
