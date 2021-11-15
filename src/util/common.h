/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#pragma GCC diagnostic ignored "-Wformat-security"

#include <vector>
#include <iostream>
#include <iterator>
#include <memory>

#if (__cplusplus <= 201103L)

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace std {
template<class T>
struct _Unique_if {
    typedef unique_ptr<T> _Single_object;
};

template<class T>
struct _Unique_if<T[]> {
    typedef unique_ptr<T[]> _Unknown_bound;
};

template<class T, size_t N>
struct _Unique_if<T[N]> {
    typedef void _Known_bound;
};

template<class T, class... Args>
typename _Unique_if<T>::_Single_object make_unique(Args &&...args)
{
    return unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<class T>
typename _Unique_if<T>::_Unknown_bound make_unique(size_t n)
{
    typedef typename remove_extent<T>::type U;
    return unique_ptr<T>(new U[n]());
}

template<class T, class... Args>
typename _Unique_if<T>::_Known_bound make_unique(Args &&...) = delete;
} // namespace std

#endif

namespace linglong {
namespace util {

typedef std::vector<std::string> str_vec;

str_vec str_spilt(const std::string &s, const std::string &sep);

std::string str_vec_join(const str_vec &vec, char sep);

template<typename... Args>
std::string format(const std::string &format, Args... args)
{
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
    if (size_s <= 0) {
        throw std::runtime_error("format error");
    }
    auto size = static_cast<size_t>(size_s);
    auto buf = std::make_unique<char[]>(size);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

class try_break : public std::logic_error
{
public:
    explicit try_break(const std::string &what)
        : std::logic_error(what)
    {
    }
    ~try_break() override = default;
};

} // namespace util
} // namespace linglong

template<typename T>
std::ostream &operator<<(std::ostream &out, const std::vector<T> &v)
{
    if (!v.empty()) {
        out << '[';
        std::copy(v.begin(), v.end(), std::ostream_iterator<T>(out, ", "));
        out << "\b\b]";
    }
    return out;
}