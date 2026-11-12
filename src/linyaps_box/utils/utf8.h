// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace linyaps_box::utils {

// Whether the consumer of the text can carry an embedded NUL: it is well-formed
// UTF-8, but truncates a field for a consumer that reads it as a C string.
enum class nul_rule : std::uint8_t {
    allowed,   // copied unchanged
    forbidden, // escapes it as \u0000
};

// True when every byte sequence in `input` is well-formed UTF-8 and,
// when `rule` forbids it, `input` holds no NUL.
[[nodiscard]] auto is_valid_utf8(std::string_view input, nul_rule rule = nul_rule::allowed) noexcept
  -> bool;

// Replace every ill-formed sequence with U+FFFD, and each NUL per `rule`.
// `buffer` receives the result and must not overlap `input`.
auto to_valid_utf8_lossy(std::string_view input,
                         std::string &buffer,
                         nul_rule rule = nul_rule::allowed) -> void;

[[nodiscard]] auto to_valid_utf8_lossy(std::string_view input, nul_rule rule = nul_rule::allowed)
  -> std::string;

} // namespace linyaps_box::utils
