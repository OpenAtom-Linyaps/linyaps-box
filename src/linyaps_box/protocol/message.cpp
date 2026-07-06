// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/protocol/message.h"

#include "linyaps_box/utils/utils.h"

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace linyaps_box::protocol::msg {

namespace {

template <typename T>
auto append_pod(std::vector<std::byte> &buf, const T &val) -> void
{
    static_assert(std::is_trivially_copyable_v<T>);
    const auto *ptr = reinterpret_cast<const std::byte *>(&val);
    const utils::span data_view{ ptr, sizeof(T) };
    buf.insert(buf.end(), data_view.cbegin(), data_view.cend());
}

template <typename T>
auto read_pod(utils::span<const std::byte> data, std::size_t &offset) -> T
{
    static_assert(std::is_trivially_copyable_v<T>);
    if (UNLIKELY(offset + sizeof(T) > data.size())) {
        throw std::runtime_error("payload too short for pod read");
    }

    T val{ };
    std::memcpy(&val, data.data() + offset, sizeof(T));
    offset += sizeof(T);
    return val;
}

} // namespace

auto datagram::take_fds() -> std::vector<utils::file_descriptor>
{
    if (UNLIKELY(fds.empty())) {
        throw std::runtime_error("expected fd but none received");
    }

    return std::move(fds);
}

auto serialize(const message &msg) -> std::vector<std::byte>
{
    return std::visit(utils::Overload{
                        [](const die &m) -> std::vector<std::byte> {
                            std::vector<std::byte> buf;
                            buf.reserve(sizeof(msg_id) + sizeof(m.errnum) + m.message.size());
                            append_pod(buf, msg_id::die);
                            append_pod(buf, m.errnum);
                            const auto msg_view = utils::as_bytes(utils::span{ m.message });
                            buf.insert(buf.end(), msg_view.cbegin(), msg_view.cend());
                            return buf;
                        },
                        [](const stage &m) -> std::vector<std::byte> {
                            std::vector<std::byte> buf;
                            buf.reserve(sizeof(msg_id) + sizeof(m.value));
                            append_pod(buf, msg_id::stage);
                            append_pod(buf, m.value);
                            return buf;
                        },
                        [](const pid_report &m) -> std::vector<std::byte> {
                            std::vector<std::byte> buf;
                            buf.reserve(sizeof(msg_id) + sizeof(m.value));
                            append_pod(buf, msg_id::pid_report);
                            append_pod(buf, m.value);
                            return buf;
                        },
                        [](const console_fd &) -> std::vector<std::byte> {
                            std::vector<std::byte> buf;
                            buf.reserve(sizeof(msg_id));
                            append_pod(buf, msg_id::console_fd);
                            return buf;
                        },
                        [](const proceed &) -> std::vector<std::byte> {
                            std::vector<std::byte> buf;
                            buf.reserve(sizeof(msg_id));
                            append_pod(buf, msg_id::proceed);
                            return buf;
                        },
                      },
                      msg);
}

auto deserialize(utils::span<const std::byte> wire) -> message
{
    if (UNLIKELY(wire.size() < sizeof(msg_id))) {
        throw std::runtime_error("wire data too short for msg_id");
    }

    msg_id id{ };
    std::memcpy(&id, wire.data(), sizeof(id));

    const auto payload = wire.subspan(sizeof(msg_id));
    std::size_t offset{ 0 };

    switch (id) {
    case msg_id::die: {
        auto errnum = read_pod<decltype(die::errnum)>(payload, offset);
        const auto remaining = payload.subspan(offset);
        // convert to char* explicitly for older libstdc++ versions which before 14.1.0
        // more detailed:
        // https://gitlab.com/gnutools/gcc/-/commit/cc3d7baf2741777e99567d4301802c99f5775619
        return die{ errnum,
                    std::string{ reinterpret_cast<const char *>(remaining.data()), // NOLINT
                                 remaining.size() } };
    }
    case msg_id::stage: {
        auto value = read_pod<protocol::stage::type>(payload, offset);
        if (UNLIKELY(!protocol::stage::is_valid(value))) {
            throw std::runtime_error(
              fmt::format("unknown stage::type: {}",
                          static_cast<std::underlying_type_t<protocol::stage::type>>(value)));
        }

        return stage{ value };
    }
    case msg_id::pid_report: {
        return pid_report{ read_pod<decltype(pid_report::value)>(payload, offset) };
    }
    case msg_id::console_fd: {
        return console_fd{ };
    }
    case msg_id::proceed: {
        return proceed{ };
    }
    default: {
        throw std::runtime_error(
          fmt::format("unknown msg_id: {}", static_cast<std::underlying_type_t<msg_id>>(id)));
    }
    }
}

} // namespace linyaps_box::protocol::msg
