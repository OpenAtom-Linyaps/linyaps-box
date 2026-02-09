// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

namespace linyaps_box {
class unixSocketClient : public linyaps_box::utils::file_descriptor
{
public:
    explicit unixSocketClient(linyaps_box::utils::file_descriptor socket);

    static unixSocketClient connect(const std::filesystem::path &path);

    unixSocketClient(const unixSocketClient &) = delete;
    auto operator=(const unixSocketClient &) -> unixSocketClient & = delete;

    unixSocketClient(unixSocketClient &&other) noexcept = default;
    auto operator=(unixSocketClient &&other) noexcept -> unixSocketClient & = default;

    ~unixSocketClient() override = default;

    auto send_fd(utils::file_descriptor &&fd, std::string_view payload = {}) const -> void;

    [[nodiscard]] auto recv_fd(std::string &payload) const -> utils::file_descriptor;
};
} // namespace linyaps_box
