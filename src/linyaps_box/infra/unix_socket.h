// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/socket.h"
#include "linyaps_box/utils/file_describer.h"

#include <filesystem>
#include <vector>

#include <sys/socket.h>

namespace linyaps_box::infra {

inline constexpr std::size_t kMaxScmFds = 16;

class unix_socket
{
public:
    static unix_socket connect(const std::filesystem::path &path);

    unix_socket(const unix_socket &) = delete;
    auto operator=(const unix_socket &) -> unix_socket & = delete;

    unix_socket(unix_socket &&other) noexcept = default;
    auto operator=(unix_socket &&other) noexcept -> unix_socket & = default;

    ~unix_socket() = default;

    auto release() & -> int { return fd_.release(); }

    auto close() & -> void { fd_.close(); }

    [[nodiscard]] auto fd() const & noexcept -> const utils::file_descriptor & { return fd_; }

    [[nodiscard]] auto fd() const && = delete;

    auto send(utils::span<const std::byte> data) const -> void;

    auto send_fd(const utils::file_descriptor &fd) const -> void;

    [[nodiscard]] auto recv_fd() const -> utils::file_descriptor;

    auto send_data_with_fds(utils::span<const std::byte> data,
                            utils::span<const utils::file_descriptor> fds) const -> void;

    [[nodiscard]] auto recv_data_with_fds(utils::span<std::byte> data) const
      -> std::pair<std::vector<utils::file_descriptor>, std::size_t>;

    static auto create_socketpair() -> std::pair<unix_socket, unix_socket>;

private:
    explicit unix_socket(utils::file_descriptor fd);

    auto send_msg(struct msghdr &msg) const -> int;
    static auto append_fds(struct msghdr &msg, utils::span<const int> fds) -> void;
    [[nodiscard]] auto recv_msg(struct msghdr &msg) const -> os::recv_result;
    [[nodiscard]] static auto extract_fds(struct msghdr &msg)
      -> std::vector<utils::file_descriptor>;

    utils::file_descriptor fd_;
};

} // namespace linyaps_box::infra
