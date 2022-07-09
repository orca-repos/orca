// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "resourcefile_p.hpp"

#include <core/fileiconprovider.hpp>
#include <core/fileutils.hpp>
#include <core/icore.hpp>
#include <core/vcsmanager.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>

#include <utils/algorithm.hpp>
#include <utils/filepath.hpp>
#include <utils/removefiledialog.hpp>
#include <utils/theme/theme.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMimeData>
#include <QtAlgorithms>
#include <QTextCodec>
#include <QTextStream>

#include <QIcon>
#include <QImageReader>

#include <QDomDocument>

using namespace Utils;

namespace ResourceEditor {
namespace Internal {

File::File(Prefix *prefix, const QString &_name, const QString &_alias) : Node(this, prefix), name(_name), alias(_alias), m_checked(false), m_exists(false) {}

auto File::checkExistence() -> void
{
  m_checked = false;
}

auto File::exists() -> bool
{
  if (!m_checked) {
    m_exists = QFile::exists(name);
    m_checked = true;
  }

  return m_exists;
}

auto File::setExists(bool exists) -> void
{
  m_exists = exists;
}

auto FileList::containsFile(File *file) -> bool
{
  foreach(const File *tmpFile, *this) if (tmpFile->name == file->name && tmpFile->prefix() == file->prefix())
    return true;
  return false;
}

ResourceFile::ResourceFile(const FilePath &filePath, const QString &contents)
{
  setFilePath(filePath);
  m_contents = contents;
}

ResourceFile::~ResourceFile()
{
  clearPrefixList();
}

auto ResourceFile::load() -> Core::IDocument::OpenResult
{
  m_error_message.clear();

  if (m_filePath.isEmpty()) {
    m_error_message = tr("The file name is empty.");
    return Core::IDocument::OpenResult::ReadError;
  }

  clearPrefixList();

  QDomDocument doc;

  if (m_contents.isEmpty()) {

    // Regular file
    QFile file(m_filePath.toString());
    if (!file.open(QIODevice::ReadOnly)) {
      m_error_message = file.errorString();
      return Core::IDocument::OpenResult::ReadError;
    }
    const auto data = file.readAll();
    // Detect line ending style
    m_textFileFormat = Utils::TextFileFormat::detect(data);
    // we always write UTF-8 when saving
    m_textFileFormat.codec = QTextCodec::codecForName("UTF-8");
    file.close();

    QString error_msg;
    int error_line, error_col;
    if (!doc.setContent(data, &error_msg, &error_line, &error_col)) {
      m_error_message = tr("XML error on line %1, col %2: %3").arg(error_line).arg(error_col).arg(error_msg);
      return Core::IDocument::OpenResult::CannotHandle;
    }

  } else {

    // Virtual file from qmake evaluator
    QString error_msg;
    int error_line, error_col;
    if (!doc.setContent(m_contents, &error_msg, &error_line, &error_col)) {
      m_error_message = tr("XML error on line %1, col %2: %3").arg(error_line).arg(error_col).arg(error_msg);
      return Core::IDocument::OpenResult::CannotHandle;
    }

  }

  const auto root = doc.firstChildElement(QLatin1String("RCC"));
  if (root.isNull()) {
    m_error_message = tr("The <RCC> root element is missing.");
    return Core::IDocument::OpenResult::CannotHandle;
  }

  auto relt = root.firstChildElement(QLatin1String("qresource"));
  for (; !relt.isNull(); relt = relt.nextSiblingElement(QLatin1String("qresource"))) {

    auto prefix = fixPrefix(relt.attribute(QLatin1String("prefix")));
    if (prefix.isEmpty())
      prefix = QString(QLatin1Char('/'));
    const auto language = relt.attribute(QLatin1String("lang"));

    const auto idx = indexOfPrefix(prefix, language);
    Prefix *p = nullptr;
    if (idx == -1) {
      p = new Prefix(prefix, language);
      m_prefix_list.append(p);
    } else {
      p = m_prefix_list[idx];
    }
    Q_ASSERT(p);

    auto felt = relt.firstChildElement(QLatin1String("file"));
    for (; !felt.isNull(); felt = felt.nextSiblingElement(QLatin1String("file"))) {
      const auto fileName = absolutePath(felt.text());
      const auto alias = felt.attribute(QLatin1String("alias"));
      const auto file = new File(p, fileName, alias);
      file->compress = felt.attribute(QLatin1String("compress"));
      file->compressAlgo = felt.attribute(QLatin1String("compress-algo"));
      file->threshold = felt.attribute(QLatin1String("threshold"));
      p->file_list.append(file);
    }
  }

  return Core::IDocument::OpenResult::Success;
}

auto ResourceFile::contents() const -> QString
{
  QDomDocument doc;
  auto root = doc.createElement(QLatin1String("RCC"));
  doc.appendChild(root);

  foreach(const Prefix *pref, m_prefix_list) {
    auto file_list = pref->file_list;
    const auto &name = pref->name;
    const auto &lang = pref->lang;

    auto relt = doc.createElement(QLatin1String("qresource"));
    root.appendChild(relt);
    relt.setAttribute(QLatin1String("prefix"), name);
    if (!lang.isEmpty())
      relt.setAttribute(QLatin1String("lang"), lang);

    foreach(const File *f, file_list) {
      const auto &file = *f;
      auto felt = doc.createElement(QLatin1String("file"));
      relt.appendChild(felt);
      const auto conv_file = QDir::fromNativeSeparators(relativePath(file.name));
      const auto text = doc.createTextNode(conv_file);
      felt.appendChild(text);
      if (!file.alias.isEmpty())
        felt.setAttribute(QLatin1String("alias"), file.alias);
      if (!file.compress.isEmpty())
        felt.setAttribute(QLatin1String("compress"), file.compress);
      if (!file.compressAlgo.isEmpty())
        felt.setAttribute(QLatin1String("compress-algo"), file.compressAlgo);
      if (!file.threshold.isEmpty())
        felt.setAttribute(QLatin1String("threshold"), file.threshold);
    }
  }
  return doc.toString(4);
}

auto ResourceFile::save() -> bool
{
  m_error_message.clear();

  if (m_filePath.isEmpty()) {
    m_error_message = tr("The file name is empty.");
    return false;
  }

  return m_textFileFormat.writeFile(m_filePath, contents(), &m_error_message);
}

auto ResourceFile::refresh() -> void
{
  for (auto i = 0; i < prefixCount(); ++i) {
    const auto &file_list = m_prefix_list.at(i)->file_list;
    foreach(File *file, file_list)
      file->checkExistence();
  }
}

auto ResourceFile::addFile(int prefix_idx, const QString &file, int file_idx) -> int
{
  const auto p = m_prefix_list[prefix_idx];
  Q_ASSERT(p);
  auto &files = p->file_list;
  Q_ASSERT(file_idx >= -1 && file_idx <= files.size());
  if (file_idx == -1)
    file_idx = files.size();
  files.insert(file_idx, new File(p, absolutePath(file)));
  return file_idx;
}

auto ResourceFile::addPrefix(const QString &prefix, const QString &lang, int prefix_idx) -> int
{
  const auto fixed_prefix = fixPrefix(prefix);
  if (indexOfPrefix(fixed_prefix, lang) != -1)
    return -1;

  Q_ASSERT(prefix_idx >= -1 && prefix_idx <= m_prefix_list.size());
  if (prefix_idx == -1)
    prefix_idx = m_prefix_list.size();
  m_prefix_list.insert(prefix_idx, new Prefix(fixed_prefix));
  m_prefix_list[prefix_idx]->lang = lang;
  return prefix_idx;
}

auto ResourceFile::removePrefix(int prefix_idx) -> void
{
  Q_ASSERT(prefix_idx >= 0 && prefix_idx < m_prefix_list.count());
  const Prefix *const p = m_prefix_list.at(prefix_idx);
  delete p;
  m_prefix_list.removeAt(prefix_idx);
}

auto ResourceFile::removeFile(int prefix_idx, int file_idx) -> void
{
  Q_ASSERT(prefix_idx >= 0 && prefix_idx < m_prefix_list.count());
  auto &fileList = m_prefix_list[prefix_idx]->file_list;
  Q_ASSERT(file_idx >= 0 && file_idx < fileList.count());
  delete fileList.at(file_idx);
  fileList.removeAt(file_idx);
}

auto ResourceFile::replacePrefix(int prefix_idx, const QString &prefix) -> bool
{
  const auto fixed_prefix = fixPrefix(prefix);
  Q_ASSERT(prefix_idx >= 0 && prefix_idx < m_prefix_list.count());
  const auto existingIndex = indexOfPrefix(fixed_prefix, m_prefix_list.at(prefix_idx)->lang, prefix_idx);
  if (existingIndex != -1) // prevent duplicated prefix + lang combinations
    return false;

  // no change
  if (m_prefix_list.at(prefix_idx)->name == fixed_prefix)
    return false;

  m_prefix_list[prefix_idx]->name = fixed_prefix;
  return true;
}

auto ResourceFile::replaceLang(int prefix_idx, const QString &lang) -> bool
{
  Q_ASSERT(prefix_idx >= 0 && prefix_idx < m_prefix_list.count());
  const auto existingIndex = indexOfPrefix(m_prefix_list.at(prefix_idx)->name, lang, prefix_idx);
  if (existingIndex != -1) // prevent duplicated prefix + lang combinations
    return false;

  // no change
  if (m_prefix_list.at(prefix_idx)->lang == lang)
    return false;

  Q_ASSERT(prefix_idx >= 0 && prefix_idx < m_prefix_list.count());
  m_prefix_list[prefix_idx]->lang = lang;
  return true;
}

auto ResourceFile::replacePrefixAndLang(int prefix_idx, const QString &prefix, const QString &lang) -> bool
{
  const auto fixed_prefix = fixPrefix(prefix);
  Q_ASSERT(prefix_idx >= 0 && prefix_idx < m_prefix_list.count());
  const auto existingIndex = indexOfPrefix(fixed_prefix, lang, prefix_idx);
  if (existingIndex != -1) // prevent duplicated prefix + lang combinations
    return false;

  // no change
  if (m_prefix_list.at(prefix_idx)->name == fixed_prefix && m_prefix_list.at(prefix_idx)->lang == lang)
    return false;

  Q_ASSERT(prefix_idx >= 0 && prefix_idx < m_prefix_list.count());
  m_prefix_list[prefix_idx]->name = fixed_prefix;
  m_prefix_list[prefix_idx]->lang = lang;
  return true;
}

auto ResourceFile::replaceAlias(int prefix_idx, int file_idx, const QString &alias) -> void
{
  Q_ASSERT(prefix_idx >= 0 && prefix_idx < m_prefix_list.count());
  auto &fileList = m_prefix_list.at(prefix_idx)->file_list;
  Q_ASSERT(file_idx >= 0 && file_idx < fileList.count());
  fileList[file_idx]->alias = alias;
}

auto ResourceFile::renameFile(const QString &fileName, const QString &newFileName) -> bool
{
  auto success = true;

  FileList entries;
  for (auto i = 0; i < prefixCount(); ++i) {
    const auto &file_list = m_prefix_list.at(i)->file_list;
    foreach(File *file, file_list) {
      if (file->name == fileName)
        entries.append(file);
      if (file->name == newFileName)
        return false; // prevent conflicts
    }
  }

  Q_ASSERT(!entries.isEmpty());

  entries.at(0)->checkExistence();
  if (entries.at(0)->exists()) {
    foreach(File *file, entries)
      file->setExists(true);
    success = Core::FileUtils::renameFile(Utils::FilePath::fromString(entries.at(0)->name), Utils::FilePath::fromString(newFileName));
  }

  if (success) {
    const auto exists = QFile::exists(newFileName);
    foreach(File *file, entries) {
      file->name = newFileName;
      file->setExists(exists);
    }
  }

  return success;
}

auto ResourceFile::replaceFile(int pref_idx, int file_idx, const QString &file) -> void
{
  Q_ASSERT(pref_idx >= 0 && pref_idx < m_prefix_list.count());
  auto &fileList = m_prefix_list.at(pref_idx)->file_list;
  Q_ASSERT(file_idx >= 0 && file_idx < fileList.count());
  fileList[file_idx]->name = file;
}

auto ResourceFile::indexOfPrefix(const QString &prefix, const QString &lang) const -> int
{
  return indexOfPrefix(prefix, lang, -1);
}

auto ResourceFile::indexOfPrefix(const QString &prefix, const QString &lang, int skip) const -> int
{
  const auto fixed_prefix = fixPrefix(prefix);
  for (auto i = 0; i < m_prefix_list.size(); ++i) {
    if (i == skip)
      continue;
    if (m_prefix_list.at(i)->name == fixed_prefix && m_prefix_list.at(i)->lang == lang)
      return i;
  }
  return -1;
}

auto ResourceFile::indexOfFile(int pref_idx, const QString &file) const -> int
{
  Q_ASSERT(pref_idx >= 0 && pref_idx < m_prefix_list.count());
  const auto p = m_prefix_list.at(pref_idx);
  File equalFile(p, absolutePath(file));
  return p->file_list.indexOf(&equalFile);
}

auto ResourceFile::relativePath(const QString &abs_path) const -> QString
{
  if (m_filePath.isEmpty() || QFileInfo(abs_path).isRelative())
    return abs_path;

  const auto fileInfo = m_filePath.toFileInfo();
  return fileInfo.absoluteDir().relativeFilePath(abs_path);
}

auto ResourceFile::absolutePath(const QString &rel_path) const -> QString
{
  const QFileInfo fi(rel_path);
  if (fi.isAbsolute())
    return rel_path;

  auto rc = m_filePath.toFileInfo().path();
  rc += QLatin1Char('/');
  rc += rel_path;
  return QDir::cleanPath(rc);
}

auto ResourceFile::orderList() -> void
{
  for (auto p : qAsConst(m_prefix_list)) {
    std::sort(p->file_list.begin(), p->file_list.end(), [&](File *f1, File *f2) {
      return *f1 < *f2;
    });
  }

  if (!save())
    m_error_message = tr("Cannot save file.");
}

auto ResourceFile::contains(const QString &prefix, const QString &lang, const QString &file) const -> bool
{
  const auto pref_idx = indexOfPrefix(prefix, lang);
  if (pref_idx == -1)
    return false;
  if (file.isEmpty())
    return true;
  Q_ASSERT(pref_idx >= 0 && pref_idx < m_prefix_list.count());
  const auto p = m_prefix_list.at(pref_idx);
  Q_ASSERT(p);
  File equalFile(p, absolutePath(file));
  return p->file_list.containsFile(&equalFile);
}

auto ResourceFile::contains(int pref_idx, const QString &file) const -> bool
{
  Q_ASSERT(pref_idx >= 0 && pref_idx < m_prefix_list.count());
  const auto p = m_prefix_list.at(pref_idx);
  File equalFile(p, absolutePath(file));
  return p->file_list.containsFile(&equalFile);
}

/*static*/
auto ResourceFile::fixPrefix(const QString &prefix) -> QString
{
  const QChar slash = QLatin1Char('/');
  auto result = QString(slash);
  for (const auto c : prefix) {
    if (c == slash && result.at(result.size() - 1) == slash)
      continue;
    result.append(c);
  }

  if (result.size() > 1 && result.endsWith(slash))
    result = result.mid(0, result.size() - 1);

  return result;
}

auto ResourceFile::prefixCount() const -> int
{
  return m_prefix_list.size();
}

auto ResourceFile::prefix(int idx) const -> QString
{
  Q_ASSERT((idx >= 0) && (idx < m_prefix_list.count()));
  return m_prefix_list.at(idx)->name;
}

auto ResourceFile::lang(int idx) const -> QString
{
  Q_ASSERT(idx >= 0 && idx < m_prefix_list.count());
  return m_prefix_list.at(idx)->lang;
}

auto ResourceFile::fileCount(int prefix_idx) const -> int
{
  Q_ASSERT(prefix_idx >= 0 && prefix_idx < m_prefix_list.count());
  return m_prefix_list.at(prefix_idx)->file_list.size();
}

auto ResourceFile::file(int prefix_idx, int file_idx) const -> QString
{
  Q_ASSERT(prefix_idx >= 0 && prefix_idx < m_prefix_list.count());
  const auto &fileList = m_prefix_list.at(prefix_idx)->file_list;
  Q_ASSERT(file_idx >= 0 && file_idx < fileList.count());
  fileList.at(file_idx)->checkExistence();
  return fileList.at(file_idx)->name;
}

auto ResourceFile::alias(int prefix_idx, int file_idx) const -> QString
{
  Q_ASSERT(prefix_idx >= 0 && prefix_idx < m_prefix_list.count());
  const auto &fileList = m_prefix_list.at(prefix_idx)->file_list;
  Q_ASSERT(file_idx >= 0 && file_idx < fileList.count());
  return fileList.at(file_idx)->alias;
}

auto ResourceFile::prefixPointer(int prefixIndex) const -> void*
{
  Q_ASSERT(prefixIndex >= 0 && prefixIndex < m_prefix_list.count());
  return m_prefix_list.at(prefixIndex);
}

auto ResourceFile::filePointer(int prefixIndex, int fileIndex) const -> void*
{
  Q_ASSERT(prefixIndex >= 0 && prefixIndex < m_prefix_list.count());
  const auto &fileList = m_prefix_list.at(prefixIndex)->file_list;
  Q_ASSERT(fileIndex >= 0 && fileIndex < fileList.count());
  return fileList.at(fileIndex);
}

auto ResourceFile::prefixPointerIndex(const Prefix *prefix) const -> int
{
  int const count = m_prefix_list.count();
  for (auto i = 0; i < count; i++) {
    const Prefix *const other = m_prefix_list.at(i);
    if (*other == *prefix)
      return i;
  }
  return -1;
}

auto ResourceFile::clearPrefixList() -> void
{
  qDeleteAll(m_prefix_list);
  m_prefix_list.clear();
}

/******************************************************************************
** ResourceModel
*/

ResourceModel::ResourceModel(QObject *parent) : QAbstractItemModel(parent), m_dirty(false)
{
  static auto resourceFolderIcon = Core::FileIconProvider::directoryIcon(QLatin1String(ProjectExplorer::Constants::FILEOVERLAY_QRC));
  m_prefixIcon = resourceFolderIcon;
}

auto ResourceModel::setDirty(bool b) -> void
{
  if (b) emit contentsChanged();
  if (b == m_dirty)
    return;

  m_dirty = b;
  emit dirtyChanged(b);
}

auto ResourceModel::index(int row, int column, const QModelIndex &parent) const -> QModelIndex
{
  if (column != 0)
    return QModelIndex();

  const void *internalPointer = nullptr;
  if (parent.isValid()) {
    const auto pip = parent.internalPointer();
    if (!pip)
      return QModelIndex();

    // File node
    const Node *const node = reinterpret_cast<Node*>(pip);
    const Prefix *const prefix = node->prefix();
    Q_ASSERT(prefix);
    if (row < 0 || row >= prefix->file_list.count())
      return QModelIndex();
    const auto prefixIndex = m_resource_file.prefixPointerIndex(prefix);
    const auto fileIndex = row;
    internalPointer = m_resource_file.filePointer(prefixIndex, fileIndex);
  } else {
    // Prefix node
    if (row < 0 || row >= m_resource_file.prefixCount())
      return QModelIndex();
    internalPointer = m_resource_file.prefixPointer(row);
  }
  Q_ASSERT(internalPointer);
  return createIndex(row, 0, internalPointer);
}

auto ResourceModel::parent(const QModelIndex &index) const -> QModelIndex
{
  if (!index.isValid())
    return QModelIndex();

  const auto internalPointer = index.internalPointer();
  if (!internalPointer)
    return QModelIndex();
  const Node *const node = reinterpret_cast<Node*>(internalPointer);
  const Prefix *const prefix = node->prefix();
  Q_ASSERT(prefix);
  auto const isFileNode = (prefix != node);

  if (isFileNode) {
    const auto row = m_resource_file.prefixPointerIndex(prefix);
    Q_ASSERT(row >= 0);
    return createIndex(row, 0, prefix);
  } else {
    return QModelIndex();
  }
}

auto ResourceModel::rowCount(const QModelIndex &parent) const -> int
{
  if (parent.isValid()) {
    const auto internalPointer = parent.internalPointer();
    const Node *const node = reinterpret_cast<Node*>(internalPointer);
    const Prefix *const prefix = node->prefix();
    Q_ASSERT(prefix);
    auto const isFileNode = (prefix != node);

    if (isFileNode)
      return 0;
    else
      return prefix->file_list.count();
  } else {
    return m_resource_file.prefixCount();
  }
}

auto ResourceModel::columnCount(const QModelIndex &) const -> int
{
  return 1;
}

auto ResourceModel::hasChildren(const QModelIndex &parent) const -> bool
{
  return rowCount(parent) != 0;
}

auto ResourceModel::refresh() -> void
{
  m_resource_file.refresh();
}

auto ResourceModel::errorMessage() const -> QString
{
  return m_resource_file.errorMessage();
}

auto ResourceModel::nonExistingFiles() const -> QList<QModelIndex>
{
  QList<QModelIndex> files;
  QFileInfo fi;
  const auto prefixCount = rowCount(QModelIndex());
  for (auto i = 0; i < prefixCount; ++i) {
    auto prefix = index(i, 0, QModelIndex());
    const auto fileCount = rowCount(prefix);
    for (auto j = 0; j < fileCount; ++j) {
      auto fileIndex = index(j, 0, prefix);
      auto fileName = file(fileIndex);
      fi.setFile(fileName);
      if (!fi.exists())
        files << fileIndex;
    }
  }
  return files;
}

auto ResourceModel::orderList() -> void
{
  m_resource_file.orderList();
}

auto ResourceModel::flags(const QModelIndex &index) const -> Qt::ItemFlags
{
  auto f = QAbstractItemModel::flags(index);

  const void *internalPointer = index.internalPointer();
  auto node = reinterpret_cast<const Node*>(internalPointer);
  const Prefix *prefix = node->prefix();
  Q_ASSERT(prefix);
  const auto isFileNode = (prefix != node);

  if (isFileNode)
    f |= Qt::ItemIsEditable;

  return f;
}

auto ResourceModel::iconFileExtension(const QString &path) -> bool
{
  static QStringList ext_list;
  if (ext_list.isEmpty()) {
    const auto _ext_list = QImageReader::supportedImageFormats();
    foreach(const QByteArray &ext, _ext_list) {
      auto dotExt = QString(QLatin1Char('.'));
      dotExt += QString::fromLatin1(ext);
      ext_list.append(dotExt);
    }
  }

  foreach(const QString &ext, ext_list) {
    if (path.endsWith(ext, Qt::CaseInsensitive))
      return true;
  }

  return false;
}

static inline auto appendParenthesized(const QString &what, QString &s) -> void
{
  s += QLatin1String(" (");
  s += what;
  s += QLatin1Char(')');
}

auto ResourceModel::data(const QModelIndex &index, int role) const -> QVariant
{
  if (!index.isValid())
    return QVariant();

  const void *internalPointer = index.internalPointer();
  auto node = reinterpret_cast<const Node*>(internalPointer);
  const Prefix *prefix = node->prefix();
  auto file = node->file();
  Q_ASSERT(prefix);
  const auto isFileNode = (prefix != node);

  QVariant result;

  switch (role) {
  case Qt::DisplayRole: {
    QString stringRes;
    if (!isFileNode) {
      // Prefix node
      stringRes = prefix->name;
      const auto &lang = prefix->lang;
      if (!lang.isEmpty())
        appendParenthesized(lang, stringRes);
    } else {
      // File node
      Q_ASSERT(file);
      const auto conv_file = m_resource_file.relativePath(file->name);
      stringRes = QDir::fromNativeSeparators(conv_file);
      const auto alias = file->alias;
      if (!alias.isEmpty())
        appendParenthesized(alias, stringRes);
    }
    result = stringRes;
  }
  break;
  case Qt::DecorationRole:
    if (isFileNode) {
      // File node
      Q_ASSERT(file);
      if (file->icon.isNull()) {
        const auto path = m_resource_file.absolutePath(file->name);
        if (iconFileExtension(path))
          file->icon = QIcon(path);
        else
          file->icon = Core::FileIconProvider::icon(Utils::FilePath::fromString(path));
      }
      if (!file->icon.isNull())
        result = file->icon;

    } else {
      result = m_prefixIcon;
    }
    break;
  case Qt::EditRole:
    if (isFileNode) {
      Q_ASSERT(file);
      const auto conv_file = m_resource_file.relativePath(file->name);
      result = QDir::fromNativeSeparators(conv_file);
    }
    break;
  case Qt::ForegroundRole:
    if (isFileNode) {
      // File node
      Q_ASSERT(file);
      if (!file->exists())
        result = Utils::orcaTheme()->color(Utils::Theme::TextColorError);
    }
    break;
  default:
    break;
  }
  return result;
}

auto ResourceModel::setData(const QModelIndex &index, const QVariant &value, int role) -> bool
{
  if (!index.isValid())
    return false;
  if (role != Qt::EditRole)
    return false;

  const auto baseDir = filePath().toFileInfo().absoluteDir();
  const auto newFileName = Utils::FilePath::fromUserInput(baseDir.absoluteFilePath(value.toString()));

  if (newFileName.isEmpty())
    return false;

  if (!newFileName.isChildOf(filePath().absolutePath()))
    return false;

  return renameFile(file(index), newFileName.toString());
}

auto ResourceModel::getItem(const QModelIndex &index, QString &prefix, QString &file) const -> void
{
  prefix.clear();
  file.clear();

  if (!index.isValid())
    return;

  const void *internalPointer = index.internalPointer();
  auto node = reinterpret_cast<const Node*>(internalPointer);
  const Prefix *p = node->prefix();
  Q_ASSERT(p);
  const auto isFileNode = (p != node);

  if (isFileNode) {
    const File *f = node->file();
    Q_ASSERT(f);
    if (!f->alias.isEmpty())
      file = f->alias;
    else
      file = f->name;
  } else {
    prefix = p->name;
  }
}

auto ResourceModel::lang(const QModelIndex &index) const -> QString
{
  if (!index.isValid())
    return QString();

  return m_resource_file.lang(index.row());
}

auto ResourceModel::alias(const QModelIndex &index) const -> QString
{
  if (!index.isValid() || !index.parent().isValid())
    return QString();
  return m_resource_file.alias(index.parent().row(), index.row());
}

auto ResourceModel::file(const QModelIndex &index) const -> QString
{
  if (!index.isValid() || !index.parent().isValid())
    return QString();
  return m_resource_file.file(index.parent().row(), index.row());
}

auto ResourceModel::getIndex(const QString &prefix, const QString &lang, const QString &file) -> QModelIndex
{
  if (prefix.isEmpty())
    return QModelIndex();

  const auto pref_idx = m_resource_file.indexOfPrefix(prefix, lang);
  if (pref_idx == -1)
    return QModelIndex();

  const auto pref_model_idx = index(pref_idx, 0, QModelIndex());
  if (file.isEmpty())
    return pref_model_idx;

  const auto file_idx = m_resource_file.indexOfFile(pref_idx, file);
  if (file_idx == -1)
    return QModelIndex();

  return index(file_idx, 0, pref_model_idx);
}

auto ResourceModel::prefixIndex(const QModelIndex &sel_idx) const -> QModelIndex
{
  if (!sel_idx.isValid())
    return QModelIndex();
  const auto parentIndex = parent(sel_idx);
  return parentIndex.isValid() ? parentIndex : sel_idx;
}

auto ResourceModel::addNewPrefix() -> QModelIndex
{
  const QString format = QLatin1String("/new/prefix%1");
  auto i = 1;
  auto prefix = format.arg(i);
  for (; m_resource_file.contains(prefix, QString()); i++)
    prefix = format.arg(i);

  i = rowCount(QModelIndex());
  beginInsertRows(QModelIndex(), i, i);
  m_resource_file.addPrefix(prefix, QString());
  endInsertRows();

  setDirty(true);

  return index(i, 0, QModelIndex());
}

auto ResourceModel::addFiles(const QModelIndex &model_idx, const QStringList &file_list) -> QModelIndex
{
  const auto prefixModelIndex = prefixIndex(model_idx);
  const auto prefixArrayIndex = prefixModelIndex.row();
  const auto cursorFileArrayIndex = (prefixModelIndex == model_idx) ? 0 : model_idx.row();
  int dummy;
  int lastFileArrayIndex;
  addFiles(prefixArrayIndex, file_list, cursorFileArrayIndex, dummy, lastFileArrayIndex);
  return index(lastFileArrayIndex, 0, prefixModelIndex);
}

auto ResourceModel::existingFilesSubtracted(int prefixIndex, const QStringList &fileNames) const -> QStringList
{
  const auto prefixModelIdx = index(prefixIndex, 0, QModelIndex());
  QStringList uniqueList;

  if (prefixModelIdx.isValid()) {
    foreach(const QString &file, fileNames) {
      if (!m_resource_file.contains(prefixIndex, file) && !uniqueList.contains(file))
        uniqueList.append(file);
    }
  }
  return uniqueList;
}

auto ResourceModel::addFiles(int prefixIndex, const QStringList &fileNames, int cursorFile, int &firstFile, int &lastFile) -> void
{
  Q_UNUSED(cursorFile)
  const auto prefix_model_idx = index(prefixIndex, 0, QModelIndex());
  firstFile = -1;
  lastFile = -1;

  if (!prefix_model_idx.isValid())
    return;

  auto unique_list = existingFilesSubtracted(prefixIndex, fileNames);

  if (unique_list.isEmpty())
    return;

  const auto cnt = m_resource_file.fileCount(prefixIndex);
  beginInsertRows(prefix_model_idx, cnt, cnt + unique_list.count() - 1); // ### FIXME

  foreach(const QString &file, unique_list)
    m_resource_file.addFile(prefixIndex, file);

  const QFileInfo fi(unique_list.last());
  m_lastResourceDir = fi.absolutePath();

  endInsertRows();
  setDirty(true);

  firstFile = cnt;
  lastFile = cnt + unique_list.count() - 1;

  Core::VcsManager::promptToAdd(m_resource_file.filePath().absolutePath(), Utils::transform(fileNames, &FilePath::fromString));
}

auto ResourceModel::insertPrefix(int prefixIndex, const QString &prefix, const QString &lang) -> void
{
  beginInsertRows(QModelIndex(), prefixIndex, prefixIndex);
  m_resource_file.addPrefix(prefix, lang, prefixIndex);
  endInsertRows();
  setDirty(true);
}

auto ResourceModel::insertFile(int prefixIndex, int fileIndex, const QString &fileName, const QString &alias) -> void
{
  const auto parent = index(prefixIndex, 0, QModelIndex());
  beginInsertRows(parent, fileIndex, fileIndex);
  m_resource_file.addFile(prefixIndex, fileName, fileIndex);
  m_resource_file.replaceAlias(prefixIndex, fileIndex, alias);
  endInsertRows();
  setDirty(true);
}

auto ResourceModel::renameFile(const QString &fileName, const QString &newFileName) -> bool
{
  const auto success = m_resource_file.renameFile(fileName, newFileName);
  if (success)
    setDirty(true);
  return success;
}

auto ResourceModel::changePrefix(const QModelIndex &model_idx, const QString &prefix) -> void
{
  if (!model_idx.isValid())
    return;

  const auto prefix_model_idx = prefixIndex(model_idx);
  const auto prefix_idx = model_idx.row();
  if (!m_resource_file.replacePrefix(prefix_idx, prefix))
    return;
  emit dataChanged(prefix_model_idx, prefix_model_idx);
  setDirty(true);
}

auto ResourceModel::changeLang(const QModelIndex &model_idx, const QString &lang) -> void
{
  if (!model_idx.isValid())
    return;

  const auto prefix_model_idx = prefixIndex(model_idx);
  const auto prefix_idx = model_idx.row();
  if (!m_resource_file.replaceLang(prefix_idx, lang))
    return;
  emit dataChanged(prefix_model_idx, prefix_model_idx);
  setDirty(true);
}

auto ResourceModel::changeAlias(const QModelIndex &index, const QString &alias) -> void
{
  if (!index.parent().isValid())
    return;

  if (m_resource_file.alias(index.parent().row(), index.row()) == alias)
    return;
  m_resource_file.replaceAlias(index.parent().row(), index.row(), alias);
  emit dataChanged(index, index);
  setDirty(true);
}

auto ResourceModel::deleteItem(const QModelIndex &idx) -> QModelIndex
{
  if (!idx.isValid())
    return QModelIndex();

  QString dummy, file;
  getItem(idx, dummy, file);
  auto prefix_idx = -1;
  auto file_idx = -1;

  beginRemoveRows(parent(idx), idx.row(), idx.row());
  if (file.isEmpty()) {
    // Remove prefix
    prefix_idx = idx.row();
    m_resource_file.removePrefix(prefix_idx);
    if (prefix_idx == m_resource_file.prefixCount())
      --prefix_idx;
  } else {
    // Remove file
    prefix_idx = prefixIndex(idx).row();
    file_idx = idx.row();
    m_resource_file.removeFile(prefix_idx, file_idx);
    if (file_idx == m_resource_file.fileCount(prefix_idx))
      --file_idx;
  }
  endRemoveRows();

  setDirty(true);

  if (prefix_idx == -1)
    return QModelIndex();
  const auto prefix_model_idx = index(prefix_idx, 0, QModelIndex());
  if (file_idx == -1)
    return prefix_model_idx;
  return index(file_idx, 0, prefix_model_idx);
}

auto ResourceModel::reload() -> Core::IDocument::OpenResult
{
  beginResetModel();
  auto result = m_resource_file.load();
  if (result == Core::IDocument::OpenResult::Success)
    setDirty(false);
  endResetModel();
  return result;
}

auto ResourceModel::save() -> bool
{
  const auto result = m_resource_file.save();
  if (result)
    setDirty(false);
  return result;
}

auto ResourceModel::lastResourceOpenDirectory() const -> QString
{
  if (m_lastResourceDir.isEmpty())
    return absolutePath(QString());
  return m_lastResourceDir;
}

// Create a resource path 'prefix:/file'
auto ResourceModel::resourcePath(const QString &prefix, const QString &file) -> QString
{
  auto rc = QString(QLatin1Char(':'));
  rc += prefix;
  rc += QLatin1Char('/');
  rc += file;
  return QDir::cleanPath(rc);
}

auto ResourceModel::mimeData(const QModelIndexList &indexes) const -> QMimeData*
{
  if (indexes.size() != 1)
    return nullptr;

  QString prefix, file;
  getItem(indexes.front(), prefix, file);
  if (prefix.isEmpty() || file.isEmpty())
    return nullptr;

  // DnD format of Designer 4.4
  QDomDocument doc;
  auto elem = doc.createElement(QLatin1String("resource"));
  elem.setAttribute(QLatin1String("type"), QLatin1String("image"));
  elem.setAttribute(QLatin1String("file"), resourcePath(prefix, file));
  doc.appendChild(elem);

  const auto rc = new QMimeData;
  rc->setText(doc.toString());
  return rc;
}

/*!
    \class FileEntryBackup

    Backups a file node.
*/
class FileEntryBackup : public EntryBackup {
private:
  int m_fileIndex;
  QString m_alias;

public:
  FileEntryBackup(ResourceModel &model, int prefixIndex, int fileIndex, const QString &fileName, const QString &alias) : EntryBackup(model, prefixIndex, fileName), m_fileIndex(fileIndex), m_alias(alias) { }
  auto restore() const -> void override;
};

auto FileEntryBackup::restore() const -> void
{
  m_model->insertFile(m_prefixIndex, m_fileIndex, m_name, m_alias);
}

/*!
    \class PrefixEntryBackup

    Backups a prefix node including children.
*/
class PrefixEntryBackup : public EntryBackup {
private:
  QString m_language;
  QList<FileEntryBackup> m_files;

public:
  PrefixEntryBackup(ResourceModel &model, int prefixIndex, const QString &prefix, const QString &language, const QList<FileEntryBackup> &files) : EntryBackup(model, prefixIndex, prefix), m_language(language), m_files(files) { }
  auto restore() const -> void override;
};

auto PrefixEntryBackup::restore() const -> void
{
  m_model->insertPrefix(m_prefixIndex, m_name, m_language);
  foreach(const FileEntryBackup &entry, m_files) {
    entry.restore();
  }
}

RelativeResourceModel::RelativeResourceModel(QObject *parent) : ResourceModel(parent), m_resourceDragEnabled(false) {}

auto RelativeResourceModel::flags(const QModelIndex &index) const -> Qt::ItemFlags
{
  auto rc = ResourceModel::flags(index);
  if ((rc & Qt::ItemIsEnabled) && m_resourceDragEnabled)
    rc |= Qt::ItemIsDragEnabled;
  return rc;
}

auto RelativeResourceModel::removeEntry(const QModelIndex &index) -> EntryBackup*
{
  const auto prefixIndex = this->prefixIndex(index);
  const auto isPrefixNode = (prefixIndex == index);

  // Create backup, remove, return backup
  if (isPrefixNode) {
    QString dummy;
    QString prefixBackup;
    getItem(index, prefixBackup, dummy);
    const auto languageBackup = lang(index);
    const auto childCount = rowCount(index);
    QList<FileEntryBackup> filesBackup;
    for (auto i = 0; i < childCount; i++) {
      const auto childIndex = this->index(i, 0, index);
      const auto fileNameBackup = file(childIndex);
      const auto aliasBackup = alias(childIndex);
      FileEntryBackup entry(*this, index.row(), i, fileNameBackup, aliasBackup);
      filesBackup << entry;
    }
    deleteItem(index);
    return new PrefixEntryBackup(*this, index.row(), prefixBackup, languageBackup, filesBackup);
  } else {
    const auto fileNameBackup = file(index);
    const auto aliasBackup = alias(index);
    if (!QFile::exists(fileNameBackup)) {
      deleteItem(index);
      return new FileEntryBackup(*this, prefixIndex.row(), index.row(), fileNameBackup, aliasBackup);
    }
    Utils::RemoveFileDialog removeFileDialog(Utils::FilePath::fromString(fileNameBackup), Core::ICore::dialogParent());
    if (removeFileDialog.exec() == QDialog::Accepted) {
      deleteItem(index);
      Core::FileUtils::removeFiles({Utils::FilePath::fromString(fileNameBackup)}, removeFileDialog.isDeleteFileChecked());
      return new FileEntryBackup(*this, prefixIndex.row(), index.row(), fileNameBackup, aliasBackup);
    }
    return nullptr;
  }
}

} // Internal
} // ResourceEditor
