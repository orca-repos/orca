// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "filepropertiesdialog.hpp"
#include "ui_filepropertiesdialog.h"

#include <core/editormanager/editormanager.hpp>
#include <core/editormanager/ieditorfactory.hpp>

#include <utils/fileutils.hpp>
#include <utils/mimetypes/mimedatabase.hpp>

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLocale>

using namespace Utils;

namespace Core {

FilePropertiesDialog::FilePropertiesDialog(FilePath file_path, QWidget *parent) : QDialog(parent), m_ui(new Ui::FilePropertiesDialog), m_file_path(std::move(file_path))
{
  m_ui->setupUi(this);

  connect(m_ui->readable, &QCheckBox::clicked, [this](const bool checked) {
    setPermission(QFile::ReadUser | QFile::ReadOwner, checked);
  });
  connect(m_ui->writable, &QCheckBox::clicked, [this](const bool checked) {
    setPermission(QFile::WriteUser | QFile::WriteOwner, checked);
  });
  connect(m_ui->executable, &QCheckBox::clicked, [this](const bool checked) {
    setPermission(QFile::ExeUser | QFile::ExeOwner, checked);
  });

  refresh();
}

FilePropertiesDialog::~FilePropertiesDialog()
{
  delete m_ui;
}

auto FilePropertiesDialog::detectTextFileSettings() const -> void
{
  QFile file(m_file_path.toString());
  if (!file.open(QIODevice::ReadOnly)) {
    m_ui->lineEndings->setText(tr("Unknown"));
    m_ui->indentation->setText(tr("Unknown"));
    return;
  }

  auto line_separator = '\n';
  const auto data = file.read(50000);
  file.close();

  // Try to guess the files line endings
  if (data.contains("\r\n")) {
    m_ui->lineEndings->setText(tr("Windows (CRLF)"));
  } else if (data.contains("\n")) {
    m_ui->lineEndings->setText(tr("Unix (LF)"));
  } else if (data.contains("\r")) {
    m_ui->lineEndings->setText(tr("Mac (CR)"));
    line_separator = '\r';
  } else {
    // That does not look like a text file at all
    m_ui->lineEndings->setText(tr("Unknown"));
    return;
  }

  auto leading_spaces = [](const QByteArray &line) {
    for (int i = 0, max = line.size(); i < max; ++i) {
      if (line.at(i) != ' ') {
        return i;
      }
    }
    return 0;
  };

  // Try to guess the files indentation style
  auto tab_indented = false;
  auto last_line_indent = 0;

  std::map<int, int> indents;
  for (const auto list = data.split(line_separator); const auto &line : list) {
    if (line.startsWith(' ')) {
      const auto spaces = leading_spaces(line);
      auto relative_current_line_indent = qAbs(spaces - last_line_indent);
      // Ignore zero or one character indentation changes
      if (relative_current_line_indent < 2)
        continue;
      indents[relative_current_line_indent]++;
      last_line_indent = spaces;
    } else if (line.startsWith('\t')) {
      tab_indented = true;
    }
    if (!indents.empty() && tab_indented)
      break;
  }

  const auto max = std::ranges::max_element(indents, [](const std::pair<int, int> &a, const std::pair<int, int> &b) {
    return a.second < b.second;
  });

  if (!indents.empty()) {
    if (tab_indented) {
      m_ui->indentation->setText(tr("Mixed"));
    } else {
      m_ui->indentation->setText(tr("%1 Spaces").arg(max->first));
    }
  } else if (tab_indented) {
    m_ui->indentation->setText(tr("Tabs"));
  } else {
    m_ui->indentation->setText(tr("Unknown"));
  }
}

auto FilePropertiesDialog::refresh() const -> void
{
  Utils::withNtfsPermissions<void>([this] {
    const auto file_info = m_file_path.toFileInfo();
    const QLocale locale;

    m_ui->name->setText(file_info.fileName());
    m_ui->path->setText(QDir::toNativeSeparators(file_info.canonicalPath()));

    const auto mime_type = mimeTypeForFile(m_file_path);
    m_ui->mimeType->setText(mime_type.name());

    const auto factories = IEditorFactory::preferredEditorTypes(m_file_path);
    m_ui->defaultEditor->setText(!factories.isEmpty() ? factories.at(0)->displayName() : tr("Undefined"));

    m_ui->owner->setText(file_info.owner());
    m_ui->group->setText(file_info.group());
    m_ui->size->setText(locale.formattedDataSize(file_info.size()));
    m_ui->readable->setChecked(file_info.isReadable());
    m_ui->writable->setChecked(file_info.isWritable());
    m_ui->executable->setChecked(file_info.isExecutable());
    m_ui->symLink->setChecked(file_info.isSymLink());
    m_ui->lastRead->setText(file_info.lastRead().toString(locale.dateTimeFormat()));
    m_ui->lastModified->setText(file_info.lastModified().toString(locale.dateTimeFormat()));
    if (mime_type.inherits("text/plain")) {
      detectTextFileSettings();
    } else {
      m_ui->lineEndings->setText(tr("Unknown"));
      m_ui->indentation->setText(tr("Unknown"));
    }
  });
}

auto FilePropertiesDialog::setPermission(QFile::Permissions new_permissions, bool set) const -> void
{
  Utils::withNtfsPermissions<void>([this, new_permissions, set] {
    auto permissions = m_file_path.permissions();
    if (set)
      permissions |= new_permissions;
    else
      permissions &= ~new_permissions;

    if (!m_file_path.setPermissions(permissions))
      qWarning() << "Cannot change permissions for file" << m_file_path;
  });

  refresh();
}

} // Core
