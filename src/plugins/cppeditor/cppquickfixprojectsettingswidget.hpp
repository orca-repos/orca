// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppquickfixprojectsettings.hpp"

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class CppQuickFixProjectSettingsWidget;
}
QT_END_NAMESPACE

namespace ProjectExplorer {
class Project;
}

namespace CppEditor {
namespace Internal {

class CppQuickFixSettingsWidget;
class CppQuickFixProjectSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CppQuickFixProjectSettingsWidget(ProjectExplorer::Project *project, QWidget *parent = nullptr);
    ~CppQuickFixProjectSettingsWidget();

private slots:
    auto currentItemChanged() -> void;
    auto buttonCustomClicked() -> void;

private:
    auto useGlobalSettings() -> bool;

    QT_PREPEND_NAMESPACE(Ui)::CppQuickFixProjectSettingsWidget *ui;
    CppQuickFixSettingsWidget *m_settingsWidget;
    CppQuickFixProjectsSettings::CppQuickFixProjectsSettingsPtr m_projectSettings;
};

} // namespace Internal
} // namespace CppEditor
