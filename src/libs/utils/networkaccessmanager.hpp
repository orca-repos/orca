// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QNetworkAccessManager>

QT_FORWARD_DECLARE_CLASS(QUrl)

namespace Utils {

class ORCA_UTILS_EXPORT NetworkAccessManager : public QNetworkAccessManager {
  Q_OBJECT

public:
  NetworkAccessManager(QObject *parent = nullptr);

  static auto instance() -> NetworkAccessManager*;

protected:
  auto createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) -> QNetworkReply* override;
};


} // namespace utils
