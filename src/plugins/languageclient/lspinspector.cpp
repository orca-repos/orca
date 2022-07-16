// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "lspinspector.hpp"

#include "client.hpp"
#include "languageclientmanager.hpp"

#include <core/core-interface.hpp>
#include <core/core-mini-splitter.hpp>
#include <languageserverprotocol/jsonkeys.h>
#include <languageserverprotocol/jsonrpcmessages.h>
#include <utils/jsontreeitem.hpp>
#include <utils/listmodel.hpp>

#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTextCodec>
#include <QTreeView>

using namespace LanguageServerProtocol;
using namespace Utils;

namespace LanguageClient {

class JsonTreeItemDelegate : public QStyledItemDelegate {
public:
  auto displayText(const QVariant &value, const QLocale &) const -> QString override
  {
    auto result = value.toString();
    if (result.size() == 1) {
      switch (result.at(0).toLatin1()) {
      case '\n':
        return QString("\\n");
      case '\t':
        return QString("\\t");
      case '\r':
        return QString("\\r");
      }
    }
    return result;
  }
};

using JsonModel = Utils::TreeModel<Utils::JsonTreeItem>;

auto createJsonModel(const QString &displayName, const QJsonValue &value) -> JsonModel*
{
  if (value.isNull())
    return nullptr;
  const auto root = new Utils::JsonTreeItem(displayName, value);
  if (root->canFetchMore())
    root->fetchMore();

  const auto model = new JsonModel(root);
  model->setHeader({{"Name"}, {"Value"}, {"Type"}});
  return model;
}

auto createJsonTreeView() -> QTreeView*
{
  const auto view = new QTreeView;
  view->setContextMenuPolicy(Qt::ActionsContextMenu);
  const auto action = new QAction(LspInspector::tr("Expand All"), view);
  QObject::connect(action, &QAction::triggered, view, &QTreeView::expandAll);
  view->addAction(action);
  view->setAlternatingRowColors(true);
  view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  view->setItemDelegate(new JsonTreeItemDelegate);
  return view;
}

auto createJsonTreeView(const QString &displayName, const QJsonValue &value) -> QTreeView*
{
  const auto view = createJsonTreeView();
  view->setModel(createJsonModel(displayName, value));
  return view;
}

class MessageDetailWidget : public QGroupBox {
public:
  MessageDetailWidget();

  auto setMessage(const LspLogMessage &message) -> void;
  auto clear() -> void;

private:
  QLabel *m_contentLength = nullptr;
  QLabel *m_mimeType = nullptr;
};

class LspCapabilitiesWidget : public QWidget {
  Q_DECLARE_TR_FUNCTIONS(LspCapabilitiesWidget)

public:
  LspCapabilitiesWidget();
  auto setCapabilities(const Capabilities &serverCapabilities) -> void;

private:
  auto updateOptionsView(const QString &method) -> void;

  DynamicCapabilities m_dynamicCapabilities;
  QTreeView *m_capabilitiesView = nullptr;
  QListWidget *m_dynamicCapabilitiesView = nullptr;
  QTreeView *m_dynamicOptionsView = nullptr;
  QGroupBox *m_dynamicCapabilitiesGroup = nullptr;
};

LspCapabilitiesWidget::LspCapabilitiesWidget()
{
  const auto mainLayout = new QHBoxLayout;

  const auto group = new QGroupBox(tr("Capabilities:"));
  QLayout *layout = new QHBoxLayout;
  m_capabilitiesView = createJsonTreeView();
  layout->addWidget(m_capabilitiesView);
  group->setLayout(layout);
  mainLayout->addWidget(group);

  m_dynamicCapabilitiesGroup = new QGroupBox(tr("Dynamic Capabilities:"));
  layout = new QVBoxLayout;
  auto label = new QLabel(tr("Method:"));
  layout->addWidget(label);
  m_dynamicCapabilitiesView = new QListWidget();
  layout->addWidget(m_dynamicCapabilitiesView);
  label = new QLabel(tr("Options:"));
  layout->addWidget(label);
  m_dynamicOptionsView = createJsonTreeView();
  layout->addWidget(m_dynamicOptionsView);
  m_dynamicCapabilitiesGroup->setLayout(layout);
  mainLayout->addWidget(m_dynamicCapabilitiesGroup);

  setLayout(mainLayout);

  connect(m_dynamicCapabilitiesView, &QListWidget::currentTextChanged, this, &LspCapabilitiesWidget::updateOptionsView);
}

auto LspCapabilitiesWidget::setCapabilities(const Capabilities &serverCapabilities) -> void
{
  m_capabilitiesView->setModel(createJsonModel(tr("Server Capabilities"), QJsonObject(serverCapabilities.capabilities)));
  m_dynamicCapabilities = serverCapabilities.dynamicCapabilities;
  const auto &methods = m_dynamicCapabilities.registeredMethods();
  if (methods.isEmpty()) {
    m_dynamicCapabilitiesGroup->hide();
    return;
  }
  m_dynamicCapabilitiesGroup->show();
  m_dynamicCapabilitiesView->clear();
  m_dynamicCapabilitiesView->addItems(methods);
}

auto LspCapabilitiesWidget::updateOptionsView(const QString &method) -> void
{
  const auto oldModel = m_dynamicOptionsView->model();
  m_dynamicOptionsView->setModel(createJsonModel(method, m_dynamicCapabilities.option(method)));
  delete oldModel;
}

class LspLogWidget : public Orca::Plugin::Core::MiniSplitter {
public:
  LspLogWidget();

  auto addMessage(const LspLogMessage &message) -> void;
  auto setMessages(const std::list<LspLogMessage> &messages) -> void;
  auto saveLog() -> void;

  MessageDetailWidget *m_clientDetails = nullptr;
  QListView *m_messages = nullptr;
  MessageDetailWidget *m_serverDetails = nullptr;
  Utils::ListModel<LspLogMessage> m_model;

private:
  auto currentMessageChanged(const QModelIndex &index) -> void;
  auto selectMatchingMessage(const LspLogMessage &message) -> void;
};

static auto messageData(const LspLogMessage &message, int, int role) -> QVariant
{
  if (role == Qt::DisplayRole)
    return message.displayText();
  if (role == Qt::TextAlignmentRole)
    return message.sender == LspLogMessage::ClientMessage ? Qt::AlignLeft : Qt::AlignRight;
  return {};
}

LspLogWidget::LspLogWidget()
{
  setOrientation(Qt::Horizontal);

  m_clientDetails = new MessageDetailWidget;
  m_clientDetails->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  m_clientDetails->setTitle(LspInspector::tr("Client Message"));
  addWidget(m_clientDetails);
  setStretchFactor(0, 1);

  m_model.setDataAccessor(&messageData);
  m_messages = new QListView;
  m_messages->setModel(&m_model);
  m_messages->setAlternatingRowColors(true);
  m_model.setHeader({LspInspector::tr("Messages")});
  m_messages->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
  m_messages->setSelectionMode(QAbstractItemView::MultiSelection);
  addWidget(m_messages);
  setStretchFactor(1, 0);

  m_serverDetails = new MessageDetailWidget;
  m_serverDetails->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  m_serverDetails->setTitle(LspInspector::tr("Server Message"));
  addWidget(m_serverDetails);
  setStretchFactor(2, 1);

  connect(m_messages->selectionModel(), &QItemSelectionModel::currentChanged, this, &LspLogWidget::currentMessageChanged);
}

auto LspLogWidget::currentMessageChanged(const QModelIndex &index) -> void
{
  m_messages->clearSelection();
  if (!index.isValid())
    return;
  const auto message = m_model.itemAt(index.row())->itemData;
  if (message.sender == LspLogMessage::ClientMessage)
    m_clientDetails->setMessage(message);
  else
    m_serverDetails->setMessage(message);
  selectMatchingMessage(message);
}

static auto matches(LspLogMessage::MessageSender sender, const MessageId &id, const LspLogMessage &message) -> bool
{
  if (message.sender != sender)
    return false;
  if (message.message.mimeType != JsonRpcMessageHandler::jsonRpcMimeType())
    return false;
  return message.id() == id;
}

auto LspLogWidget::selectMatchingMessage(const LspLogMessage &message) -> void
{
  const auto id = message.id();
  if (!id.isValid())
    return;
  const auto sender = message.sender == LspLogMessage::ServerMessage ? LspLogMessage::ClientMessage : LspLogMessage::ServerMessage;
  const auto matchingMessage = m_model.findData([&](const LspLogMessage &message) { return matches(sender, id, message); });
  if (!matchingMessage)
    return;
  const auto index = m_model.findIndex([&](const LspLogMessage &message) { return &message == matchingMessage; });

  m_messages->selectionModel()->select(index, QItemSelectionModel::Select);
  if (matchingMessage->sender == LspLogMessage::ServerMessage)
    m_serverDetails->setMessage(*matchingMessage);
  else
    m_clientDetails->setMessage(*matchingMessage);
}

auto LspLogWidget::addMessage(const LspLogMessage &message) -> void
{
  m_model.appendItem(message);
}

auto LspLogWidget::setMessages(const std::list<LspLogMessage> &messages) -> void
{
  m_model.clear();
  for (const auto &message : messages)
    m_model.appendItem(message);
}

auto LspLogWidget::saveLog() -> void
{
  QString contents;
  QTextStream stream(&contents);
  m_model.forAllData([&](const LspLogMessage &message) {
    stream << message.time.toString("hh:mm:ss.zzz") << ' ';
    stream << (message.sender == LspLogMessage::ClientMessage ? QString{"Client"} : QString{"Server"});
    stream << '\n';
    stream << message.message.codec->toUnicode(message.message.content);
    stream << "\n\n";
  });

  const auto filePath = FileUtils::getSaveFilePath(this, LspInspector::tr("Log File"));
  if (filePath.isEmpty())
    return;
  FileSaver saver(filePath, QIODevice::Text);
  saver.write(contents.toUtf8());
  if (!saver.finalize(this))
    saveLog();
}

class LspInspectorWidget : public QDialog {
  Q_DECLARE_TR_FUNCTIONS(LspInspectorWidget)
public:
  explicit LspInspectorWidget(LspInspector *inspector);

  auto selectClient(const QString &clientName) -> void;
private:
  auto addMessage(const QString &clientName, const LspLogMessage &message) -> void;
  auto updateCapabilities(const QString &clientName) -> void;
  auto currentClientChanged(const QString &clientName) -> void;
  auto log() const -> LspLogWidget*;
  auto capabilities() const -> LspCapabilitiesWidget*;

  LspInspector *const m_inspector = nullptr;
  QTabWidget *const m_tabWidget;

  enum class TabIndex {
    Log,
    Capabilities,
    Custom
  };

  QListWidget *m_clients = nullptr;
};

auto LspInspector::createWidget(const QString &defaultClient) -> QWidget*
{
  auto *inspector = new LspInspectorWidget(this);
  inspector->selectClient(defaultClient);
  return inspector;
}

auto LspInspector::log(const LspLogMessage::MessageSender sender, const QString &clientName, const BaseMessage &message) -> void
{
  auto &clientLog = m_logs[clientName];
  while (clientLog.size() >= static_cast<std::size_t>(m_logSize))
    clientLog.pop_front();
  clientLog.push_back({sender, QTime::currentTime(), message});
  emit newMessage(clientName, clientLog.back());
}

auto LspInspector::clientInitialized(const QString &clientName, const ServerCapabilities &capabilities) -> void
{
  m_capabilities[clientName].capabilities = capabilities;
  m_capabilities[clientName].dynamicCapabilities.reset();
  emit capabilitiesUpdated(clientName);
}

auto LspInspector::updateCapabilities(const QString &clientName, const DynamicCapabilities &dynamicCapabilities) -> void
{
  m_capabilities[clientName].dynamicCapabilities = dynamicCapabilities;
  emit capabilitiesUpdated(clientName);
}

auto LspInspector::messages(const QString &clientName) const -> std::list<LspLogMessage>
{
  return m_logs.value(clientName);
}

auto LspInspector::capabilities(const QString &clientName) const -> Capabilities
{
  return m_capabilities.value(clientName);
}

auto LspInspector::clients() const -> QList<QString>
{
  return m_logs.keys();
}

LspInspectorWidget::LspInspectorWidget(LspInspector *inspector) : m_inspector(inspector), m_tabWidget(new QTabWidget(this))
{
  setWindowTitle(tr("Language Client Inspector"));

  connect(inspector, &LspInspector::newMessage, this, &LspInspectorWidget::addMessage);
  connect(inspector, &LspInspector::capabilitiesUpdated, this, &LspInspectorWidget::updateCapabilities);
  connect(Orca::Plugin::Core::ICore::instance(), &Orca::Plugin::Core::ICore::coreAboutToClose, this, &QWidget::close);

  m_clients = new QListWidget;
  m_clients->addItems(inspector->clients());
  m_clients->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);

  const auto mainLayout = new QVBoxLayout;
  const auto mainSplitter = new Orca::Plugin::Core::MiniSplitter;
  mainSplitter->setOrientation(Qt::Horizontal);
  mainSplitter->addWidget(m_clients);
  mainSplitter->addWidget(m_tabWidget);
  mainSplitter->setStretchFactor(0, 0);
  mainSplitter->setStretchFactor(1, 1);
  m_tabWidget->addTab(new LspLogWidget, tr("Log"));
  m_tabWidget->addTab(new LspCapabilitiesWidget, tr("Capabilities"));
  mainLayout->addWidget(mainSplitter);

  const auto buttonBox = new QDialogButtonBox(this);
  buttonBox->setStandardButtons(QDialogButtonBox::Save | QDialogButtonBox::Close);
  const auto clearButton = buttonBox->addButton(tr("Clear"), QDialogButtonBox::ResetRole);
  connect(clearButton, &QPushButton::clicked, this, [this] {
    m_inspector->clear();
    if (m_clients->currentItem())
      currentClientChanged(m_clients->currentItem()->text());
  });
  mainLayout->addWidget(buttonBox);
  setLayout(mainLayout);

  connect(m_clients, &QListWidget::currentTextChanged, this, &LspInspectorWidget::currentClientChanged);

  // save
  connect(buttonBox, &QDialogButtonBox::accepted, log(), &LspLogWidget::saveLog);

  // close
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  resize(1024, 768);
}

auto LspInspectorWidget::selectClient(const QString &clientName) -> void
{
  auto items = m_clients->findItems(clientName, Qt::MatchExactly);
  if (items.isEmpty())
    return;
  m_clients->setCurrentItem(items.first());
}

auto LspInspectorWidget::addMessage(const QString &clientName, const LspLogMessage &message) -> void
{
  if (m_clients->findItems(clientName, Qt::MatchExactly).isEmpty())
    m_clients->addItem(clientName);
  if (const QListWidgetItem *currentItem = m_clients->currentItem(); currentItem && currentItem->text() == clientName) {
    log()->addMessage(message);
  }
}

auto LspInspectorWidget::updateCapabilities(const QString &clientName) -> void
{
  if (m_clients->findItems(clientName, Qt::MatchExactly).isEmpty())
    m_clients->addItem(clientName);
  if (const QListWidgetItem *currentItem = m_clients->currentItem(); currentItem && clientName == currentItem->text()) {
    capabilities()->setCapabilities(m_inspector->capabilities(clientName));
  }
}

auto LspInspectorWidget::currentClientChanged(const QString &clientName) -> void
{
  log()->setMessages(m_inspector->messages(clientName));
  capabilities()->setCapabilities(m_inspector->capabilities(clientName));
  for (auto i = m_tabWidget->count() - 1; i >= int(TabIndex::Custom); --i) {
    const auto w = m_tabWidget->widget(i);
    m_tabWidget->removeTab(i);
    delete w;
  }
  for (const auto c : LanguageClientManager::clients()) {
    if (c->name() != clientName)
      continue;
    for (const auto &tab : c->createCustomInspectorTabs())
      m_tabWidget->addTab(tab.first, tab.second);
    break;
  }
}

auto LspInspectorWidget::log() const -> LspLogWidget*
{
  return static_cast<LspLogWidget*>(m_tabWidget->widget(int(TabIndex::Log)));
}

auto LspInspectorWidget::capabilities() const -> LspCapabilitiesWidget*
{
  return static_cast<LspCapabilitiesWidget*>(m_tabWidget->widget(int(TabIndex::Capabilities)));
}

MessageDetailWidget::MessageDetailWidget()
{
  const auto layout = new QFormLayout;
  setLayout(layout);

  m_contentLength = new QLabel;
  m_mimeType = new QLabel;

  layout->addRow("Content Length:", m_contentLength);
  layout->addRow("MIME Type:", m_mimeType);
}

auto MessageDetailWidget::setMessage(const LspLogMessage &message) -> void
{
  m_contentLength->setText(QString::number(message.message.contentLength));
  m_mimeType->setText(QString::fromLatin1(message.message.mimeType));

  QWidget *newContentWidget = nullptr;
  if (message.message.mimeType == JsonRpcMessageHandler::jsonRpcMimeType()) {
    newContentWidget = createJsonTreeView("content", message.json());
  } else {
    const auto edit = new QPlainTextEdit();
    edit->setReadOnly(true);
    edit->setPlainText(message.message.codec->toUnicode(message.message.content));
    newContentWidget = edit;
  }
  const auto formLayout = static_cast<QFormLayout*>(layout());
  if (formLayout->rowCount() > 2)
    formLayout->removeRow(2);
  formLayout->setWidget(2, QFormLayout::SpanningRole, newContentWidget);
}

auto MessageDetailWidget::clear() -> void
{
  m_contentLength->setText({});
  m_mimeType->setText({});
  const auto formLayout = static_cast<QFormLayout*>(layout());
  if (formLayout->rowCount() > 2)
    formLayout->removeRow(2);
}

LspLogMessage::LspLogMessage() = default;

LspLogMessage::LspLogMessage(MessageSender sender, const QTime &time, const LanguageServerProtocol::BaseMessage &message) : sender(sender), time(time), message(message) {}

auto LspLogMessage::id() const -> MessageId
{
  if (!m_id.has_value())
    m_id = MessageId(json().value(idKey));
  return *m_id;
}

auto LspLogMessage::displayText() const -> QString
{
  if (!m_displayText.has_value()) {
    m_displayText = QString(time.toString("hh:mm:ss.zzz") + '\n');
    if (message.mimeType == JsonRpcMessageHandler::jsonRpcMimeType())
      m_displayText->append(json().value(QString{methodKey}).toString(id().toString()));
    else
      m_displayText->append(message.codec->toUnicode(message.content));
  }
  return *m_displayText;
}

auto LspLogMessage::json() const -> QJsonObject&
{
  if (!m_json.has_value()) {
    if (message.mimeType == JsonRpcMessageHandler::jsonRpcMimeType()) {
      QString error;
      m_json = JsonRpcMessageHandler::toJsonObject(message.content, message.codec, error);
    } else {
      m_json = QJsonObject();
    }
  }
  return *m_json;
}

} // namespace LanguageClient
