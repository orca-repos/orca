// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#define UTILS_DELETE_MOVE_AND_COPY(Class) \
    Class(const Class&) = delete; \
    Class &operator=(const Class&) = delete; \
    Class(Class&&) = delete; \
    Class &operator=(Class&&) = delete;
