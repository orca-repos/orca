// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/ioutlinewidget.hpp>

namespace TextEditor {
class TextDocument;
}

namespace Utils {
class TreeViewComboBox;
}

namespace LanguageClient {

class Client;

class LanguageClientOutlineWidgetFactory : public TextEditor::IOutlineWidgetFactory {
public:
  using IOutlineWidgetFactory::IOutlineWidgetFactory;

  static auto createComboBox(Client *client, Core::IEditor *editor) -> Utils::TreeViewComboBox*;
  static auto clientSupportsDocumentSymbols(const Client *client, const TextEditor::TextDocument *doc) -> bool;
  auto supportsEditor(Core::IEditor *editor) const -> bool override;
  auto createWidget(Core::IEditor *editor) -> TextEditor::IOutlineWidget* override;
  auto supportsSorting() const -> bool override { return true; }
};

} // namespace LanguageClient
