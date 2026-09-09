// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include <linyaps_box/infra/ringbuffer.h>

#include <cstring>
#include <limits>
#include <vector>

namespace infra = linyaps_box::infra;

namespace {

auto is_power_of_two(std::size_t n) -> bool
{
    return n != 0 && (n & (n - 1)) == 0;
}

} // anonymous namespace

TEST(RingBuffer, CreateRoundsUpToPowerOfTwo)
{
    for (const auto requested : { std::size_t{ 0 },
                                  std::size_t{ 1 },
                                  std::size_t{ 4096 },
                                  std::size_t{ 4097 },
                                  std::size_t{ 8192 },
                                  std::size_t{ 100000 } }) {
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

TEST(RingBuffer, FreshBufferState)
{
    auto rb = infra::ring_buffer::create(4096);
    EXPECT_TRUE(rb->empty());
    EXPECT_EQ(rb->size(), 0);
    EXPECT_EQ(rb->free_space(), rb->capacity());
    EXPECT_FALSE(rb->full());
    EXPECT_EQ(rb->get_read_ptr(), rb->get_write_ptr());
}

TEST(RingBuffer, WriteReadRoundTrip)
{
    auto rb = infra::ring_buffer::create(4096);
    constexpr std::array data{ 'h', 'e', 'l', 'l', 'o' };

    std::memcpy(rb->get_write_ptr(), data.data(), data.size());
    rb->advance_head(data.size());

    EXPECT_EQ(rb->size(), data.size());
    EXPECT_EQ(rb->free_space(), rb->capacity() - data.size());
    EXPECT_FALSE(rb->empty());
    EXPECT_FALSE(rb->full());
    EXPECT_EQ(rb->get_write_ptr(), rb->get_read_ptr() + data.size());

    std::array<char, data.size()> out{ };
    std::memcpy(out.data(), rb->get_read_ptr(), data.size());
    EXPECT_EQ(std::memcmp(out.data(), data.data(), data.size()), 0);

    rb->advance_tail(2);
    EXPECT_EQ(rb->size(), data.size() - 2);
    EXPECT_EQ(rb->get_write_ptr(), rb->get_read_ptr() + rb->size());
}

TEST(RingBuffer, FullWhenCapacityWritten)
{
    auto rb = infra::ring_buffer::create(4096);
    const auto cap = rb->capacity();
    const std::vector<std::byte> data(cap);

    std::memcpy(rb->get_write_ptr(), data.data(), data.size());
    rb->advance_head(data.size());

    EXPECT_TRUE(rb->full());
    EXPECT_EQ(rb->size(), cap);
    EXPECT_EQ(rb->free_space(), 0);
}

TEST(RingBuffer, ClearResetsState)
{
    auto rb = infra::ring_buffer::create(4096);
    const auto cap = rb->capacity();
    const std::vector<std::byte> data(cap);

    std::memcpy(rb->get_write_ptr(), data.data(), data.size());
    rb->advance_head(data.size());
    ASSERT_TRUE(rb->full());

    rb->clear();

    EXPECT_TRUE(rb->empty());
    EXPECT_EQ(rb->size(), 0);
    EXPECT_EQ(rb->free_space(), cap);
    EXPECT_FALSE(rb->full());
    EXPECT_EQ(rb->get_read_ptr(), rb->get_write_ptr());
}

TEST(RingBuffer, WrapAroundMirrorAccess)
{
    auto rb = infra::ring_buffer::create(4096);
    const auto cap = rb->capacity();

    std::vector<std::byte> fill(cap);
    for (std::size_t i = 0; i < cap; ++i) {
        fill[i] = static_cast<std::byte>(i & 0xFF);
    }

    std::memcpy(rb->get_write_ptr(), fill.data(), fill.size());
    rb->advance_head(fill.size());
    ASSERT_TRUE(rb->full());

    rb->advance_tail(cap - 3);
    EXPECT_EQ(rb->size(), 3);

    constexpr std::array fresh{ std::byte{ 'A' }, std::byte{ 'B' }, std::byte{ 'C' } };
    std::memcpy(rb->get_write_ptr(), fresh.data(), fresh.size());
    rb->advance_head(fresh.size());
    EXPECT_EQ(rb->size(), 6);

    std::vector<std::byte> out(6);
    std::memcpy(out.data(), rb->get_read_ptr(), out.size());

    EXPECT_EQ(out[0], fill[cap - 3]);
    EXPECT_EQ(out[1], fill[cap - 2]);
    EXPECT_EQ(out[2], fill[cap - 1]);
    EXPECT_EQ(out[3], fresh[0]);
    EXPECT_EQ(out[4], fresh[1]);
    EXPECT_EQ(out[5], fresh[2]);
}

TEST(RingBuffer, DrainAllAfterWrapReturnsToEmpty)
{
    auto rb = infra::ring_buffer::create(4096);
    const auto cap = rb->capacity();
    const std::vector<std::byte> data(cap);

    std::memcpy(rb->get_write_ptr(), data.data(), data.size());
    rb->advance_head(data.size());
    ASSERT_TRUE(rb->full());

    rb->advance_tail(cap);

    EXPECT_TRUE(rb->empty());
    EXPECT_EQ(rb->size(), 0);
    EXPECT_EQ(rb->free_space(), cap);
    EXPECT_EQ(rb->get_read_ptr(), rb->get_write_ptr());
}

TEST(RingBuffer, WritePtrWrapsAfterFullCycle)
{
    auto rb = infra::ring_buffer::create(4096);
    const auto cap = rb->capacity();
    const std::vector<std::byte> data(cap);

    auto *first = rb->get_write_ptr();
    std::memcpy(rb->get_write_ptr(), data.data(), data.size());
    rb->advance_head(data.size());
    rb->advance_tail(data.size());
    ASSERT_TRUE(rb->empty());

    EXPECT_EQ(rb->get_write_ptr(), first);
}
