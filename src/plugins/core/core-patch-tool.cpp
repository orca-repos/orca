// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-patch-tool.hpp"
#include "core-interface.hpp"
#include "core-message-manager.hpp"

#include <QApplication>

#include <utils/environment.hpp>
#include <utils/qtcprocess.hpp>

using namespace Utils;

namespace Orca::Plugin::Core {

constexpr char g_settings_group_c[] = "General";
constexpr char g_patch_command_key_c[] = "PatchCommand";
constexpr char g_patch_command_default_c[] = "patch";

auto PatchTool::patchCommand() -> FilePath
{
  QSettings *s = ICore::settings();
  s->beginGroup(g_settings_group_c);
  auto command = FilePath::fromVariant(s->value(g_patch_command_key_c, g_patch_command_default_c));
  s->endGroup();
  return command;
}

auto PatchTool::setPatchCommand(const FilePath &new_command) -> void
{
  const auto s = ICore::settings();
  s->beginGroup(g_settings_group_c);
  s->setValueWithDefault(g_patch_command_key_c, new_command.toVariant(), QVariant(QString(g_patch_command_default_c)));
  s->endGroup();
}

static auto runPatchHelper(const QByteArray &input, const FilePath &working_directory, const int strip, const bool reverse, const bool with_crlf) -> bool
{
  const auto patch = PatchTool::patchCommand();

  if (patch.isEmpty()) {
    MessageManager::writeDisrupting(QApplication::translate("Core::PatchTool", "There is no patch-command configured in the general \"Environment\" settings."));
    return false;
  }

  if (!patch.exists() && !patch.searchInPath().exists()) {
    MessageManager::writeDisrupting(QApplication::translate("Core::PatchTool", "The patch-command configured in the general \"Environment\" " "settings does not exist."));
    return false;
  }

  QtcProcess patch_process;

  if (!working_directory.isEmpty())
    patch_process.setWorkingDirectory(working_directory);

  auto env = Environment::systemEnvironment();
  env.setupEnglishOutput();
  patch_process.setEnvironment(env);
  QStringList args;

  // Add argument 'apply' when git is used as patch command since git 2.5/Windows
  // no longer ships patch.exe.
  if (patch.endsWith("git") || patch.endsWith("git.exe"))
    args << "apply";

  if (strip >= 0)
    args << ("-p" + QString::number(strip));

  if (reverse)
    args << "-R";

  if (with_crlf)
    args << "--binary";

  MessageManager::writeDisrupting(QApplication::translate("Core::PatchTool", "Running in %1: %2 %3").arg(working_directory.toUserOutput(), patch.toUserOutput(), args.join(' ')));
  patch_process.setCommand({patch, args});
  patch_process.setWriteData(input);
  patch_process.start();

  if (!patch_process.waitForStarted()) {
    MessageManager::writeFlashing(QApplication::translate("Core::PatchTool", "Unable to launch \"%1\": %2").arg(patch.toUserOutput(), patch_process.errorString()));
    return false;
  }

  QByteArray std_out;
  QByteArray std_err;

  if (!patch_process.readDataFromProcess(30, &std_out, &std_err, true)) {
    patch_process.stopProcess();
    MessageManager::writeFlashing(QApplication::translate("Core::PatchTool", "A timeout occurred running \"%1\"").arg(patch.toUserOutput()));
    return false;

  }

  if (!std_out.isEmpty()) {
    if (std_out.contains("(different line endings)") && !with_crlf) {
      auto crlf_input = input;
      crlf_input.replace('\n', "\r\n");
      return runPatchHelper(crlf_input, working_directory, strip, reverse, true);
    }
    MessageManager::writeFlashing(QString::fromLocal8Bit(std_out));
  }

  if (!std_err.isEmpty())
    MessageManager::writeFlashing(QString::fromLocal8Bit(std_err));

  if (patch_process.exitStatus() != QProcess::NormalExit) {
    MessageManager::writeFlashing(QApplication::translate("Core::PatchTool", "\"%1\" crashed.").arg(patch.toUserOutput()));
    return false;
  }

  if (patch_process.exitCode() != 0) {
    MessageManager::writeFlashing(QApplication::translate("Core::PatchTool", "\"%1\" failed (exit code %2).").arg(patch.toUserOutput()).arg(patch_process.exitCode()));
    return false;
  }

  return true;
}

auto PatchTool::runPatch(const QByteArray &input, const FilePath &working_directory, const int strip, const bool reverse) -> bool
{
  return runPatchHelper(input, working_directory, strip, reverse, false);
}

} // namespace Orca::Plugin::Core
