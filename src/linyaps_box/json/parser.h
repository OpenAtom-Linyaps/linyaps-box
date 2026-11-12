// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

// SAX-based JSON direct-fill parsing.
//
// Instead of building a nlohmann DOM and then walking it a second time, the
// parser drives nlohmann's `sax_parse` with a handler that fills the target
// structs directly, using the type's registered schema table of field
// descriptors. It keeps nlohmann's lexer (and therefore its exact strictness
// and embedded-NUL handling) while skipping the intermediate tree and the
// second traversal.
//
// Why this layer validates on its own: nlohmann/json deliberately leaves input
// hardening to the caller (see https://json.nlohmann.me/features/parsing/untrusted_input)
// so the binder adds the parts upstream names as the caller's responsibility.
//   - number range: upstream get<T>() is an unchecked static_cast and UB out of
//     range (#288, #647), checked_from_* enforce the range and reject a
//     fractional part.
//   - nesting depth: upstream parsing has no depth limit ("track the depth
//     yourself", #5393). max_nesting_depth caps it here.
//   - duplicate keys: upstream keeps one value of a repeated key and offers only
//     a parser-callback recipe (#375, #4052), a repeated known field or map key
//     is rejected here. A repeated unknown key is not: unknown values are
//     skipped without recording their names (see object_frame_ops::on_key).
//   - NUL: upstream accepts \u0000 and treats a raw NUL as EOF (#1095,
//     JSON_STRICT_NUL_HANDLING is opt-in, default only in 4.0.0). parse_as
//     rejects a raw NUL byte up front, and check_nul rejects a decoded \u0000
//     in both string values and keys.
//   - required / null: upstream does no schema validation, required() and
//     ignore_null() encode it here.
//   - field shape: upstream checks only the JSON type at get<T>(), the frame
//     handlers check the expected object/array/scalar shape.
//   - error location: upstream has no position in SAX callbacks (#4649,
//     declined), build_path/raise_decoded rebuild a JSON Pointer from the live
//     frame stack on failure only.
// UTF-8 and trailing input are the two checks reused from upstream: its lexer
// rejects ill-formed UTF-8 in strings and keys (lone/overlong sequences,
// UTF-8-encoded surrogates, code points above U+10FFFF, and \uD800 escapes),
// and strict sax_parse rejects trailing content. Valid UTF-8 is a hard
// requirement, so parsed strings never need a lenient dump error handler.

#include "linyaps_box/json/dsl_fwd.h"
#include "linyaps_box/json/error.h"
#include "linyaps_box/utils/enum_formatter.h" // IWYU pragma: keep
#include "linyaps_box/utils/enum_traits.h"
#include "linyaps_box/utils/utf8.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

#include <array>
#include <bitset>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace linyaps_box::json {

namespace detail {
class value_view;
struct frame;
class frame_stack;
struct location_edge;
} // namespace detail

struct frame_ops
{
    bool (*on_key)(detail::frame &, detail::frame_stack &, std::string &);
    void (*on_value)(detail::frame &, detail::frame_stack &, const detail::value_view &);
    void (*on_object_begin)(detail::frame &, detail::frame_stack &);
    void (*on_array_begin)(detail::frame &, detail::frame_stack &);
    // Completes this frame at its closing bracket. nlohmann json guarantees bracket
    // matching and the route filters skipped subtrees, so a frame only ever sees
    // its own end, whatever the container kind.
    void (*on_end)(detail::frame &, detail::frame_stack &);
    // The frame's active child edge, derived from its own cursor state.  Used
    // only on failure, to rebuild the logical path from the frame stack.
    detail::location_edge (*segment)(const detail::frame &);
};

template <typename T>
struct object_tag
{
    using target_type = T;
};

template <typename T>
struct array_tag
{
    using target_type = T;
};

template <typename T>
struct map_tag
{
    using target_type = T;
};

template <auto Parse, typename Owner>
struct string_array_tag
{
    using target_type = Owner;
};

namespace detail {

inline constexpr std::size_t field_index_npos{ static_cast<std::size_t>(-1) };

inline constexpr std::size_t max_fields{ 32 };

// Maximum JSON nesting depth accepted by the parser.
inline constexpr std::size_t max_nesting_depth{ 128 };

// A borrowed view of one JSON scalar value delivered by the SAX handler:
// null, boolean, signed/unsigned integer, double, or a string pointing at the
// lexer's reused buffer (valid until the next callback).
// Object/array are handled by the SAX begin events, not carried here.
class value_view
{
public:
    using storage =
      std::variant<std::monostate, bool, std::int64_t, std::uint64_t, double, std::string *>;

    static auto make_null() noexcept -> value_view { return value_view{ std::monostate{ } }; }

    static auto make_bool(bool b) noexcept -> value_view { return value_view{ b }; }

    static auto make_int(std::int64_t n) noexcept -> value_view { return value_view{ n }; }

    static auto make_uint(std::uint64_t n) noexcept -> value_view { return value_view{ n }; }

    static auto make_float(double d) noexcept -> value_view { return value_view{ d }; }

    // non-const lvalue reference can't bind to a temporary
    // it could be prevented some lifetime issues
    static auto make_string(std::string &s) noexcept -> value_view { return value_view{ &s }; }

    [[nodiscard]] auto is_null() const noexcept -> bool
    {
        return std::holds_alternative<std::monostate>(value_);
    }

    [[nodiscard]] auto is_string() const noexcept -> bool
    {
        return std::holds_alternative<std::string *>(value_);
    }

    [[nodiscard]] auto is_floating() const noexcept -> bool
    {
        return std::holds_alternative<double>(value_);
    }

    [[nodiscard]] auto is_unsigned() const noexcept -> bool
    {
        return std::holds_alternative<std::uint64_t>(value_);
    }

    [[nodiscard]] auto is_integer() const noexcept -> bool
    {
        return std::holds_alternative<std::int64_t>(value_);
    }

    [[nodiscard]] auto is_number() const noexcept -> bool
    {
        return is_floating() || is_unsigned() || is_integer();
    }

    [[nodiscard]] auto type_name() const noexcept -> std::string_view
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

    [[noreturn]] auto raise_expected(std::string_view expected) const -> void
    {
        throw decode_error{ fmt::format("type must be {}, but is {}", expected, type_name()) };
    }

    [[nodiscard]] auto as_bool() const -> bool
    {
        if (const auto *p = std::get_if<bool>(&value_)) {
            return *p;
        }

        raise_expected("boolean");
    }

    [[nodiscard]] auto as_string_view() const -> std::string_view
    {
        if (const auto *p = std::get_if<std::string *>(&value_)) {
            return **p;
        }

        raise_expected("string");
    }

    [[nodiscard]] auto int_raw() const noexcept -> std::int64_t
    {
        return *std::get_if<std::int64_t>(&value_);
    }

    [[nodiscard]] auto uint_raw() const noexcept -> std::uint64_t
    {
        return *std::get_if<std::uint64_t>(&value_);
    }

    [[nodiscard]] auto float_raw() const noexcept -> double
    {
        return *std::get_if<double>(&value_);
    }

private:
    explicit value_view(storage v) noexcept
        : value_(v)
    {
    }

    storage value_;
};

[[nodiscard]] inline auto type_error_message(std::string_view expected, std::string_view actual)
  -> std::string
{
    return fmt::format("type must be {}, but is {}", expected, actual);
}

[[noreturn]] inline auto raise_type_error(std::string_view expected, std::string_view actual)
  -> void
{
    throw decode_error{ type_error_message(expected, actual) };
}

template <typename T>
struct member_traits;

template <typename Member, typename Owner>
struct member_traits<Member Owner::*>
{
    using type = Member;
};

template <auto Callback, typename Signature>
inline constexpr bool is_non_null_callback_v = [] {
    if constexpr (std::is_convertible_v<decltype(Callback), Signature>) {
        // Avoid GCC's UBSan-sensitive constexpr comparison of template function pointers.
        using callback = std::integral_constant<Signature, Callback>;
        using null_callback = std::integral_constant<Signature, nullptr>;
        return !std::is_same_v<callback, null_callback>;
    }

    return false;
}();

template <typename T>
struct is_optional : std::false_type
{
};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type
{
    using type = T;
};

template <typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

template <typename T>
struct unwrap_optional
{
    using type = T;
};

template <typename T>
struct unwrap_optional<std::optional<T>>
{
    using type = T;
};

template <typename T>
using unwrap_optional_t = typename unwrap_optional<T>::type;

// A scalar is anything the value callbacks can fill directly: arithmetic,
// bool, std::string, std::filesystem::path or an enum.
template <typename T>
inline constexpr bool is_scalar_v = std::is_arithmetic_v<T> || std::is_enum_v<T>
  || std::is_same_v<T, std::string> || std::is_same_v<T, std::filesystem::path>;

template <typename U>
constexpr auto scalar_expected_name() noexcept -> std::string_view
{
    using namespace std::string_view_literals;
    if constexpr (std::is_same_v<U, bool>) {
        return "boolean"sv;
    } else if constexpr (std::is_enum_v<U>) {
        return utils::has_enum_table_v<U> ? "string"sv : "number"sv;
    } else if constexpr (std::is_same_v<U, std::string>
                         || std::is_same_v<U, std::filesystem::path>) {
        return "string"sv;
    } else {
        static_assert(std::is_arithmetic_v<U>, "unsupported scalar member type");
        return "number"sv;
    }
}

enum class field_kind : std::uint8_t {
    scalar,
    object,
    array,
    map,
};

template <typename T>
struct field_traits
{
    static constexpr field_kind kind = is_scalar_v<T> ? field_kind::scalar : field_kind::object;
    using frame_tag = object_tag<T>;
};

template <typename X, typename A>
struct field_traits<std::vector<X, A>>
{
    static constexpr field_kind kind = field_kind::array;
    using element_type = X;
    using frame_tag = array_tag<std::vector<X, A>>;
};

template <typename V>
struct field_traits<std::unordered_map<std::string, V>>
{
    static constexpr field_kind kind = field_kind::map;
    using element_type = V;
    using frame_tag = map_tag<std::unordered_map<std::string, V>>;
};

template <typename E>
struct field_traits<utils::bitflags<E>>
{
    static constexpr field_kind kind = field_kind::array;
    using frame_tag = array_tag<utils::bitflags<E>>;
};

// A JSON number must never be silently truncated into the target integer:
// nlohmann's own get<T>() is an unchecked static_cast and is undefined
// behaviour when the double is not representable.
[[noreturn]] inline auto raise_number_out_of_range() -> void
{
    throw decode_error{ "number out of range for the target integer type" };
}

template <typename T>
[[nodiscard]] auto checked_from_float(double value) -> T
{
    static_assert(std::is_integral_v<T>, "checked_from_float requires an integer type");
    constexpr auto digits = std::numeric_limits<T>::digits;
    constexpr auto upper = [] {
        double bound = 1.0;
        for (int i = 0; i < digits; ++i) {
            bound *= 2.0;
        }

        return bound;
    }();

    constexpr auto lower = std::is_signed_v<T> ? -upper : 0.0;
    if (UNLIKELY(!std::isfinite(value) || value < lower || value >= upper)) {
        raise_number_out_of_range();
    }

    if (UNLIKELY(std::trunc(value) != value)) {
        throw decode_error{ "number has a fractional part" };
    }

    return static_cast<T>(value);
}

template <typename T>
[[nodiscard]] auto checked_from_unsigned(std::uint64_t value) -> T
{
    static_assert(std::is_integral_v<T>, "checked_from_unsigned requires an integer type");
    if (UNLIKELY(value > static_cast<std::uint64_t>((std::numeric_limits<T>::max)()))) {
        raise_number_out_of_range();
    }

    return static_cast<T>(value);
}

template <typename T>
[[nodiscard]] auto checked_from_signed(std::int64_t value) -> T
{
    static_assert(std::is_integral_v<T>, "checked_from_signed requires an integer type");
    if (value < 0) {
        bool out_of_range = std::is_unsigned_v<T>;
        if constexpr (!std::is_unsigned_v<T>) {
            out_of_range = value < static_cast<std::int64_t>((std::numeric_limits<T>::min)());
        }

        if (UNLIKELY(out_of_range)) {
            raise_number_out_of_range();
        }
    } else if (UNLIKELY(static_cast<std::uint64_t>(value)
                        > static_cast<std::uint64_t>((std::numeric_limits<T>::max)()))) {
        raise_number_out_of_range();
    }

    return static_cast<T>(value);
}

template <typename T>
auto number_from(const value_view &v) -> T
{
    if constexpr (std::is_floating_point_v<T>) {
        if (LIKELY(v.is_number())) {
            if (v.is_floating()) {
                return static_cast<T>(v.float_raw());
            }

            if (v.is_unsigned()) {
                return static_cast<T>(v.uint_raw());
            }

            return static_cast<T>(v.int_raw());
        }

        v.raise_expected("number");
    } else {
        static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>,
                      "number_from requires a plain integer or floating type");

        if (v.is_floating()) {
            return checked_from_float<T>(v.float_raw());
        }

        if (v.is_unsigned()) {
            return checked_from_unsigned<T>(v.uint_raw());
        }

        if (v.is_integer()) {
            return checked_from_signed<T>(v.int_raw());
        }

        v.raise_expected("number");
    }
}

template <typename E>
[[nodiscard]] constexpr auto enum_from_name(std::string_view name) -> E
{
    static_assert(utils::has_enum_table_v<E>, "enum requires a registered name table");
    constexpr auto &table = utils::enum_table_v<E>;
    const auto value = table.from_name(name);
    if (UNLIKELY(!value)) {
        using namespace std::string_view_literals;

        const auto &entries = table.entries();
        throw decode_error{
            fmt::format("value must be one of {}: {}",
                        fmt::join(entries.cbegin(), entries.cend(), "/"sv),
                        name),
        };
    }

    return *value;
}

template <typename T>
auto assign_enum(const value_view &v, T &out) -> void
{
    if constexpr (utils::has_enum_table_v<T>) {
        out = enum_from_name<T>(v.as_string_view());
    } else {
        using underlying = std::underlying_type_t<T>;
        out = static_cast<T>(number_from<underlying>(v));
    }
}

template <typename U>
void assign_scalar(const value_view &v, U &out)
{
    using T = std::remove_cv_t<U>;
    if constexpr (std::is_same_v<T, bool>) {
        out = v.as_bool();
    } else if constexpr (std::is_enum_v<T>) {
        assign_enum(v, out);
    } else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
        out = number_from<T>(v);
    } else if constexpr (std::is_same_v<T, std::string>) {
        // Copy rather than move: the lexer reuses its token buffer, and moving
        // out would force it to re-grow the buffer character by character for
        // the next string value.
        out.assign(v.as_string_view());
    } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
        out = std::filesystem::path{ v.as_string_view() };
    } else {
        static_assert(std::is_same_v<T, void>, "unsupported scalar member type");
    }
}

// The active child of a container frame.
// `name` is a static field name or a dynamic map key borrowed
// from the target map's stable storage, the path snapshot copies it on failure.
struct location_edge
{
    enum class kind : std::uint8_t { root, field, index };

    std::string_view name;
    std::size_t index{ 0 };
    kind type{ kind::root };

    [[nodiscard]] static auto root() noexcept -> location_edge { return { }; }

    [[nodiscard]] static auto field(std::string_view n) noexcept -> location_edge
    {
        location_edge e;
        e.type = kind::field;
        e.name = n;
        return e;
    }

    [[nodiscard]] static auto element(std::size_t i) noexcept -> location_edge
    {
        location_edge e;
        e.type = kind::index;
        e.index = i;
        return e;
    }
};

template <typename Derived>
struct frame_handler
{
    [[nodiscard]] static constexpr auto make() -> frame_ops
    {
        return frame_ops{
            &Derived::on_key,         &Derived::on_value, &Derived::on_object_begin,
            &Derived::on_array_begin, &Derived::on_end,   &Derived::segment,
        };
    }
};

// Consumes the value of an unknown object key without pushing a frame.
class skip_counter
{
public:
    enum class action : bool { consume, handle };

    enum class state : std::uint8_t {
        idle,
        skipping_value,
        skipping_container,
    };

    [[nodiscard]] constexpr auto on_key() const noexcept -> action
    {
        return state_ == state::idle ? action::handle : action::consume;
    }

    [[nodiscard]] constexpr auto on_value() noexcept -> action
    {
        switch (state_) {
        case state::idle: {
            return action::handle;
        }
        case state::skipping_value: {
            // The unknown key's value was a scalar: the skip is done.
            state_ = state::idle;
            return action::consume;
        }
        case state::skipping_container: {
            return action::consume;
        }
        }

        return action::handle;
    }

    [[nodiscard]] constexpr auto on_begin() noexcept -> action
    {
        switch (state_) {
        case state::idle: {
            return action::handle;
        }
        case state::skipping_value: {
            // The unknown key's value is a container: start counting it.
            state_ = state::skipping_container;
            depth_ = 1;
            return action::consume;
        }
        case state::skipping_container: {
            ++depth_;
            return action::consume;
        }
        }

        return action::handle;
    }

    [[nodiscard]] constexpr auto on_end() noexcept -> action
    {
        switch (state_) {
        case state::idle: {
            return action::handle;
        }
        case state::skipping_value: {
            // A key is always followed by a value or a container start, never by
            // an end, so nlohmann's SAX stream cannot reach this.
            assert(false && "container end right after an object key");
            return action::consume;
        }
        case state::skipping_container: {
            if (--depth_ == 0) {
                state_ = state::idle;
            }

            return action::consume;
        }
        }

        return action::handle;
    }

    constexpr auto begin_skip() noexcept -> void
    {
        state_ = state::skipping_value;
        depth_ = 0;
    }

private:
    std::size_t depth_{ 0 };
    state state_{ state::idle };
};

// A type-erased, non-owning handle to the object a frame is filling.
class target_ref
{
public:
    target_ref() noexcept = default;

    // The expected type is stated instead of deduced
    template <typename T, typename Object>
    [[nodiscard]] static auto of(Object &object) noexcept -> target_ref
    {
        static_assert(std::is_same_v<std::remove_cv_t<Object>, T>,
                      "target_ref::of<T> expects an lvalue of exactly T");
        target_ref ref;
        ref.ptr_ = static_cast<void *>(&object);
        return ref;
    }

    template <typename T>
    [[nodiscard]] auto as() const noexcept -> T &
    {
        assert(ptr_ != nullptr);
        return *static_cast<T *>(ptr_);
    }

private:
    void *ptr_{ nullptr };
};

// A frame fills one target object, array, or map.
//
// `child` and `key` borrow into the current map node and are read only during a
// single member's value window (from its `on_key` until that value, with any
// nested subtree, completes).  That window is safe because:
//   - std::unordered_map never invalidates references/pointers to elements on
//     insert or rehash (only iterators), so the node never moves; and
//   - during the window we neither insert into this map (the next insert opens
//     the next member's window) nor erase from it.
// `child` also becomes the child frame's `target` for a nested value, within the
// same window.
struct frame
{
    frame(target_ref t, const frame_ops &o) noexcept
        : target(t)
        , ops(&o)
    {
    }

    target_ref target;

    // A function-local static constexpr table, so the pointer stays valid.
    const frame_ops *ops;

    // Meaning depends on the frame kind:
    //   - object:        index of the active field in the schema table
    //   - string-array:  index of the next element to fill (flags/options)
    //   - plain vector / map: unused
    std::size_t active{ field_index_npos };

    // For object frames, the set of known fields that have been seen so far.
    std::bitset<max_fields> seen;

    // For map frames, borrowed handle into the current value's map node.
    target_ref child;

    // For map frames, borrowed view of the current key in the map node.
    std::string_view key;
};

class frame_stack
{
public:
    frame_stack() { frames_.reserve(16); }

    [[nodiscard]] auto frames() const noexcept -> const std::vector<frame> & { return frames_; }

    template <typename Tag>
    auto push_frame(typename Tag::target_type &target, Tag tag) -> void
    {
        frames_.emplace_back(target_ref::of<typename Tag::target_type>(target), ops_of(tag));
    }

    auto pop() -> void { frames_.pop_back(); }

    [[nodiscard]] auto top() -> frame & { return frames_.back(); }

    [[nodiscard]] auto empty() const noexcept -> bool { return frames_.empty(); }

private:
    std::vector<frame> frames_;
};

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

[[nodiscard]] inline auto build_path(const frame_stack &st, const location_edge &leaf)
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

[[noreturn]] inline auto raise_decoded(const frame_stack &st,
                                       const location_edge &leaf,
                                       std::string_view message) -> void
{
    const auto path = build_path(st, leaf).to_string();
    throw decode_error{ fmt::format("at '{}': {}", path.empty() ? "<root>" : path, message) };
}

template <typename Owner>
auto push_child_frame(frame_stack &st, Owner &child) -> void
{
    st.push_frame(child, typename field_traits<Owner>::frame_tag{ });
    if constexpr (field_traits<Owner>::kind == field_kind::array) {
        st.top().active = 0;
    }
}

template <typename Derived>
class field_descriptor
{
public:
    explicit constexpr field_descriptor(std::string_view field_name) noexcept
        : name_(field_name)
    {
    }

    [[nodiscard]] constexpr auto name() const noexcept -> std::string_view { return name_; }

    [[nodiscard]] constexpr auto is_required() const noexcept -> bool { return is_required_; }

    [[nodiscard]] constexpr auto required() const -> Derived
    {
        auto result = static_cast<const Derived &>(*this);
        result.is_required_ = true;
        return result;
    }

    [[nodiscard]] constexpr auto ignore_null() const -> Derived
    {
        auto result = static_cast<const Derived &>(*this);
        result.allows_null_ = true;
        return result;
    }

    [[nodiscard]] auto skip_null(const value_view &v) const -> bool
    {
        if (!v.is_null()) {
            return false;
        }

        if (allows_null_) {
            return true;
        }

        throw decode_error{ "member must not be null" };
    }

    template <typename Owner>
    void begin_object([[maybe_unused]] frame_stack &st, [[maybe_unused]] Owner &obj) const
    {
        raise_type_error(expected_name(), "object");
    }

    template <typename Owner>
    void begin_array([[maybe_unused]] frame_stack &st, [[maybe_unused]] Owner &obj) const
    {
        raise_type_error(expected_name(), "array");
    }

private:
    [[nodiscard]] static constexpr auto expected_name() noexcept -> std::string_view
    {
        return Derived::kind == field_kind::array ? "array" : "scalar";
    }

    std::string_view name_;
    bool is_required_{ false };
    bool allows_null_{ false };
};

template <auto Member>
struct member_field : public field_descriptor<member_field<Member>>
{
    using base = field_descriptor<member_field<Member>>;
    using base::base;
    using member_type = typename member_traits<decltype(Member)>::type;
    using value_type = unwrap_optional_t<member_type>;

    static constexpr bool optional_type = is_optional_v<member_type>;
    static constexpr field_kind kind = field_traits<value_type>::kind;

    template <typename Owner>
    auto assign_value(const value_view &v, Owner &obj) const -> void
    {
        if (this->skip_null(v)) {
            return;
        }

        if constexpr (kind == field_kind::scalar) {
            if constexpr (optional_type) {
                value_type value{ };
                assign_scalar(v, value);
                obj.*Member = std::move(value);
            } else {
                assign_scalar(v, obj.*Member);
            }
        } else {
            v.raise_expected(element_name());
        }
    }

    template <typename Owner>
    auto begin_object(frame_stack &st, Owner &obj) const -> void
    {
        if constexpr (kind == field_kind::object || kind == field_kind::map) {
            push_child_frame(st, payload(obj));
        } else {
            raise_type_error(element_name(), "object");
        }
    }

    template <typename Owner>
    auto begin_array(frame_stack &st, Owner &obj) const -> void
    {
        if constexpr (kind == field_kind::array) {
            push_child_frame(st, payload(obj));
        } else {
            raise_type_error(element_name(), "array");
        }
    }

private:
    [[nodiscard]] static constexpr auto element_name() noexcept -> std::string_view
    {
        if constexpr (kind == field_kind::scalar) {
            return scalar_expected_name<value_type>();
        } else {
            return kind == field_kind::array ? "array" : "object";
        }
    }

    template <typename Owner>
    static auto payload(Owner &obj) -> value_type &
    {
        if constexpr (optional_type) {
            return (obj.*Member).emplace();
        } else {
            return obj.*Member;
        }
    }
};

template <auto Member, auto Read>
struct converted_field : public field_descriptor<converted_field<Member, Read>>
{
    using base = field_descriptor<converted_field<Member, Read>>;
    using base::base;
    using member_type = typename member_traits<decltype(Member)>::type;
    using value_type = unwrap_optional_t<member_type>;

    static constexpr field_kind kind = field_kind::scalar;

    static_assert(is_non_null_callback_v<Read, value_type (*)(const value_view &)>,
                  "field conversion must be a non-null function pointer with signature "
                  "Payload(const value_view&)");

    template <typename Owner>
    auto assign_value(const value_view &v, Owner &obj) const -> void
    {
        if (!this->skip_null(v)) {
            obj.*Member = Read(v);
        }
    }
};

template <auto Check>
struct checked_field : public field_descriptor<checked_field<Check>>
{
    using base = field_descriptor<checked_field<Check>>;
    using base::base;

    static constexpr field_kind kind = field_kind::scalar;

    static_assert(is_non_null_callback_v<Check, void (*)(const value_view &)>,
                  "check must be a non-null function pointer with signature "
                  "void(const value_view&)");

    template <typename Owner>
    auto assign_value(const value_view &v, [[maybe_unused]] Owner &obj) const -> void
    {
        if (!this->skip_null(v)) {
            Check(v);
        }
    }
};

template <auto Parse>
struct custom_array_field : public field_descriptor<custom_array_field<Parse>>
{
    using base = field_descriptor<custom_array_field<Parse>>;
    using base::base;

    static constexpr field_kind kind = field_kind::array;

    template <typename Owner>
    auto assign_value(const value_view &v, [[maybe_unused]] Owner &obj) const -> void
    {
        if (!this->skip_null(v)) {
            v.raise_expected("array");
        }
    }

    template <typename Owner>
    auto begin_array(frame_stack &st, Owner &obj) const -> void
    {
        st.push_frame(obj, string_array_tag<Parse, Owner>{ });
        st.top().active = 0;
    }
};

// A registered schema is a user-supplied struct with a static constexpr
// `fields` (the tuple of field descriptors) plus, optionally, a
// `post_parse(Owner &)` function.
template <typename Schema, typename Owner, typename = void>
struct has_post_parse : std::false_type
{
};

template <typename Schema, typename Owner>
struct has_post_parse<Schema,
                      Owner,
                      std::void_t<decltype(Schema::post_parse(std::declval<Owner &>()))>>
    : std::true_type
{
};

template <typename T, typename Schema>
struct object_frame_ops : frame_handler<object_frame_ops<T, Schema>>
{
    using fields_type = decltype(Schema::fields());
    static constexpr fields_type table = Schema::fields();
    static constexpr std::size_t count = std::tuple_size_v<fields_type>;
    static_assert(count <= max_fields, "the schema has more fields than max_fields");

    [[nodiscard]] static auto segment(const frame &f) -> location_edge { return leaf_of(f); }

    static auto on_value(frame &f, frame_stack &st, const value_view &v) -> void
    {
        try {
            value_dispatch(f, v, std::make_index_sequence<count>{ });
        } catch (const decode_error &e) {
            raise_decoded(st, leaf_of(f), e.what());
        }
    }

    static auto on_key(frame &f, frame_stack &st, std::string &key) -> bool
    {
        const auto idx = find_key(key);
        if (idx != field_index_npos) {
            if (UNLIKELY(f.seen.test(idx))) {
                raise_decoded(st,
                              location_edge::field(name_at(idx)),
                              fmt::format("duplicate object key: '{}'", key));
            }

            f.seen.set(idx);
            f.active = idx;
            return true;
        }

        // An unknown key is simply discarded: its value is skipped and nothing
        // consumes it, so we neither record the name nor reject a repeat of it.
        f.active = field_index_npos;
        return false;
    }

    static auto on_object_begin(frame &f, frame_stack &st) -> void
    {
        try {
            object_dispatch(f, st, std::make_index_sequence<count>{ });
        } catch (const decode_error &e) {
            raise_decoded(st, leaf_of(f), e.what());
        }
    }

    static auto on_array_begin(frame &f, frame_stack &st) -> void
    {
        try {
            array_dispatch(f, st, std::make_index_sequence<count>{ });
        } catch (const decode_error &e) {
            raise_decoded(st, leaf_of(f), e.what());
        }
    }

    static auto on_end(frame &f, frame_stack &st) -> void { finish(f, st); }

private:
    template <std::size_t... I>
    static auto find_key_impl(std::string_view key,
                              [[maybe_unused]] std::index_sequence<I...> seq) noexcept
      -> std::size_t
    {
        std::size_t result = field_index_npos;
        std::ignore = ((result != field_index_npos
                        || (std::get<I>(table).name() == key ? (result = I, true) : false))
                       || ...);
        return result;
    }

    template <std::size_t... I>
    static auto name_at_impl(std::size_t i, [[maybe_unused]] std::index_sequence<I...> seq) noexcept
      -> std::string_view
    {
        std::string_view result;
        std::ignore = ((i == I ? (result = std::get<I>(table).name(), true) : false) || ...);
        return result;
    }

    [[nodiscard]] static auto find_key(std::string_view key) noexcept -> std::size_t
    {
        return find_key_impl(key, std::make_index_sequence<count>{ });
    }

    [[nodiscard]] static auto name_at(std::size_t i) noexcept -> std::string_view
    {
        return name_at_impl(i, std::make_index_sequence<count>{ });
    }

    template <std::size_t I>
    static auto value_one(frame &f, const value_view &v) -> void
    {
        auto &obj = f.target.as<T>();
        std::get<I>(table).assign_value(v, obj);
    }

    template <std::size_t... I>
    static auto value_dispatch(frame &f,
                               const value_view &v,
                               [[maybe_unused]] std::index_sequence<I...> seq) -> void
    {
        std::ignore = ((f.active == I ? (value_one<I>(f, v), true) : false) || ...);
    }

    [[nodiscard]] static auto leaf_of(const frame &f) noexcept -> location_edge
    {
        return f.active < count ? location_edge::field(name_at(f.active)) : location_edge::root();
    }

    template <std::size_t I>
    static auto object_one(frame &f, frame_stack &st) -> void
    {
        auto &obj = f.target.as<T>();
        std::get<I>(table).begin_object(st, obj);
    }

    template <std::size_t I>
    static auto array_one(frame &f, frame_stack &st) -> void
    {
        auto &obj = f.target.as<T>();
        std::get<I>(table).begin_array(st, obj);
    }

    template <std::size_t I>
    static auto finish_one(frame &f, frame_stack &st) -> void
    {
        constexpr const auto &field = std::get<I>(table);
        if constexpr (field.is_required()) {
            if (!f.seen.test(I)) {
                raise_decoded(st,
                              location_edge::field(field.name()),
                              fmt::format("{} is required", field.name()));
            }
        }
    }

    template <std::size_t... I>
    static auto object_dispatch(frame &f,
                                frame_stack &st,
                                [[maybe_unused]] std::index_sequence<I...> seq) -> void
    {
        std::ignore = ((f.active == I ? (object_one<I>(f, st), true) : false) || ...);
    }

    template <std::size_t... I>
    static auto array_dispatch(frame &f,
                               frame_stack &st,
                               [[maybe_unused]] std::index_sequence<I...> seq) -> void
    {
        std::ignore = ((f.active == I ? (array_one<I>(f, st), true) : false) || ...);
    }

    static auto finish(frame &f, frame_stack &st) -> void
    {
        finish_dispatch(f, st, std::make_index_sequence<count>{ });

        if constexpr (has_post_parse<Schema, T>::value) {
            try {
                Schema::post_parse(f.target.as<T>());
            } catch (const decode_error &e) {
                raise_decoded(st, location_edge::root(), e.what());
            }
        }

        st.pop();
    }

    template <std::size_t... I>
    static auto finish_dispatch(frame &f,
                                frame_stack &st,
                                [[maybe_unused]] std::index_sequence<I...> seq) -> void
    {
        (finish_one<I>(f, st), ...);
    }
};

template <typename E>
[[nodiscard]] constexpr auto element_expected_name() noexcept -> std::string_view
{
    if constexpr (is_scalar_v<E>) {
        return scalar_expected_name<E>();
    } else if constexpr (field_traits<E>::kind == field_kind::array) {
        return "array";
    } else {
        return "object";
    }
}

template <typename E>
inline constexpr bool is_object_element_v =
  field_traits<E>::kind == field_kind::object || field_traits<E>::kind == field_kind::map;

template <typename E>
inline constexpr bool is_array_element_v = field_traits<E>::kind == field_kind::array;

template <typename Vec>
struct array_frame_ops;

template <typename X, typename A>
struct array_frame_ops<std::vector<X, A>> : public frame_handler<array_frame_ops<std::vector<X, A>>>
{
    using vec_type = std::vector<X, A>;

    [[nodiscard]] static auto segment(const frame &f) -> location_edge
    {
        const auto &vec = f.target.as<vec_type>();
        return location_edge::element(vec.empty() ? 0 : vec.size() - 1);
    }

    static auto on_key(frame &f, frame_stack &s, [[maybe_unused]] std::string &v) -> bool
    {
        raise_decoded(s,
                      location_edge::element(f.target.as<vec_type>().size()),
                      "unexpected object key inside an array");
    }

    static auto on_value(frame &f, frame_stack &s, const value_view &v) -> void
    {
        auto &vec = f.target.as<vec_type>();
        const auto index = vec.size();
        if constexpr (is_scalar_v<X>) {
            try {
                assign_scalar(v, vec.emplace_back());
            } catch (const decode_error &e) {
                raise_decoded(s, location_edge::element(index), e.what());
            }
        } else {
            raise_decoded(s,
                          location_edge::element(index),
                          type_error_message(element_expected_name<X>(), v.type_name()));
        }
    }

    static auto on_object_begin(frame &f, frame_stack &st) -> void
    {
        auto &vec = f.target.as<vec_type>();
        if constexpr (is_object_element_v<X>) {
            push_child_frame(st, vec.emplace_back());
        } else {
            raise_decoded(st,
                          location_edge::element(vec.size()),
                          type_error_message(element_expected_name<X>(), "object"));
        }
    }

    static auto on_array_begin(frame &f, frame_stack &st) -> void
    {
        auto &vec = f.target.as<vec_type>();
        const auto index = vec.size();
        if constexpr (is_array_element_v<X>) {
            push_child_frame(st, vec.emplace_back());
        } else {
            raise_decoded(st,
                          location_edge::element(index),
                          type_error_message(element_expected_name<X>(), "array"));
        }
    }

    static auto on_end([[maybe_unused]] frame &f, frame_stack &s) -> void { s.pop(); }
};

template <typename Map>
struct map_frame_ops;

template <typename V>
struct map_frame_ops<std::unordered_map<std::string, V>>
    : public frame_handler<map_frame_ops<std::unordered_map<std::string, V>>>
{
    using map_type = std::unordered_map<std::string, V>;

    [[nodiscard]] static auto segment(const frame &f) -> location_edge
    {
        return location_edge::field(f.key);
    }

    static auto on_key(frame &f, frame_stack &s, std::string &key) -> bool
    {
        auto &map = f.target.as<map_type>();
        // Copy the key rather than moving it out of the lexer's token buffer,
        // so the lexer keeps its buffer for the next key.
        auto [it, inserted] = map.try_emplace(key);
        if (UNLIKELY(!inserted)) {
            raise_decoded(s,
                          location_edge::field(it->first),
                          fmt::format("duplicate object key: '{}'", it->first));
        }

        f.child = target_ref::of<V>(it->second);
        f.key = it->first;
        return true;
    }

    static auto on_value(frame &f, frame_stack &s, const value_view &v) -> void
    {
        if constexpr (is_scalar_v<V>) {
            try {
                assign_scalar(v, f.child.as<V>());
            } catch (const decode_error &e) {
                raise_decoded(s, location_edge::field(f.key), e.what());
            }
        } else {
            raise_decoded(s,
                          location_edge::field(f.key),
                          type_error_message(element_expected_name<V>(), v.type_name()));
        }
    }

    static auto on_object_begin(frame &f, frame_stack &s) -> void
    {
        if constexpr (is_object_element_v<V>) {
            push_child_frame(s, f.child.as<V>());
        } else {
            raise_decoded(s,
                          location_edge::field(f.key),
                          type_error_message(element_expected_name<V>(), "object"));
        }
    }

    static auto on_array_begin(frame &f, frame_stack &s) -> void
    {
        if constexpr (is_array_element_v<V>) {
            push_child_frame(s, f.child.as<V>());
        } else {
            raise_decoded(s,
                          location_edge::field(f.key),
                          type_error_message(element_expected_name<V>(), "array"));
        }
    }

    static void on_end([[maybe_unused]] frame &f, [[maybe_unused]] frame_stack &s) { s.pop(); }
};

template <auto Parse, typename Owner>
struct string_array_ops : public frame_handler<string_array_ops<Parse, Owner>>
{
    static_assert(is_non_null_callback_v<Parse, void (*)(std::string_view, Owner &)>,
                  "array element callback must be a non-null function pointer with signature "
                  "void(string_view, Owner&)");

    [[nodiscard]] static auto segment(const frame &f) -> location_edge
    {
        return location_edge::element(f.active);
    }

    static auto on_key(frame &f, frame_stack &s, [[maybe_unused]] std::string &v) -> bool
    {
        raise_decoded(s, location_edge::element(f.active), "unexpected object key inside an array");
    }

    static auto on_value(frame &f, frame_stack &s, const value_view &v) -> void
    {
        const auto index = f.active++;
        if (UNLIKELY(!v.is_string())) {
            raise_decoded(s,
                          location_edge::element(index),
                          type_error_message("string", v.type_name()));
        }

        try {
            Parse(v.as_string_view(), f.target.as<Owner>());
        } catch (const decode_error &e) {
            raise_decoded(s, location_edge::element(index), e.what());
        }
    }

    static auto on_object_begin(frame &f, frame_stack &s) -> void
    {
        raise_decoded(s, location_edge::element(f.active), type_error_message("string", "object"));
    }

    static auto on_array_begin(frame &f, frame_stack &s) -> void
    {
        raise_decoded(s, location_edge::element(f.active), type_error_message("string", "array"));
    }

    static auto on_end([[maybe_unused]] frame &f, frame_stack &st) -> void { st.pop(); }
};

template <typename E>
auto append_flag(std::string_view name, utils::bitflags<E> &target) -> void
{
    target |= enum_from_name<E>(name);
}

template <typename E>
struct array_frame_ops<utils::bitflags<E>>
    : public string_array_ops<&append_flag<E>, utils::bitflags<E>>
{
};

template <typename T, typename Schema>
[[nodiscard]] auto make_object_frame_ops() -> const frame_ops &
{
    static constexpr frame_ops ops = object_frame_ops<T, Schema>::make();
    return ops;
}

template <typename Tuple, std::size_t... I>
constexpr auto names_are_unique_impl(const Tuple &fields,
                                     [[maybe_unused]] std::index_sequence<I...> seq)
{
    const std::array<std::string_view, sizeof...(I)> names{ std::get<I>(fields).name()... };
    for (std::size_t i = 1; i < names.size(); ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            if (names[i] == names[j]) {
                return false;
            }
        }
    }

    return true;
}

template <typename Tuple>
constexpr auto names_are_unique(const Tuple &fields)
{
    return names_are_unique_impl(fields, std::make_index_sequence<std::tuple_size_v<Tuple>>{ });
}

// https://json.nlohmann.me/features/parsing/sax_interface
template <typename Root>
class sax_fill_parser
{
public:
    using number_integer_t = nlohmann::json::number_integer_t;
    using number_unsigned_t = nlohmann::json::number_unsigned_t;
    using number_float_t = nlohmann::json::number_float_t;
    using string_t = nlohmann::json::string_t;
    using binary_t = nlohmann::json::binary_t;
    using exception_t = nlohmann::json::exception;

    explicit sax_fill_parser(Root &root)
        : root_(root)
    {
    }

    auto null() -> bool { return dispatch_value(value_view::make_null()); }

    auto boolean(bool val) -> bool { return dispatch_value(value_view::make_bool(val)); }

    auto number_integer(number_integer_t val) -> bool
    {
        return dispatch_value(value_view::make_int(val));
    }

    auto number_unsigned(number_unsigned_t val) -> bool
    {
        return dispatch_value(value_view::make_uint(val));
    }

    auto number_float(number_float_t val, [[maybe_unused]] const string_t &s) -> bool
    {
        return dispatch_value(value_view::make_float(val));
    }

    auto string(string_t &val) -> bool
    {
        check_nul(val, "string contains an embedded NUL byte");
        return dispatch_value(value_view::make_string(val));
    }

    auto binary([[maybe_unused]] binary_t &val) -> bool
    {
        throw decode_error{ "binary values are not supported" };
    }

    auto start_object([[maybe_unused]] std::size_t elements) -> bool
    {
        if (UNLIKELY(++nesting_ > max_nesting_depth)) {
            raise_root("JSON nesting exceeds the maximum depth");
        }

        if (UNLIKELY(state_.empty())) {
            state_.push_frame(root_, object_tag<Root>{ });
            return true;
        }

        if (skip_.on_begin() == skip_counter::action::handle) {
            state_.top().ops->on_object_begin(state_.top(), state_);
        }

        return true;
    }

    auto key(string_t &val) -> bool
    {
        check_nul(val, "object key contains an embedded NUL byte");

        if (UNLIKELY(state_.empty())) {
            raise_root("unexpected object key outside of an object");
        }

        if (UNLIKELY(skip_.on_key() == skip_counter::action::consume)) {
            return true;
        }

        if (!state_.top().ops->on_key(state_.top(), state_, val)) {
            skip_.begin_skip();
        }

        return true;
    }

    auto end_object() -> bool
    {
        --nesting_;

        if (skip_.on_end() == skip_counter::action::handle) {
            state_.top().ops->on_end(state_.top(), state_);
        }

        return true;
    }

    auto start_array([[maybe_unused]] std::size_t elements) -> bool
    {
        if (UNLIKELY(++nesting_ > max_nesting_depth)) {
            raise_root("JSON nesting exceeds the maximum depth");
        }

        if (UNLIKELY(state_.empty())) {
            raise_root("type must be object, but is array");
        }

        if (skip_.on_begin() == skip_counter::action::handle) {
            state_.top().ops->on_array_begin(state_.top(), state_);
        }

        return true;
    }

    auto end_array() -> bool
    {
        --nesting_;

        if (skip_.on_end() == skip_counter::action::handle) {
            state_.top().ops->on_end(state_.top(), state_);
        }

        return true;
    }

    auto parse_error(std::size_t position,
                     [[maybe_unused]] const std::string &last_token,
                     const exception_t &ex) -> bool
    {
        // nlohmann's token string keeps bytes above 0x1F verbatim, so a rejected
        // ill-formed UTF-8 input produces a message with the same ill-formed
        // bytes.
        throw decode_error{
            fmt::format("at input byte {}: {}", position, utils::to_valid_utf8_lossy(ex.what()))
        };
    }

private:
    [[noreturn]] static auto raise_root(std::string_view detail) -> void
    {
        throw decode_error{ fmt::format("at '<root>': {}", detail) };
    }

    auto dispatch_value(const value_view &v) -> bool
    {
        if (UNLIKELY(state_.empty())) {
            raise_root(fmt::format("type must be object, but is {}", v.type_name()));
        }

        if (skip_.on_value() == skip_counter::action::handle) {
            state_.top().ops->on_value(state_.top(), state_, v);
        }

        return true;
    }

    static auto check_nul(const string_t &val, const char *message) -> void
    {
        if (UNLIKELY(val.find('\0') != string_t::npos)) {
            throw decode_error{ message };
        }
    }

    Root &root_;
    frame_stack state_;
    skip_counter skip_;
    std::size_t nesting_{ 0 };
};

} // namespace detail

template <typename T>
auto ops_of(object_tag<T>) -> const frame_ops & = delete;

template <typename T>
auto ops_of([[maybe_unused]] array_tag<T> tag) -> const frame_ops &
{
    static constexpr frame_ops ops = detail::array_frame_ops<T>::make();
    return ops;
}

template <typename M>
auto ops_of([[maybe_unused]] map_tag<M> tag) -> const frame_ops &
{
    static constexpr frame_ops ops = detail::map_frame_ops<M>::make();
    return ops;
}

template <auto Parse, typename Owner>
auto ops_of([[maybe_unused]] string_array_tag<Parse, Owner> tag) -> const frame_ops &
{
    static constexpr frame_ops ops = detail::string_array_ops<Parse, Owner>::make();
    return ops;
}

template <auto Member>
[[nodiscard]] constexpr auto field(std::string_view name)
{
    if constexpr (std::is_member_object_pointer_v<decltype(Member)>) {
        static_assert(Member != nullptr, "field() requires a non-null data member pointer");
        return detail::member_field<Member>{ name };
    } else {
        static_assert(std::is_member_object_pointer_v<decltype(Member)>,
                      "field() expects a pointer to a data member");
    }
}

template <auto Member, auto Read>
[[nodiscard]] constexpr auto field(std::string_view name)
{
    if constexpr (std::is_member_object_pointer_v<decltype(Member)>) {
        static_assert(Member != nullptr, "field() requires a non-null data member pointer");
        return detail::converted_field<Member, Read>{ name };
    } else {
        static_assert(std::is_member_object_pointer_v<decltype(Member)>,
                      "field() expects a pointer to a data member");
    }
}

template <auto Check>
[[nodiscard]] constexpr auto check(std::string_view name)
{
    return detail::checked_field<Check>{ name };
}

template <auto Parse>
[[nodiscard]] constexpr auto custom_array(std::string_view name)
{
    if constexpr (std::is_pointer_v<decltype(Parse)>
                  && std::is_function_v<std::remove_pointer_t<decltype(Parse)>>) {
        static_assert(detail::is_non_null_callback_v<Parse, decltype(Parse)>,
                      "custom_array() requires a non-null function pointer");
        return detail::custom_array_field<Parse>{ name };
    } else {
        static_assert(std::is_pointer_v<decltype(Parse)>
                        && std::is_function_v<std::remove_pointer_t<decltype(Parse)>>,
                      "custom_array() expects a function pointer");
    }
}

template <typename T>
[[nodiscard]] auto parse_as(std::string_view content) -> T
{
    const auto nul = content.find('\0');
    if (UNLIKELY(nul != std::string_view::npos)) {
        throw decode_error{ fmt::format("input contains an embedded NUL byte at offset {}", nul) };
    }

    T value{ };
    detail::sax_fill_parser<T> parser(value);
    std::ignore = nlohmann::json::sax_parse(content, &parser);

    return value;
}

} // namespace linyaps_box::json

// Emits the type's schema struct -- object name, the field descriptor tuple and
// the compile-time duplicate-name check -- together with its single registration
// point: the ADL function `ops_of(object_tag<TYPE>)` the parser resolves.  A type
// therefore opts in with one declaration in its own header and one macro call in
// its .cpp, and no library template is specialized:
//
//   // header
//   auto ops_of(json::object_tag<TYPE>) -> const json::frame_ops &;
//   // .cpp
//   LINYAPS_BOX_CONFIG_SCHEMA(TYPE, TYPE_schema, "object", field<&TYPE::member>("key"), ...)

#define LINYAPS_BOX_CONFIG_SCHEMA(TYPE, SCHEMA, OBJECT, ...)                                   \
    struct SCHEMA                                                                              \
    {                                                                                          \
        static constexpr auto fields()                                                         \
        {                                                                                      \
            using ::linyaps_box::json::check;                                                  \
            using ::linyaps_box::json::custom_array;                                           \
            using ::linyaps_box::json::field;                                                  \
            return std::make_tuple(__VA_ARGS__);                                               \
        }                                                                                      \
    };                                                                                         \
                                                                                               \
    static_assert(::linyaps_box::json::detail::names_are_unique(SCHEMA::fields()),             \
                  "duplicate JSON field key in " OBJECT);                                      \
    auto ops_of(::linyaps_box::json::object_tag<TYPE>)->const ::linyaps_box::json::frame_ops & \
    {                                                                                          \
        return ::linyaps_box::json::detail::make_object_frame_ops<TYPE, SCHEMA>();             \
    }

#define LINYAPS_BOX_CONFIG_SCHEMA_WITH_POST(TYPE, SCHEMA, OBJECT, POST, ...)                   \
    struct SCHEMA                                                                              \
    {                                                                                          \
        static_assert(                                                                         \
          ::linyaps_box::json::detail::is_non_null_callback_v<POST, void (*)(TYPE &)>,         \
          "post_parse must be a non-null function pointer with signature void(Owner&)");       \
        static constexpr auto fields()                                                         \
        {                                                                                      \
            using ::linyaps_box::json::check;                                                  \
            using ::linyaps_box::json::custom_array;                                           \
            using ::linyaps_box::json::field;                                                  \
            return std::make_tuple(__VA_ARGS__);                                               \
        }                                                                                      \
                                                                                               \
        static void post_parse(TYPE &value)                                                    \
        {                                                                                      \
            {                                                                                  \
                POST(value);                                                                   \
            }                                                                                  \
        }                                                                                      \
    };                                                                                         \
                                                                                               \
    static_assert(::linyaps_box::json::detail::names_are_unique(SCHEMA::fields()),             \
                  "duplicate JSON field key in " OBJECT);                                      \
    auto ops_of(::linyaps_box::json::object_tag<TYPE>)->const ::linyaps_box::json::frame_ops & \
    {                                                                                          \
        return ::linyaps_box::json::detail::make_object_frame_ops<TYPE, SCHEMA>();             \
    }
