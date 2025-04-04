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

#ifndef CLIENT_UI_SESSION_WINDOW_H
#define CLIENT_UI_SESSION_WINDOW_H

#include "client/client_config.h"
#include "client/client_session_state.h"
#include "client/status_window.h"

#include <QWidget>

namespace base {
class TaskRunner;
} // namespace base

namespace common {
class StatusDialog;
} // namespace common

namespace client {

class Client;
class ClientProxy;
class StatusWindowProxy;

class SessionWindow
    : public QWidget,
      public StatusWindow
{
    Q_OBJECT

public:
    explicit SessionWindow(std::shared_ptr<SessionState> session_state, QWidget* parent = nullptr);
    virtual ~SessionWindow() override;

    // Connects to a host.
    // If the username and/or password are not specified in the connection parameters, the
    // authorization dialog will be displayed.
    bool connectToHost(Config config);

    std::shared_ptr<SessionState> sessionState() { return session_state_; }

protected:
    virtual std::unique_ptr<Client> createClient() = 0;
    virtual void onInternalReset() = 0;

    // QWidget implementation.
    void closeEvent(QCloseEvent* event) override;

    // StatusWindow implementation.
    void onStarted() final;
    void onStopped() final;
    void onRouterConnecting() final;
    void onRouterConnected() final;
    void onHostConnecting() final;
    void onHostConnected() final;
    void onHostDisconnected(base::TcpChannel::ErrorCode error_code) final;
    void onWaitForRouter() final;
    void onWaitForRouterTimeout() final;
    void onWaitForHost() final;
    void onWaitForHostTimeout() final;
    void onVersionMismatch() final;
    void onAccessDenied(base::ClientAuthenticator::ErrorCode error_code) final;
    void onRouterError(const RouterController::Error& error) final;

private:
    void setClientTitle(const Config& config);
    void onErrorOccurred(const QString& message);

    static QString netErrorToString(base::TcpChannel::ErrorCode error_code);
    static QString authErrorToString(base::ClientAuthenticator::ErrorCode error_code);
    static QString routerErrorToString(RouterController::ErrorCode error_code);

    std::shared_ptr<SessionState> session_state_;
    std::shared_ptr<StatusWindowProxy> status_window_proxy_;
    std::unique_ptr<ClientProxy> client_proxy_;
    common::StatusDialog* status_dialog_ = nullptr;
};

} // namespace client

#endif // CLIENT_UI_SESSION_WINDOW_H
