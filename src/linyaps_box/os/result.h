// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <zeus/expected.hpp>

#include <string_view>
#include <system_error>

namespace linyaps_box::os {

// if we update to C++ 23, just replace below
// alias template.

template <typename T>
using Result = zeus::expected<T, std::error_code>;

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
        throw std::system_error(res.error());
    }

    return std::move(res).value();
}

template <typename T>
auto throw_if_error(Result<T> &&res, std::string_view context)
{
    if (!res) {
        throw std::system_error(res.error(), context.data());
    }

    return std::move(res).value();
}

inline auto make_error_code(int err) noexcept
{
    return std::error_code{ err, std::generic_category() };
}

} // namespace linyaps_box::os
