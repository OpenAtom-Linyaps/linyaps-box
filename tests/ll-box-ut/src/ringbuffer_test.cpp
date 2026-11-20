// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "gtest_compat.h" // IWYU pragma: keep

#include <linyaps_box/infra/ringbuffer.h>

#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

namespace infra = linyaps_box::infra;

namespace {

auto is_power_of_two(std::size_t n) -> bool
{
    return n != 0 && (n & (n - 1)) == 0;
}

auto readable_bytes(const infra::ring_buffer &rb) -> std::string_view
{
    const auto bytes = rb.readable();
    return { reinterpret_cast<const char *>(bytes.data()), bytes.size() };
}

} // anonymous namespace

TEST(RingBuffer, CreateRoundsUpToPowerOfTwo)
{
    for (const auto requested : {
           std::size_t{ 0 },
           std::size_t{ 1 },
           std::size_t{ 4096 },
           std::size_t{ 4097 },
           std::size_t{ 8192 },
           std::size_t{ 100000 },
         }) {
        auto rb = infra::ring_buffer::create(requested);
        ASSERT_NE(rb.get(), nullptr);
        const auto cap = rb->capacity();
        EXPECT_GE(cap, requested);
        EXPECT_GE(cap, 4096);
        EXPECT_TRUE(is_power_of_two(cap));
    }
}

TEST(RingBuffer, CreateTooLargeThrows)
{
    EXPECT_THROW(std::ignore = infra::ring_buffer::create(std::numeric_limits<std::size_t>::max()),
                 std::runtime_error);
}

TEST(RingBuffer, ClearResetsState)
{
    auto rb = infra::ring_buffer::create(4096);
    const auto cap = rb->capacity();
    const std::vector<std::byte> data(cap);

    std::memcpy(rb->get_write_ptr(), data.data(), data.size());
    rb->advance_head(data.size());
    ASSERT_TRUE(rb->full());
    EXPECT_TRUE(rb->writable().empty());

    rb->clear();

    EXPECT_TRUE(rb->empty());
    EXPECT_EQ(rb->size(), 0);
    EXPECT_EQ(rb->free_space(), cap);
    EXPECT_FALSE(rb->full());
    EXPECT_EQ(rb->get_read_ptr(), rb->get_write_ptr());
}

TEST(RingBuffer, WritableSpanCrossesWrapSeam)
{
    const auto minimum_capacity = infra::ring_buffer::create(0)->capacity();
    for (const auto requested : { std::size_t{ 0 }, minimum_capacity + 1 }) {
        SCOPED_TRACE(::testing::Message() << "requested_capacity=" << requested);

        constexpr std::size_t bytes_before_wrap = 3;
        constexpr std::string_view data = "ABCDEF";
        auto rb = infra::ring_buffer::create(requested);
        const auto cap = rb->capacity();
        ASSERT_GE(cap, requested);
        ASSERT_GE(cap, data.size());

        const auto offset = cap - bytes_before_wrap;
        const auto initial = rb->writable();
        ASSERT_EQ(initial.size(), cap);
        std::memset(initial.data(), 0, offset);
        rb->advance_head(offset);
        rb->advance_tail(offset);
        ASSERT_TRUE(rb->empty());

        const auto writable = rb->writable();
        ASSERT_EQ(writable.size(), cap);
        EXPECT_EQ(writable.data(), rb->get_write_ptr());
        std::memcpy(writable.data(), data.data(), data.size());
        rb->advance_head(data.size());

        EXPECT_EQ(rb->readable().data(), rb->get_read_ptr());
        ASSERT_EQ(readable_bytes(*rb), data);

        rb->advance_tail(bytes_before_wrap);
        EXPECT_EQ(readable_bytes(*rb), data.substr(bytes_before_wrap));
    }
}

TEST(RingBuffer, AppendAcrossWrapPreservesUnreadBytes)
{
    constexpr std::string_view unread = "AB";
    constexpr std::string_view appended = "CDEF";
    constexpr std::size_t bytes_before_wrap = 2;
    auto rb = infra::ring_buffer::create(0);
    const auto cap = rb->capacity();
    ASSERT_GE(cap, unread.size() + appended.size());

    const auto initial = rb->writable();
    ASSERT_EQ(initial.size(), cap);

    const auto prefix_size = cap - bytes_before_wrap - unread.size();
    std::memset(initial.data(), 0, prefix_size);
    std::memcpy(initial.data() + prefix_size, unread.data(), unread.size());
    rb->advance_head(prefix_size + unread.size());
    rb->advance_tail(prefix_size);
    ASSERT_EQ(readable_bytes(*rb), unread);

    const auto writable = rb->writable();
    ASSERT_EQ(writable.size(), cap - unread.size());
    std::memcpy(writable.data(), appended.data(), appended.size());
    rb->advance_head(appended.size());
    ASSERT_EQ(readable_bytes(*rb), std::string_view{ "ABCDEF" });

    rb->advance_tail(4);
    ASSERT_EQ(readable_bytes(*rb), std::string_view{ "EF" });
    rb->advance_tail(2);
    EXPECT_TRUE(rb->empty());
    EXPECT_TRUE(rb->readable().empty());
    EXPECT_EQ(rb->writable().size(), cap);
}
