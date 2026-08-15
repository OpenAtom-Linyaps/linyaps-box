// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/backend.h"
#include "linyaps_box/log/utils.h"
#include "linyaps_box/utils/file_describer.h"

namespace linyaps_box::log {

struct file_spec
{
    utils::file_descriptor fd;
};

class file_sink final : public sink
{
public:
    explicit file_sink(file_spec spec, output_format fmt);
    file_sink(file_sink &&) noexcept = default;
    file_sink &operator=(file_sink &&) noexcept = default;
    file_sink(const file_sink &) = delete;
    file_sink &operator=(const file_sink &) = delete;
    ~file_sink() noexcept = default;
    auto log(fmt::memory_buffer &buf, const log_context &ctx) const noexcept -> void final;

private:
    utils::file_descriptor fd;
    output_format format_;
};

} // namespace linyaps_box::log
