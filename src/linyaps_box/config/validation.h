// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>

#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace linyaps_box::config {

template <typename T>
void validate_if_present(const std::optional<T> &value)
{
    if (value) {
        validate(*value);
    }
}

template <typename... Args>
void require(bool condition, fmt::format_string<Args...> message, Args &&...args)
{
    if (UNLIKELY(!condition)) {
        throw std::runtime_error(fmt::format(message, std::forward<Args>(args)...));
    }
}

// Lazy variant: the message is only produced when the condition fails, so
// expensive arguments are not evaluated on the happy path.
// The callable must return the (already formatted) message.
template <typename Fn, std::enable_if_t<std::is_invocable_v<Fn &>, int> = 0>
void require(bool condition, Fn &&format_on_failure)
{
    if (UNLIKELY(!condition)) {
        throw std::runtime_error(std::forward<Fn>(format_on_failure)());
    }
}

} // namespace linyaps_box::config
