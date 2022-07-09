// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qmakekitinformation.hpp"

#include "qmakeprojectmanagerconstants.hpp"

#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/toolchain.hpp>
#include <projectexplorer/toolchainmanager.hpp>

#include <qtsupport/qtkitinformation.hpp>

#include <utils/algorithm.hpp>
#include <utils/layoutbuilder.hpp>
#include <utils/qtcassert.hpp>

#include <QDir>
#include <QLineEdit>

using namespace ProjectExplorer;
using namespace Utils;

namespace QmakeProjectManager {
namespace Internal {

class QmakeKitAspectWidget final : public KitAspectWidget {
  Q_DECLARE_TR_FUNCTIONS(QmakeProjectManager::Internal::QmakeKitAspect)

public:
  QmakeKitAspectWidget(Kit *k, const KitAspect *ki) : KitAspectWidget(k, ki), m_lineEdit(createSubWidget<QLineEdit>())
  {
    refresh(); // set up everything according to kit
    m_lineEdit->setToolTip(ki->description());
    connect(m_lineEdit, &QLineEdit::textEdited, this, &QmakeKitAspectWidget::mkspecWasChanged);
  }

  ~QmakeKitAspectWidget() override { delete m_lineEdit; }

private:
  auto addToLayout(LayoutBuilder &builder) -> void override
  {
    addMutableAction(m_lineEdit);
    builder.addItem(m_lineEdit);
  }

  auto makeReadOnly() -> void override { m_lineEdit->setEnabled(false); }

  auto refresh() -> void override
  {
    if (!m_ignoreChange)
      m_lineEdit->setText(QDir::toNativeSeparators(QmakeKitAspect::mkspec(m_kit)));
  }

  auto mkspecWasChanged(const QString &text) -> void
  {
    m_ignoreChange = true;
    QmakeKitAspect::setMkspec(m_kit, text, QmakeKitAspect::MkspecSource::User);
    m_ignoreChange = false;
  }

  QLineEdit *m_lineEdit = nullptr;
  bool m_ignoreChange = false;
};

QmakeKitAspect::QmakeKitAspect()
{
  setObjectName(QLatin1String("QmakeKitAspect"));
  setId(QmakeKitAspect::id());
  setDisplayName(tr("Qt mkspec"));
  setDescription(tr("The mkspec to use when building the project with qmake.<br>" "This setting is ignored when using other build systems."));
  setPriority(24000);
}

auto QmakeKitAspect::validate(const Kit *k) const -> Tasks
{
  Tasks result;
  auto version = QtSupport::QtKitAspect::qtVersion(k);

  const auto mkspec = QmakeKitAspect::mkspec(k);
  if (!version && !mkspec.isEmpty())
    result << BuildSystemTask(Task::Warning, tr("No Qt version set, so mkspec is ignored."));
  if (version && !version->hasMkspec(mkspec))
    result << BuildSystemTask(Task::Error, tr("Mkspec not found for Qt version."));

  return result;
}

auto QmakeKitAspect::createConfigWidget(Kit *k) const -> KitAspectWidget*
{
  return new Internal::QmakeKitAspectWidget(k, this);
}

auto QmakeKitAspect::toUserOutput(const Kit *k) const -> KitAspect::ItemList
{
  return {qMakePair(tr("mkspec"), QDir::toNativeSeparators(mkspec(k)))};
}

auto QmakeKitAspect::addToMacroExpander(Kit *kit, MacroExpander *expander) const -> void
{
  expander->registerVariable("Qmake:mkspec", tr("Mkspec configured for qmake by the kit."), [kit]() -> QString {
    return QDir::toNativeSeparators(mkspec(kit));
  });
}

auto QmakeKitAspect::id() -> Utils::Id
{
  return Constants::KIT_INFORMATION_ID;
}

auto QmakeKitAspect::mkspec(const Kit *k) -> QString
{
  if (!k)
    return {};
  return k->value(QmakeKitAspect::id()).toString();
}

auto QmakeKitAspect::effectiveMkspec(const Kit *k) -> QString
{
  if (!k)
    return {};
  const auto spec = mkspec(k);
  if (spec.isEmpty())
    return defaultMkspec(k);
  return spec;
}

auto QmakeKitAspect::setMkspec(Kit *k, const QString &mkspec, MkspecSource source) -> void
{
  QTC_ASSERT(k, return);
  k->setValue(QmakeKitAspect::id(), source == MkspecSource::Code && mkspec == defaultMkspec(k) ? QString() : mkspec);
}

auto QmakeKitAspect::defaultMkspec(const Kit *k) -> QString
{
  auto version = QtSupport::QtKitAspect::qtVersion(k);
  if (!version) // No version, so no qmake
    return {};

  return version->mkspecFor(ToolChainKitAspect::cxxToolChain(k));
}

} // namespace Internal
} // namespace QmakeProjectManager
