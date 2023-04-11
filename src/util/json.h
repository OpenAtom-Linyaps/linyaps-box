/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_UTIL_JSON_H_
#define LINGLONG_BOX_SRC_UTIL_JSON_H_

#define JSON_USE_IMPLICIT_CONVERSIONS 0

#include "3party/nlohmann/json.hpp"
#include "3party/optional/optional.hpp"

namespace nlohmann {

template<class J, class T>
inline void from_json(const J &j, tl::optional<T> &v)
{
    if (j.is_null()) {
        v = tl::nullopt;
    } else {
        v = j.template get<T>();
    }
}

template<class J, class T>
inline void to_json(J &j, const tl::optional<T> &o)
{
    if (o.has_value()) {
        j = o.value();
    }
}

} // namespace nlohmann

namespace linglong {

template<class T>
tl::optional<T> optional(const nlohmann::json &j, const char *key)
{
    tl::optional<T> o;
    auto iter = j.template find(key);
    if (iter != j.end()) {
        // check object is empty: {}, skip it
        if (iter->is_object() && iter->size() == 0) {
            return o;
        }

        o = iter->template get<tl::optional<T>>();
    }
    return o;
}

} // namespace linglong

#endif /* LINGLONG_BOX_SRC_UTIL_JSON_H_ */
