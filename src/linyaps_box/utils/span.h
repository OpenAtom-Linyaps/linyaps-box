// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

// from stdc++20's std::span, compatible with c++17

#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace linyaps_box::utils {

namespace detail {
// std::remove_cvref was introduced at c++20
template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

struct view_base
{
};

template <typename T>
struct enable_view : std::is_base_of<view_base, T>
{
};

template <typename T, typename = void>
struct is_view : std::false_type
{
};

template <typename T>
struct is_view<T, std::enable_if_t<enable_view<T>::value>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_view_v = is_view<T>::value;

template <typename CharT, typename Traits>
struct enable_view<std::basic_string_view<CharT, Traits>> : std::true_type
{
};

template <typename Container, typename T, typename = void>
struct is_compatible_container : std::false_type
{
};

template <typename Container, typename T>
struct is_compatible_container<Container,
                               T,
                               std::void_t<decltype(std::declval<Container &>().data()),
                                           decltype(std::declval<Container &>().size())>>
    : std::is_convertible<std::remove_pointer_t<decltype(std::declval<Container &>().data())> (*)[],
                          T (*)[]>
{
};

} // namespace detail

inline constexpr std::size_t dynamic_extent = static_cast<std::size_t>(-1);

template <typename T, std::size_t Extent = dynamic_extent>
class span;

template <typename T, std::size_t Extent>
struct span_storage
{
    T *data_;

    constexpr span_storage() noexcept
        : data_(nullptr)
    {
    }

    constexpr span_storage(T *ptr, [[maybe_unused]] std::size_t sz) noexcept
        : data_(ptr)
    {
    }

    [[nodiscard]] constexpr T *data() const noexcept { return data_; }

    [[nodiscard]] constexpr std::size_t size() const noexcept { return Extent; }
};

template <typename T>
struct span_storage<T, dynamic_extent>
{
    T *data_;
    std::size_t size_;

    constexpr span_storage() noexcept
        : data_(nullptr)
        , size_(0)
    {
    }

    constexpr span_storage(T *ptr, std::size_t sz) noexcept
        : data_(ptr)
        , size_(sz)
    {
    }

    [[nodiscard]] constexpr T *data() const noexcept { return data_; }

    [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
};

template <typename T, std::size_t Extent>
class span : public detail::view_base
{
private:
    struct internal_static_construct_tag
    {
    };

    constexpr span(T *ptr, [[maybe_unused]] internal_static_construct_tag tag) noexcept
        : storage_(ptr, Extent)
    {
    }

    template <std::size_t Offset, std::size_t Count>
    static constexpr std::size_t subspan_extent()
    {
        if constexpr (Count != dynamic_extent) {
            return Count;
        } else if constexpr (Extent != dynamic_extent) {
            return Extent - Offset;
        } else {
            return dynamic_extent;
        }
    }

public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using iterator = T *;
    using const_iterator = const T *;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    static constexpr size_type extent = Extent;

    template <std::size_t E = Extent, typename = std::enable_if_t<E == dynamic_extent || E == 0>>
    constexpr span() noexcept
        : storage_()
    {
    }

    constexpr span(pointer ptr, size_type count) noexcept
        : storage_(ptr, count)
    {
        if constexpr (Extent != dynamic_extent) {
            assert(count == Extent);
        }
    }

    constexpr span(pointer first, pointer last) noexcept
        : storage_(first, static_cast<size_type>(last - first))
    {
        if constexpr (Extent != dynamic_extent) {
            assert(static_cast<size_type>(last - first) == Extent);
        }
    }

    template <std::size_t N, typename = std::enable_if_t<Extent == dynamic_extent || Extent == N>>
    constexpr explicit span(T (&arr)[N]) noexcept
        : storage_(arr, N)
    {
    }

    template <typename U,
              std::size_t N,
              typename = std::enable_if_t<(Extent == dynamic_extent || Extent == N)
                                          && std::is_convertible_v<U (*)[], T (*)[]>>>
    constexpr explicit span(std::array<U, N> &arr) noexcept
        : storage_(arr.data(), N)
    {
    }

    template <typename U,
              std::size_t N,
              typename = std::enable_if_t<(Extent == dynamic_extent || Extent == N)
                                          && std::is_convertible_v<const U (*)[], T (*)[]>>>
    constexpr explicit span(const std::array<U, N> &arr) noexcept
        : storage_(arr.data(), N)
    {
    }

    template <
      typename Container,
      typename = std::enable_if_t<!std::is_same_v<detail::remove_cvref_t<Container>, span>
                                  && !std::is_array_v<std::remove_reference_t<Container>>
                                  && detail::is_compatible_container<Container, T>::value
                                  && (std::is_lvalue_reference_v<Container>
                                      || detail::is_view_v<detail::remove_cvref_t<Container>>)>>
    constexpr explicit span(Container &&cont)
        : storage_(std::forward<Container>(cont).data(), std::forward<Container>(cont).size())
    {
        if constexpr (Extent != dynamic_extent) {
            assert(cont.size() == Extent);
        }
    }

    template <typename U,
              std::size_t OtherExtent,
              typename = std::enable_if_t<(Extent == dynamic_extent || OtherExtent == dynamic_extent
                                           || Extent == OtherExtent)
                                          && std::is_convertible_v<U (*)[], T (*)[]>>>
    constexpr explicit span(const span<U, OtherExtent> &other) noexcept
        : storage_(other.data(), other.size())
    {
        if constexpr (Extent != dynamic_extent) {
            assert(other.size() == Extent);
        }
    }

    ~span() = default;
    constexpr span(const span &) noexcept = default;
    constexpr span(span &&) noexcept = default;
    constexpr span &operator=(const span &) noexcept = default;
    constexpr span &operator=(span &&) noexcept = default;

    [[nodiscard]] constexpr pointer data() const noexcept { return storage_.data(); }

    [[nodiscard]] constexpr size_type size() const noexcept { return storage_.size(); }

    [[nodiscard]] constexpr size_type size_bytes() const noexcept
    {
        return size() * sizeof(element_type);
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

    [[nodiscard]] constexpr reference operator[](size_type idx) const noexcept
    {
        assert(idx < size());
        return data()[idx];
    }

    [[nodiscard]] constexpr reference front() const noexcept
    {
        assert(!empty());
        return data()[0];
    }

    [[nodiscard]] constexpr reference back() const noexcept
    {
        assert(!empty());
        return data()[size() - 1];
    }

    [[nodiscard]] constexpr iterator begin() const noexcept { return data(); }

    [[nodiscard]] constexpr iterator end() const noexcept { return data() + size(); }

    [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return data(); }

    [[nodiscard]] constexpr const_iterator cend() const noexcept { return data() + size(); }

    [[nodiscard]] constexpr reverse_iterator rbegin() const noexcept
    {
        return reverse_iterator(end());
    }

    [[nodiscard]] constexpr reverse_iterator rend() const noexcept
    {
        return reverse_iterator(begin());
    }

    template <std::size_t Count>
    [[nodiscard]] constexpr span<element_type, Count> first() const noexcept
    {
        if constexpr (Extent == dynamic_extent) {
            assert(Count <= size());
        } else {
            static_assert(Count <= Extent, "Count out of bounds in span::first()");
        }

        return { data(), internal_static_construct_tag{ } };
    }

    template <std::size_t Count>
    [[nodiscard]] constexpr span<element_type, Count> last() const noexcept
    {
        if constexpr (Extent == dynamic_extent) {
            assert(Count <= size());
        } else {
            static_assert(Count <= Extent, "Count out of bounds in span::last()");
        }

        return { data() + (size() - Count), internal_static_construct_tag{ } };
    }

    template <std::size_t Offset, std::size_t Count = dynamic_extent>
    [[nodiscard]] constexpr auto subspan() const noexcept
      -> span<element_type, subspan_extent<Offset, Count>()>
    {
        if constexpr (Extent == dynamic_extent) {
            assert(Offset <= size());
        } else {
            static_assert(Offset <= Extent, "Offset out of bounds in span::subspan()");
        }

        constexpr std::size_t E = subspan_extent<Offset, Count>();

        if constexpr (E != dynamic_extent) {
            if constexpr (Extent == dynamic_extent && Count != dynamic_extent) {
                assert(Count <= (size() - Offset));
            } else if constexpr (Extent != dynamic_extent) {
                static_assert(Count == dynamic_extent || Count <= (Extent - Offset),
                              "Count out of bounds");
            }

            return { data() + Offset, internal_static_construct_tag{ } };
        } else {
            return { data() + Offset, Count == dynamic_extent ? size() - Offset : Count };
        }
    }

    [[nodiscard]] constexpr span<element_type, dynamic_extent> first(size_type count) const noexcept
    {
        assert(count <= size());
        return { data(), count };
    }

    [[nodiscard]] constexpr span<element_type, dynamic_extent> last(size_type count) const noexcept
    {
        assert(count <= size());
        return { data() + (size() - count), count };
    }

    [[nodiscard]] constexpr span<element_type, dynamic_extent>
    subspan(size_type offset, size_type count = dynamic_extent) const noexcept
    {
        assert(offset <= size());
        if (count == dynamic_extent) {
            return { data() + offset, size() - offset };
        }

        assert(offset + count <= size());
        return { data() + offset, count };
    }

private:
    span_storage<T, Extent> storage_;
};

} // namespace linyaps_box::utils
