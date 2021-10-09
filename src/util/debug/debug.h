/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string>

namespace linglong {

void dumpIDMap();

void dumpUidGidGroup();

void dumpFilesystem(const std::string &path);

void dumpFileInfo(const std::string &path);

} // namespace linglong