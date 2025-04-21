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

#ifndef CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_ROUTER_H
#define CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_ROUTER_H

#include "base/net/tcp_channel.h"
#include "client/router_config.h"

#include <deque>

#include <QTimer>

namespace base {
class ClientAuthenticator;
class Location;
class TaskRunner;
} // namespace base

namespace client {

class OnlineCheckerRouter final
    : public QObject,
      public base::TcpChannel::Listener
{
    Q_OBJECT

public:
    explicit OnlineCheckerRouter(const RouterConfig& router_config, QObject* parent = nullptr);
    ~OnlineCheckerRouter() final;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onRouterCheckerResult(int computer_id, bool online) = 0;
        virtual void onRouterCheckerFinished() = 0;
    };

    struct Computer
    {
        int computer_id = -1;
        base::HostId host_id = base::kInvalidHostId;
    };
    using ComputerList = std::deque<Computer>;

    void start(const ComputerList& computers, Delegate* delegate);

protected:
    // base::TcpChannel::Listener implementation.
    void onTcpConnected() final;
    void onTcpDisconnected(base::NetworkChannel::ErrorCode error_code) final;
    void onTcpMessageReceived(uint8_t channel_id, const base::ByteArray& buffer) final;
    void onTcpMessageWritten(uint8_t channel_id, base::ByteArray&& buffer, size_t pending) final;

private:
    void checkNextComputer();
    void onFinished(const base::Location& location);

    std::unique_ptr<base::TcpChannel> channel_;
    std::unique_ptr<base::ClientAuthenticator> authenticator_;
    QTimer timer_;
    RouterConfig router_config_;

    ComputerList computers_;
    Delegate* delegate_ = nullptr;
};

} // namespace client

#endif // CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_ROUTER_H
