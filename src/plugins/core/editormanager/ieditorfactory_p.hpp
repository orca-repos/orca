// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/mimetypes/mimetype.hpp>
#include <utils/mimetypes/mimedatabase.hpp>

#include <QHash>
#include <QSet>

namespace Core {

class EditorType;

namespace Internal {

auto userPreferredEditorTypes() -> QHash<Utils::MimeType, EditorType*>;
auto setUserPreferredEditorTypes(const QHash<Utils::MimeType, EditorType*> &factories) -> void;

/* Find the one best matching the mimetype passed in.
 * Recurse over the parent classes of the mimetype to find them. */
template <class EditorTypeLike>
static auto mimeTypeFactoryLookup(const Utils::MimeType &mime_type, const QList<EditorTypeLike*> &all_factories, QList<EditorTypeLike*> *list) -> void
{
  QSet<EditorTypeLike*> matches;

  // search breadth-first through parent hierarchy, e.g. for hierarchy
  // * application/x-ruby
  //     * application/x-executable
  //         * application/octet-stream
  //     * text/plain

  QList<Utils::MimeType> queue;
  QSet<QString> seen;
  queue.append(mime_type);
  seen.insert(mime_type.name());

  while (!queue.isEmpty()) {
    auto mt = queue.takeFirst();
    // check for matching factories
    for(const auto factory: all_factories) {
      if (!matches.contains(factory)) {
        for(const auto &mime_name: factory->mimeTypes()) {
          if (mt.matchesName(mime_name)) {
            list->append(factory);
            matches.insert(factory);
          }
        }
      }
    }
    // add parent mime types
    for(auto parent_names = mt.parentMimeTypes(); const auto &parent_name:  parent_names) {
      if (const auto parent = Utils::mimeTypeForName(parent_name); parent.isValid()) {
        const int seen_size = seen.size();
        seen.insert(parent.name());
        if (seen.size() != seen_size) // not seen before, so add
          queue.append(parent);
      }
    }
  }
}

} // Internal
} // Core
