// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/log/logger.h"

#include "linyaps_box/log/macro.h"
#include "linyaps_box/log/sinks/stderr_sink.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/ostream.h>

#include <chrono>
#include <iostream>

#include <unistd.h>

namespace linyaps_box::log {

auto get_current_log_level() noexcept -> level
{
    return global_logger::instance().get_level();
}

auto dispatch(level lvl,
              fmt::string_view fmt_str,
              fmt::format_args args,
              std::string_view file,
              std::string_view function,
              int line) noexcept -> void
{
    global_logger::instance().dispatch_log(lvl, fmt_str, args, file, function, line);
}

auto dispatch(level lvl,
              int errno_val,
              fmt::string_view fmt_str,
              fmt::format_args args,
              std::string_view file,
              std::string_view function,
              int line) noexcept -> void
{
    global_logger::instance().dispatch_log(lvl, fmt_str, args, file, function, line, errno_val);
}

auto global_logger::instance() noexcept -> global_logger &
{
    static global_logger logger;
    return logger;
}

global_logger::global_logger() noexcept
{
    std::vector<std::unique_ptr<sink>> sinks;
    sinks.push_back(std::make_unique<stderr_sink>(stderr_spec{ }, output_format::text));
    backend_ = std::move(sinks);
}

auto global_logger::set_level(level lvl) noexcept -> void
{
    level_ = lvl;
}

auto global_logger::get_level() const noexcept -> level
{
    return level_;
}

auto global_logger::set_sinks(std::vector<std::unique_ptr<sink>> sinks) noexcept -> void
{
    backend_ = std::move(sinks);
}

auto global_logger::set_forwarder(std::unique_ptr<forwarder> fwd) noexcept -> void
{
    backend_ = std::move(fwd);
}

auto global_logger::unset_backend() noexcept -> void
{
    backend_ = std::monostate{ };
}

void global_logger::dispatch_log(level lvl,
                                 fmt::string_view fmt_str,
                                 fmt::format_args args,
                                 std::string_view file,
                                 std::string_view function,
                                 int line,
                                 int errno_val) const noexcept
{
    if (UNLIKELY(lvl > level_)) {
        return;
    }

    fmt::memory_buffer msg_buf;
    fmt::vformat_to(std::back_inserter(msg_buf), fmt_str, args);

    const log_context ctx{
        lvl,
        std::string_view{ msg_buf.data(), msg_buf.size() },
        std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now().time_since_epoch()),
        ::getpid(),
        errno_val,
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
        file,
        function,
        line,
#endif
    };

    std::visit(utils::Overload{
                 [](const std::monostate &) {
                     fmt::println(std::cerr, "logger uninitialized");
                     std::terminate();
                 },

                 [&](const std::vector<std::unique_ptr<sink>> &sinks) {
                     fmt::memory_buffer out_buf;
                     for (const auto &s : sinks) {
                         out_buf.clear();
                         s->log(out_buf, ctx);
                     }
                 },

                 [&](const std::unique_ptr<forwarder> &fwd) {
                     fwd->forward(ctx);
                 },
               },
               backend_);
}

void global_logger::dispatch_context(const log_context &ctx) const noexcept
{
    if (UNLIKELY(ctx.lvl > level_)) {
        return;
    }

    std::visit(utils::Overload{
                 [](const std::monostate &) {
                     fmt::println(std::cerr, "logger uninitialized");
                     std::terminate();
                 },

                 [&](const std::vector<std::unique_ptr<sink>> &sinks) {
                     fmt::memory_buffer out_buf;
                     for (const auto &s : sinks) {
                         out_buf.clear();
                         s->log(out_buf, ctx);
                     }
                 },

                 [](const std::unique_ptr<forwarder> &) {
                     fmt::println(std::cerr, "try to dispatch context through forwarder");
                     std::terminate();
                 },
               },
               backend_);
}

} // namespace linyaps_box::log
