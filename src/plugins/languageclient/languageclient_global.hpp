// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QtGlobal>

#if defined(LANGUAGECLIENT_LIBRARY)
#  define LANGUAGECLIENT_EXPORT Q_DECL_EXPORT
#else
#  define LANGUAGECLIENT_EXPORT Q_DECL_IMPORT
#endif

namespace LanguageClient {
namespace Constants {

constexpr char LANGUAGECLIENT_SETTINGS_CATEGORY[] = "ZY.LanguageClient";
constexpr char LANGUAGECLIENT_SETTINGS_PAGE[] = "LanguageClient.General";
constexpr char LANGUAGECLIENT_STDIO_SETTINGS_ID[] = "LanguageClient::StdIOSettingsID";
constexpr char LANGUAGECLIENT_SETTINGS_TR[] = QT_TRANSLATE_NOOP("LanguageClient", "Language Client");
constexpr char LANGUAGECLIENT_DOCUMENT_FILTER_ID[] = "Current Document Symbols";
constexpr char LANGUAGECLIENT_DOCUMENT_FILTER_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("LanguageClient", "Symbols in Current Document");
constexpr char LANGUAGECLIENT_WORKSPACE_FILTER_ID[] = "Workspace Symbols";
constexpr char LANGUAGECLIENT_WORKSPACE_FILTER_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("LanguageClient", "Symbols in Workspace");
constexpr char LANGUAGECLIENT_WORKSPACE_CLASS_FILTER_ID[] = "Workspace Classes and Structs";
constexpr char LANGUAGECLIENT_WORKSPACE_CLASS_FILTER_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("LanguageClient", "Classes and Structs in Workspace");
constexpr char LANGUAGECLIENT_WORKSPACE_METHOD_FILTER_ID[] = "Workspace Functions and Methods";
constexpr char LANGUAGECLIENT_WORKSPACE_METHOD_FILTER_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("LanguageClient", "Functions and Methods in Workspace");

} // namespace Constants
} // namespace LanguageClient
