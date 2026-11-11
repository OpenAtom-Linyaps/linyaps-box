// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/user.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"

#include <filesystem>

#include <sys/types.h>

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(user,
                          user_schema,
                          "user",
                          field<&user::uid>("uid").required(),
                          field<&user::gid>("gid").required(),
                          field<&user::umask>("umask").ignore_null(),
                          field<&user::additional_gids>("additionalGids").ignore_null())

void validate(const user &v)
{
    require(!v.umask || (*v.umask & ~std::filesystem::perms::all) == std::filesystem::perms::none,
            "user.umask is invalid: 0{:o}",
            static_cast<mode_t>(v.umask.value_or(std::filesystem::perms::none)));
}

} // namespace linyaps_box::config
