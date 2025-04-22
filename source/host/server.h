//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef HOST_SERVER_H
#define HOST_SERVER_H

#include "build/build_config.h"
#include "base/net/tcp_server.h"
#include "base/peer/server_authenticator_manager.h"
#include "common/http_file_downloader.h"
#include "common/update_checker.h"
#include "host/router_controller.h"
#include "host/user_session_manager.h"
#include "host/system_settings.h"

#include <QTimer>

namespace base {
class FilePathWatcher;
class TaskRunner;
} // namespace base

namespace host {

class Server final
    : public QObject,
      public base::TcpServer::Delegate,
      public RouterController::Delegate,
      public base::ServerAuthenticatorManager::Delegate,
      public UserSessionManager::Delegate,
      public common::UpdateChecker::Delegate,
      public common::HttpFileDownloader::Delegate
{
    Q_OBJECT

public:
    explicit Server(std::shared_ptr<base::TaskRunner> task_runner, QObject* parent = nullptr);
    ~Server() final;

    void start();
    void setSessionEvent(base::win::SessionStatus status, base::SessionId session_id);
    void setPowerEvent(uint32_t power_event);

protected:
    // net::TcpServer::Delegate implementation.
    void onNewConnection(std::unique_ptr<base::TcpChannel> channel) final;

    // RouterController::Delegate implementation.
    void onRouterStateChanged(const proto::internal::RouterState& router_state) final;
    void onHostIdAssigned(const QString& session_name, base::HostId host_id) final;
    void onClientConnected(std::unique_ptr<base::TcpChannel> channel) final;

    // base::AuthenticatorManager::Delegate implementation.
    void onNewSession(base::ServerAuthenticatorManager::SessionInfo&& session_info) final;

    // UserSessionManager::Delegate implementation.
    void onHostIdRequest(const QString& session_name) final;
    void onResetHostId(base::HostId host_id) final;
    void onUserListChanged() final;

    // common::UpdateChecker::Delegate implementation.
    void onUpdateCheckedFinished(const QByteArray& result) final;

    // common::HttpFileDownloader::Delegate implementation.
    void onFileDownloaderError(int error_code) final;
    void onFileDownloaderCompleted() final;
    void onFileDownloaderProgress(int percentage) final;

private:
    void startAuthentication(std::unique_ptr<base::TcpChannel> channel);
    void addFirewallRules();
    void deleteFirewallRules();
    void updateConfiguration(const std::filesystem::path& path, bool error);
    void reloadUserList();
    void connectToRouter();
    void disconnectFromRouter();
    void checkForUpdates();

    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<QTimer> update_timer_;

    std::unique_ptr<base::FilePathWatcher> settings_watcher_;
    SystemSettings settings_;

    // Accepts incoming network connections.
    std::unique_ptr<base::TcpServer> server_;
    std::unique_ptr<RouterController> router_controller_;
    std::unique_ptr<base::ServerAuthenticatorManager> authenticator_manager_;
    std::unique_ptr<UserSessionManager> user_session_manager_;

    std::unique_ptr<common::UpdateChecker> update_checker_;
    std::unique_ptr<common::HttpFileDownloader> update_downloader_;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

} // namespace host

#endif // HOST_SERVER_H
