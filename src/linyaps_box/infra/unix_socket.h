// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/net.h"
#include "linyaps_box/utils/file_describer.h"

#include <filesystem>
#include <vector>

namespace linyaps_box::infra {

inline constexpr std::size_t kMaxScmFds = 16;

struct recv_result
{
    std::vector<utils::file_descriptor> fds;
    std::size_t bytes;
};

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

    [[nodiscard]] auto send(utils::span<const std::byte> data) const -> os::Result<std::size_t>;

    [[nodiscard]] auto
    recv(utils::span<std::byte> buf,
         utils::bitflags<os::sys::recv_flag> flags = os::sys::recv_flag::none) const
      -> os::Result<std::size_t>;

    [[nodiscard]] auto send_fd(utils::file_descriptor_ref fd) const -> os::Result<void>;

    [[nodiscard]] auto recv_fd() const -> utils::file_descriptor;

    // TODO: after removing file_descriptor::auto_close, file_descriptor will
    // have the same layout as int; change this parameter to
    // span<const file_descriptor> so senders pass owning fds directly without
    // file_descriptor_ref.
    [[nodiscard]] auto send_data_with_fds(utils::span<const std::byte> data,
                                          utils::span<const utils::file_descriptor_ref> fds) const
      -> os::Result<std::size_t>;

    [[nodiscard]] auto recv_data_with_fds(utils::span<std::byte> data) const
      -> os::Result<recv_result>;

    static auto create_pair(os::sys::socket_type type, os::sys::socket_flag flag)
      -> std::pair<unix_socket, unix_socket>;

private:
    explicit unix_socket(utils::file_descriptor fd);

    utils::file_descriptor fd_;
};

} // namespace linyaps_box::infra
