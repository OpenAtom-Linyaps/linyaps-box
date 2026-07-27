// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/protocol/message.h"

#include "linyaps_box/utils/utils.h"

#include <chrono>
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
    if (UNLIKELY(sizeof(T) > data.size() - offset)) {
        throw std::runtime_error("payload too short for pod read");
    }

    T val{ };
    std::memcpy(&val, data.data() + offset, sizeof(T));
    offset += sizeof(T);
    return val;
}

auto append_string(std::vector<std::byte> &buf, std::string_view s) -> void
{
    if (s.size() > UINT32_MAX) {
        throw std::logic_error("string too large for wire format");
    }
    auto len = static_cast<uint32_t>(s.size());
    append_pod(buf, len);
    auto view = utils::as_bytes(utils::span{ s });
    buf.insert(buf.end(), view.cbegin(), view.cend());
}

auto read_string(utils::span<const std::byte> data, std::size_t &offset) -> std::string
{
    auto len = read_pod<uint32_t>(data, offset);
    if (offset > data.size() || len > data.size() - offset) {
        throw std::runtime_error("payload too short for string read");
    }

    std::string result(reinterpret_cast<const char *>(data.data() + offset), len);
    offset += len;
    return result;
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
                        [](const log &m) -> std::vector<std::byte> {
                            return serialize_log(m);
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
    case msg_id::log: {
        auto lvl_raw = read_pod<uint8_t>(payload, offset);
        if (UNLIKELY(lvl_raw > static_cast<uint8_t>(linyaps_box::log::level::debug))) {
            throw std::runtime_error(fmt::format("invalid log level: {}", lvl_raw));
        }

        auto lvl = static_cast<linyaps_box::log::level>(lvl_raw);
        auto message = read_string(payload, offset);
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
        auto file = read_string(payload, offset);
        auto line = read_pod<int>(payload, offset);
        auto function = read_string(payload, offset);
#endif
        auto errno_val = read_pod<int>(payload, offset);
        auto pid = read_pod<pid_t>(payload, offset);
        auto time_count = read_pod<std::int64_t>(payload, offset);

        return log{ std::move(message),
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
                    std::move(file),
                    std::move(function),
                    line,
#endif
                    errno_val,
                    std::chrono::nanoseconds{ time_count },
                    pid,
                    lvl };
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

namespace {

auto serialize_log_into(std::vector<std::byte> &buf,
                        uint8_t lvl,
                        std::string_view message,
                        int errno_val,
                        pid_t pid,
                        int64_t time_ns
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
                        ,
                        std::string_view file,
                        int line,
                        std::string_view function
#endif
                        ) -> void
{
    append_pod(buf, msg_id::log);
    append_pod(buf, lvl);
    append_string(buf, message);
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    append_string(buf, file);
    append_pod(buf, line);
    append_string(buf, function);
#endif
    append_pod(buf, errno_val);
    append_pod(buf, pid);
    append_pod<std::int64_t>(buf, time_ns);
}

} // namespace

auto serialize_log(const msg::log &m) -> std::vector<std::byte>
{
    std::vector<std::byte> buf;
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    buf.reserve(sizeof(msg_id) + 1 + m.message.size() + m.file.size() + m.function.size()
                + sizeof(int) + sizeof(int) + sizeof(pid_t) + sizeof(std::int64_t)
                + (3 * sizeof(uint32_t)));
#else
    buf.reserve(sizeof(msg_id) + 1 + m.message.size() + sizeof(int) + sizeof(pid_t)
                + sizeof(std::int64_t) + sizeof(uint32_t));
#endif
    const auto ns = m.time.count();
    static_assert(std::is_signed_v<decltype(ns)>);
    serialize_log_into(buf,
                       static_cast<uint8_t>(m.lvl),
                       m.message,
                       m.errno_,
                       m.pid,
                       ns
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
                       ,
                       m.file,
                       m.line,
                       m.function
#endif
    );
    return buf;
}

auto serialize_log(const linyaps_box::log::log_context &ctx) -> std::vector<std::byte>
{
    std::vector<std::byte> buf;
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    buf.reserve(sizeof(msg_id) + 1 + ctx.msg.size() + ctx.file.size() + ctx.function.size()
                + sizeof(int) + sizeof(int) + sizeof(pid_t) + sizeof(std::int64_t)
                + (3 * sizeof(uint32_t)));
#else
    buf.reserve(sizeof(msg_id) + 1 + ctx.msg.size() + sizeof(int) + sizeof(pid_t)
                + sizeof(std::int64_t) + sizeof(uint32_t));
#endif
    const auto ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(ctx.wall_time.time_since_epoch())
        .count();
    static_assert(std::is_signed_v<decltype(ns)>);
    serialize_log_into(buf,
                       static_cast<uint8_t>(ctx.lvl),
                       ctx.msg,
                       ctx.errno_,
                       ctx.pid,
                       ns
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
                       ,
                       ctx.file,
                       ctx.line,
                       ctx.function
#endif
    );
    return buf;
}

} // namespace linyaps_box::protocol::msg
