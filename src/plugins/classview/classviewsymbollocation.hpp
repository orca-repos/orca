// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/porting.hpp>

#include <QMetaType>
#include <QString>

namespace ClassView {
namespace Internal {

class SymbolLocation {
public:
  //! Default constructor
  SymbolLocation();

  //! Constructor
  explicit SymbolLocation(const QString &file, int lineNumber = 0, int columnNumber = 0);

  auto fileName() const -> const QString& { return m_fileName; }
  auto line() const -> int { return m_line; }
  auto column() const -> int { return m_column; }
  auto hash() const -> Utils::QHashValueType { return m_hash; }

  auto operator==(const SymbolLocation &other) const -> bool
  {
    return hash() == other.hash() && line() == other.line() && column() == other.column() && fileName() == other.fileName();
  }

private:
  const QString m_fileName;
  const int m_line;
  const int m_column;
  const Utils::QHashValueType m_hash; // precalculated hash value - to speed up qHash
};

//! qHash overload for QHash/QSet
inline auto qHash(const ClassView::Internal::SymbolLocation &location) -> Utils::QHashValueType
{
  return location.hash();
}

} // namespace Internal
} // namespace ClassView

Q_DECLARE_METATYPE(ClassView::Internal::SymbolLocation)
