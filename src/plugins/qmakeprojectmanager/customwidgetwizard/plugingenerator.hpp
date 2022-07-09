// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>

QT_BEGIN_NAMESPACE
QT_END_NAMESPACE

namespace Core {
class GeneratedFile;
}

namespace QmakeProjectManager {
namespace Internal {

struct PluginOptions;

struct GenerationParameters {
  QString path;
  QString fileName;
  QString templatePath;
};

class PluginGenerator : public QObject {
  Q_OBJECT public:
  static auto generatePlugin(const GenerationParameters &p, const PluginOptions &options, QString *errorMessage) -> QList<Core::GeneratedFile>;

private:
  using SubstitutionMap = QMap<QString, QString>;
  static auto processTemplate(const QString &tmpl, const SubstitutionMap &substMap, QString *errorMessage) -> QString;
  static auto cStringQuote(QString s) -> QString;
};

}
}
