// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "macroexpander.h"

#include "algorithm.h"
#include "fileutils.h"
#include "commandline.h"
#include "qtcassert.h"
#include "stringutils.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMap>

namespace Utils {
namespace Internal {

static Q_LOGGING_CATEGORY(expanderLog, "qtc.utils.macroexpander", QtWarningMsg)

constexpr char kFilePathPostfix[] = ":FilePath";
constexpr char kPathPostfix[] = ":Path";
constexpr char kNativeFilePathPostfix[] = ":NativeFilePath";
constexpr char kNativePathPostfix[] = ":NativePath";
constexpr char kFileNamePostfix[] = ":FileName";
constexpr char kFileBaseNamePostfix[] = ":FileBaseName";

class MacroExpanderPrivate : public AbstractMacroExpander {
public:
  MacroExpanderPrivate() = default;

  auto resolveMacro(const QString &name, QString *ret, QSet<AbstractMacroExpander*> &seen) -> bool override
  {
    // Prevent loops:
    const int count = seen.count();
    seen.insert(this);
    if (seen.count() == count)
      return false;

    bool found;
    *ret = value(name.toUtf8(), &found);
    if (found)
      return true;

    found = Utils::anyOf(m_subProviders, [name, ret, &seen](const MacroExpanderProvider &p) -> bool {
      MacroExpander *expander = p ? p() : 0;
      return expander && expander->d->resolveMacro(name, ret, seen);
    });

    if (found)
      return true;

    found = Utils::anyOf(m_extraResolvers, [name, ret](const MacroExpander::ResolverFunction &resolver) {
      return resolver(name, ret);
    });

    if (found)
      return true;

    return this == globalMacroExpander()->d ? false : globalMacroExpander()->d->resolveMacro(name, ret, seen);
  }

  auto value(const QByteArray &variable, bool *found) const -> QString
  {
    MacroExpander::StringFunction sf = m_map.value(variable);
    if (sf) {
      if (found)
        *found = true;
      return sf();
    }

    for (auto it = m_prefixMap.constBegin(); it != m_prefixMap.constEnd(); ++it) {
      if (variable.startsWith(it.key())) {
        MacroExpander::PrefixFunction pf = it.value();
        if (found)
          *found = true;
        return pf(QString::fromUtf8(variable.mid(it.key().count())));
      }
    }
    if (found)
      *found = false;

    return QString();
  }

  QHash<QByteArray, MacroExpander::StringFunction> m_map;
  QHash<QByteArray, MacroExpander::PrefixFunction> m_prefixMap;
  QVector<MacroExpander::ResolverFunction> m_extraResolvers;
  QMap<QByteArray, QString> m_descriptions;
  QString m_displayName;
  QVector<MacroExpanderProvider> m_subProviders;
  bool m_accumulating = false;

  bool m_aborted = false;
  int m_lockDepth = 0;
};

} // Internal

using namespace Internal;

/*!
    \class Utils::MacroExpander
    \brief The MacroExpander class manages \QC wide variables, that a user
    can enter into many string settings. The variables are replaced by an actual value when the string
    is used, similar to how environment variables are expanded by a shell.

    \section1 Variables

    Variable names can be basically any string without dollar sign and braces,
    though it is recommended to only use 7-bit ASCII without special characters and whitespace.

    If there are several variables that contain different aspects of the same object,
    it is convention to give them the same prefix, followed by a colon and a postfix
    that describes the aspect.
    Examples of this are \c{CurrentDocument:FilePath} and \c{CurrentDocument:Selection}.

    When the variable manager is requested to replace variables in a string, it looks for
    variable names enclosed in %{ and }, like %{CurrentDocument:FilePath}.

    Environment variables are accessible using the %{Env:...} notation.
    For example, to access the SHELL environment variable, use %{Env:SHELL}.

    \note The names of the variables are stored as QByteArray. They are typically
    7-bit-clean. In cases where this is not possible, UTF-8 encoding is
    assumed.

    \section1 Providing Variable Values

    Plugins can register variables together with a description through registerVariable().
    A typical setup is to register variables in the Plugin::initialize() function.

    \code
    bool MyPlugin::initialize(const QStringList &arguments, QString *errorString)
    {
        [...]
        MacroExpander::registerVariable(
            "MyVariable",
            tr("The current value of whatever I want."));
            []() -> QString {
                QString value;
                // do whatever is necessary to retrieve the value
                [...]
                return value;
            }
        );
        [...]
    }
    \endcode


    For variables that refer to a file, you should use the convenience function
    MacroExpander::registerFileVariables().
    The functions take a variable prefix, like \c MyFileVariable,
    and automatically handle standardized postfixes like \c{:FilePath},
    \c{:Path} and \c{:FileBaseName}, resulting in the combined variables, such as
    \c{MyFileVariable:FilePath}.

    \section1 Providing and Expanding Parametrized Strings

    Though it is possible to just ask the variable manager for the value of some variable in your
    code, the preferred use case is to give the user the possibility to parametrize strings, for
    example for settings.

    (If you ever think about doing the former, think twice. It is much more efficient
    to just ask the plugin that provides the variable value directly, without going through
    string conversions, and through the variable manager which will do a large scale poll. To be
    more concrete, using the example from the Providing Variable Values section: instead of
    calling \c{MacroExpander::value("MyVariable")}, it is much more efficient to just ask directly
    with \c{MyPlugin::variableValue()}.)

    \section2 User Interface

    If the string that you want to parametrize is settable by the user, through a QLineEdit or
    QTextEdit derived class, you should add a variable chooser to your UI, which allows adding
    variables to the string by browsing through a list. See Utils::VariableChooser for more
    details.

    \section2 Expanding Strings

    Expanding variable values in strings is done by "macro expanders".
    Utils::AbstractMacroExpander is the base class for these, and the variable manager
    provides an implementation that expands \QC variables through
    MacroExpander::macroExpander().

    There are several different ways to expand a string, covering the different use cases,
    listed here sorted by relevance:
    \list
    \li Using MacroExpander::expandedString(). This is the most comfortable way to get a string
        with variable values expanded, but also the least flexible one. If this is sufficient for
        you, use it.
    \li Using the Utils::expandMacros() functions. These take a string and a macro expander (for which
        you would use the one provided by the variable manager). Mostly the same as
        MacroExpander::expandedString(), but also has a variant that does the replacement inline
        instead of returning a new string.
    \li Using Utils::QtcProcess::expandMacros(). This expands the string while conforming to the
        quoting rules of the platform it is run on. Use this function with the variable manager's
        macro expander if your string will be passed as a command line parameter string to an
        external command.
    \li Writing your own macro expander that nests the variable manager's macro expander. And then
        doing one of the above. This allows you to expand additional "local" variables/macros,
        that do not come from the variable manager.
    \endlist

*/

/*!
 * \internal
 */
MacroExpander::MacroExpander()
{
  d = new MacroExpanderPrivate;
}

/*!
 * \internal
 */
MacroExpander::~MacroExpander()
{
  delete d;
}

/*!
 * \internal
 */
auto MacroExpander::resolveMacro(const QString &name, QString *ret) const -> bool
{
  QSet<AbstractMacroExpander*> seen;
  return d->resolveMacro(name, ret, seen);
}

/*!
 * Returns the value of the given \a variable. If \a found is given, it is
 * set to true if the variable has a value at all, false if not.
 */
auto MacroExpander::value(const QByteArray &variable, bool *found) const -> QString
{
  return d->value(variable, found);
}

/*!
 * Returns \a stringWithVariables with all variables replaced by their values.
 * See the MacroExpander overview documentation for other ways to expand variables.
 *
 * \sa MacroExpander
 * \sa macroExpander()
 */
auto MacroExpander::expand(const QString &stringWithVariables) const -> QString
{
  if (d->m_lockDepth == 0)
    d->m_aborted = false;

  if (d->m_lockDepth > 10) {
    // Limit recursion.
    d->m_aborted = true;
    return QString();
  }

  ++d->m_lockDepth;

  QString res = stringWithVariables;
  Utils::expandMacros(&res, d);

  --d->m_lockDepth;

  if (d->m_lockDepth == 0 && d->m_aborted)
    return tr("Infinite recursion error") + QLatin1String(": ") + stringWithVariables;

  return res;
}

auto MacroExpander::expand(const FilePath &fileNameWithVariables) const -> FilePath
{
  FilePath result = fileNameWithVariables;
  result.setPath(expand(result.path()));
  result.setHost(expand(result.host()));
  result.setScheme(expand(result.scheme()));
  return result;
}

auto MacroExpander::expand(const QByteArray &stringWithVariables) const -> QByteArray
{
  return expand(QString::fromLatin1(stringWithVariables)).toLatin1();
}

auto MacroExpander::expandVariant(const QVariant &v) const -> QVariant
{
  const auto type = QMetaType::Type(v.type());
  if (type == QMetaType::QString) {
    return expand(v.toString());
  } else if (type == QMetaType::QStringList) {
    return Utils::transform(v.toStringList(), [this](const QString &s) -> QVariant { return expand(s); });
  } else if (type == QMetaType::QVariantList) {
    return Utils::transform(v.toList(), [this](const QVariant &v) { return expandVariant(v); });
  } else if (type == QMetaType::QVariantMap) {
    const auto map = v.toMap();
    QVariantMap result;
    for (auto it = map.cbegin(), end = map.cend(); it != end; ++it)
      result.insert(it.key(), expandVariant(it.value()));
    return result;
  }
  return v;
}

auto MacroExpander::expandProcessArgs(const QString &argsWithVariables) const -> QString
{
  QString result = argsWithVariables;
  const bool ok = ProcessArgs::expandMacros(&result, d);
  QTC_ASSERT(ok, qCDebug(expanderLog) << "Expanding failed: " << argsWithVariables);
  return result;
}

static auto fullPrefix(const QByteArray &prefix) -> QByteArray
{
  QByteArray result = prefix;
  if (!result.endsWith(':'))
    result.append(':');
  return result;
}

/*!
 * Makes the given string-valued \a prefix known to the variable manager,
 * together with a localized \a description.
 *
 * The \a value PrefixFunction will be called and gets the full variable name
 * with the prefix stripped as input.
 *
 * \sa registerVariables(), registerIntVariable(), registerFileVariables()
 */
auto MacroExpander::registerPrefix(const QByteArray &prefix, const QString &description, const MacroExpander::PrefixFunction &value, bool visible) -> void
{
  QByteArray tmp = fullPrefix(prefix);
  if (visible)
    d->m_descriptions.insert(tmp + "<value>", description);
  d->m_prefixMap.insert(tmp, value);
}

/*!
 * Makes the given string-valued \a variable known to the variable manager,
 * together with a localized \a description.
 *
 * \sa registerFileVariables(), registerIntVariable(), registerPrefix()
 */
auto MacroExpander::registerVariable(const QByteArray &variable, const QString &description, const StringFunction &value, bool visibleInChooser) -> void
{
  if (visibleInChooser)
    d->m_descriptions.insert(variable, description);
  d->m_map.insert(variable, value);
}

/*!
 * Makes the given integral-valued \a variable known to the variable manager,
 * together with a localized \a description.
 *
 * \sa registerVariable(), registerFileVariables(), registerPrefix()
 */
auto MacroExpander::registerIntVariable(const QByteArray &variable, const QString &description, const MacroExpander::IntFunction &value) -> void
{
  const MacroExpander::IntFunction valuecopy = value; // do not capture a reference in a lambda
  registerVariable(variable, description, [valuecopy]() { return QString::number(valuecopy ? valuecopy() : 0); });
}

/*!
 * Convenience function to register several variables with the same \a prefix, that have a file
 * as a value. Takes the prefix and registers variables like \c{prefix:FilePath} and
 * \c{prefix:Path}, with descriptions that start with the given \a heading.
 * For example \c{registerFileVariables("CurrentDocument", tr("Current Document"))} registers
 * variables such as \c{CurrentDocument:FilePath} with description
 * "Current Document: Full path including file name."
 *
 * \sa registerVariable(), registerIntVariable(), registerPrefix()
 */
auto MacroExpander::registerFileVariables(const QByteArray &prefix, const QString &heading, const FileFunction &base, bool visibleInChooser) -> void
{
  registerVariable(prefix + kFilePathPostfix, tr("%1: Full path including file name.").arg(heading), [base]() -> QString {
    QString tmp = base().toString();
    return tmp.isEmpty() ? QString() : QFileInfo(tmp).filePath();
  }, visibleInChooser);

  registerVariable(prefix + kPathPostfix, tr("%1: Full path excluding file name.").arg(heading), [base]() -> QString {
    QString tmp = base().toString();
    return tmp.isEmpty() ? QString() : QFileInfo(tmp).path();
  }, visibleInChooser);

  registerVariable(prefix + kNativeFilePathPostfix, tr("%1: Full path including file name, with native path separator (backslash on Windows).").arg(heading), [base]() -> QString {
    QString tmp = base().toString();
    return tmp.isEmpty() ? QString() : QDir::toNativeSeparators(QFileInfo(tmp).filePath());
  }, visibleInChooser);

  registerVariable(prefix + kNativePathPostfix, tr("%1: Full path excluding file name, with native path separator (backslash on Windows).").arg(heading), [base]() -> QString {
    QString tmp = base().toString();
    return tmp.isEmpty() ? QString() : QDir::toNativeSeparators(QFileInfo(tmp).path());
  }, visibleInChooser);

  registerVariable(prefix + kFileNamePostfix, tr("%1: File name without path.").arg(heading), [base]() -> QString {
    QString tmp = base().toString();
    return tmp.isEmpty() ? QString() : FilePath::fromString(tmp).fileName();
  }, visibleInChooser);

  registerVariable(prefix + kFileBaseNamePostfix, tr("%1: File base name without path and suffix.").arg(heading), [base]() -> QString {
    QString tmp = base().toString();
    return tmp.isEmpty() ? QString() : QFileInfo(tmp).baseName();
  }, visibleInChooser);
}

auto MacroExpander::registerExtraResolver(const MacroExpander::ResolverFunction &value) -> void
{
  d->m_extraResolvers.append(value);
}

/*!
 * Returns all registered variable names.
 *
 * \sa registerVariable()
 * \sa registerFileVariables()
 */
auto MacroExpander::visibleVariables() const -> QList<QByteArray>
{
  return d->m_descriptions.keys();
}

/*!
 * Returns the description that was registered for the \a variable.
 */
auto MacroExpander::variableDescription(const QByteArray &variable) const -> QString
{
  return d->m_descriptions.value(variable);
}

auto MacroExpander::isPrefixVariable(const QByteArray &variable) const -> bool
{
  return d->m_prefixMap.contains(fullPrefix(variable));
}

auto MacroExpander::subProviders() const -> MacroExpanderProviders
{
  return d->m_subProviders;
}

auto MacroExpander::displayName() const -> QString
{
  return d->m_displayName;
}

auto MacroExpander::setDisplayName(const QString &displayName) -> void
{
  d->m_displayName = displayName;
}

auto MacroExpander::registerSubProvider(const MacroExpanderProvider &provider) -> void
{
  d->m_subProviders.append(provider);
}

auto MacroExpander::isAccumulating() const -> bool
{
  return d->m_accumulating;
}

auto MacroExpander::setAccumulating(bool on) -> void
{
  d->m_accumulating = on;
}

class GlobalMacroExpander : public MacroExpander {
public:
  GlobalMacroExpander()
  {
    setDisplayName(MacroExpander::tr("Global variables"));
    registerPrefix("Env", MacroExpander::tr("Access environment variables."), [](const QString &value) { return QString::fromLocal8Bit(qgetenv(value.toLocal8Bit())); });
  }
};

/*!
 * Returns the expander for globally registered variables.
 */
auto globalMacroExpander() -> MacroExpander*
{
  static GlobalMacroExpander theGlobalExpander;
  return &theGlobalExpander;
}

} // namespace Utils
