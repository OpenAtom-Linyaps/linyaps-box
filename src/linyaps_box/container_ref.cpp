// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/container_ref.h"

#include "linyaps_box/utils/log.h"

#include <csignal> // IWYU pragma: keep
#include <utility>

#include <unistd.h>

linyaps_box::container_ref::container_ref(const status_directory &status_dir, std::string id)
    : id(std::move(id))
    , status_dir_(status_dir)
{
}

linyaps_box::container_status_t linyaps_box::container_ref::status() const
{
    return this->status_dir_.read(this->id);
}

void linyaps_box::container_ref::kill(int signal) const
{
    auto pid = this->status().PID;

    LINYAPS_BOX_DEBUG() << "kill process " << pid << " with signal " << signal;
    if (::kill(pid, signal) == 0) {
        return;
    }

    std::stringstream ss;
    ss << "failed to kill process " << pid << " with signal " << signal;
    throw std::system_error(errno, std::generic_category(), std::move(ss).str());
}

void linyaps_box::container_ref::exec(const linyaps_box::config::process_t &process)
{
    auto target = std::to_string(this->status().PID);
    auto wd = "--wdns=" + process.cwd.string();

    std::vector<const char *> argv{
        "nsenter",
        "--target",
        target.c_str(),
        "--user",
        "--mount",
        "--pid",
        // FIXME:
        // Old nsenter command do not support --wdns,
        // so we have to implement nsenter by ourself in the future.
        wd.c_str(),
        "--preserve-credentials",
    };

    for (const auto &arg : process.args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);

    LINYAPS_BOX_DEBUG() << [&argv]() -> std::string {
        auto result = std::accumulate(argv.cbegin(),
                                      argv.cend() - 1,
                                      std::string{ "args:[" },
                                      [](std::string init, const std::string &val) {
                                          init += val;
                                          init.push_back(' ');
                                          return init;
                                      });
        result.push_back(']');
        result.insert(0, "execvp nsenter with arguments: ");
        return result;
    }();

    // FIXME:
    // We only handle the command arguments for now
    // here are some other fields in process we need to consider:
    // terminal
    // console.height
    // console.width
    // cwd
    // env
    // rlimits
    // apparmor_profile
    // capabilities
    // no_new_privileges
    // oom_score_adj

    ::execvp("nsenter", const_cast<char **>(argv.data()));

    std::stringstream ss;
    ss << "execvp nsenter with arguments:";
    for (const auto &arg : argv) {
        ss << " " << arg;
    }

    throw std::system_error(errno, std::generic_category(), std::move(ss).str());
}

const linyaps_box::status_directory &linyaps_box::container_ref::status_dir() const
{
    return this->status_dir_;
}

const std::string &linyaps_box::container_ref::get_id() const
{
    return this->id;
}
