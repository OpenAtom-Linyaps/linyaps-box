/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_CONTAINER_CONTAINER_OPTION_H_
#define LINGLONG_BOX_SRC_CONTAINER_CONTAINER_OPTION_H_

#include "util/json.h"

namespace linglong {

/*!
 * It's seem not a good idea to add extra config option.
 * The oci runtime json should contain option, but now it's hard to modify
 * the work flow.
 * If there is more information about how to deal user namespace, it can remove
 */
struct Option {
    bool rootless = false;
    bool link_lfs = true;
};

} // namespace linglong

#endif /* LINGLONG_BOX_SRC_CONTAINER_CONTAINER_OPTION_H_ */
