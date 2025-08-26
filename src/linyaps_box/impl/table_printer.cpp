// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/impl/table_printer.h"

#include <iostream>

namespace {
auto get_status_string(const linyaps_box::container_status_t::runtime_status &status) -> std::string
{
    switch (status) {
    case linyaps_box::container_status_t::runtime_status::CREATING:
        return "creating";
    case linyaps_box::container_status_t::runtime_status::CREATED:
        return "created";
    case linyaps_box::container_status_t::runtime_status::RUNNING:
        return "running";
    case linyaps_box::container_status_t::runtime_status::STOPPED:
        return "stopped";
    default:
        throw std::logic_error("unknown status");
    }
}
} // namespace

void linyaps_box::impl::table_printer::print_statuses(const std::vector<container_status_t> &status)
{
    int max_length = 4;
    for (const auto &s : status) {
        max_length = std::max(max_length, static_cast<int>(s.ID.length()));
    }

    std::cout << std::left << std::setw(max_length + 1) << "NAME" << std::setw(10) << "PID"
              << std::setw(9) << "STATUS" << std::setw(40) << "BUNDLE PATH" << std::setw(31)
              << "CREATED" << std::setw(0) << "OWNER" << '\n';
    for (const auto &s : status) {
        std::cout << std::left << std::setw(max_length) << s.ID << std::setw(10) << s.PID
                  << std::setw(9) << get_status_string(s.status) << std::setw(40) << s.bundle
                  << std::setw(31) << s.created << std::setw(0) << s.owner << '\n';
    }

    std::cout.flush();
}

void linyaps_box::impl::table_printer::print_status(const container_status_t &status)
{
    std::cout << "ociVersion\t" << status.oci_version << '\n';
    std::cout << "ID\t" << status.ID << '\n';
    std::cout << "PID\t" << status.PID << '\n';
    std::cout << "status\t" << get_status_string(status.status) << '\n';
    std::cout << "bundle\t" << status.bundle << '\n';
    std::cout << "created\t" << status.created << '\n';
    std::cout << "owner\t" << status.owner << '\n';
    std::cout << "annotations" << '\n';
    for (const auto &a : status.annotations) {
        std::cout << "\t" << a.first << "\t" << a.second << '\n';
    }

    std::cout.flush();
}
