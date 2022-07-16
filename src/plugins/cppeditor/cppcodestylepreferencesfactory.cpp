// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppcodestylepreferencesfactory.hpp"

#include "cppcodestylesettingspage.hpp"
#include "cppcodestylepreferences.hpp"
#include "cppeditorconstants.hpp"
#include "cppqtstyleindenter.hpp"

#include <QLayout>

namespace CppEditor {

static const char *defaultPreviewText = "#include <math.hpp>\n" "\n" "class Complex\n" "    {\n" "public:\n" "    Complex(double re, double im)\n" "        : _re(re), _im(im)\n" "        {}\n" "    double modulus() const\n" "        {\n" "        return sqrt(_re * _re + _im * _im);\n" "        }\n" "private:\n" "    double _re;\n" "    double _im;\n" "    };\n" "\n" "void bar(int i)\n" "    {\n" "    static int counter = 0;\n" "    counter += i;\n" "    }\n" "\n" "namespace Foo\n" "    {\n" "    namespace Bar\n" "        {\n" "        void foo(int a, int b)\n" "            {\n" "            for (int i = 0; i < a; i++)\n" "                {\n" "                if (i < b)\n" "                    bar(i);\n" "                else\n" "                    {\n" "                    bar(i);\n" "                    bar(b);\n" "                    }\n" "                }\n" "            }\n" "        } // namespace Bar\n" "    } // namespace Foo\n";

CppCodeStylePreferencesFactory::CppCodeStylePreferencesFactory() = default;

auto CppCodeStylePreferencesFactory::languageId() -> Utils::Id
{
  return Constants::CPP_SETTINGS_ID;
}

auto CppCodeStylePreferencesFactory::displayName() -> QString
{
  return QString::fromUtf8(Constants::CPP_SETTINGS_NAME);
}

auto CppCodeStylePreferencesFactory::createCodeStyle() const -> TextEditor::ICodeStylePreferences*
{
  return new CppCodeStylePreferences();
}

auto CppCodeStylePreferencesFactory::createEditor(TextEditor::ICodeStylePreferences *preferences, ProjectExplorer::Project *project, QWidget *parent) const -> QWidget*
{
  auto cppPreferences = qobject_cast<CppCodeStylePreferences*>(preferences);
  if (!cppPreferences)
    return nullptr;
  auto widget = new Internal::CppCodeStylePreferencesWidget(parent);

  widget->layout()->setContentsMargins(0, 0, 0, 0);
  widget->setCodeStyle(cppPreferences);

  const auto tab = additionalTab(project, parent);
  widget->addTab(tab.first, tab.second);

  return widget;
}

auto CppCodeStylePreferencesFactory::createIndenter(QTextDocument *doc) const -> TextEditor::Indenter*
{
  return new Internal::CppQtStyleIndenter(doc);
}

auto CppCodeStylePreferencesFactory::snippetProviderGroupId() const -> QString
{
  return QString(CppEditor::Constants::CPP_SNIPPETS_GROUP_ID);
}

auto CppCodeStylePreferencesFactory::previewText() const -> QString
{
  return QLatin1String(defaultPreviewText);
}

auto CppCodeStylePreferencesFactory::additionalTab(ProjectExplorer::Project *project, QWidget *parent) const -> std::pair<CppCodeStyleWidget*, QString>
{
  Q_UNUSED(parent)
  Q_UNUSED(project)
  return {nullptr, ""};
}

} // namespace CppEditor
