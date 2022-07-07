// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "mimemagicrule_p.hpp"

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>


namespace Utils {
namespace Internal {

class MimeMagicRuleMatcher {
public:
  explicit MimeMagicRuleMatcher(const QString &mime, unsigned priority = 65535);

  auto operator==(const MimeMagicRuleMatcher &other) const -> bool;
  auto addRule(const MimeMagicRule &rule) -> void;
  auto addRules(const QList<MimeMagicRule> &rules) -> void;
  auto magicRules() const -> QList<MimeMagicRule>;
  auto matches(const QByteArray &data) const -> bool;
  auto priority() const -> unsigned;
  auto mimetype() const -> QString { return m_mimetype; }

private:
  QList<MimeMagicRule> m_list;
  unsigned m_priority;
  QString m_mimetype;
};

} // Internal
} // Utils
