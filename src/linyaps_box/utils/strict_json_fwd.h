// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

// The project's JSON type.
//
// nlohmann::json converts numbers with an unchecked static_cast: number_float
// -> integer is undefined behaviour when the double is not representable
// (traps under UBSan; yields INT64_MIN-style garbage otherwise), and integer
// narrowing silently wraps around.  Upstream declined to change this (issue
// #288; PR #5207 closed unmerged): get<T>() is documented as cast semantics
// and range checking is the caller's responsibility.
//
// Instead of specializing the global adl_serializer -- a specialization is
// only visible in the translation units that include its header, so every
// such TU would have to opt in while the rest silently fell back to the
// unchecked primary template -- the checked behaviour is encoded in the JSON
// type itself: utils::strict_json is a distinct basic_json instantiation
// whose JSONSerializer (utils::detail::strict_serializer) routes every
// integer extraction through the checked conversion.
// Consequences:
//   - every extraction performed on utils::strict_json is checked, regardless of
//     include order or link order;
//   - passing a plain nlohmann::json where utils::strict_json is expected (e.g. to a
//     from_json overload) is a compile error, so the checked path cannot be
//     silently bypassed;
//   - only code that opts into utils::strict_json pays for it.

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace linyaps_box::utils::detail {

template <typename T, typename SFINAE = void>
struct strict_serializer;

} // namespace linyaps_box::utils::detail

namespace linyaps_box::utils {

using strict_json = nlohmann::basic_json<std::map,
                                         std::vector,
                                         std::string,
                                         bool,
                                         std::int64_t,
                                         std::uint64_t,
                                         double,
                                         std::allocator,
                                         detail::strict_serializer>;

} // namespace linyaps_box::utils
