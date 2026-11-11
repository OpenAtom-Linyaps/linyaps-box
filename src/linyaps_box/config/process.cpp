// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/process.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/io/stream.h"
#include "linyaps_box/json/parser.h"
#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/enum_formatter.h" // IWYU pragma: keep
#include "linyaps_box/utils/environ.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/std.h>

#include <algorithm>
#include <bitset>
#include <stdexcept>

#include <sys/types.h>

namespace linyaps_box::config {

namespace {

void process_post_parse(process &v)
{
    // consoleSize only meaningful with terminal enabled
    if (!v.terminal.value_or(false)) {
        v.console_size_.reset();
    }
}

void validate_environment(const process &v)
{
    if (!v.env) {
        return;
    }

    const auto invalid = std::find_if(v.env->cbegin(), v.env->cend(), utils::is_invalid_env);
    if (UNLIKELY(invalid != v.env->cend())) {
        throw std::runtime_error(fmt::format("process.env contains a invalid env: {}", *invalid));
    }
}

void validate_rlimits(const process &v)
{
    if (!v.rlimits) {
        return;
    }

    std::bitset<16> seen;
    for (const auto &rl : *v.rlimits) {
        const auto type_val = static_cast<std::size_t>(rl.type_);
        require(type_val < seen.size(), "invalid rlimit type value out of range");
        require(!seen.test(type_val), "duplicate rlimit type: {}", rl.type_);
        seen.set(type_val);
    }
}

// A build capability, not a model invariant: capabilities need the compile-time
// switch.  Kept as its own step so validate() stays a plain composition.
void require_capabilities_build(const process &v)
{
    if (v.capabilities_) {
#ifndef LINYAPS_BOX_ENABLE_CAP
        throw std::runtime_error("capabilities support is not compiled in");
#endif
    }
}

} // namespace

LINYAPS_BOX_CONFIG_SCHEMA_WITH_POST(
  process,
  process_schema,
  "process",
  process_post_parse,
  field<&process::terminal>("terminal").ignore_null(),
  field<&process::console_size_>("consoleSize").ignore_null(),
  field<&process::cwd>("cwd").required(),
  field<&process::env>("env").ignore_null(),
  field<&process::args>("args").required(),
  field<&process::rlimits>("rlimits").ignore_null(),
  field<&process::apparmor_profile>("apparmorProfile").ignore_null(),
  field<&process::capabilities_>("capabilities").ignore_null(),
  field<&process::no_new_privileges>("noNewPrivileges").ignore_null(),
  field<&process::oom_score_adj>("oomScoreAdj").ignore_null(),
  field<&process::scheduler_>("scheduler").ignore_null(),
  field<&process::selinux_label>("selinuxLabel").ignore_null(),
  field<&process::io_priority_>("ioPriority").ignore_null(),
  field<&process::exec_cpu_affinity_>("execCPUAffinity").ignore_null(),
  field<&process::user_>("user").ignore_null())

void validate(const process &v)
{
    // Unknown capability names are not rejected here: the OCI spec requires them
    // to be warned about at runtime instead.
    require_capabilities_build(v);

    validate(v.user_);

    require(v.cwd.is_absolute(), "process.cwd must be an absolute path, got: {}", v.cwd);

    validate_environment(v);

    require(!v.args.empty(), "process.args must not be empty");

    validate_rlimits(v);

    validate_if_present(v.scheduler_);
    validate_if_present(v.io_priority_);
    validate_if_present(v.exec_cpu_affinity_);
}

auto process::parse(std::string_view content) -> process
{
    auto process_config = json::parse_as<process>(content);
    validate(process_config);
    return process_config;
}

auto process::load(const std::filesystem::path &path) -> process
{
    const auto content =
      os::throw_if_error(io::read_all(path), fmt::format("failed to read config file: {}", path));
    return parse(std::string_view{ content });
}

} // namespace linyaps_box::config
