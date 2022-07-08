// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "importwidget.hpp"

#include <utils/detailswidget.hpp>
#include <utils/pathchooser.hpp>

#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace ProjectExplorer {
namespace Internal {

ImportWidget::ImportWidget(QWidget *parent) : QWidget(parent), m_pathChooser(new Utils::PathChooser)
{
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  const auto vboxLayout = new QVBoxLayout();
  setLayout(vboxLayout);
  vboxLayout->setContentsMargins(0, 0, 0, 0);
  const auto detailsWidget = new Utils::DetailsWidget(this);
  detailsWidget->setUseCheckBox(false);
  detailsWidget->setSummaryText(tr("Import Build From..."));
  detailsWidget->setSummaryFontBold(true);
  // m_detailsWidget->setIcon(); // FIXME: Set icon!
  vboxLayout->addWidget(detailsWidget);

  const auto widget = new QWidget;
  const auto layout = new QVBoxLayout(widget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_pathChooser);

  m_pathChooser->setExpectedKind(Utils::PathChooser::ExistingDirectory);
  m_pathChooser->setHistoryCompleter(QLatin1String("Import.SourceDir.History"));
  const auto importButton = new QPushButton(tr("Import"), widget);
  layout->addWidget(importButton);

  connect(importButton, &QAbstractButton::clicked, this, &ImportWidget::handleImportRequest);
  connect(m_pathChooser->lineEdit(), &QLineEdit::returnPressed, this, [this] {
    if (m_pathChooser->isValid()) {
      m_ownsReturnKey = true;
      handleImportRequest();

      // The next return should trigger the "Configure" button.
      QTimer::singleShot(0, this, [this] {
        setFocus();
        m_ownsReturnKey = false;
      });
    }
  });

  detailsWidget->setWidget(widget);
}

auto ImportWidget::setCurrentDirectory(const Utils::FilePath &dir) -> void
{
  m_pathChooser->setBaseDirectory(dir);
  m_pathChooser->setFilePath(dir);
}

auto ImportWidget::ownsReturnKey() const -> bool
{
  return m_ownsReturnKey;
}

auto ImportWidget::handleImportRequest() -> void
{
  const auto dir = m_pathChooser->filePath();
  emit importFrom(dir);

  m_pathChooser->setFilePath(m_pathChooser->baseDirectory());
}

} // namespace Internal
} // namespace ProjectExplorer
