// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/security/privilege.h"

#include "linyaps_box/log/macro.h"
#include "linyaps_box/os/process.h"
#include "linyaps_box/utils/defer.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/std.h>
#include <sys/prctl.h>

#include <algorithm>
#include <fstream>
#include <system_error>

#include <grp.h>
#include <unistd.h>

#ifdef LINYAPS_BOX_ENABLE_CAP
#  include <sys/capability.h>
#endif

namespace linyaps_box::security {

unsigned long last_cap()
{
    static const auto cached = []() -> unsigned long {
        std::ifstream ifs("/proc/sys/kernel/cap_last_cap");
        if (!ifs) {
            throw std::runtime_error("failed to read /proc/sys/kernel/cap_last_cap");
        }

        unsigned long val{ };
        ifs >> val;
        return val;
    }();

    return cached;
}

namespace {

bool can_setgroups()
{
    std::ifstream ifs("/proc/self/setgroups");
    if (!ifs) {
        return true;
    }

    std::string content;
    ifs >> content;
    return content != "deny";
}

bool should_setgroups(bool can, bool has_additional_gids)
{
    if (can) {
        return true;
    }

    if (UNLIKELY(has_additional_gids)) {
        throw std::runtime_error(
          "setgroups is denied (user namespace); cannot set additional gids");
    }

    return false;
}

#ifdef LINYAPS_BOX_ENABLE_CAP

auto parse_names(const std::optional<std::vector<std::string>> &names)
  -> std::optional<std::vector<int>>
{
    if (!names) {
        return std::nullopt;
    }

    std::vector<int> vals;
    vals.reserve(names->size());
    std::transform(names->cbegin(),
                   names->cend(),
                   std::back_inserter(vals),
                   [](const std::string &name) {
                       cap_value_t v{ };
                       if (UNLIKELY(cap_from_name(name.c_str(), &v) < 0)) {
                           throw std::system_error(errno,
                                                   std::system_category(),
                                                   fmt::format("unknown capability: {}", name));
                       }

                       return v;
                   });

    return vals;
}

void drop_bounding(const std::optional<std::vector<int>> &keep)
{
    if (!keep) {
        return;
    }

    auto last = last_cap();
    if (UNLIKELY(last == 0)) {
        throw std::runtime_error("kernel does not support capabilities");
    }

    std::vector<bool> keep_mask(last + 1, false);
    for (auto c : *keep) {
        keep_mask[c] = true;
    }

    for (int i = 0; i <= static_cast<int>(last); ++i) {
        if (keep_mask[i]) {
            continue;
        }

        if (UNLIKELY(cap_drop_bound(i) < 0)) {
            throw std::system_error(
              errno,
              std::system_category(),
              fmt::format("failed to drop boundary cap {}({})", cap_to_name(i), i));
        }
    }
}

void apply_caps(const std::optional<std::vector<int>> &effective,
                const std::optional<std::vector<int>> &permitted,
                const std::optional<std::vector<int>> &inheritable)
{
    auto *raw = cap_init();
    if (UNLIKELY(raw == nullptr)) {
        throw std::system_error(errno, std::system_category(), "failed to init cap");
    }
    auto release_cap = utils::make_defer([raw]() noexcept {
        cap_free(raw);
    });

    auto set_flag =
      [&raw](cap_flag_t flag, const std::optional<std::vector<int>> &vals, std::string_view label) {
          if (!vals || vals->empty()) {
              return;
          }

          if (UNLIKELY(cap_set_flag(raw, flag, vals->size(), vals->data(), CAP_SET) < 0)) {
              throw std::system_error(errno,
                                      std::system_category(),
                                      fmt::format("failed to set {}", label));
          }
      };

    set_flag(CAP_EFFECTIVE, effective, "effective caps");
    set_flag(CAP_PERMITTED, permitted, "permitted caps");
    set_flag(CAP_INHERITABLE, inheritable, "inheritable caps");

    if (UNLIKELY(cap_set_proc(raw) < 0)) {
        throw std::system_error(errno, std::system_category(), "failed to apply caps configures");
    }
}

void apply_ambient(const std::optional<std::vector<int>> &ambient)
{
    if (!ambient) {
        return;
    }

    // cap_reset_ambient/cap_set_ambient was introduced after 2.25, but we need support 2.25
    // use ourself implement.
    os::throw_if_error(os::clear_ambient_capability_set(), "failed to clear ambient caps");

    std::for_each(ambient->cbegin(), ambient->cend(), [](int cap) {
        auto ret = os::add_ambient_capability(cap);
        if (UNLIKELY(!ret)) {
            LINYAPS_BOX_LOG_WARN("failed to raise ambient capability {}({}): {}",
                                 cap_to_name(cap),
                                 cap,
                                 ret.error());
        }
    });
}

#else

auto parse_names(const std::optional<std::vector<std::string>> &) -> std::optional<std::vector<int>>
{
    return std::nullopt;
}

auto drop_bounding(const std::optional<std::vector<int>> &) -> void { }

void apply_caps(const std::optional<std::vector<int>> &,
                const std::optional<std::vector<int>> &,
                const std::optional<std::vector<int>> &)
{
}

auto apply_ambient(const std::optional<std::vector<int>> &) -> void { }

#endif

} // anonymous namespace

privilege_context::privilege_context(std::optional<oci_config::process_t::user_t> user)
    : user_(std::move(user))
{
}

auto privilege_context::set_capabilities(std::optional<oci_config::process_t::capabilities_t> caps)
  -> privilege_context &
{
    if (caps) {
        cap_sets s;
        s.bounding = parse_names(caps->bounding);
        s.effective = parse_names(caps->effective);
        s.permitted = parse_names(caps->permitted);
        s.inheritable = parse_names(caps->inheritable);
        s.ambient = parse_names(caps->ambient);
        caps_ = std::move(s);
    } else {
        caps_.reset();
    }

    return *this;
}

auto privilege_context::set_no_new_privs(bool value) -> privilege_context &
{
    no_new_privs_ = value;
    return *this;
}

void privilege_context::apply()
{
    if (caps_) {
        drop_bounding(caps_->bounding);
        os::throw_if_error(os::set_keep_capabilities(true));
    }

    if (user_) {
        const bool has_gids = user_->additional_gids && !user_->additional_gids->empty();
        if (should_setgroups(can_setgroups(), has_gids)) {
            const auto ret = user_->additional_gids
              ? setgroups(user_->additional_gids->size(), user_->additional_gids->data())
              : setgroups(0, nullptr);
            if (UNLIKELY(ret < 0)) {
                throw std::system_error(errno, std::system_category(), "setgroups");
            }
        }

        int ret = setresgid(user_->gid, user_->gid, user_->gid);
        if (UNLIKELY(ret < 0)) {
            throw std::system_error(errno, std::system_category(), "setresgid");
        }

        ret = setresuid(user_->uid, user_->uid, user_->uid);
        if (UNLIKELY(ret < 0)) {
            throw std::system_error(errno, std::system_category(), "setresuid");
        }
    }

    if (caps_) {
        apply_caps(caps_->effective, caps_->permitted, caps_->inheritable);
        apply_ambient(caps_->ambient);

        os::throw_if_error(os::set_keep_capabilities(false));
    }

    if (no_new_privs_) {
        os::throw_if_error(os::set_no_new_privileges(true));
    }
}

} // namespace linyaps_box::security
