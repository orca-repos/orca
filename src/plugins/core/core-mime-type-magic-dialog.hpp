// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ui_core-mime-type-magic-dialog.h"

#include <utils/mimetypes/mimemagicrule_p.hpp>

namespace Orca::Plugin::Core {

class MagicData {
public:
  MagicData() : m_rule(Utils::Internal::MimeMagicRule::String, QByteArray(" "), 0, 0) { }
  MagicData(const Utils::Internal::MimeMagicRule &rule, const int priority) : m_rule(rule), m_priority(priority) { }

  auto operator==(const MagicData &other) const -> bool;
  auto operator!=(const MagicData &other) const -> bool { return !(*this == other); }
  static auto normalizedMask(const Utils::Internal::MimeMagicRule &rule) -> QByteArray;

  Utils::Internal::MimeMagicRule m_rule;
  int m_priority = 0;
};

class MimeTypeMagicDialog final : public QDialog {
  Q_DECLARE_TR_FUNCTIONS(Orca::Plugin::Core::MimeTypeMagicDialog)

public:
  explicit MimeTypeMagicDialog(QWidget *parent = nullptr);

  auto setMagicData(const MagicData &data) const -> void;
  auto magicData() const -> MagicData;

private:
  auto setToRecommendedValues() const -> void;
  auto applyRecommended(bool checked) -> void;
  auto validateAccept() -> void;
  auto createRule(QString *error_message = nullptr) const -> Utils::Internal::MimeMagicRule;

  Ui::MimeTypeMagicDialog ui{};
  int m_custom_range_start = 0;
  int m_custom_range_end = 0;
  int m_custom_priority = 50;
};

} // namespace Orca::Plugin::Core

Q_DECLARE_METATYPE(Orca::Plugin::Core::MagicData)
