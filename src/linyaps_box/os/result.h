// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <fmt/format.h>
#include <zeus/expected.hpp>

#include <string>

namespace linyaps_box::os {

struct Err
{
    std::string msg;
    int err;
};

// if we update to C++ 23, just replace below
// alias template.

template <typename T>
using Result = zeus::expected<T, Err>;

// alias template CTAD available after C++ 20
// this is a workaround
template <typename E>
class unexpected : public zeus::unexpected<E>
{
    using zeus::unexpected<E>::unexpected;
};

template <typename E>
unexpected(E) -> unexpected<E>;

// Unwraps Result or throws system_error on failure.
// Used in mixed expected/exception codebases. Bypasses C++'s one-way template
// deduction limit in monadic `.or_else()` (which forces explicit annotations
// like `.or_else(throw_os_error<int>)`), restoring automatic type inference.
template <typename T>
auto throw_if_error(Result<T> &&res) -> T
{
    if (!res) {
        throw std::system_error(res.error().err, std::system_category(), res.error().msg);
    }

    return std::move(res).value();
}

} // namespace linyaps_box::os

template <>
struct fmt::formatter<linyaps_box::os::Err> : fmt::formatter<std::string>
{
    auto format(const linyaps_box::os::Err &error, fmt::format_context &ctx) const
    {
        return fmt::format_to(ctx.out(), "{}: {}", error.msg, ::strerror(error.err));
    }
};
