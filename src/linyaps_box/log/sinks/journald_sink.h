// SPDX-FileCopyrightText: 2026 UnionCraft Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/utils.h"
#include "linyaps_box/utils/span.h"

#include <fmt/format.h>

#include <array>
#include <string>

#include <sys/uio.h>

namespace linyaps_box::log {

namespace detail {

template <typename T, typename = std::void_t<>>
struct has_valid_journald_backend : std::false_type
{
};

template <typename T>
struct has_valid_journald_backend<
  T,
  std::void_t<decltype(T::send(std::declval<utils::span<const struct iovec>>()))>>
    : std::bool_constant<noexcept(T::send(std::declval<utils::span<const struct iovec>>()))>
{
};

template <typename T>
constexpr inline bool has_valid_journald_backend_v = has_valid_journald_backend<T>::value;

} // namespace detail

struct journald_backend
{
    static auto send(utils::span<const struct iovec> iov) noexcept -> void;
};

template <typename Backend>
class basic_journald_sink
{
    static_assert(detail::has_valid_journald_backend_v<Backend>,
                  "Journald backend policy must implement a static "
                  "'send(utils::span<const struct iovec>) noexcept'");

    std::string ident_;

public:
    template <typename T>
    explicit basic_journald_sink(T spec) noexcept
        : ident_(std::move(spec.ident))
    {
    }

    basic_journald_sink(const basic_journald_sink &) = delete;
    basic_journald_sink(basic_journald_sink &&) noexcept = default;
    auto operator=(const basic_journald_sink &) -> basic_journald_sink & = delete;
    auto operator=(basic_journald_sink &&) noexcept -> basic_journald_sink & = default;
    ~basic_journald_sink() noexcept = default;

    auto log(const log_context &ctx) const noexcept -> void
    try {
        constexpr std::size_t max_fields =
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
          7
#else
          4
#endif
          ;

        fmt::memory_buffer buf;
        std::array<struct iovec, max_fields> iov{ };
        std::size_t n = 0;

        auto add_field = [&](std::string_view key, const auto &value) {
            const auto start = buf.size();
            fmt::format_to(std::back_inserter(buf), "{}={}", key, value);
            iov[n] = { buf.data() + start, buf.size() - start };
            ++n;
        };

        add_field("MESSAGE", ctx.msg);
        add_field("PRIORITY", to_syslog_priority(ctx.lvl));
        add_field("SYSLOG_IDENTIFIER", ident_);
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
        add_field("CODE_FILE", ctx.file);
        add_field("CODE_LINE", ctx.line);
        add_field("CODE_FUNC", ctx.function);
#endif
        add_field("ERRNO", ctx.errno_);

        Backend::send({ iov.data(), n });
    } catch (...) { // NOLINT
        // swallow
    }
};

using journald_sink = basic_journald_sink<journald_backend>;

struct journald_spec
{
    std::string ident;
};

} // namespace linyaps_box::log
