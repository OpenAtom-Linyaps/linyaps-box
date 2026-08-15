// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/backend.h"
#include "linyaps_box/log/utils.h"
#include "linyaps_box/utils/file_describer.h"

namespace linyaps_box::log {

struct stderr_spec
{
};

class stderr_sink final : public sink
{
public:
    explicit stderr_sink(stderr_spec spec, output_format fmt) noexcept;
    stderr_sink(const stderr_sink &) = delete;
    stderr_sink(stderr_sink &&) noexcept = default;
    stderr_sink &operator=(const stderr_sink &) = delete;
    stderr_sink &operator=(stderr_sink &&) noexcept = default;
    ~stderr_sink() noexcept = default;

    auto log(fmt::memory_buffer &buf, const log_context &ctx) const noexcept -> void final;

private:
    utils::file_descriptor stderr_fd;
    output_format format_;
    bool color;
};
} // namespace linyaps_box::log
