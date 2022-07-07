// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.hpp"

#include <utils/fileutils.hpp>
#include <utils/id.hpp>

#include <QObject>

namespace Utils {
class FilePath;
class InfoBar;
} // namespace Utils

namespace Core {

namespace Internal {
class IDocumentPrivate;
}

class CORE_EXPORT IDocument : public QObject {
  Q_OBJECT

public:
  enum class OpenResult {
    Success,
    ReadError,
    CannotHandle
  };

  // This enum must match the indexes of the reloadBehavior widget
  // in generalsettings.ui
  enum ReloadSetting {
    AlwaysAsk = 0,
    ReloadUnmodified = 1,
    IgnoreAll = 2
  };

  enum ChangeTrigger {
    TriggerInternal,
    TriggerExternal
  };

  enum ChangeType {
    TypeContents,
    TypeRemoved
  };

  enum ReloadBehavior {
    BehaviorAsk,
    BehaviorSilent
  };

  enum ReloadFlag {
    FlagReload,
    FlagIgnore
  };

  explicit IDocument(QObject *parent = nullptr);
  ~IDocument() override;

  auto setId(Utils::Id id) const -> void;
  auto id() const -> Utils::Id;
  virtual auto open(QString *error_string, const Utils::FilePath &file_path, const Utils::FilePath &real_file_path) -> OpenResult;
  virtual auto save(QString *error_string, const Utils::FilePath &file_path = Utils::FilePath(), bool auto_save = false) -> bool;
  virtual auto contents() const -> QByteArray;
  virtual auto setContents(const QByteArray &contents) -> bool;
  auto filePath() const -> const Utils::FilePath&;
  virtual auto setFilePath(const Utils::FilePath &file_path) -> void;
  auto displayName() const -> QString;
  auto setPreferredDisplayName(const QString &name) -> void;
  auto preferredDisplayName() const -> QString;
  auto plainDisplayName() const -> QString;
  auto setUniqueDisplayName(const QString &name) const -> void;
  auto uniqueDisplayName() const -> QString;
  auto isFileReadOnly() const -> bool;
  auto isTemporary() const -> bool;
  auto setTemporary(bool temporary) const -> void;
  virtual auto fallbackSaveAsPath() const -> Utils::FilePath;
  virtual auto fallbackSaveAsFileName() const -> QString;
  auto mimeType() const -> QString;
  auto setMimeType(const QString &mime_type) -> void;
  virtual auto shouldAutoSave() const -> bool;
  virtual auto isModified() const -> bool;
  virtual auto isSaveAsAllowed() const -> bool;
  auto isSuspendAllowed() const -> bool;
  auto setSuspendAllowed(bool value) const -> void;
  virtual auto reloadBehavior(ChangeTrigger state, ChangeType type) const -> ReloadBehavior;
  virtual auto reload(QString *error_string, ReloadFlag flag, ChangeType type) -> bool;
  auto checkPermissions() -> void;
  auto autoSave(QString *error_string, const Utils::FilePath &file_path) -> bool;
  auto setRestoredFrom(const Utils::FilePath &path) const -> void;
  auto removeAutoSaveFile() const -> void;
  auto hasWriteWarning() const -> bool;
  auto setWriteWarning(bool has) const -> void;
  auto infoBar() const -> Utils::InfoBar*;

signals:
  // For meta data changes: file name, modified state, ...
  auto changed() -> void;

  // For changes in the contents of the document
  auto contentsChanged() -> void;
  auto mimeTypeChanged() -> void;
  auto aboutToReload() -> void;
  auto reloadFinished(bool success) -> void;
  auto filePathChanged(const Utils::FilePath &old_name, const Utils::FilePath &new_name) -> void;

private:
  Internal::IDocumentPrivate *d;
};

} // namespace Core
