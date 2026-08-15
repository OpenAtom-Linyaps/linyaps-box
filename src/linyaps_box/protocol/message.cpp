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

// Fixed wire-format overhead for a log message (excluding variable-length
// string payloads).  Branch on source-location so the reserve hint is exact.
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
// msg_id + lvl + (errno + line)*int + pid + time + (message+file+function)*uint32
constexpr auto log_wire_overhead = sizeof(msg_id) + sizeof(uint8_t) + (2 * sizeof(int))
  + sizeof(pid_t) + sizeof(std::int64_t) + (3 * sizeof(uint32_t));
#else
// msg_id + lvl + errno*int + pid + time + message*uint32
constexpr auto log_wire_overhead = sizeof(msg_id) + sizeof(uint8_t) + sizeof(int) + sizeof(pid_t)
  + sizeof(std::int64_t) + sizeof(uint32_t);
#endif

template <typename T>
auto append_pod(std::vector<std::byte> &buf, const T &val) -> void
{
    static_assert(std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>);
    const auto old_size = buf.size();
    buf.resize(old_size + sizeof(T));
    std::memcpy(buf.data() + old_size, &val, sizeof(T));
}

template <typename T>
auto read_pod(utils::span<const std::byte> data, std::size_t &offset) -> T
{
    static_assert(std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>);
    if (UNLIKELY(offset > data.size() || sizeof(T) > data.size() - offset)) {
        throw std::runtime_error("payload too short for pod read");
    }

    T val{ };
    std::memcpy(&val, data.data() + offset, sizeof(T));
    offset += sizeof(T);
    return val;
}

auto append_string(std::vector<std::byte> &buf, std::string_view s) -> void
{
    if (UNLIKELY(s.size() > std::numeric_limits<uint32_t>::max())) {
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
    if (UNLIKELY(offset > data.size() || len > data.size() - offset)) {
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
                            std::vector<std::byte> buf;
                            serialize_log_into(buf, to_log_context(m));
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

auto serialize_log_into(std::vector<std::byte> &buf, const linyaps_box::log::log_context &ctx)
  -> void
{
    buf.reserve(buf.size() + ctx.message.size()
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
                + ctx.file.size() + ctx.function.size()
#endif
                + log_wire_overhead);

    const auto ns = ctx.time.count();
    static_assert(std::is_signed_v<decltype(ns)>);

    append_pod(buf, msg_id::log);
    append_pod(buf, static_cast<uint8_t>(ctx.lvl));
    append_string(buf, ctx.message);
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    append_string(buf, ctx.file);
    append_pod(buf, ctx.line);
    append_string(buf, ctx.function);
#endif
    append_pod(buf, ctx.errno_);
    append_pod(buf, ctx.pid);
    append_pod<std::int64_t>(buf, ns);
}

auto to_log_context(const log &l) noexcept -> linyaps_box::log::log_context
{
    return {
        l.lvl,  l.message,  l.time, l.pid, l.errno_,
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
        l.file, l.function, l.line,
#endif
    };
}

} // namespace linyaps_box::protocol::msg
