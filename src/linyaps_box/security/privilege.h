// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config.h"

#include <optional>
#include <vector>

namespace linyaps_box::security {

// Read and cache the value of /proc/sys/kernel/cap_last_cap.
// Call early (before pivot_root) to prime the cache from the host /proc.
[[nodiscard]] unsigned long last_cap();

class privilege_context
{
public:
    explicit privilege_context(std::optional<oci_config::process_t::user_t> user);

    auto set_capabilities(std::optional<oci_config::process_t::capabilities_t> caps)
      -> privilege_context &;

    auto set_no_new_privs(bool value) -> privilege_context &;

    void apply();

private:
    struct cap_sets
    {
        std::optional<std::vector<int>> bounding;
        std::optional<std::vector<int>> effective;
        std::optional<std::vector<int>> permitted;
        std::optional<std::vector<int>> inheritable;
        std::optional<std::vector<int>> ambient;
    };

    std::optional<cap_sets> caps_;
    std::optional<oci_config::process_t::user_t> user_;
    bool no_new_privs_{ false };
};

} // namespace linyaps_box::security
