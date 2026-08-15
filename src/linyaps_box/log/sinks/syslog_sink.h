// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/backend.h"
#include "linyaps_box/log/formatter.h"
#include "linyaps_box/log/utils.h"

#include <fmt/format.h>

#include <string>
#include <string_view>

namespace linyaps_box::log {

namespace detail {

template <typename T, typename = std::void_t<>>
struct has_valid_syslog_backend : std::false_type
{
};

template <typename T>
struct has_valid_syslog_backend<T,
                                std::void_t<decltype(std::declval<const T &>().syslog(
                                  std::declval<level>(), std::declval<std::string_view>()))>>
    : std::bool_constant<noexcept(
        std::declval<const T &>().syslog(std::declval<level>(), std::declval<std::string_view>()))>
{
};

template <typename T>
constexpr inline bool has_valid_syslog_backend_v = has_valid_syslog_backend<T>::value;

} // namespace detail

class syslog_backend
{
public:
    explicit syslog_backend(std::string ident) noexcept;
    syslog_backend(syslog_backend &&other) noexcept;
    auto operator=(syslog_backend &&other) noexcept -> syslog_backend & = delete;
    syslog_backend(const syslog_backend &) = delete;
    auto operator=(const syslog_backend &) -> syslog_backend & = delete;
    ~syslog_backend() noexcept;

    auto syslog(level lvl, std::string_view msg) const noexcept -> void;

private:
    auto openlog() const noexcept -> void;
    std::string ident_;
    bool opened;
};

template <typename Backend>
class basic_syslog_sink : public sink
{
    static_assert(detail::has_valid_syslog_backend_v<Backend>,
                  "Syslog backend policy must implement 'syslog(level, std::string_view)'");

    Backend backend_;
    bool cee_{ false };
    output_format format_;

public:
    template <typename T>
    explicit basic_syslog_sink(T spec, output_format fmt) noexcept
        : backend_(std::move(spec.ident))
        , cee_(spec.cee)
        , format_(fmt)
    {
    }

    basic_syslog_sink(basic_syslog_sink &&) noexcept = default;
    basic_syslog_sink(const basic_syslog_sink &) = delete;
    auto operator=(const basic_syslog_sink &) -> basic_syslog_sink & = delete;
    auto operator=(basic_syslog_sink &&) noexcept -> basic_syslog_sink & = delete;
    ~basic_syslog_sink() noexcept = default;

    auto &backend() const noexcept { return backend_; }

    auto log(fmt::memory_buffer &buf, const log_context &ctx) const noexcept -> void final
    try {
        if (format_ == output_format::json && cee_) {
            buf.append(fmt::string_view("@cee: "));
        }

        format_log(buf, ctx, format_, { });
        backend_.syslog(ctx.lvl, std::string_view{ buf.data(), buf.size() });
    } catch (...) { // NOLINT
        // swallow
    }
};

using syslog_sink = basic_syslog_sink<syslog_backend>;

struct syslog_spec
{
    std::string ident;
    bool cee{ false };
};

} // namespace linyaps_box::log
