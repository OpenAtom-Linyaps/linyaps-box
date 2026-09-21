// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/strict_json.h"

#include <fmt/format.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace linyaps_box::utils {

namespace {

// Single-pass strict parser
class strict_dom_parser
{
public:
    using number_integer_t = strict_json::number_integer_t;
    using number_unsigned_t = strict_json::number_unsigned_t;
    using number_float_t = strict_json::number_float_t;
    using string_t = strict_json::string_t;
    using binary_t = strict_json::binary_t;

    auto null() -> bool
    {
        insert(nullptr);
        return true;
    }

    auto boolean(bool val) -> bool
    {
        insert(val);
        return true;
    }

    auto number_integer(number_integer_t val) -> bool
    {
        insert(val);
        return true;
    }

    auto number_unsigned(number_unsigned_t val) -> bool
    {
        insert(val);
        return true;
    }

    auto number_float(number_float_t val, const string_t & /*unused*/) -> bool
    {
        insert(val);
        return true;
    }

    auto binary(binary_t &val) -> bool
    {
        insert(std::move(val));
        return true;
    }

    auto string(string_t &val) -> bool
    {
        check_nul(val, "string contains an embedded NUL byte");
        insert(std::move(val));
        return true;
    }

    auto start_object([[maybe_unused]] std::size_t elements) -> bool
    {
        ref_stack_.push_back(insert(strict_json::object()));
        return true;
    }

    auto key(string_t &val) -> bool
    {
        check_nul(val, "object key contains an embedded NUL byte");

        auto &object = ref_stack_.back()->get_ref<strict_json::object_t &>();
        auto [it, inserted] = object.try_emplace(std::move(val), nullptr);
        if (UNLIKELY(!inserted)) {
            throw strict_json::type_error::create(
              302,
              fmt::format("duplicate object key: '{}'", it->first),
              nullptr);
        }

        pending_slot_ = &it->second;
        return true;
    }

    auto end_object() -> bool
    {
        ref_stack_.pop_back();
        return true;
    }

    auto start_array([[maybe_unused]] std::size_t elems) -> bool
    {
        ref_stack_.push_back(insert(strict_json::array()));
        return true;
    }

    auto end_array() -> bool
    {
        ref_stack_.pop_back();
        return true;
    }

    template <typename Exception>
    auto parse_error([[maybe_unused]] std::size_t position,
                     [[maybe_unused]] const std::string &last_token,
                     const Exception &ex) -> bool
    {
        throw ex;
    }

    auto take_root() -> strict_json { return std::move(root_); }

private:
    static auto check_nul(const string_t &val, const std::string &message) -> void
    {
        if (UNLIKELY(val.find('\0') != string_t::npos)) {
            throw strict_json::type_error::create(302, message, nullptr);
        }
    }

    template <typename T>
    auto insert(T &&value) -> strict_json *
    {
        if (ref_stack_.empty()) {
            root_ = std::forward<T>(value);
            return &root_;
        }

        auto *parent = ref_stack_.back();
        if (parent->is_array()) {
            parent->push_back(std::forward<T>(value));
            return &parent->back();
        }

        *pending_slot_ = std::forward<T>(value);
        return pending_slot_;
    }

    strict_json root_;
    std::vector<strict_json *> ref_stack_;
    strict_json *pending_slot_{ nullptr };
};

} // namespace

auto strict_parse(std::string_view content) -> strict_json
{
    strict_dom_parser parser;

    // The parser's callbacks always return true and report failure by throwing
    // from parse_error, so sax_parse cannot return false here.
    std::ignore = strict_json::sax_parse(content, &parser);

    return parser.take_root();
}

auto require_object(const strict_json &j) -> void
{
    if (UNLIKELY(!j.is_object())) {
        throw strict_json::type_error::create(
          302,
          fmt::format("type must be object, but is {}", j.type_name()),
          &j);
    }
}

} // namespace linyaps_box::utils
