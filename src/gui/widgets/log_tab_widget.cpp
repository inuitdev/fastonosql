/*  Copyright (C) 2014-2016 FastoGT. All right reserved.

    This file is part of FastoNoSQL.

    SiteOnYourDevice is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SiteOnYourDevice is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SiteOnYourDevice.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "gui/widgets/log_tab_widget.h"

#include <QTabBar>
#include <QEvent>

#include "gui/widgets/log_widget.h"
#include "gui/widgets/commands_widget.h"
#include "gui/gui_factory.h"

#include "translations/global.h"

namespace fastonosql {

LogTabWidget::LogTabWidget(QWidget* parent)
  : QTabWidget(parent) {
  using namespace translations;

  QTabBar *tab = new QTabBar;
  setTabBar(tab);
  setTabsClosable(false);
  setElideMode(Qt::ElideRight);
  setMovable(true);
  setDocumentMode(true);

  log_ = new LogWidget;
  addTab(log_, GuiFactory::instance().loggingIcon(), trLogs);
  commands_ = new CommandsWidget;
  addTab(commands_, GuiFactory::instance().commandIcon(), trCommands);
  retranslateUi();
}

void LogTabWidget::addLogMessage(const QString& message, common::logging::LEVEL_LOG level) {
  log_->addLogMessage(message, level);
}

void LogTabWidget::addCommand(const Command& command) {
  commands_->addCommand(command);
}

void LogTabWidget::changeEvent(QEvent* e) {
  if(e->type() == QEvent::LanguageChange){
    retranslateUi();
  }

  QTabWidget::changeEvent(e);
}

void LogTabWidget::retranslateUi() {
  using namespace translations;

  setTabText(0, trLogs);
  setTabText(1, trCommands);
}

}
