// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

// from STL std::span, compatible with c++17

#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <type_traits>

namespace linyaps_box::utils {

namespace detail {
template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename T>
struct type_identity
{
    using type = T;
};

template <typename T>
using type_identity_t = typename type_identity<T>::type;

template <typename To, typename From>
using is_array_convertible = std::is_convertible<From (*)[], To (*)[]>;

template <typename To, typename From>
inline constexpr bool is_array_convertible_v = is_array_convertible<To, From>::value;

template <typename T>
constexpr T *to_address(T *p) noexcept
{
    return p;
}

template <typename Ptr, typename = void>
struct has_pointer_traits_to_address : std::false_type
{
};

template <typename Ptr>
struct has_pointer_traits_to_address<
  Ptr,
  std::void_t<decltype(std::pointer_traits<Ptr>::to_address(std::declval<const Ptr &>()))>>
    : std::true_type
{
};

template <typename Ptr>
constexpr auto to_address(const Ptr &p) noexcept
{
    if constexpr (has_pointer_traits_to_address<Ptr>::value) {
        return std::pointer_traits<Ptr>::to_address(p);
    } else {
        return to_address(p.operator->());
    }
}

template <typename It, typename T, typename = void>
struct is_span_compatible_iter : std::false_type
{
};

template <typename It, typename T>
struct is_span_compatible_iter<It,
                               T,
                               std::void_t<decltype(*std::declval<It &>()),
                                           decltype(std::declval<It &>() - std::declval<It &>()),
                                           decltype(detail::to_address(std::declval<It &>()))>>
    : std::conjunction<
        is_array_convertible<T, std::remove_reference_t<decltype(*std::declval<It &>())>>,
        std::is_convertible<decltype(detail::to_address(std::declval<It &>())), T *>>
{
};

template <typename It, typename T>
inline constexpr bool is_span_compatible_iter_v = is_span_compatible_iter<It, T>::value;

template <typename It, typename = void>
struct is_contiguous_iterator : std::false_type
{
};

template <typename It>
struct is_contiguous_iterator<It,
                              std::void_t<decltype(*std::declval<It &>()),
                                          decltype(std::declval<It &>() - std::declval<It &>()),
                                          decltype(detail::to_address(std::declval<It &>()))>>
    : std::true_type
{
};

template <typename It>
inline constexpr bool is_contiguous_iterator_v = is_contiguous_iterator<It>::value;

template <typename It, typename = void>
struct has_data_and_size : std::false_type
{
};

template <typename It>
struct has_data_and_size<
  It,
  std::void_t<decltype(std::data(std::declval<It &>())), decltype(std::size(std::declval<It &>()))>>
    : std::true_type
{
};

template <typename It>
inline constexpr bool has_data_and_size_v = has_data_and_size<It>::value;

template <typename It>
constexpr auto span_to_address(It &&it) noexcept
{
    static_assert(is_contiguous_iterator_v<std::decay_t<It>>,
                  "span: It must be a contiguous iterator");
    return detail::to_address(std::forward<It>(it));
}

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
                               std::void_t<decltype(std::data(std::declval<Container &>())),
                                           decltype(std::size(std::declval<Container &>()))>>
    : is_array_convertible<T,
                           std::remove_pointer_t<decltype(std::data(std::declval<Container &>()))>>
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
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = element_type *;
    using const_pointer = const element_type *;
    using reference = element_type &;
    using const_reference = const element_type &;
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

    template <std::size_t E = Extent, std::enable_if_t<E == dynamic_extent> * = nullptr>
    constexpr span(pointer ptr, size_type count) noexcept
        : storage_(ptr, count)
    {
        assert((count == 0 || ptr != nullptr) && "span: null pointer with non-zero count");
    }

    template <std::size_t E = Extent, std::enable_if_t<E != dynamic_extent> * = nullptr>
    constexpr explicit span(pointer ptr, size_type count) noexcept
        : storage_(ptr, count)
    {
        assert(count == E && "span: size mismatch for fixed-extent span");
        assert((count == 0 || ptr != nullptr) && "span: null pointer with non-zero count");
    }

    template <std::size_t E = Extent, std::enable_if_t<E == dynamic_extent> * = nullptr>
    constexpr span(pointer first, pointer last) noexcept
        : storage_(first, static_cast<size_type>(last - first))
    {
        assert(first <= last && "span: (first,last) last precedes first");
    }

    template <std::size_t E = Extent, std::enable_if_t<E != dynamic_extent> * = nullptr>
    constexpr explicit span(pointer first, pointer last) noexcept
        : storage_(first, static_cast<size_type>(last - first))
    {
        assert(first <= last && "span: (first,last) last precedes first");
        assert(static_cast<size_type>(last - first) == E
               && "span: size mismatch for fixed-extent span");
    }

    // (It, count) for dynamic extent — implicit
    template <typename It,
              std::size_t E = Extent,
              std::enable_if_t<E == dynamic_extent> * = nullptr>
    constexpr span(It first, size_type count) noexcept
        : storage_(detail::span_to_address(first), count)
    {
        static_assert(
          detail::is_array_convertible_v<T,
                                         std::remove_reference_t<decltype(*std::declval<It &>())>>,
          "span: iterator value type is not compatible");
        static_assert(
          std::is_convertible_v<decltype(detail::to_address(std::declval<It &>())), T *>,
          "span: iterator to_address result is not convertible to span pointer");
        assert((count == 0 || detail::to_address(first) != nullptr)
               && "span: null pointer with non-zero count");
    }

    // (It, count) for fixed extent — explicit
    template <typename It,
              std::size_t E = Extent,
              std::enable_if_t<E != dynamic_extent> * = nullptr>
    constexpr explicit span(It first, size_type count) noexcept
        : storage_(detail::span_to_address(first), count)
    {
        static_assert(
          detail::is_array_convertible_v<T,
                                         std::remove_reference_t<decltype(*std::declval<It &>())>>,
          "span: iterator value type is not compatible");
        static_assert(
          std::is_convertible_v<decltype(detail::to_address(std::declval<It &>())), T *>,
          "span: iterator to_address result is not convertible to span pointer");
        assert(count == E && "span: size mismatch for fixed-extent span");
        assert((count == 0 || detail::to_address(first) != nullptr)
               && "span: null pointer with non-zero count");
    }

    // (It, End) for dynamic extent — implicit
    template <
      typename It,
      typename End,
      std::size_t E = Extent,
      std::enable_if_t<E == dynamic_extent && !std::is_convertible_v<End, size_type>> * = nullptr>
    constexpr span(It first, End last) noexcept(noexcept(last - first))
        : storage_(detail::span_to_address(first), static_cast<size_type>(last - first))
    {
        static_assert(
          detail::is_array_convertible_v<T,
                                         std::remove_reference_t<decltype(*std::declval<It &>())>>,
          "span: iterator value type is not compatible");
        static_assert(
          std::is_convertible_v<decltype(detail::to_address(std::declval<It &>())), T *>,
          "span: iterator to_address result is not convertible to span pointer");
        assert((last - first >= 0) && "span: (It,End) last precedes first");
    }

    // (It, End) for fixed extent — explicit
    template <
      typename It,
      typename End,
      std::size_t E = Extent,
      std::enable_if_t<E != dynamic_extent && !std::is_convertible_v<End, size_type>> * = nullptr>
    constexpr explicit span(It first, End last) noexcept(noexcept(last - first))
        : storage_(detail::span_to_address(first), static_cast<size_type>(last - first))
    {
        static_assert(
          detail::is_array_convertible_v<T,
                                         std::remove_reference_t<decltype(*std::declval<It &>())>>,
          "span: iterator value type is not compatible");
        static_assert(
          std::is_convertible_v<decltype(detail::to_address(std::declval<It &>())), T *>,
          "span: iterator to_address result is not convertible to span pointer");
        assert((last - first >= 0) && "span: (It,End) last precedes first");
        assert(static_cast<size_type>(last - first) == E
               && "span: size mismatch for fixed-extent span");
    }

    template <std::size_t N>
    constexpr span(detail::type_identity_t<element_type> (&arr)[N]) noexcept
        : storage_(arr, N)
    {
        static_assert(Extent == dynamic_extent || Extent == N,
                      "span: array size does not match fixed extent");
    }

    template <typename U,
              std::size_t N,
              typename = std::enable_if_t<Extent == dynamic_extent || Extent == N>>
    constexpr span(std::array<U, N> &arr) noexcept
        : storage_(arr.data(), N)
    {
        static_assert(detail::is_array_convertible_v<T, U>,
                      "span: array element type is not compatible");
    }

    template <typename U,
              std::size_t N,
              typename = std::enable_if_t<Extent == dynamic_extent || Extent == N>>
    constexpr span(const std::array<U, N> &arr) noexcept
        : storage_(arr.data(), N)
    {
        static_assert(detail::is_array_convertible_v<T, const U>,
                      "span: array element type is not compatible");
    }

    template <typename Container,
              std::enable_if_t<!std::is_same_v<detail::remove_cvref_t<Container>, span>
                               && !std::is_array_v<std::remove_reference_t<Container>>
                               && detail::has_data_and_size_v<Container>
                               && (std::is_lvalue_reference_v<Container>
                                   || detail::is_view_v<detail::remove_cvref_t<Container>>)
                               && Extent == dynamic_extent> * = nullptr>
    constexpr span(Container &&cont)
        : storage_(std::forward<Container>(cont).data(), std::forward<Container>(cont).size())
    {
        static_assert(detail::is_array_convertible_v<
                        T,
                        std::remove_pointer_t<decltype(std::data(std::declval<Container &>()))>>,
                      "span: container element type is not compatible");
    }

    template <typename Container,
              std::enable_if_t<!std::is_same_v<detail::remove_cvref_t<Container>, span>
                               && !std::is_array_v<std::remove_reference_t<Container>>
                               && detail::has_data_and_size_v<Container>
                               && (std::is_lvalue_reference_v<Container>
                                   || detail::is_view_v<detail::remove_cvref_t<Container>>)
                               && Extent != dynamic_extent> * = nullptr>
    constexpr explicit span(Container &&cont)
        : storage_(std::forward<Container>(cont).data(), std::forward<Container>(cont).size())
    {
        static_assert(detail::is_array_convertible_v<
                        T,
                        std::remove_pointer_t<decltype(std::data(std::declval<Container &>()))>>,
                      "span: container element type is not compatible");
        assert(cont.size() == Extent && "span: container size mismatch for fixed-extent span");
    }

    template <typename U,
              std::size_t OtherExtent,
              std::enable_if_t<
                (Extent == dynamic_extent || OtherExtent == dynamic_extent || Extent == OtherExtent)
                && detail::is_array_convertible_v<T, U>
                && (Extent == dynamic_extent || OtherExtent != dynamic_extent)> * = nullptr>
    constexpr span(const span<U, OtherExtent> &other) noexcept
        : storage_(other.data(), other.size())
    {
        if constexpr (Extent != dynamic_extent) {
            assert(other.size() == Extent && "span: span size mismatch for fixed-extent span");
        }
    }

    template <typename U,
              std::size_t OtherExtent,
              std::enable_if_t<
                (Extent == dynamic_extent || OtherExtent == dynamic_extent || Extent == OtherExtent)
                && detail::is_array_convertible_v<T, U>
                && (Extent != dynamic_extent && OtherExtent == dynamic_extent)> * = nullptr>
    constexpr explicit span(const span<U, OtherExtent> &other) noexcept
        : storage_(other.data(), other.size())
    {
        assert(other.size() == Extent && "span: span size mismatch for fixed-extent span");
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
        assert(idx < size() && "span: index out of range");
        return data()[idx];
    }

    [[nodiscard]] constexpr reference at(size_type idx) const
    {
        if (idx >= size()) {
            throw std::out_of_range("span::at: index out of range");
        }
        return data()[idx];
    }

    [[nodiscard]] constexpr reference front() const noexcept
    {
        assert(!empty() && "span: front() called on empty span");
        return data()[0];
    }

    [[nodiscard]] constexpr reference back() const noexcept
    {
        assert(!empty() && "span: back() called on empty span");
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

    [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(end());
    }

    [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(begin());
    }

    template <std::size_t Count>
    [[nodiscard]] constexpr span<element_type, Count> first() const noexcept
    {
        if constexpr (Extent == dynamic_extent) {
            assert(Count <= size() && "span::first<Count>(): Count out of range");
        } else {
            static_assert(Count <= Extent, "Count out of bounds in span::first()");
        }

        return span<element_type, Count>(data(), Count);
    }

    template <std::size_t Count>
    [[nodiscard]] constexpr span<element_type, Count> last() const noexcept
    {
        if constexpr (Extent == dynamic_extent) {
            assert(Count <= size() && "span::last<Count>(): Count out of range");
        } else {
            static_assert(Count <= Extent, "Count out of bounds in span::last()");
        }

        return span<element_type, Count>(data() + (size() - Count), Count);
    }

    template <std::size_t Offset, std::size_t Count = dynamic_extent>
    [[nodiscard]] constexpr auto subspan() const noexcept
      -> span<element_type, subspan_extent<Offset, Count>()>
    {
        if constexpr (Extent == dynamic_extent) {
            assert(Offset <= size() && "span::subspan<Offset,Count>(): Offset out of range");
        } else {
            static_assert(Offset <= Extent, "Offset out of bounds in span::subspan()");
        }

        constexpr std::size_t E = subspan_extent<Offset, Count>();

        if constexpr (E != dynamic_extent) {
            if constexpr (Extent == dynamic_extent && Count != dynamic_extent) {
                assert(Count <= (size() - Offset)
                       && "span::subspan<Offset,Count>(): Count out of range");
            } else if constexpr (Extent != dynamic_extent) {
                static_assert(Count == dynamic_extent || Count <= (Extent - Offset),
                              "Count out of bounds");
            }

            return span<element_type, E>(data() + Offset, E);
        } else {
            return { data() + Offset, Count == dynamic_extent ? size() - Offset : Count };
        }
    }

    [[nodiscard]] constexpr span<element_type, dynamic_extent> first(size_type count) const noexcept
    {
        assert(count <= size() && "span::first(count): count out of range");
        return { data(), count };
    }

    [[nodiscard]] constexpr span<element_type, dynamic_extent> last(size_type count) const noexcept
    {
        assert(count <= size() && "span::last(count): count out of range");
        return { data() + (size() - count), count };
    }

    [[nodiscard]] constexpr span<element_type, dynamic_extent>
    subspan(size_type offset, size_type count = dynamic_extent) const noexcept
    {
        assert(offset <= size() && "span::subspan(offset,count): offset out of range");
        if (count == dynamic_extent) {
            return { data() + offset, size() - offset };
        }

        assert(count <= size() - offset && "span::subspan(offset,count): count out of range");
        return { data() + offset, count };
    }

private:
    span_storage<T, Extent> storage_;
};

template <typename T, std::size_t N>
span(T (&)[N]) -> span<T, N>;

template <typename T, std::size_t N>
span(std::array<T, N> &) -> span<T, N>;

template <typename T, std::size_t N>
span(const std::array<T, N> &) -> span<const T, N>;

template <typename It>
span(It, std::size_t) -> span<std::remove_reference_t<decltype(*std::declval<It &>())>>;

template <typename It, typename End>
span(It, End) -> span<std::remove_reference_t<decltype(*std::declval<It &>())>>;

template <typename Container>
span(Container &&) -> span<std::remove_pointer_t<decltype(std::data(std::declval<Container &>()))>>;

template <typename T, std::size_t Extent, std::enable_if_t<!std::is_volatile_v<T>, int> = 0>
[[nodiscard]] constexpr auto as_bytes(span<T, Extent> s) noexcept
{
    using ReturnType =
      span<const std::byte, Extent == dynamic_extent ? dynamic_extent : Extent * sizeof(T)>;
    return ReturnType(reinterpret_cast<const std::byte *>(s.data()), s.size_bytes());
}

template <typename T,
          std::size_t Extent,
          std::enable_if_t<!std::is_const_v<T> && !std::is_volatile_v<T>, int> = 0>
[[nodiscard]] constexpr auto as_writable_bytes(span<T, Extent> s) noexcept
{
    using ReturnType =
      span<std::byte, Extent == dynamic_extent ? dynamic_extent : Extent * sizeof(T)>;
    return ReturnType(reinterpret_cast<std::byte *>(s.data()), s.size_bytes());
}

template <typename T>
struct is_span : std::false_type
{
};

template <typename T, std::size_t Extent>
struct is_span<span<T, Extent>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_span_v = is_span<std::decay_t<T>>::value;

} // namespace linyaps_box::utils
