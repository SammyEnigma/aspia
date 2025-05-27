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

#ifndef HOST_USER_SESSION_MANAGER_H
#define HOST_USER_SESSION_MANAGER_H

#include "base/session_id.h"
#include "base/ipc/ipc_server.h"
#include "base/win/session_status.h"
#include "host/user_session.h"

#include <QObject>
#include <QList>

namespace host {

class UserSessionManager final : public QObject
{
    Q_OBJECT

public:
    explicit UserSessionManager(QObject* parent = nullptr);
    ~UserSessionManager() final;

    bool start();
    void onUserSessionEvent(base::SessionStatus status, base::SessionId session_id);
    void onRouterStateChanged(const proto::internal::RouterState& router_state);
    void onUpdateCredentials(base::HostId host_id, const QString& password);
    void onSettingsChanged();
    void onClientSession(ClientSession* client_session);

signals:
    void sig_routerStateRequested();
    void sig_credentialsRequested();
    void sig_changeOneTimePassword();
    void sig_changeOneTimeSessions(quint32 sessions);

private slots:
    void onUserSessionFinished();
    void onIpcNewConnection();

private:
    void startSessionProcess(const base::Location& location, base::SessionId session_id);
    void addUserSession(const base::Location& location, base::SessionId session_id,
                        base::IpcChannel* channel);

    base::IpcServer* ipc_server_ = nullptr;
    QList<UserSession*> sessions_;

    DISALLOW_COPY_AND_ASSIGN(UserSessionManager);
};

} // namespace host

#endif // HOST_USER_SESSION_MANAGER_H
