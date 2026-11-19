// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/json/parser.h"

#include "linyaps_box/utils/utf8.h"

#include <nlohmann/json.hpp>

namespace linyaps_box::json::detail {

namespace {

inline auto append_edge(nlohmann::json::json_pointer &path, const location_edge &edge) -> void
{
    switch (edge.type) {
    case location_edge::kind::root: {
    } break;
    case location_edge::kind::field: {
        path /= std::string{ edge.name };
    } break;
    case location_edge::kind::index: {
        path /= edge.index;
    } break;
    }
}

[[nodiscard]] auto build_path(const frame_stack &st, const location_edge &leaf)
  -> nlohmann::json::json_pointer
{
    nlohmann::json::json_pointer path;
    const auto &frames = st.frames();
    for (std::size_t i = 1; i < frames.size(); ++i) {
        append_edge(path, frames[i - 1].ops->segment(frames[i - 1]));
    }

    append_edge(path, leaf);
    return path;
}

class sax_fill_parser
{
public:
    sax_fill_parser(target_ref root, const frame_ops &root_ops)
        : root_(root)
        , root_ops_(&root_ops)
    {
    }

    auto null() -> bool { return value(scalar_value::make_null()); }

    auto boolean(bool v) -> bool { return value(scalar_value::make_bool(v)); }

    auto number_integer(nlohmann::json::number_integer_t v) -> bool
    {
        return value(scalar_value::make_int(v));
    }

    auto number_unsigned(nlohmann::json::number_unsigned_t v) -> bool
    {
        return value(scalar_value::make_uint(v));
    }

    auto number_float(nlohmann::json::number_float_t v, [[maybe_unused]] const std::string &f)
      -> bool
    {
        return value(scalar_value::make_float(v));
    }

    auto string(std::string &v) -> bool
    {
        check_nul(v, "string contains an embedded NUL byte");
        return value(scalar_value::make_string(v));
    }

    auto binary([[maybe_unused]] nlohmann::json::binary_t &b) -> bool
    {
        throw decode_error{ "binary values are not supported" };
    }

    auto key(std::string &v) -> bool
    {
        check_nul(v, "object key contains an embedded NUL byte");

        // `key` is only emitted inside an object, whose frame is always on the
        // stack (the root object is pushed in start_container), so it is safe to
        // read the top frame without an emptiness check.
        auto &frame = frames_.top();
        if (skip_depth_ < 0 && !frame.ops->on_key(frame, frames_, v)) {
            skip_depth_ = 0;
        }

        return true;
    }

    auto start_object([[maybe_unused]] std::size_t s) -> bool { return start_container(true); }

    auto start_array([[maybe_unused]] std::size_t s) -> bool { return start_container(false); }

    auto end_object() -> bool { return end_container(); }

    auto end_array() -> bool { return end_container(); }

    auto parse_error(std::size_t position, const std::string &, const nlohmann::json::exception &ex)
      -> bool
    {
        throw decode_error{
            fmt::format("at input byte {}: {}", position, utils::to_valid_utf8_lossy(ex.what()))
        };
    }

private:
    static constexpr std::size_t max_nesting_depth{ 128 };

    auto start_container(bool is_object) -> bool
    {
        if (UNLIKELY(++nesting_ > max_nesting_depth)) {
            raise_decoded(frames_, location_edge::root(), "JSON nesting exceeds the maximum depth");
        }

        if (UNLIKELY(frames_.empty())) {
            if (!is_object) {
                raise_decoded(frames_, location_edge::root(), "type must be object, but is array");
            }

            frames_.push_frame(root_, *root_ops_);
            return true;
        }

        if (skip_depth_ >= 0) {
            ++skip_depth_;
        } else {
            auto &frame = frames_.top();
            if (is_object) {
                frame.ops->on_object_begin(frame, frames_);
            } else {
                frame.ops->on_array_begin(frame, frames_);
            }
        }

        return true;
    }

    auto end_container() -> bool
    {
        --nesting_;

        if (skip_depth_ < 0) {
            auto &frame = frames_.top();
            frame.ops->on_end(frame, frames_);
        } else {
            assert(skip_depth_ > 0);
            if (--skip_depth_ == 0) {
                skip_depth_ = -1;
            }
        }

        return true;
    }

    auto value(const scalar_value &v) -> bool
    {
        if (UNLIKELY(frames_.empty())) {
            raise_decoded(frames_,
                          location_edge::root(),
                          type_error_message("object", v.type_name()));
        }

        if (skip_depth_ < 0) {
            auto &frame = frames_.top();
            frame.ops->on_value(frame, frames_, v);
        } else if (skip_depth_ == 0) {
            skip_depth_ = -1;
        }

        return true;
    }

    static auto check_nul(const std::string &value, const char *message) -> void
    {
        if (UNLIKELY(value.find('\0') != std::string::npos)) {
            throw decode_error{ message };
        }
    }

    frame_stack frames_;
    target_ref root_;
    const frame_ops *root_ops_{ nullptr };
    std::size_t nesting_{ 0 };
    int skip_depth_{ -1 };
};

} // namespace

scalar_value::scalar_value(storage v) noexcept
    : value_(v)
{
}

auto scalar_value::make_null() noexcept -> scalar_value
{
    return scalar_value{ std::monostate{ } };
}

auto scalar_value::make_bool(bool b) noexcept -> scalar_value
{
    return scalar_value{ b };
}

auto scalar_value::make_int(std::int64_t n) noexcept -> scalar_value
{
    return scalar_value{ n };
}

auto scalar_value::make_uint(std::uint64_t n) noexcept -> scalar_value
{
    return scalar_value{ n };
}

auto scalar_value::make_float(double d) noexcept -> scalar_value
{
    return scalar_value{ d };
}

auto scalar_value::make_string(std::string &s) noexcept -> scalar_value
{
    return scalar_value{ &s };
}

auto scalar_value::is_null() const noexcept -> bool
{
    return std::holds_alternative<std::monostate>(value_);
}

auto scalar_value::is_floating() const noexcept -> bool
{
    return std::holds_alternative<double>(value_);
}

auto scalar_value::is_unsigned() const noexcept -> bool
{
    return std::holds_alternative<std::uint64_t>(value_);
}

auto scalar_value::is_integer() const noexcept -> bool
{
    return std::holds_alternative<std::int64_t>(value_);
}

auto scalar_value::is_number() const noexcept -> bool
{
    return is_floating() || is_unsigned() || is_integer();
}

auto scalar_value::type_name() const noexcept -> std::string_view
{
    using namespace std::string_view_literals;
    return std::visit(utils::Overload{
                        [](std::monostate) noexcept {
                            return "null"sv;
                        },
                        [](bool) noexcept {
                            return "boolean"sv;
                        },
                        [](std::int64_t) noexcept {
                            return "number"sv;
                        },
                        [](std::uint64_t) noexcept {
                            return "number"sv;
                        },
                        [](double) noexcept {
                            return "number"sv;
                        },
                        [](std::string *) noexcept {
                            return "string"sv;
                        },
                      },
                      value_);
}

auto scalar_value::raise_expected(std::string_view expected) const -> void
{
    throw decode_error{ type_error_message(expected, type_name()) };
}

auto scalar_value::as_bool() const -> bool
{
    if (const auto *p = std::get_if<bool>(&value_)) {
        return *p;
    }

    raise_expected("boolean");
}

auto scalar_value::as_string_view() const -> std::string_view
{
    if (const auto *p = std::get_if<std::string *>(&value_)) {
        return **p;
    }

    raise_expected("string");
}

auto scalar_value::int_raw() const noexcept -> std::int64_t
{
    return *std::get_if<std::int64_t>(&value_);
}

auto scalar_value::uint_raw() const noexcept -> std::uint64_t
{
    return *std::get_if<std::uint64_t>(&value_);
}

auto scalar_value::float_raw() const noexcept -> double
{
    return *std::get_if<double>(&value_);
}

auto type_error_message(std::string_view expected, std::string_view actual) -> std::string
{
    return fmt::format("type must be {}, but is {}", expected, actual);
}

auto raise_type_error(std::string_view expected, std::string_view actual) -> void
{
    throw decode_error{ type_error_message(expected, actual) };
}

auto raise_number_out_of_range() -> void
{
    throw decode_error{ "number out of range for the target integer type" };
}

auto location_edge::root() noexcept -> location_edge
{
    return { };
}

auto location_edge::field(std::string_view n) noexcept -> location_edge
{
    location_edge e;
    e.type = kind::field;
    e.name = n;
    return e;
}

auto location_edge::element(std::size_t i) noexcept -> location_edge
{
    location_edge e;
    e.type = kind::index;
    e.index = i;
    return e;
}

frame::frame(target_ref t, const frame_ops &o) noexcept
    : target(t)
    , ops(&o)
{
}

frame_stack::frame_stack()
{
    frames_.reserve(8);
}

auto frame_stack::frames() const noexcept -> const std::vector<frame> &
{
    return frames_;
}

auto frame_stack::push_frame(target_ref target, const frame_ops &ops) -> void
{
    frames_.emplace_back(target, ops);
}

auto frame_stack::pop() -> void
{
    frames_.pop_back();
}

auto frame_stack::top() -> frame &
{
    return frames_.back();
}

auto frame_stack::empty() const noexcept -> bool
{
    return frames_.empty();
}

[[noreturn]] auto raise_decoded(const frame_stack &st,
                                const location_edge &leaf,
                                std::string_view message) -> void
{
    const auto path = build_path(st, leaf).to_string();
    throw decode_error{ fmt::format("at '{}': {}", path.empty() ? "<root>" : path, message) };
}

void parse_document(std::string_view content, target_ref root, const frame_ops &root_ops)
{
    sax_fill_parser parser(root, root_ops);
    std::ignore = nlohmann::json::sax_parse(content, &parser);
}

} // namespace linyaps_box::json::detail
