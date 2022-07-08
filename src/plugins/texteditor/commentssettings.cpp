// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "commentssettings.hpp"

#include <QSettings>

using namespace TextEditor;

namespace {

constexpr char kDocumentationCommentsGroup[] = "CppToolsDocumentationComments";
constexpr char kEnableDoxygenBlocks[] = "EnableDoxygenBlocks";
constexpr char kGenerateBrief[] = "GenerateBrief";
constexpr char kAddLeadingAsterisks[] = "AddLeadingAsterisks";

}

CommentsSettings::CommentsSettings() : m_enableDoxygen(true), m_generateBrief(true), m_leadingAsterisks(true) {}

auto CommentsSettings::toSettings(QSettings *s) const -> void
{
  s->beginGroup(kDocumentationCommentsGroup);
  s->setValue(kEnableDoxygenBlocks, m_enableDoxygen);
  s->setValue(kGenerateBrief, m_generateBrief);
  s->setValue(kAddLeadingAsterisks, m_leadingAsterisks);
  s->endGroup();
}

auto CommentsSettings::fromSettings(QSettings *s) -> void
{
  s->beginGroup(kDocumentationCommentsGroup);
  m_enableDoxygen = s->value(kEnableDoxygenBlocks, true).toBool();
  m_generateBrief = m_enableDoxygen && s->value(kGenerateBrief, true).toBool();
  m_leadingAsterisks = s->value(kAddLeadingAsterisks, true).toBool();
  s->endGroup();
}

auto CommentsSettings::equals(const CommentsSettings &other) const -> bool
{
  return m_enableDoxygen == other.m_enableDoxygen && m_generateBrief == other.m_generateBrief && m_leadingAsterisks == other.m_leadingAsterisks;
}
