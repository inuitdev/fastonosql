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

#include "proxy/db/leveldb/driver.h"

#include <common/convert2string.h>

#include "core/db/leveldb/db_connection.h"  // for DBConnection
#include "core/value.h"

#include "proxy/command/command.h"                 // for CreateCommand, etc
#include "proxy/command/command_logger.h"          // for LOG_COMMAND
#include "proxy/db/leveldb/command.h"              // for Command
#include "proxy/db/leveldb/connection_settings.h"  // for ConnectionSettings
#include "proxy/db/leveldb/database.h"             // for DataBaseInfo

namespace fastonosql {
namespace proxy {
namespace leveldb {

Driver::Driver(IConnectionSettingsBaseSPtr settings)
    : IDriverLocal(settings), impl_(new core::leveldb::DBConnection(this)) {
  COMPILE_ASSERT(core::leveldb::DBConnection::connection_t == core::LEVELDB,
                 "DBConnection must be the same type as Driver!");
  CHECK(GetType() == core::LEVELDB);
}

Driver::~Driver() {
  delete impl_;
}

bool Driver::IsInterrupted() const {
  return impl_->IsInterrupted();
}

void Driver::SetInterrupted(bool interrupted) {
  return impl_->SetInterrupted(interrupted);
}

core::translator_t Driver::GetTranslator() const {
  return impl_->GetTranslator();
}

bool Driver::IsConnected() const {
  return impl_->IsConnected();
}

bool Driver::IsAuthenticated() const {
  return impl_->IsAuthenticated();
}

void Driver::InitImpl() {}

void Driver::ClearImpl() {}

core::FastoObjectCommandIPtr Driver::CreateCommand(core::FastoObject* parent,
                                                   const core::command_buffer_t& input,
                                                   core::CmdLoggingType ct) {
  return proxy::CreateCommand<Command>(parent, input, ct);
}

core::FastoObjectCommandIPtr Driver::CreateCommandFast(const core::command_buffer_t& input, core::CmdLoggingType ct) {
  return proxy::CreateCommandFast<Command>(input, ct);
}

common::Error Driver::SyncConnect() {
  auto leveldb_settings = GetSpecificSettings<ConnectionSettings>();
  return impl_->Connect(leveldb_settings->GetInfo());
}

common::Error Driver::SyncDisconnect() {
  return impl_->Disconnect();
}

common::Error Driver::ExecuteImpl(const core::command_buffer_t& command, core::FastoObject* out) {
  return impl_->Execute(command, out);
}

common::Error Driver::GetCurrentServerInfo(core::IServerInfo** info) {
  core::FastoObjectCommandIPtr cmd = CreateCommandFast(DB_INFO_COMMAND, core::C_INNER);
  LOG_COMMAND(cmd);
  core::leveldb::ServerInfo::Stats cm;
  common::Error err = impl_->Info(std::string(), &cm);
  if (err) {
    return err;
  }

  *info = new core::leveldb::ServerInfo(cm);
  return common::Error();
}

common::Error Driver::GetServerCommands(std::vector<const core::CommandInfo*>* commands) {
  std::vector<const core::CommandInfo*> lcommands;
  const core::ConstantCommandsArray& origin = core::leveldb::DBConnection::GetCommands();
  for (size_t i = 0; i < origin.size(); ++i) {
    lcommands.push_back(&origin[i]);
  }
  *commands = lcommands;
  return common::Error();
}

common::Error Driver::GetServerLoadedModules(std::vector<core::ModuleInfo>* modules) {
  *modules = std::vector<core::ModuleInfo>();
  return common::Error();
}

common::Error Driver::GetCurrentDataBaseInfo(core::IDataBaseInfo** info) {
  if (!info) {
    return common::make_error_inval();
  }

  return impl_->Select(impl_->GetCurrentDBName(), info);
}

void Driver::HandleLoadDatabaseContentEvent(events::LoadDatabaseContentRequestEvent* ev) {
  QObject* sender = ev->sender();
  NotifyProgress(sender, 0);
  events::LoadDatabaseContentResponceEvent::value_type res(ev->value());
  const core::command_buffer_t pattern_result =
      core::internal::GetKeysPattern(res.cursor_in, res.pattern, res.count_keys);
  core::FastoObjectCommandIPtr cmd = CreateCommandFast(pattern_result, core::C_INNER);
  NotifyProgress(sender, 50);
  common::Error err = Execute(cmd);
  if (err) {
    res.setErrorInfo(err);
  } else {
    core::FastoObject::childs_t rchildrens = cmd->GetChildrens();
    if (rchildrens.size()) {
      CHECK_EQ(rchildrens.size(), 1);
      core::FastoObject* array = rchildrens[0].get();
      CHECK(array);
      auto array_value = array->GetValue();
      common::ArrayValue* arm = nullptr;
      if (!array_value->GetAsList(&arm)) {
        goto done;
      }

      CHECK_EQ(arm->GetSize(), 2);
      std::string cursor;
      bool isok = arm->GetString(0, &cursor);
      if (!isok) {
        goto done;
      }

      uint64_t lcursor;
      if (common::ConvertFromString(cursor, &lcursor)) {
        res.cursor_out = lcursor;
      }

      common::ArrayValue* ar = nullptr;
      isok = arm->GetList(1, &ar);
      if (!isok) {
        goto done;
      }

      for (size_t i = 0; i < ar->GetSize(); ++i) {
        std::string key_str;
        if (ar->GetString(i, &key_str)) {
          core::key_t key(key_str);
          core::NKey k(key);
          core::NValue empty_val(core::CreateEmptyValueFromType(common::Value::TYPE_STRING));
          core::NDbKValue ress(k, empty_val);
          res.keys.push_back(ress);
        }
      }

      common::Error err = impl_->DBkcount(&res.db_keys_count);
      DCHECK(!err);
    }
  }
done:
  NotifyProgress(sender, 75);
  Reply(sender, new events::LoadDatabaseContentResponceEvent(this, res));
  NotifyProgress(sender, 100);
}

core::IServerInfoSPtr Driver::MakeServerInfoFromString(const std::string& val) {
  core::IServerInfoSPtr res(core::leveldb::MakeLeveldbServerInfo(val));
  return res;
}

}  // namespace leveldb
}  // namespace proxy
}  // namespace fastonosql
