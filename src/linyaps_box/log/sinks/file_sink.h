// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/utils.h"
#include "linyaps_box/utils/file_describer.h"

namespace linyaps_box::log {

class file_sink;

struct file_spec
{
    utils::file_descriptor fd;
};

class file_sink
{
public:
    explicit file_sink(file_spec spec);
    file_sink(file_sink &&) noexcept = default;
    file_sink &operator=(file_sink &&) noexcept = default;
    file_sink(const file_sink &) = delete;
    file_sink &operator=(const file_sink &) = delete;
    ~file_sink() noexcept = default;
    auto log(const log_context &ctx) const noexcept -> void;

private:
    utils::file_descriptor fd;
};

} // namespace linyaps_box::log
