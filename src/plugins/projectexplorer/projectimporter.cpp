// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectimporter.hpp"

#include "buildinfo.hpp"
#include "kit.hpp"
#include "kitinformation.hpp"
#include "kitmanager.hpp"
#include "project.hpp"
#include "projectexplorerconstants.hpp"
#include "target.hpp"
#include "toolchain.hpp"
#include "toolchainmanager.hpp"

#include <core/icore.hpp>

#include <utils/algorithm.hpp>
#include <utils/environment.hpp>
#include <utils/qtcassert.hpp>

#include <QLoggingCategory>
#include <QMessageBox>
#include <QPushButton>
#include <QString>

namespace ProjectExplorer {

static const Utils::Id KIT_IS_TEMPORARY("PE.tmp.isTemporary");
static const Utils::Id KIT_TEMPORARY_NAME("PE.tmp.Name");
static const Utils::Id KIT_FINAL_NAME("PE.tmp.FinalName");
static const Utils::Id TEMPORARY_OF_PROJECTS("PE.tmp.ForProjects");

static auto fullId(Utils::Id id) -> Utils::Id
{
  const QString prefix = "PE.tmp.";

  const auto idStr = id.toString();
  QTC_ASSERT(!idStr.startsWith(prefix), return Utils::Id::fromString(idStr));

  return Utils::Id::fromString(prefix + idStr);
}

static auto hasOtherUsers(Utils::Id id, const QVariant &v, Kit *k) -> bool
{
  return Utils::contains(KitManager::kits(), [id, v, k](Kit *in) -> bool {
    if (in == k)
      return false;
    const auto tmp = in->value(id).toList();
    return tmp.contains(v);
  });
}

ProjectImporter::ProjectImporter(const Utils::FilePath &path) : m_projectPath(path)
{
  useTemporaryKitAspect(ToolChainKitAspect::id(), [this](Kit *k, const QVariantList &vl) { cleanupTemporaryToolChains(k, vl); }, [this](Kit *k, const QVariantList &vl) { persistTemporaryToolChains(k, vl); });
}

ProjectImporter::~ProjectImporter()
{
  foreach(Kit *k, KitManager::kits())
    removeProject(k);
}

auto ProjectImporter::import(const Utils::FilePath &importPath, bool silent) -> const QList<BuildInfo>
{
  QList<BuildInfo> result;

  const QLoggingCategory log("qtc.projectexplorer.import", QtWarningMsg);
  qCDebug(log) << "ProjectImporter::import" << importPath << silent;

  auto fi = importPath.toFileInfo();
  if (!fi.exists() && !fi.isDir()) {
    qCDebug(log) << "**doesn't exist";
    return result;
  }

  const auto absoluteImportPath = Utils::FilePath::fromString(fi.absoluteFilePath());

  const auto handleFailure = [this, importPath, silent] {
    if (silent)
      return;
    QMessageBox::critical(Core::ICore::dialogParent(), tr("No Build Found"), tr("No build found in %1 matching project %2.").arg(importPath.toUserOutput(), projectFilePath().toUserOutput()));
  };
  qCDebug(log) << "Examining directory" << absoluteImportPath.toString();
  QString warningMessage;
  auto dataList = examineDirectory(absoluteImportPath, &warningMessage);
  if (dataList.isEmpty()) {
    qCDebug(log) << "Nothing to import found in" << absoluteImportPath.toString();
    handleFailure();
    return result;
  }
  if (!warningMessage.isEmpty()) {
    qCDebug(log) << "Warning when examining" << absoluteImportPath.toString();
    // we should ask user before importing
    if (silent)
      return result;
    QMessageBox dialog(Core::ICore::dialogParent());
    dialog.setWindowTitle(tr("Import Warning"));
    dialog.setText(warningMessage);
    dialog.setIcon(QMessageBox::Warning);
    auto acceptButton = dialog.addButton(tr("Import Build"), QMessageBox::AcceptRole);
    dialog.addButton(QMessageBox::Cancel);
    dialog.exec();
    if (dialog.clickedButton() != acceptButton)
      return result;
  }

  qCDebug(log) << "Looking for kits";
  foreach(void *data, dataList) {
    QTC_ASSERT(data, continue);
    QList<Kit*> kitList;
    const auto tmp = Utils::filtered(KitManager::kits(), [this, data](Kit *k) { return matchKit(data, k); });
    if (tmp.isEmpty()) {
      auto k = createKit(data);
      if (k)
        kitList.append(k);
      qCDebug(log) << "  no matching kit found, temporary kit created.";
    } else {
      kitList += tmp;
      qCDebug(log) << "  " << tmp.count() << "matching kits found.";
    }

    foreach(Kit *k, kitList) {
      qCDebug(log) << "Creating buildinfos for kit" << k->displayName();
      const auto infoList = buildInfoList(data);
      if (infoList.isEmpty()) {
        qCDebug(log) << "No build infos for kit" << k->displayName();
        continue;
      }

      auto factory = BuildConfigurationFactory::find(k, projectFilePath());
      for (auto i : infoList) {
        i.kitId = k->id();
        i.factory = factory;
        if (!result.contains(i))
          result += i;
      }
    }
  }

  foreach(auto *dd, dataList)
    deleteDirectoryData(dd);
  dataList.clear();

  if (result.isEmpty())
    handleFailure();

  return result;
}

auto ProjectImporter::preferredTarget(const QList<Target*> &possibleTargets) -> Target*
{
  // Select active target
  // a) The default target
  // c) Desktop target
  // d) the first target
  Target *activeTarget = nullptr;
  if (possibleTargets.isEmpty())
    return activeTarget;

  activeTarget = possibleTargets.at(0);
  auto pickedFallback = false;
  foreach(Target *t, possibleTargets) {
    if (t->kit() == KitManager::defaultKit())
      return t;
    if (pickedFallback)
      continue;
    if (DeviceTypeKitAspect::deviceTypeId(t->kit()) == Constants::DESKTOP_DEVICE_TYPE) {
      activeTarget = t;
      pickedFallback = true;
    }
  }
  return activeTarget;
}

auto ProjectImporter::markKitAsTemporary(Kit *k) const -> void
{
  QTC_ASSERT(!k->hasValue(KIT_IS_TEMPORARY), return);

  UpdateGuard guard(*this);

  const auto name = k->displayName();
  k->setUnexpandedDisplayName(QCoreApplication::translate("ProjectExplorer::ProjectImporter", "%1 - temporary").arg(name));

  k->setValue(KIT_TEMPORARY_NAME, k->displayName());
  k->setValue(KIT_FINAL_NAME, name);
  k->setValue(KIT_IS_TEMPORARY, true);
}

auto ProjectImporter::makePersistent(Kit *k) const -> void
{
  QTC_ASSERT(k, return);
  if (!k->hasValue(KIT_IS_TEMPORARY))
    return;

  UpdateGuard guard(*this);

  KitGuard kitGuard(k);
  k->removeKey(KIT_IS_TEMPORARY);
  k->removeKey(TEMPORARY_OF_PROJECTS);
  const auto tempName = k->value(KIT_TEMPORARY_NAME).toString();
  if (!tempName.isNull() && k->displayName() == tempName)
    k->setUnexpandedDisplayName(k->value(KIT_FINAL_NAME).toString());
  k->removeKey(KIT_TEMPORARY_NAME);
  k->removeKey(KIT_FINAL_NAME);

  foreach(const TemporaryInformationHandler &tih, m_temporaryHandlers) {
    const auto fid = fullId(tih.id);
    const auto temporaryValues = k->value(fid).toList();

    // Mark permanent in all other kits:
    foreach(Kit *ok, KitManager::kits()) {
      if (ok == k || !ok->hasValue(fid))
        continue;
      const auto otherTemporaryValues = Utils::filtered(ok->value(fid).toList(), [&temporaryValues](const QVariant &v) {
        return !temporaryValues.contains(v);
      });
      ok->setValueSilently(fid, otherTemporaryValues);
    }

    // persist:
    tih.persist(k, temporaryValues);
    k->removeKeySilently(fid);
  }
}

auto ProjectImporter::cleanupKit(Kit *k) const -> void
{
  QTC_ASSERT(k, return);
  foreach(const TemporaryInformationHandler &tih, m_temporaryHandlers) {
    const auto fid = fullId(tih.id);
    const auto temporaryValues = Utils::filtered(k->value(fid).toList(), [fid, k](const QVariant &v) {
      return !hasOtherUsers(fid, v, k);
    });
    tih.cleanup(k, temporaryValues);
    k->removeKeySilently(fid);
  }

  // remove keys to manage temporary state of kit:
  k->removeKeySilently(KIT_IS_TEMPORARY);
  k->removeKeySilently(TEMPORARY_OF_PROJECTS);
  k->removeKeySilently(KIT_FINAL_NAME);
  k->removeKeySilently(KIT_TEMPORARY_NAME);
}

auto ProjectImporter::addProject(Kit *k) const -> void
{
  QTC_ASSERT(k, return);
  if (!k->hasValue(KIT_IS_TEMPORARY))
    return;

  UpdateGuard guard(*this);
  auto projects = k->value(TEMPORARY_OF_PROJECTS, QStringList()).toStringList();
  projects.append(m_projectPath.toString()); // note: There can be more than one instance of the project added!
  k->setValueSilently(TEMPORARY_OF_PROJECTS, projects);
}

auto ProjectImporter::removeProject(Kit *k) const -> void
{
  QTC_ASSERT(k, return);
  if (!k->hasValue(KIT_IS_TEMPORARY))
    return;

  UpdateGuard guard(*this);
  auto projects = k->value(TEMPORARY_OF_PROJECTS, QStringList()).toStringList();
  projects.removeOne(m_projectPath.toString());

  if (projects.isEmpty()) {
    cleanupKit(k);
    KitManager::deregisterKit(k);
  } else {
    k->setValueSilently(TEMPORARY_OF_PROJECTS, projects);
  }
}

auto ProjectImporter::isTemporaryKit(Kit *k) const -> bool
{
  QTC_ASSERT(k, return false);
  return k->hasValue(KIT_IS_TEMPORARY);
}

auto ProjectImporter::createTemporaryKit(const KitSetupFunction &setup) const -> Kit*
{
  UpdateGuard guard(*this);
  const auto init = [&](Kit *k) {
    KitGuard kitGuard(k);
    k->setUnexpandedDisplayName(QCoreApplication::translate("ProjectExplorer::ProjectImporter", "Imported Kit"));
    k->setup();
    setup(k);
    k->fix();
    markKitAsTemporary(k);
    addProject(k);
  };                                    // ~KitGuard, sending kitUpdated
  return KitManager::registerKit(init); // potentially adds kits to other targetsetuppages
}

auto ProjectImporter::findTemporaryHandler(Utils::Id id) const -> bool
{
  return Utils::contains(m_temporaryHandlers, [id](const TemporaryInformationHandler &ch) { return ch.id == id; });
}

static auto toolChainFromVariant(const QVariant &v) -> ToolChain*
{
  const auto tcId = v.toByteArray();
  return ToolChainManager::findToolChain(tcId);
}

auto ProjectImporter::cleanupTemporaryToolChains(Kit *k, const QVariantList &vl) -> void
{
  for (const auto &v : vl) {
    const auto tc = toolChainFromVariant(v);
    QTC_ASSERT(tc, continue);
    ToolChainManager::deregisterToolChain(tc);
    ToolChainKitAspect::setToolChain(k, nullptr);
  }
}

auto ProjectImporter::persistTemporaryToolChains(Kit *k, const QVariantList &vl) -> void
{
  for (const auto &v : vl) {
    const auto tmpTc = toolChainFromVariant(v);
    QTC_ASSERT(tmpTc, continue);
    const auto actualTc = ToolChainKitAspect::toolChain(k, tmpTc->language());
    if (tmpTc && actualTc != tmpTc)
      ToolChainManager::deregisterToolChain(tmpTc);
  }
}

auto ProjectImporter::useTemporaryKitAspect(Utils::Id id, CleanupFunction cleanup, PersistFunction persist) -> void
{
  QTC_ASSERT(!findTemporaryHandler(id), return);
  m_temporaryHandlers.append({id, cleanup, persist});
}

auto ProjectImporter::addTemporaryData(Utils::Id id, const QVariant &cleanupData, Kit *k) const -> void
{
  QTC_ASSERT(k, return);
  QTC_ASSERT(findTemporaryHandler(id), return);
  const auto fid = fullId(id);

  KitGuard guard(k);
  auto tmp = k->value(fid).toList();
  QTC_ASSERT(!tmp.contains(cleanupData), return);
  tmp.append(cleanupData);
  k->setValue(fid, tmp);
}

auto ProjectImporter::hasKitWithTemporaryData(Utils::Id id, const QVariant &data) const -> bool
{
  auto fid = fullId(id);
  return Utils::contains(KitManager::kits(), [data, fid](Kit *k) {
    return k->value(fid).toList().contains(data);
  });
}

static auto createToolChains(const ToolChainDescription &tcd) -> ProjectImporter::ToolChainData
{
  ProjectImporter::ToolChainData data;

  for (const auto factory : ToolChainFactory::allToolChainFactories()) {
    data.tcs = factory->detectForImport(tcd);
    if (data.tcs.isEmpty())
      continue;

    for (const auto tc : qAsConst(data.tcs))
      ToolChainManager::registerToolChain(tc);

    data.areTemporary = true;
    break;
  }

  return data;
}

auto ProjectImporter::findOrCreateToolChains(const ToolChainDescription &tcd) const -> ToolChainData
{
  ToolChainData result;
  result.tcs = ToolChainManager::toolchains([&tcd](const ToolChain *tc) {
    return tc->language() == tcd.language && Utils::Environment::systemEnvironment().isSameExecutable(tc->compilerCommand().toString(), tcd.compilerPath.toString());
  });
  for (const ToolChain *tc : qAsConst(result.tcs)) {
    const auto tcId = tc->id();
    result.areTemporary = result.areTemporary ? true : hasKitWithTemporaryData(ToolChainKitAspect::id(), tcId);
  }
  if (!result.tcs.isEmpty())
    return result;

  // Create a new toolchain:
  UpdateGuard guard(*this);
  return createToolChains(tcd);
}

} // namespace ProjectExplorer
