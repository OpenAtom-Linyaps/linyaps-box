// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstdint>
#include <exception>
#include <type_traits>
#include <utility>

// Type constraints for defer, the cleanup function should not throw exceptions
template<typename Fn>
constexpr bool compatible_defer = std::is_nothrow_invocable_r_v<void, Fn>;

// Execution policies for defer
enum class defer_policy : std::uint8_t {
    always,  // Always execute the cleanup function
    on_error // Execute the cleanup function only when an exception is active
};

// defer executes the function based on the specified policy when the object is destroyed
template<typename Fn,
         defer_policy Policy = defer_policy::always,
         std::enable_if_t<compatible_defer<Fn>, bool> = true>
struct defer
{
    explicit defer(Fn &&fn) noexcept
        : fn(std::move(fn))
        , cancelled(false)
    {
    }

    defer(const defer &) = delete;
    defer &operator=(const defer &) = delete;

    defer(defer &&other) noexcept
        : fn(std::move(other.fn))
        , cancelled(other.cancelled)
    {
        other.cancelled = true;
    }

    defer &operator=(defer &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        if (!cancelled) {
            execute();
        }

        fn = std::move(other.fn);
        cancelled = other.cancelled;
        other.cancelled = true;

        return *this;
    }

    ~defer() noexcept
    {
        if (!cancelled) {
            execute();
        }
    }

    void cancel() noexcept { cancelled = true; }

    [[nodiscard]] bool is_cancelled() const noexcept { return cancelled; }

private:
    void execute() noexcept
    {
        if constexpr (Policy == defer_policy::always) {
            fn();
        } else if constexpr (Policy == defer_policy::on_error) {
            if (std::uncaught_exceptions() > 0) {
                fn();
            }
        }
    }

    Fn fn;
    bool cancelled;
};

// Helper functions to create defer objects
template<typename Fn>
auto make_defer(Fn &&fn) noexcept
{
    return defer<std::decay_t<Fn>>(std::forward<Fn>(fn));
}

// This project use exception to indicate the failure of a function, so we provide a way to create
// a defer object that executes the cleanup function only when an exception is active during
// destruction
template<typename Fn>
auto make_errdefer(Fn &&fn) noexcept
{
    return defer<std::decay_t<Fn>, defer_policy::on_error>(std::forward<Fn>(fn));
}
