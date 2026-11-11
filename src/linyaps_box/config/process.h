// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config/capabilities.h"
#include "linyaps_box/config/console_size.h"
#include "linyaps_box/config/exec_cpu_affinity.h"
#include "linyaps_box/config/io_priority.h"
#include "linyaps_box/config/rlimit.h"
#include "linyaps_box/config/scheduler.h"
#include "linyaps_box/config/user.h"
#include "linyaps_box/json/dsl_fwd.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace linyaps_box::config {

struct process
{
    // Not a full OCI config, but shares the process section's parsing and validation.
    static auto parse(std::string_view content) -> process;
    // Explicit file entry: reads the file (system errors preserved), then
    // parses the text.
    static auto load(const std::filesystem::path &path) -> process;

    std::optional<capabilities> capabilities_;
    std::optional<exec_cpu_affinity> exec_cpu_affinity_;
    user user_{ };
    std::optional<std::string> apparmor_profile;
    std::optional<scheduler> scheduler_;
    std::optional<std::string> selinux_label;
    std::filesystem::path cwd;
    std::optional<std::vector<std::string>> env;
    std::optional<std::vector<rlimit>> rlimits;
    std::vector<std::string> args;
    std::optional<int> oom_score_adj;
    std::optional<io_priority> io_priority_;
    std::optional<console_size> console_size_;
    std::optional<bool> terminal;
    std::optional<bool> no_new_privileges;
};

void validate(const process &v);

auto ops_of(json::object_tag<process>) -> const json::frame_ops &;
} // namespace linyaps_box::config
