// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "codegenerator.hpp"

#include "codegensettings.hpp"

#include <core/icore.hpp>

#include <utils/codegeneration.hpp>
#include <utils/qtcassert.hpp>

#include <QDomDocument>
#include <QSettings>
#include <QTextStream>
#include <QXmlStreamReader>

namespace QtSupport {

// Change the contents of a DOM element to a new value if it matches
// a predicate
template <class Predicate>
auto changeDomElementContents(const QDomElement &element, Predicate p, const QString &newValue, QString *ptrToOldValue = nullptr) -> bool
{
  // Find text in "<element>text</element>"
  const auto children = element.childNodes();
  if (children.size() != 1)
    return false;
  const auto first = children.at(0);
  if (first.nodeType() != QDomNode::TextNode)
    return false;
  auto data = first.toCharacterData();
  const auto oldValue = data.data();

  if (p(oldValue)) {
    if (ptrToOldValue)
      *ptrToOldValue = oldValue;
    data.setData(newValue);
    return true;
  }
  return false;
}

namespace {
auto truePredicate(const QString &) -> bool { return true; }

// Predicate that matches a string value
class MatchPredicate {
public:
  MatchPredicate(const QString &m) : m_match(m) {}
  auto operator()(const QString &s) const -> bool { return s == m_match; }
private:
  const QString m_match;
};

// Change <sender> and <receiver> in a Dom UI <connections> list
// if they match the class name passed on
auto changeDomConnectionList(const QDomElement &connectionsNode, const QString &oldClassName, const QString &newClassName) -> void
{
  const MatchPredicate oldClassPredicate(oldClassName);
  const QString senderTag = QLatin1String("sender");
  const QString receiverTag = QLatin1String("receiver");
  const auto connections = connectionsNode.childNodes();
  const auto connectionsCount = connections.size();
  // Loop <connection>
  for (auto c = 0; c < connectionsCount; c++) {
    const auto connectionElements = connections.at(c).childNodes();
    const auto connectionElementCount = connectionElements.count();
    // Loop <sender>, <receiver>, <signal>, <slot>
    for (auto ce = 0; ce < connectionElementCount; ce++) {
      const auto connectionElementNode = connectionElements.at(ce);
      if (connectionElementNode.isElement()) {
        const auto connectionElement = connectionElementNode.toElement();
        const auto tagName = connectionElement.tagName();
        if (tagName == senderTag || tagName == receiverTag)
          changeDomElementContents(connectionElement, oldClassPredicate, newClassName);
      }
    }
  }
}
}

// Change the UI class name in UI xml: This occurs several times, as contents
// of the <class> element, as name of the first <widget> element, and possibly
// in the signal/slot connections

auto CodeGenerator::changeUiClassName(const QString &uiXml, const QString &newUiClassName) -> QString
{
  QDomDocument domUi;
  if (!domUi.setContent(uiXml)) {
    qWarning("Failed to parse:\n%s", uiXml.toUtf8().constData());
    return uiXml;
  }

  auto firstWidgetElementFound = false;
  QString oldClassName;

  // Loop first level children. First child is <ui>
  const auto children = domUi.firstChildElement().childNodes();
  const QString classTag = QLatin1String("class");
  const QString widgetTag = QLatin1String("widget");
  const QString connectionsTag = QLatin1String("connections");
  const auto count = children.size();
  for (auto i = 0; i < count; i++) {
    const auto node = children.at(i);
    if (node.isElement()) {
      // Replace <class> element text
      auto element = node.toElement();
      const auto name = element.tagName();
      if (name == classTag) {
        if (!changeDomElementContents(element, truePredicate, newUiClassName, &oldClassName)) {
          qWarning("Unable to change the <class> element:\n%s", uiXml.toUtf8().constData());
          return uiXml;
        }
      } else {
        // Replace first <widget> element name attribute
        if (!firstWidgetElementFound && name == widgetTag) {
          firstWidgetElementFound = true;
          const QString nameAttribute = QLatin1String("name");
          if (element.hasAttribute(nameAttribute))
            element.setAttribute(nameAttribute, newUiClassName);
        } else {
          // Replace <sender>, <receiver> tags of dialogs.
          if (name == connectionsTag)
            changeDomConnectionList(element, oldClassName, newUiClassName);
        }
      }
    }
  }
  const auto rc = domUi.toString();
  return rc;
}

auto CodeGenerator::uiData(const QString &uiXml, QString *formBaseClass, QString *uiClassName) -> bool
{
  // Parse UI xml to determine
  // 1) The ui class name from "<class>Designer::Internal::FormClassWizardPage</class>"
  // 2) the base class from: widget class="QWizardPage"...
  QXmlStreamReader reader(uiXml);
  while (!reader.atEnd()) {
    if (reader.readNext() == QXmlStreamReader::StartElement) {
      if (reader.name() == QLatin1String("class")) {
        *uiClassName = reader.readElementText();
      } else if (reader.name() == QLatin1String("widget")) {
        const auto attrs = reader.attributes();
        *formBaseClass = attrs.value(QLatin1String("class")).toString();
        return !uiClassName->isEmpty() && !formBaseClass->isEmpty();
      }
    }
  }
  return false;
}

auto CodeGenerator::uiClassName(const QString &uiXml) -> QString
{
  QString base;
  QString klass;
  QTC_ASSERT(uiData(uiXml, &base, &klass), return QString());
  return klass;
}

auto CodeGenerator::qtIncludes(const QStringList &qt4, const QStringList &qt5) -> QString
{
  CodeGenSettings settings;
  settings.fromSettings(Core::ICore::settings());

  QString result;
  QTextStream str(&result);
  Utils::writeQtIncludeSection(qt4, qt5, settings.addQtVersionCheck, settings.includeQtModule, str);
  return result;
}

auto CodeGenerator::uiAsPointer() -> bool
{
  CodeGenSettings settings;
  settings.fromSettings(Core::ICore::settings());
  return settings.embedding == CodeGenSettings::PointerAggregatedUiClass;
}

auto CodeGenerator::uiAsMember() -> bool
{
  CodeGenSettings settings;
  settings.fromSettings(Core::ICore::settings());
  return settings.embedding == CodeGenSettings::AggregatedUiClass;
}

auto CodeGenerator::uiAsInheritance() -> bool
{
  CodeGenSettings settings;
  settings.fromSettings(Core::ICore::settings());
  return settings.embedding == CodeGenSettings::InheritedUiClass;
}

} // namespace QtSupport
