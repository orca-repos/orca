/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include "sourcelocationcontainer.h"

#include <utils/smallstringio.hpp>

namespace ClangBackEnd {

class CLANGSUPPORT_EXPORT SourceLocationsContainer
{
public:
    SourceLocationsContainer() = default;
    SourceLocationsContainer(std::vector<SourceLocationContainer> &&sourceLocationContainers)
        : m_sourceLocationContainers(std::move(sourceLocationContainers))
    {}

    const std::vector<SourceLocationContainer> &sourceLocationContainers() const
    {
        return m_sourceLocationContainers;
    }

    bool hasContent() const
    {
        return !m_sourceLocationContainers.empty();
    }

    void insertSourceLocation(const Utf8String &filePath, int line, int column)
    {
        m_sourceLocationContainers.emplace_back(filePath, line, column);
    }

    void reserve(std::size_t size)
    {
        m_sourceLocationContainers.reserve(size);
    }

    friend QDataStream &operator<<(QDataStream &out, const SourceLocationsContainer &container)
    {
        out << container.m_sourceLocationContainers;

        return out;
    }

    friend QDataStream &operator>>(QDataStream &in, SourceLocationsContainer &container)
    {
        in >> container.m_sourceLocationContainers;

        return in;
    }

    friend bool operator==(const SourceLocationsContainer &first, const SourceLocationsContainer &second)
    {
        return first.m_sourceLocationContainers == second.m_sourceLocationContainers;
    }

    SourceLocationsContainer clone() const
    {
        return *this;
    }

    std::vector<SourceLocationContainer> m_sourceLocationContainers;
};

CLANGSUPPORT_EXPORT QDebug operator<<(QDebug debug, const SourceLocationsContainer &container);

} // namespace ClangBackEnd

