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

#include "gui/update_checker.h"

#include <common/net/socket_tcp.h>  // for ClientSocketTcp

#include "server_config_daemon/server_config.h"

namespace fastonosql {
namespace gui {

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent) {}

void UpdateChecker::routine() {
#if defined(FASTONOSQL)
  common::net::ClientSocketTcp client(common::net::HostAndPort(FASTONOSQL_URL, SERV_VERSION_PORT));
#elif defined(FASTOREDIS)
  common::net::ClientSocketTcp client(common::net::HostAndPort(FASTOREDIS_URL, SERV_VERSION_PORT));
#else
#error please specify url and port of version information
#endif
  common::ErrnoError err = client.Connect();
  if (err) {
    emit versionAvailibled(false, QString());
    return;
  }

  size_t nwrite = 0;
#if defined(FASTONOSQL)
  err = client.Write(GET_FASTONOSQL_VERSION, sizeof(GET_FASTONOSQL_VERSION) - 1, &nwrite);
#elif defined(FASTOREDIS)
  err = client.Write(GET_FASTOREDIS_VERSION, sizeof(GET_FASTOREDIS_VERSION) - 1, &nwrite);
#else
#error please specify request to get version information
#endif
  if (err) {
    emit versionAvailibled(false, QString());
    err = client.Close();
    if (err) {
      DNOTREACHED();
    }
    return;
  }

  char version[128] = {0};
  size_t nread = 0;
  err = client.Read(version, sizeof(version), &nread);
  if (err) {
    emit versionAvailibled(false, QString());
    err = client.Close();
    if (err) {
      DNOTREACHED();
    }
    return;
  }

  emit versionAvailibled(true, version);
  err = client.Close();
  if (err) {
    DNOTREACHED();
  }
}

}  // namespace gui
}  // namespace fastonosql
