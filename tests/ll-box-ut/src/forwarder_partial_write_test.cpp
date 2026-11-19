// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include <linyaps_box/io/epoll.h>
#include <linyaps_box/io/forwarder.h>
#include <linyaps_box/utils/file_describer.h>
#include <linyaps_box/utils/platform.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

namespace io = linyaps_box::io;
namespace utils = linyaps_box::utils;

// The destination pipe is limited to a single page while the forwarder ring buffer holds two
// pages. Pushing a full ring window therefore has to write one page first and then observe
// EAGAIN, which makes write_span() return TryAgain with bytes_written > 0. That is exactly the
// partial-write case that used to leave the already-written bytes in the ring buffer.
constexpr std::size_t payload_ring_windows = 4;

auto make_payload(std::size_t size) -> std::vector<std::byte>
{
    std::vector<std::byte> payload(size);
    for (std::size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<std::byte>((i * 131U + 7U) & 0xFFU);
    }

    return payload;
}

auto drain_fd(int fd, std::vector<std::byte> &out) -> bool
{
    std::array<std::byte, 4096> chunk{ };

    while (true) {
        const auto n = ::read(fd, chunk.data(), chunk.size());
        if (n > 0) {
            out.insert(out.end(), chunk.begin(), chunk.begin() + n);
            continue;
        }

        if (n == 0) {
            return true;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }

        return false;
    }
}

auto first_mismatch(const std::vector<std::byte> &actual, const std::vector<std::byte> &expected)
  -> std::size_t
{
    const auto common = std::min(actual.size(), expected.size());
    for (std::size_t i = 0; i < common; ++i) {
        if (actual[i] != expected[i]) {
            return i;
        }
    }

    return common;
}

auto hex_byte(std::byte value) -> std::string
{
    constexpr char digits[]{ "0123456789abcdef" };
    const auto raw = static_cast<unsigned int>(value);

    std::string text{ "0x" };
    text += digits[(raw >> 4U) & 0xFU];
    text += digits[raw & 0xFU];
    return text;
}

} // namespace

TEST(ForwarderPartialWrite, ConsumesBytesWrittenBeforeTryAgain)
{
    const auto page_size = utils::get_page_size();
    const auto ring_capacity = 2 * page_size;
    // Keep the payload larger than the ring buffer regardless of the page size, so the ring is
    // guaranteed to be full when push() runs and the destination always observes a partial write.
    const auto payload_size =
      std::max<std::size_t>(64 * 1024, payload_ring_windows * ring_capacity);
    const auto payload = make_payload(payload_size);

    int src_fds[2]{ -1, -1 };
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, src_fds), 0)
      << "socketpair() failed: " << std::strerror(errno);
    utils::file_descriptor src_read{ src_fds[0] };
    utils::file_descriptor src_write{ src_fds[1] };

    int dst_fds[2]{ -1, -1 };
    ASSERT_EQ(::pipe2(dst_fds, O_NONBLOCK | O_CLOEXEC), 0)
      << "pipe2() failed: " << std::strerror(errno);
    utils::file_descriptor dst_read{ dst_fds[0] };
    utils::file_descriptor dst_write{ dst_fds[1] };

    ASSERT_NE(::fcntl(dst_write.get(), F_SETPIPE_SZ, static_cast<int>(page_size)), -1)
      << "F_SETPIPE_SZ(" << page_size << ") failed: " << std::strerror(errno);
    ASSERT_EQ(::fcntl(dst_write.get(), F_GETPIPE_SZ), static_cast<int>(page_size));

    io::Epoll poller;
    io::Forwarder forwarder{ poller, ring_capacity };
    forwarder.set_src(src_read);
    forwarder.set_dst(dst_write);

    std::vector<std::byte> received;
    received.reserve(payload.size() + page_size);

    // Feed the source socket without blocking; whatever does not fit into the socket buffer is
    // retried on the next iteration. The write side is closed once the whole payload has been
    // queued, which is what makes the forwarder observe EOF.
    std::size_t sent{ 0 };
    bool source_closed{ false };
    const auto feed_source = [&]() -> bool {
        while (sent < payload.size()) {
            const auto n =
              ::send(src_write.get(), payload.data() + sent, payload.size() - sent, MSG_NOSIGNAL);
            if (n > 0) {
                sent += static_cast<std::size_t>(n);
                continue;
            }

            if (n < 0 && errno == EINTR) {
                continue;
            }

            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return true;
            }

            return false;
        }

        if (!source_closed) {
            src_write.close();
            source_closed = true;
        }

        return true;
    };

    constexpr std::size_t max_drive_iterations{ 4096 };
    std::size_t iterations{ 0 };
    while (iterations < max_drive_iterations && !forwarder.is_finished()) {
        ASSERT_TRUE(feed_source()) << "failed to feed the source socket after " << sent << " of "
                                   << payload.size() << " bytes";

        forwarder.drive();
        ASSERT_TRUE(drain_fd(dst_read.get(), received)) << "failed to drain the destination pipe";

        ++iterations;

        // A correct forwarder never forwards more bytes than the payload. Stop early so the
        // replay is reported instead of looping until the iteration limit.
        if (received.size() > payload.size()) {
            break;
        }
    }

    ASSERT_TRUE(feed_source()) << "failed to signal EOF on the source socket";
    ASSERT_TRUE(drain_fd(dst_read.get(), received)) << "failed to drain the destination pipe";

    if (received.size() > payload.size()) {
        FAIL() << "replay/duplication detected: forwarded " << received.size() << " bytes for a "
               << payload.size() << "-byte payload (" << (received.size() - payload.size())
               << " excess bytes after " << iterations
               << " drive() iterations); bytes written before a TryAgain must be consumed "
                  "from the ring buffer";
    }

    ASSERT_TRUE(forwarder.is_finished())
      << "forwarder did not finish within " << iterations << " drive() iterations: forwarded "
      << received.size() << " of " << payload.size()
      << " bytes, ring buffer is not empty and/or EOF was not reached";

    if (received.size() < payload.size()) {
        FAIL() << "byte loss detected: forwarded only " << received.size() << " of "
               << payload.size() << " bytes";
    }

    const auto mismatch = first_mismatch(received, payload);
    if (mismatch != payload.size()) {
        FAIL() << "payload corruption at offset " << mismatch << ": expected "
               << hex_byte(payload[mismatch]) << ", got " << hex_byte(received[mismatch])
               << " (forwarded length " << received.size() << ", payload length " << payload.size()
               << ")";
    }

    EXPECT_EQ(received.size(), payload.size());
}
