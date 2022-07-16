// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtsupport_global.hpp"

QT_FORWARD_DECLARE_CLASS(QSettings)

namespace QtSupport {

class QTSUPPORT_EXPORT CodeGenSettings {
public:
  // How to embed the Ui::Form class.
  enum UiClassEmbedding {
    PointerAggregatedUiClass,
    // "Ui::Form *m_ui";
    AggregatedUiClass,
    // "Ui::Form m_ui";
    InheritedUiClass // "...private Ui::Form..."
  };

  CodeGenSettings();

  auto equals(const CodeGenSettings &rhs) const -> bool;
  auto fromSettings(const QSettings *settings) -> void;
  auto toSettings(QSettings *settings) const -> void;

  friend auto operator==(const CodeGenSettings &p1, const CodeGenSettings &p2) -> bool { return p1.equals(p2); }
  friend auto operator!=(const CodeGenSettings &p1, const CodeGenSettings &p2) -> bool { return !p1.equals(p2); }

  UiClassEmbedding embedding;
  bool retranslationSupport; // Add handling for language change events
  bool includeQtModule;      // Include "<QtGui/[Class]>" or just "<[Class]>"
  bool addQtVersionCheck;    // Include #ifdef when using "#include <QtGui/..."
};

} // namespace QtSupport
