/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
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
