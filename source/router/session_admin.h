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

#ifndef ROUTER_SESSION_ADMIN_H
#define ROUTER_SESSION_ADMIN_H

#include "proto/router_admin.pb.h"
#include "router/session.h"

namespace router {

class ServerProxy;

class SessionAdmin final : public Session
{
public:
    SessionAdmin();
    ~SessionAdmin() final;

protected:
    // Session implementation.
    void onSessionReady() final;
    void onSessionMessageReceived(uint8_t channel_id, const QByteArray& buffer) final;
    void onSessionMessageWritten(uint8_t channel_id, size_t pending) final;

private:
    void doUserListRequest();
    void doUserRequest(const proto::UserRequest& request);
    void doSessionListRequest(const proto::SessionListRequest& request);
    void doSessionRequest(const proto::SessionRequest& request);
    void doPeerConnectionRequest(const proto::PeerConnectionRequest& request);

    proto::UserResult::ErrorCode addUser(const proto::User& user);
    proto::UserResult::ErrorCode modifyUser(const proto::User& user);
    proto::UserResult::ErrorCode deleteUser(const proto::User& user);

    DISALLOW_COPY_AND_ASSIGN(SessionAdmin);
};

} // namespace router

#endif // ROUTER_SESSION_ADMIN_H
