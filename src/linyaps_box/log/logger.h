// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/backend.h"

#include <memory>
#include <variant>
#include <vector>

namespace linyaps_box::log {

// NOTE: The global logger state (level + backend) is NOT thread-safe.
// The runtime operates in a multi-process single-threaded model: each
// process (parent, child) has its own copy of the global state after
// clone/fork, and within each process all logging happens on one thread.
// If multi-threaded logging is needed in the future, a mutex or lock-free
// approach must be added here.
class global_logger
{
public:
    global_logger(const global_logger &) = delete;
    global_logger(global_logger &&) noexcept = delete;
    global_logger &operator=(const global_logger &) = delete;
    global_logger &operator=(global_logger &&) = delete;
    ~global_logger() noexcept = default;

    static auto instance() noexcept -> global_logger &;

    auto set_level(level) noexcept -> void;
    [[nodiscard]] auto get_level() const noexcept -> level;

    auto set_sinks(std::vector<std::unique_ptr<sink>> sinks) noexcept -> void;
    auto set_forwarder(std::unique_ptr<forwarder> fwd) noexcept -> void;
    auto unset_backend() noexcept -> void;

    auto dispatch_log(level lvl,
                      fmt::string_view fmt_str,
                      fmt::format_args args,
                      std::string_view file,
                      std::string_view function,
                      int line,
                      int errno_val = 0) const noexcept -> void;

    auto dispatch_context(const log_context &ctx) const noexcept -> void;

private:
    global_logger() noexcept;

    level level_{ level::warn };

    using backend =
      std::variant<std::monostate, std::vector<std::unique_ptr<sink>>, std::unique_ptr<forwarder>>;
    backend backend_;
};

} // namespace linyaps_box::log
