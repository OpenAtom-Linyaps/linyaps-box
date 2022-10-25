/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_UTIL_SEMAPHORE_H_
#define LINGLONG_BOX_SRC_UTIL_SEMAPHORE_H_

#include <memory>

namespace linglong {

class Semaphore
{
public:
    explicit Semaphore(int key);
    ~Semaphore();

    int init();

    //! minusOne -1 to value
    //! if value < 0, block
    //! \return
    int minusOne();

    //! minusOne +1 to value
    //! if value <= 0, release process in queue
    //! \return
    int plusOne();

private:
    struct SemaphorePrivate;
    std::unique_ptr<SemaphorePrivate> semaphorePrivate;
};

} // namespace linglong

#endif /* LINGLONG_BOX_SRC_UTIL_SEMAPHORE_H_ */
