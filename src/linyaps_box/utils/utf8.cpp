// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/utf8.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace linyaps_box::utils {

namespace {

constexpr auto is_continuation(unsigned char byte) noexcept -> bool
{
    return (byte & 0xC0U) == 0x80U;
}

auto clean_run_end(std::string_view s, std::size_t from) noexcept -> std::size_t
{
    constexpr auto word_size = sizeof(std::uint64_t);
    constexpr std::uint64_t high_bits = 0x8080808080808080ULL;
    constexpr std::uint64_t ones = 0x0101010101010101ULL;

    const auto *bytes = reinterpret_cast<const std::byte *>(s.data());
    auto i = from;

    while (i + word_size <= s.size()) {
        std::uint64_t chunk{ };
        std::memcpy(&chunk, bytes + i, word_size);
        if (((chunk & high_bits) | ((chunk - ones) & ~chunk & high_bits)) != 0) {
            break;
        }

        i += word_size;
    }

    while (i < s.size() && static_cast<unsigned char>(s[i]) < 0x80U && s[i] != '\0') {
        ++i;
    }

    return i;
}

struct sequence
{
    std::size_t length; // bytes in the sequence, or in its maximal subpart
    bool well_formed;
};

auto decode_sequence(std::string_view s, std::size_t i) noexcept -> sequence
{
    const auto b0 = static_cast<unsigned char>(s[i]);

    if (b0 < 0x80U) {
        return { 1, true };
    }

    if (b0 >= 0xC2U && b0 <= 0xDFU) { // 2-byte sequence
        if (i + 1 >= s.size() || !is_continuation(static_cast<unsigned char>(s[i + 1]))) {
            return { 1, false };
        }

        return { 2, true };
    }

    if (b0 >= 0xE0U && b0 <= 0xEFU) { // 3-byte sequence
        if (i + 1 >= s.size()) {
            return { 1, false };
        }

        const auto b1 = static_cast<unsigned char>(s[i + 1]);
        if (!is_continuation(b1) || (b0 == 0xE0U && b1 < 0xA0U) || (b0 == 0xEDU && b1 > 0x9FU)) {
            return { 1, false }; // overlong encoding or UTF-16 surrogate
        }

        if (i + 2 >= s.size()) {
            return { 2, false };
        }

        if (!is_continuation(static_cast<unsigned char>(s[i + 2]))) {
            return { 2, false };
        }

        return { 3, true };
    }

    if (b0 >= 0xF0U && b0 <= 0xF4U) { // 4-byte sequence
        if (i + 1 >= s.size()) {
            return { 1, false };
        }

        const auto b1 = static_cast<unsigned char>(s[i + 1]);
        if (!is_continuation(b1) || (b0 == 0xF0U && b1 < 0x90U) || (b0 == 0xF4U && b1 > 0x8FU)) {
            return { 1, false }; // overlong encoding or code point above U+10FFFF
        }

        if (i + 2 >= s.size()) {
            return { 2, false };
        }

        if (!is_continuation(static_cast<unsigned char>(s[i + 2]))) {
            return { 2, false };
        }

        if (i + 3 >= s.size()) {
            return { 3, false };
        }

        if (!is_continuation(static_cast<unsigned char>(s[i + 3]))) {
            return { 3, false };
        }

        return { 4, true };
    }

    // 0x80..0xC1 (lone continuation byte or overlong lead) and 0xF5..0xFF.
    return { 1, false };
}

} // namespace

auto is_valid_utf8(std::string_view input, nul_rule rule) noexcept -> bool
{
    for (std::size_t i = 0; i < input.size();) {
        i = clean_run_end(input, i);
        if (i == input.size()) {
            break;
        }

        if (input[i] == '\0') {
            if (rule == nul_rule::forbidden) {
                return false;
            }

            ++i; // well-formed, just not plain ASCII
            continue;
        }

        const auto seq = decode_sequence(input, i);
        if (!seq.well_formed) {
            return false;
        }

        i += seq.length;
    }

    return true;
}

auto to_valid_utf8_lossy(std::string_view input, std::string &buffer, nul_rule rule) -> void
{
    static constexpr std::string_view replacement = "\xEF\xBF\xBD"; // U+FFFD
    static constexpr std::string_view nul_escape = "\\u0000";

    buffer.clear();
    buffer.reserve(input.size());

    std::size_t i = 0;
    while (i < input.size()) {
        const auto run_begin = i;
        std::size_t subpart_length = 0;
        bool at_nul = false;
        while (true) {
            i = clean_run_end(input, i);
            if (i == input.size()) {
                break;
            }

            if (input[i] == '\0') {
                at_nul = true;
                break;
            }

            const auto seq = decode_sequence(input, i);
            if (!seq.well_formed) {
                subpart_length = seq.length;
                break;
            }

            i += seq.length;
        }

        buffer.append(input.data() + run_begin, i - run_begin);
        if (i == input.size()) {
            break;
        }

        if (at_nul) {
            if (rule == nul_rule::forbidden) {
                buffer.append(nul_escape);
            } else {
                buffer.push_back('\0');
            }

            ++i;
        } else {
            buffer.append(replacement);
            i += subpart_length;
        }
    }
}

auto to_valid_utf8_lossy(std::string_view input, nul_rule rule) -> std::string
{
    std::string buffer;
    to_valid_utf8_lossy(input, buffer, rule);
    return buffer;
}

} // namespace linyaps_box::utils
