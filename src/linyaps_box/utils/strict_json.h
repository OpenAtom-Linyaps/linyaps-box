// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

// Checked JSON number extraction for utils::strict_json.
//
// utils::strict_json (see strict_json_fwd.h) is a basic_json instantiation whose
// JSONSerializer, detail::strict_serializer, routes every plain integer
// extraction through utils::detail::get_number.  Because the hook is part of
// the JSON type, it covers every extraction path uniformly -- explicit
// get<T>()/get_to(), nlohmann's own container deserialization
// (std::vector<uint32_t>, std::map<string, uint32_t>, ...) and
// json::value(key, numeric_default) -- in every translation unit that uses
// utils::strict_json, with no reliance on include order and no global
// adl_serializer specialization.
//
// The one exception the serializer cannot cover is enum targets: an enum with
// a JSON representation defines its own from_json over utils::strict_json and
// is left to ADL; the single numeric enum field (user.umask), which has none,
// calls utils::detail::get_number explicitly

#include "linyaps_box/utils/strict_json_fwd.h"
#include "linyaps_box/utils/utils.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

namespace linyaps_box::utils {

namespace detail {

template <typename T>
constexpr bool is_json_integer_v =
  std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char>
  && !std::is_same_v<T, wchar_t> && !std::is_same_v<T, char16_t> && !std::is_same_v<T, char32_t>
#ifdef __cpp_char8_t
  && !std::is_same_v<T, char8_t>
#endif
  && (sizeof(T) <= 8);

// The checked conversion behind the serializer
template <typename T>
[[nodiscard]] auto get_number(const strict_json &j) -> T
{
    static_assert(is_json_integer_v<T>, "get_number<T> supports plain integer types only");

    auto number_out_of_range = [](const strict_json &j) {
        throw strict_json::type_error::create(302,
                                              "number out of range for the target integer type",
                                              &j);
    };

    if (j.is_number_float()) {
        const auto value = *j.template get_ptr<const strict_json::number_float_t *>();
        // Bound with exact powers of two, which a double represents exactly, so
        // the check does not depend on the width of long double.
        // max<T>() rounds up to 2^digits, hence the exclusive upper bound.
        constexpr auto digits = std::numeric_limits<T>::digits;
        const auto upper = std::ldexp(1.0, digits);
        const auto lower = std::is_signed_v<T> ? -upper : 0.0;
        if (UNLIKELY(!std::isfinite(value) || value < lower || value >= upper)) {
            number_out_of_range(j);
        }

        if (UNLIKELY(std::trunc(value) != value)) {
            throw strict_json::type_error::create(302, "number has a fractional part", &j);
        }

        return static_cast<T>(value);
    }

    if (j.is_number_unsigned()) {
        const auto value = *j.template get_ptr<const strict_json::number_unsigned_t *>();
        if (UNLIKELY(value > static_cast<std::uint64_t>((std::numeric_limits<T>::max)()))) {
            number_out_of_range(j);
        }

        return static_cast<T>(value);
    }

    if (j.is_number_integer()) {
        const auto value = *j.template get_ptr<const strict_json::number_integer_t *>();
        if (value < 0) {
            if constexpr (std::is_unsigned_v<T>) {
                number_out_of_range(j);
            } else if (UNLIKELY(value
                                < static_cast<std::int64_t>((std::numeric_limits<T>::min)()))) {
                number_out_of_range(j);
            }
        } else if (UNLIKELY(static_cast<std::uint64_t>(value)
                            > static_cast<std::uint64_t>((std::numeric_limits<T>::max)()))) {
            number_out_of_range(j);
        }

        return static_cast<T>(value);
    }

    throw strict_json::type_error::create(302,
                                          "type must be number, but is "
                                            + std::string{ j.type_name() },
                                          &j);
}

// JSONSerializer for utils::strict_json.
template <typename T, typename SFINAE>
struct strict_serializer : nlohmann::adl_serializer<T, SFINAE>
{
};

template <typename T>
struct strict_serializer<T, std::enable_if_t<is_json_integer_v<T>>>
{
    static auto from_json(const strict_json &j, T &v) -> void { v = get_number<T>(j); }

    static auto to_json(strict_json &j, T v) -> void
    {
        if constexpr (std::is_signed_v<T>) {
            j = strict_json(strict_json::value_t::number_integer);
            j.get_ref<strict_json::number_integer_t &>() =
              static_cast<strict_json::number_integer_t>(v);
        } else {
            j = strict_json(strict_json::value_t::number_unsigned);
            j.get_ref<strict_json::number_unsigned_t &>() =
              static_cast<strict_json::number_unsigned_t>(v);
        }
    }
};

} // namespace detail

auto require_object(const strict_json &j) -> void;

[[nodiscard]] auto strict_parse(std::string_view content) -> strict_json;

} // namespace linyaps_box::utils
