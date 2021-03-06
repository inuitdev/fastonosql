/*  Copyright (C) 2014-2017 FastoGT. All right reserved.

    This file is part of FastoNoSQL.

    FastoNoSQL is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    FastoNoSQL is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with FastoNoSQL.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "gui/shell/base_shell_widget.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QSpinBox>
#include <QSplitter>
#include <QToolBar>
#include <QVBoxLayout>

#include <common/qt/convert2string.h>  // for ConvertToString
#include <common/qt/gui/icon_label.h>  // for IconLabel
#include <common/qt/logger.h>          // for LOG_ERROR

#include "proxy/server/iserver_local.h"
#include "proxy/server/iserver_remote.h"
#include "proxy/settings_manager.h"  // for SettingsManager

#include "gui/gui_factory.h"  // for GuiFactory
#include "gui/shortcuts.h"    // for g_execute_key
#include "gui/utils.h"

#include "translations/global.h"  // for trError, trSaveAs, etc

#ifdef BUILD_WITH_REDIS
#include "gui/db/redis/shell_widget.h"
#endif
#include "gui/shell/base_shell.h"

namespace {

const QString trSupportedCommandsCountTemplate_1S = QObject::tr("Supported commands count: %1");
const QString trValidatedCommandsCountTemplate_1S = QObject::tr("Validated commands count: %1");
const QString trCommandsVersion = QObject::tr("Commands version:");
const QString trCantReadTemplate_2S = QObject::tr(PROJECT_NAME_TITLE " can't read from %1:\n%2.");
const QString trCantSaveTemplate_2S = QObject::tr(PROJECT_NAME_TITLE " can't save to %1:\n%2.");
const QString trAdvancedOptions = QObject::tr("Advanced options");
const QString trIntervalMsec = QObject::tr("Interval msec:");
const QString trRepeat = QObject::tr("Repeat:");
const QString trBasedOn_2S = QObject::tr("Based on <b>%1</b> version: <b>%2</b>");

}  // namespace

namespace fastonosql {
namespace gui {

const QSize BaseShellWidget::top_bar_icon_size = QSize(24, 24);

BaseShellWidget* BaseShellWidget::createWidget(proxy::IServerSPtr server, const QString& filePath, QWidget* parent) {
#ifdef BUILD_WITH_REDIS
  core::connectionTypes ct = server->GetType();
  if (ct == core::REDIS) {
    BaseShellWidget* widget = new redis::ShellWidget(server, filePath, parent);
    widget->init();
    return widget;
  }
#endif

  BaseShellWidget* widget = new BaseShellWidget(server, filePath, parent);
  widget->init();
  return widget;
}

BaseShellWidget::BaseShellWidget(proxy::IServerSPtr server, const QString& filePath, QWidget* parent)
    : QWidget(parent),
      server_(server),
      executeAction_(nullptr),
      stopAction_(nullptr),
      connectAction_(nullptr),
      disConnectAction_(nullptr),
      loadAction_(nullptr),
      saveAction_(nullptr),
      saveAsAction_(nullptr),
      validateAction_(nullptr),
      supported_commands_count_(nullptr),
      validated_commands_count_(nullptr),
      commandsVersionApi_(nullptr),
      input_(nullptr),
      workProgressBar_(nullptr),
      connectionMode_(nullptr),
      serverName_(nullptr),
      dbName_(nullptr),
      advancedOptions_(nullptr),
      advancedOptionsWidget_(nullptr),
      repeatCount_(nullptr),
      intervalMsec_(nullptr),
      historyCall_(nullptr),
      filePath_(filePath) {}

QToolBar* BaseShellWidget::createToolBar() {
  QToolBar* savebar = new QToolBar;
  loadAction_ = new QAction;
  loadAction_->setIcon(gui::GuiFactory::GetInstance().GetLoadIcon());
  typedef void (BaseShellWidget::*lf)();
  VERIFY(connect(loadAction_, &QAction::triggered, this, static_cast<lf>(&BaseShellWidget::loadFromFile)));
  savebar->addAction(loadAction_);

  saveAction_ = new QAction;
  saveAction_->setIcon(gui::GuiFactory::GetInstance().GetSaveIcon());
  VERIFY(connect(saveAction_, &QAction::triggered, this, &BaseShellWidget::saveToFile));
  savebar->addAction(saveAction_);

  saveAsAction_ = new QAction;
  saveAsAction_->setIcon(gui::GuiFactory::GetInstance().GetSaveAsIcon());
  VERIFY(connect(saveAsAction_, &QAction::triggered, this, &BaseShellWidget::saveToFileAs));
  savebar->addAction(saveAsAction_);

  connectAction_ = new QAction;
  connectAction_->setIcon(gui::GuiFactory::GetInstance().GetConnectIcon());
  VERIFY(connect(connectAction_, &QAction::triggered, this, &BaseShellWidget::connectToServer));
  savebar->addAction(connectAction_);

  disConnectAction_ = new QAction;
  disConnectAction_->setIcon(gui::GuiFactory::GetInstance().GetDisConnectIcon());
  VERIFY(connect(disConnectAction_, &QAction::triggered, this, &BaseShellWidget::disconnectFromServer));
  savebar->addAction(disConnectAction_);

  executeAction_ = new QAction;
  executeAction_->setIcon(gui::GuiFactory::GetInstance().GetExecuteIcon());
  executeAction_->setShortcut(gui::g_execute_key);
  VERIFY(connect(executeAction_, &QAction::triggered, this, &BaseShellWidget::execute));
  savebar->addAction(executeAction_);

  stopAction_ = new QAction;
  stopAction_->setIcon(gui::GuiFactory::GetInstance().stopIcon());
  VERIFY(connect(stopAction_, &QAction::triggered, this, &BaseShellWidget::stop));
  savebar->addAction(stopAction_);
  return savebar;
}

void BaseShellWidget::init() {
  CHECK(server_);

  VERIFY(connect(server_.get(), &proxy::IServer::ConnectStarted, this, &BaseShellWidget::startConnect));
  VERIFY(connect(server_.get(), &proxy::IServer::ConnectFinished, this, &BaseShellWidget::finishConnect));
  VERIFY(connect(server_.get(), &proxy::IServer::DisconnectStarted, this, &BaseShellWidget::startDisconnect));
  VERIFY(connect(server_.get(), &proxy::IServer::DisconnectFinished, this, &BaseShellWidget::finishDisconnect));
  VERIFY(connect(server_.get(), &proxy::IServer::ProgressChanged, this, &BaseShellWidget::progressChange));

  VERIFY(connect(server_.get(), &proxy::IServer::ModeEntered, this, &BaseShellWidget::enterMode));
  VERIFY(connect(server_.get(), &proxy::IServer::ModeLeaved, this, &BaseShellWidget::leaveMode));

  VERIFY(connect(server_.get(), &proxy::IServer::LoadDiscoveryInfoStarted, this,
                 &BaseShellWidget::startLoadDiscoveryInfo));
  VERIFY(connect(server_.get(), &proxy::IServer::LoadDiscoveryInfoFinished, this,
                 &BaseShellWidget::finishLoadDiscoveryInfo));

  VERIFY(connect(server_.get(), &proxy::IServer::ExecuteStarted, this, &BaseShellWidget::startExecute,
                 Qt::DirectConnection));
  VERIFY(connect(server_.get(), &proxy::IServer::ExecuteFinished, this, &BaseShellWidget::finishExecute,
                 Qt::DirectConnection));

  VERIFY(connect(server_.get(), &proxy::IServer::DatabaseChanged, this, &BaseShellWidget::updateDefaultDatabase));
  VERIFY(connect(server_.get(), &proxy::IServer::Disconnected, this, &BaseShellWidget::serverDisconnect));

  QVBoxLayout* mainlayout = new QVBoxLayout;
  QHBoxLayout* hlayout = new QHBoxLayout;

  QToolBar* savebar = createToolBar();

  core::ConnectionMode mode = core::InteractiveMode;
  std::string mode_str = common::ConvertToString(mode);
  QString qmode_str;
  common::ConvertFromString(mode_str, &qmode_str);
  connectionMode_ =
      new common::qt::gui::IconLabel(gui::GuiFactory::GetInstance().GetModeIcon(mode), top_bar_icon_size, qmode_str);

  hlayout->addWidget(savebar);
  hlayout->addWidget(new QSplitter(Qt::Horizontal));

  hlayout->addWidget(connectionMode_);
  workProgressBar_ = new QProgressBar;
  workProgressBar_->setTextVisible(true);
  hlayout->addWidget(workProgressBar_);
  QToolBar* helpbar = new QToolBar;
  validateAction_ = new QAction(gui::GuiFactory::GetInstance().GetFailIcon(), translations::trValidate, helpbar);
  VERIFY(connect(validateAction_, &QAction::triggered, this, &BaseShellWidget::validateClick));
  helpbar->addAction(validateAction_);

  QAction* helpAction = new QAction(gui::GuiFactory::GetInstance().GetHelpIcon(), translations::trHelp, helpbar);
  VERIFY(connect(helpAction, &QAction::triggered, this, &BaseShellWidget::helpClick));
  helpbar->addAction(helpAction);
  hlayout->addWidget(helpbar);
  mainlayout->addLayout(hlayout);

  advancedOptions_ = new QCheckBox;
  VERIFY(connect(advancedOptions_, &QCheckBox::stateChanged, this, &BaseShellWidget::advancedOptionsChange));

  core::connectionTypes ct = server_->GetType();
  input_ = BaseShell::createFromType(ct, proxy::SettingsManager::GetInstance()->GetAutoCompletion());
  input_->setContextMenuPolicy(Qt::CustomContextMenu);
  VERIFY(connect(input_, &BaseShell::textChanged, this, &BaseShellWidget::inputTextChanged));

  advancedOptionsWidget_ = new QWidget;
  advancedOptionsWidget_->setVisible(false);
  QVBoxLayout* advOptLayout = new QVBoxLayout;

  QHBoxLayout* repeatLayout = new QHBoxLayout;
  QLabel* repeatLabel = new QLabel(trRepeat);
  repeatCount_ = new QSpinBox;
  repeatCount_->setRange(0, INT32_MAX);
  repeatCount_->setSingleStep(1);
  repeatLayout->addWidget(repeatLabel);
  repeatLayout->addWidget(repeatCount_);

  QHBoxLayout* intervalLayout = new QHBoxLayout;
  QLabel* intervalLabel = new QLabel(trIntervalMsec);
  intervalMsec_ = new QSpinBox;
  intervalMsec_->setRange(0, INT32_MAX);
  intervalMsec_->setSingleStep(1000);
  intervalLayout->addWidget(intervalLabel);
  intervalLayout->addWidget(intervalMsec_);

  historyCall_ = new QCheckBox;
  historyCall_->setChecked(true);
  advOptLayout->addLayout(repeatLayout);
  advOptLayout->addLayout(intervalLayout);
  advOptLayout->addWidget(historyCall_);
  advancedOptionsWidget_->setLayout(advOptLayout);

  QHBoxLayout* top_layout = createTopLayout(ct);
  QSplitter* spliter_info_and_options = new QSplitter(Qt::Horizontal);
  spliter_info_and_options->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  top_layout->addWidget(spliter_info_and_options);
  top_layout->addWidget(advancedOptions_);
  mainlayout->addLayout(top_layout);

  QHBoxLayout* inputLayout = new QHBoxLayout;
  inputLayout->addWidget(input_);
  inputLayout->addWidget(advancedOptionsWidget_);
  mainlayout->addLayout(inputLayout);

  QHBoxLayout* apilayout = new QHBoxLayout;
  supported_commands_count_ = new QLabel;
  apilayout->addWidget(supported_commands_count_);
  validated_commands_count_ = new QLabel;
  apilayout->addWidget(validated_commands_count_);
  apilayout->addWidget(new QSplitter(Qt::Horizontal));

  commandsVersionApi_ = new QComboBox;
  typedef void (QComboBox::*curc)(int);
  VERIFY(connect(commandsVersionApi_, static_cast<curc>(&QComboBox::currentIndexChanged), this,
                 &BaseShellWidget::changeVersionApi));

  std::vector<uint32_t> versions = input_->supportedVersions();
  for (size_t i = 0; i < versions.size(); ++i) {
    uint32_t cur = versions[i];
    std::string curVers = core::ConvertVersionNumberToReadableString(cur);
    QString qcurVers;
    common::ConvertFromString(curVers, &qcurVers);
    commandsVersionApi_->addItem(gui::GuiFactory::GetInstance().GetUnknownIcon(), qcurVers, cur);
    commandsVersionApi_->setCurrentIndex(i);
  }
  QLabel* version = new QLabel(trCommandsVersion);
  apilayout->addWidget(version);
  apilayout->addWidget(commandsVersionApi_);
  mainlayout->addLayout(apilayout);

  setLayout(mainlayout);

  // sync controls
  syncConnectionActions();
  updateServerInfo(server_->GetCurrentServerInfo());
  updateDefaultDatabase(server_->GetCurrentDatabaseInfo());
  updateCommands(std::vector<const core::CommandInfo*>());

  retranslateUi();
}

QHBoxLayout* BaseShellWidget::createTopLayout(core::connectionTypes ct) {
  QHBoxLayout* top_layout = new QHBoxLayout;
  serverName_ = new common::qt::gui::IconLabel(gui::GuiFactory::GetInstance().GetIcon(ct), top_bar_icon_size,
                                               translations::trCalculating);
  serverName_->setElideMode(Qt::ElideRight);
  top_layout->addWidget(serverName_);
  dbName_ = new common::qt::gui::IconLabel(gui::GuiFactory::GetInstance().GetDatabaseIcon(), top_bar_icon_size,
                                           translations::trCalculating);
  top_layout->addWidget(dbName_);
  return top_layout;
}

void BaseShellWidget::advancedOptionsChange(int state) {
  advancedOptionsWidget_->setVisible(state);
}

BaseShellWidget::~BaseShellWidget() {}

QString BaseShellWidget::text() const {
  return input_->text();
}

void BaseShellWidget::setText(const QString& text) {
  input_->setText(text);
}

void BaseShellWidget::executeText(const QString& text) {
  input_->setText(text);
  execute();
}

void BaseShellWidget::changeEvent(QEvent* ev) {
  if (ev->type() == QEvent::LanguageChange) {
    retranslateUi();
  }

  QWidget::changeEvent(ev);
}

void BaseShellWidget::retranslateUi() {
  loadAction_->setText(translations::trLoad);
  saveAction_->setText(translations::trSave);
  saveAsAction_->setText(translations::trSaveAs);
  connectAction_->setText(translations::trConnect);
  disConnectAction_->setText(translations::trDisconnect);
  executeAction_->setText(translations::trExecute);
  stopAction_->setText(translations::trStop);

  historyCall_->setText(translations::trHistory);
  setToolTip(trBasedOn_2S.arg(input_->basedOn(), input_->version()));
  advancedOptions_->setText(trAdvancedOptions);
  supported_commands_count_->setText(trSupportedCommandsCountTemplate_1S.arg(input_->commandsCount()));
  validated_commands_count_->setText(trValidatedCommandsCountTemplate_1S.arg(input_->validateCommandsCount()));
}

common::Error BaseShellWidget::validate(const QString& text) {
  core::translator_t tran = server_->GetTranslator();
  std::vector<core::command_buffer_t> cmds;
  core::command_buffer_t text_cmd = common::ConvertToString(text);
  common::Error err = core::ParseCommands(text_cmd, &cmds);
  if (err) {
    return err;
  }

  for (auto cmd : cmds) {
    err = tran->TestCommandLine(cmd);
    if (err) {
      return err;
    }
  }

  return common::Error();
}

void BaseShellWidget::execute() {
  QString selected = input_->selectedText();
  if (selected.isEmpty()) {
    selected = input_->text();
  }

  int repeat = repeatCount_->value();
  int interval = intervalMsec_->value();
  bool history = historyCall_->isChecked();
  executeArgs(selected, repeat, interval, history);
}

void BaseShellWidget::executeArgs(const QString& text, int repeat, int interval, bool history) {
  core::command_buffer_t text_cmd = common::ConvertToString(text);
  proxy::events_info::ExecuteInfoRequest req(this, text_cmd, repeat, interval, history);
  server_->Execute(req);
}

void BaseShellWidget::stop() {
  server_->StopCurrentEvent();
}

void BaseShellWidget::connectToServer() {
  proxy::events_info::ConnectInfoRequest req(this);
  server_->Connect(req);
}

void BaseShellWidget::disconnectFromServer() {
  proxy::events_info::DisConnectInfoRequest req(this);
  server_->Disconnect(req);
}

void BaseShellWidget::loadFromFile() {
  loadFromFile(filePath_);
}

bool BaseShellWidget::loadFromFile(const QString& path) {
  QString filepath = QFileDialog::getOpenFileName(this, path, QString(), translations::trfilterForScripts);
  if (!filepath.isEmpty()) {
    QString out;
    common::qt::QtFileError err = common::qt::LoadFromFileText(filepath, &out);
    if (err) {
      QString qdesc;
      common::ConvertFromString(err->GetDescription(), &qdesc);
      QMessageBox::critical(this, translations::trError, trCantReadTemplate_2S.arg(filepath, qdesc));
      return false;
    }

    setText(out);
    filePath_ = filepath;
    return true;
  }

  return false;
}

void BaseShellWidget::saveToFileAs() {
  QString filepath = ShowSaveFileDialog(this, translations::trSaveAs, filePath_, translations::trfilterForScripts);
  if (filepath.isEmpty()) {
    return;
  }

  common::qt::QtFileError err = common::qt::SaveToFileText(filepath, text());
  if (err) {
    QString qdesc;
    common::ConvertFromString(err->GetDescription(), &qdesc);
    QMessageBox::critical(this, translations::trError, trCantSaveTemplate_2S.arg(filepath, qdesc));
    return;
  }

  filePath_ = filepath;
}

void BaseShellWidget::changeVersionApi(int index) {
  if (index == -1) {
    return;
  }

  QVariant var = commandsVersionApi_->itemData(index);
  uint32_t version = qvariant_cast<uint32_t>(var);
  input_->setFilteredVersion(version);
}

void BaseShellWidget::saveToFile() {
  if (filePath_.isEmpty()) {
    saveToFileAs();
  } else {
    common::qt::QtFileError err = common::qt::SaveToFileText(filePath_, text());
    if (err) {
      QString qdesc;
      common::ConvertFromString(err->GetDescription(), &qdesc);
      QMessageBox::critical(this, translations::trError, trCantSaveTemplate_2S.arg(filePath_, qdesc));
    }
  }
}

void BaseShellWidget::validateClick() {
  QString text = input_->text();
  common::Error err = validate(text);
  if (err) {
    LOG_ERROR(err, common::logging::LOG_LEVEL_ERR, true);
  }
}

void BaseShellWidget::helpClick() {
  executeArgs(DB_HELP_COMMAND, 0, 0, false);
}

void BaseShellWidget::inputTextChanged() {
  QString text = input_->text();
  common::Error err = validate(text);
  if (err) {
    validateAction_->setIcon(gui::GuiFactory::GetInstance().GetFailIcon());
  } else {
    validateAction_->setIcon(gui::GuiFactory::GetInstance().GetSuccessIcon());
  }
}

void BaseShellWidget::startConnect(const proxy::events_info::ConnectInfoRequest& req) {
  UNUSED(req);

  syncConnectionActions();
}

void BaseShellWidget::finishConnect(const proxy::events_info::ConnectInfoResponce& res) {
  UNUSED(res);

  serverConnect();
}

void BaseShellWidget::startDisconnect(const proxy::events_info::DisConnectInfoRequest& req) {
  UNUSED(req);

  syncConnectionActions();
}

void BaseShellWidget::finishDisconnect(const proxy::events_info::DisConnectInfoResponce& res) {
  UNUSED(res);

  serverDisconnect();
}

void BaseShellWidget::progressChange(const proxy::events_info::ProgressInfoResponce& res) {
  workProgressBar_->setValue(res.progress);
}

void BaseShellWidget::enterMode(const proxy::events_info::EnterModeInfo& res) {
  core::ConnectionMode mode = res.mode;
  connectionMode_->setIcon(gui::GuiFactory::GetInstance().GetModeIcon(mode), top_bar_icon_size);
  std::string modeText = common::ConvertToString(mode);
  QString qmodeText;
  common::ConvertFromString(modeText, &qmodeText);
  connectionMode_->setText(qmodeText);
}

void BaseShellWidget::leaveMode(const proxy::events_info::LeaveModeInfo& res) {
  UNUSED(res);
}

void BaseShellWidget::startLoadDiscoveryInfo(const proxy::events_info::DiscoveryInfoRequest& res) {
  OnStartedLoadDiscoveryInfo(res);
}

void BaseShellWidget::finishLoadDiscoveryInfo(const proxy::events_info::DiscoveryInfoResponce& res) {
  OnFinishedLoadDiscoveryInfo(res);
}

void BaseShellWidget::OnStartedLoadDiscoveryInfo(const proxy::events_info::DiscoveryInfoRequest& res) {
  UNUSED(res);
}

void BaseShellWidget::OnFinishedLoadDiscoveryInfo(const proxy::events_info::DiscoveryInfoResponce& res) {
  common::Error err = res.errorInfo();
  if (err) {
    return;
  }

  updateServerInfo(res.sinfo);
  updateDefaultDatabase(res.dbinfo);
  updateCommands(res.commands);
}

void BaseShellWidget::startExecute(const proxy::events_info::ExecuteInfoRequest& req) {
  UNUSED(req);

  repeatCount_->setEnabled(false);
  intervalMsec_->setEnabled(false);
  historyCall_->setEnabled(false);
  executeAction_->setEnabled(false);
  stopAction_->setEnabled(true);
}
void BaseShellWidget::finishExecute(const proxy::events_info::ExecuteInfoResponce& res) {
  UNUSED(res);

  repeatCount_->setEnabled(true);
  intervalMsec_->setEnabled(true);
  historyCall_->setEnabled(true);
  executeAction_->setEnabled(true);
  stopAction_->setEnabled(false);
}

void BaseShellWidget::serverConnect() {
  OnServerConnected();
}

void BaseShellWidget::serverDisconnect() {
  OnServerDisconnected();
}

void BaseShellWidget::OnServerConnected() {
  syncConnectionActions();
}

void BaseShellWidget::OnServerDisconnected() {
  syncConnectionActions();
  updateServerInfo(core::IServerInfoSPtr());
  updateDefaultDatabase(core::IDataBaseInfoSPtr());
  updateCommands(std::vector<const core::CommandInfo*>());
}

void BaseShellWidget::updateServerInfo(core::IServerInfoSPtr inf) {
  if (!inf) {
    updateServerLabel(translations::trCalculating);
    for (int i = 0; i < commandsVersionApi_->count(); ++i) {
      commandsVersionApi_->setItemIcon(i, gui::GuiFactory::GetInstance().GetUnknownIcon());
    }
    return;
  }

  std::string server_label;
  if (server_->IsCanRemote()) {
    proxy::IServerRemote* rserver = dynamic_cast<proxy::IServerRemote*>(server_.get());  // +
    server_label = common::ConvertToString(rserver->GetHost());
  } else {
    proxy::IServerLocal* lserver = dynamic_cast<proxy::IServerLocal*>(server_.get());  // +
    server_label = lserver->GetPath();
  }
  QString qserver_label;
  common::ConvertFromString(server_label, &qserver_label);
  updateServerLabel(qserver_label);

  uint32_t serv_vers = inf->GetVersion();
  if (serv_vers == UNDEFINED_SINCE) {
    return;
  }

  bool updatedComboIndex = false;
  for (int i = 0; i < commandsVersionApi_->count(); ++i) {
    QVariant var = commandsVersionApi_->itemData(i);
    uint32_t version = qvariant_cast<uint32_t>(var);
    if (version == UNDEFINED_SINCE) {
      commandsVersionApi_->setItemIcon(i, gui::GuiFactory::GetInstance().GetUnknownIcon());
      continue;
    }

    if (version >= serv_vers) {
      if (!updatedComboIndex) {
        updatedComboIndex = true;
        commandsVersionApi_->setCurrentIndex(i);
        commandsVersionApi_->setItemIcon(i, gui::GuiFactory::GetInstance().GetSuccessIcon());
      } else {
        commandsVersionApi_->setItemIcon(i, gui::GuiFactory::GetInstance().GetFailIcon());
      }
    } else {
      commandsVersionApi_->setItemIcon(i, gui::GuiFactory::GetInstance().GetSuccessIcon());
    }
  }
}

void BaseShellWidget::updateDefaultDatabase(core::IDataBaseInfoSPtr dbs) {
  if (!dbs) {
    updateDBLabel(translations::trCalculating);
    return;
  }

  std::string name = dbs->GetName();
  QString qname;
  common::ConvertFromString(name, &qname);
  updateDBLabel(qname);
}

void BaseShellWidget::updateCommands(const std::vector<const core::CommandInfo*>& commands) {
  validated_commands_count_->setText(trValidatedCommandsCountTemplate_1S.arg(commands.size()));
}

void BaseShellWidget::updateServerLabel(const QString& text) {
  serverName_->setText(text);
  serverName_->setToolTip(text);
}

void BaseShellWidget::updateDBLabel(const QString& text) {
  dbName_->setText(text);
  dbName_->setToolTip(text);
}

void BaseShellWidget::syncConnectionActions() {
  bool is_connected = server_->IsConnected();

  connectAction_->setVisible(!is_connected);
  disConnectAction_->setVisible(is_connected);
  executeAction_->setEnabled(true);
  stopAction_->setEnabled(false);
}

}  // namespace gui
}  // namespace fastonosql
