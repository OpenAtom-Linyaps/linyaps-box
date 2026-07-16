// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/log_api.h" // IWYU pragma: keep

#ifndef LINYAPS_BOX_ACTIVE_LOG_LEVEL_NAME
#  error "LINYAPS_BOX_ACTIVE_LOG_LEVEL_NAME must be defined at preprocessing time"
#endif

#define LINYAPS_BOX_LOG_LEVEL_DETAIL(name) static_cast<int>(::linyaps_box::log::level::name)

#define LINYAPS_BOX_LOG_ACTIVE_LEVEL LINYAPS_BOX_LOG_LEVEL_DETAIL(LINYAPS_BOX_ACTIVE_LOG_LEVEL_NAME)

#ifndef LINYAPS_BOX_DEFAULT_LOG_LEVEL_NAME
#  error "LINYAPS_BOX_DEFAULT_LOG_LEVEL_NAME must be defined at preprocessing time"
#endif

#define LINYAPS_BOX_LOG_DEFAULT_LEVEL ::linyaps_box::log::level::LINYAPS_BOX_DEFAULT_LOG_LEVEL_NAME

#define LINYAPS_BOX_LOG_FATAL(...)                                            \
    do {                                                                      \
        ::linyaps_box::log::fatal(::linyaps_box::log::log_basename(__FILE__), \
                                  __func__,                                   \
                                  __LINE__,                                   \
                                  __VA_ARGS__);                               \
    } while (false)

#define LINYAPS_BOX_LOG_FATAL_ERRNO(errno_val, ...)                           \
    do {                                                                      \
        ::linyaps_box::log::fatal(::linyaps_box::log::log_basename(__FILE__), \
                                  __func__,                                   \
                                  __LINE__,                                   \
                                  errno_val,                                  \
                                  __VA_ARGS__);                               \
    } while (false)

#define LINYAPS_BOX_LOG_ERROR(...)                                            \
    do {                                                                      \
        ::linyaps_box::log::error(::linyaps_box::log::log_basename(__FILE__), \
                                  __func__,                                   \
                                  __LINE__,                                   \
                                  __VA_ARGS__);                               \
    } while (false)

#define LINYAPS_BOX_LOG_ERROR_ERRNO(errno_val, ...)                           \
    do {                                                                      \
        ::linyaps_box::log::error(::linyaps_box::log::log_basename(__FILE__), \
                                  __func__,                                   \
                                  __LINE__,                                   \
                                  errno_val,                                  \
                                  __VA_ARGS__);                               \
    } while (false)

#define LINYAPS_BOX_LOG_WARN(...)                                                                 \
    do {                                                                                          \
        if constexpr (LINYAPS_BOX_LOG_ACTIVE_LEVEL                                                \
                      >= static_cast<int>(::linyaps_box::log::level::warn)) {                     \
            if (::linyaps_box::log::get_current_log_level() >= ::linyaps_box::log::level::warn) { \
                ::linyaps_box::log::warn(::linyaps_box::log::log_basename(__FILE__),              \
                                         __func__,                                                \
                                         __LINE__,                                                \
                                         __VA_ARGS__);                                            \
            }                                                                                     \
        }                                                                                         \
    } while (false)

#define LINYAPS_BOX_LOG_INFO(...)                                                                 \
    do {                                                                                          \
        if constexpr (LINYAPS_BOX_LOG_ACTIVE_LEVEL                                                \
                      >= static_cast<int>(::linyaps_box::log::level::info)) {                     \
            if (::linyaps_box::log::get_current_log_level() >= ::linyaps_box::log::level::info) { \
                ::linyaps_box::log::info(::linyaps_box::log::log_basename(__FILE__),              \
                                         __func__,                                                \
                                         __LINE__,                                                \
                                         __VA_ARGS__);                                            \
            }                                                                                     \
        }                                                                                         \
    } while (false)

#define LINYAPS_BOX_LOG_DEBUG(...)                                                                 \
    do {                                                                                           \
        if constexpr (LINYAPS_BOX_LOG_ACTIVE_LEVEL                                                 \
                      >= static_cast<int>(::linyaps_box::log::level::debug)) {                     \
            if (::linyaps_box::log::get_current_log_level() >= ::linyaps_box::log::level::debug) { \
                ::linyaps_box::log::debug(::linyaps_box::log::log_basename(__FILE__),              \
                                          __func__,                                                \
                                          __LINE__,                                                \
                                          __VA_ARGS__);                                            \
            }                                                                                      \
        }                                                                                          \
    } while (false)
