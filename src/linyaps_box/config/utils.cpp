// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/utils.h"

#include "linyaps_box/io/stream.h"
#include "linyaps_box/os/fs.h"

#include <charconv>

namespace linyaps_box::config {

auto read_json_to(const std::filesystem::path &path, utils::uninit_vector<std::byte> &buffer)
  -> std::size_t
{
    auto fd = os::open(path, { os::sys::open_flag::cloexec, os::sys::access_mode::read_only });
    if (UNLIKELY(!fd)) {
        throw std::filesystem::filesystem_error("failed to open config file", path, fd.error());
    }

    auto stat = os::fstat(*fd);
    if (UNLIKELY(!stat)) {
        throw std::system_error(stat.error(), "failed to stat config file");
    }

    auto type = os::to_fs_file_type(stat->st_mode);
    if (UNLIKELY(type != std::filesystem::file_type::regular)) {
        throw std::runtime_error("config file is not a regular file");
    }

    auto ret = io::read_to_end(fd.value(), buffer);
    if (UNLIKELY(!ret)) {
        throw std::system_error(ret.error(), "failed to read config file");
    }

    return *ret;
}

auto parse_range_list(std::string_view s) -> std::vector<unsigned int>
{
    std::vector<unsigned int> result;
    if (s.empty()) {
        return result;
    }

    result.reserve(16);

    const auto *ptr = s.data();
    const auto *end = ptr + s.size();

    auto skip_whitespace = [](const char *p, const char *e) {
        while (p < e && static_cast<unsigned char>(*p) <= ' ') {
            ++p;
        }

        return p;
    };

    while (ptr < end) {
        ptr = skip_whitespace(ptr, end);
        if (ptr == end) {
            break;
        }

        auto start{ 0U };
        auto [p_start, ec_start] = std::from_chars(ptr, end, start);
        if (UNLIKELY(ec_start != std::errc{ })) {
            if (ec_start == std::errc::result_out_of_range) {
                throw std::runtime_error(
                  "value overflow in range list at: "
                  + std::string(ptr, std::min(end - ptr, static_cast<std::ptrdiff_t>(16))));
            }

            throw std::runtime_error(
              "invalid value in range list at: "
              + std::string(ptr, std::min(end - ptr, static_cast<std::ptrdiff_t>(16))));
        }

        ptr = p_start;
        ptr = skip_whitespace(ptr, end);

        if (ptr < end && *ptr == '-') {
            ++ptr;
            ptr = skip_whitespace(ptr, end);

            auto finish{ 0U };
            auto [p_finish, ec_finish] = std::from_chars(ptr, end, finish);
            if (UNLIKELY(ec_finish != std::errc{ })) {
                if (ec_finish == std::errc::result_out_of_range) {
                    throw std::runtime_error(
                      "value overflow in range list at: "
                      + std::string(ptr, std::min(end - ptr, static_cast<std::ptrdiff_t>(16))));
                }
                throw std::runtime_error(
                  "invalid range end in range list at: "
                  + std::string(ptr, std::min(end - ptr, static_cast<std::ptrdiff_t>(16))));
            }

            ptr = p_finish;
            if (UNLIKELY(finish < start)) {
                throw std::runtime_error(
                  "invalid range in range list (finish < start) at: "
                  + std::string(ptr, std::min(end - ptr, static_cast<std::ptrdiff_t>(16))));
            }

            if (UNLIKELY(start == 0 && finish == std::numeric_limits<unsigned int>::max())) {
                throw std::runtime_error(
                  "range too large in range list at: "
                  + std::string(ptr, std::min(end - ptr, static_cast<std::ptrdiff_t>(16))));
            }

            auto n = finish - start + 1;
            auto pos = result.size();
            result.resize(pos + n);
            for (unsigned int j = 0; j < n; ++j) {
                result[pos + j] = start + j;
            }
        } else {
            result.push_back(start);
        }

        ptr = skip_whitespace(ptr, end);
        if (ptr < end) {
            if (UNLIKELY(*ptr != ',')) {
                throw std::runtime_error(
                  "expected ',' or end-of-string in range list at: "
                  + std::string(ptr, std::min(end - ptr, static_cast<std::ptrdiff_t>(16))));
            }

            ++ptr;
        }
    }

    return result;
}

} // namespace linyaps_box::config
