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

#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include "base/net/tcp_channel.h"
#include "build/build_config.h"
#include "proto/router_relay.pb.h"
#include "relay/sessions_worker.h"
#include "relay/shared_pool.h"

#include <QTimer>

namespace base {
class ClientAuthenticator;
} // namespace base

namespace relay {

class Controller final
    : public QObject,
      public base::TcpChannel::Listener,
      public SessionManager::Delegate,
      public SharedPool::Delegate
{
    Q_OBJECT

public:
    explicit Controller(std::shared_ptr<base::TaskRunner> task_runner, QObject* parent = nullptr);
    ~Controller() final;

    bool start();

protected:
    // base::TcpChannel::Listener implementation.
    void onTcpConnected() final;
    void onTcpDisconnected(base::NetworkChannel::ErrorCode error_code) final;
    void onTcpMessageReceived(uint8_t channel_id, const QByteArray& buffer) final;
    void onTcpMessageWritten(uint8_t channel_id, size_t pending) final;

    // SessionManager::Delegate implementation.
    void onSessionStarted() final;
    void onSessionStatistics(const proto::RelayStat& relay_stat) final;
    void onSessionFinished() final;

    // SharedPool::Delegate implementation.
    void onPoolKeyExpired(uint32_t key_id) final;

private:
    void connectToRouter();
    void delayedConnectToRouter();
    void sendKeyPool(uint32_t key_count);

    // Router settings.
    QString router_address_;
    uint16_t router_port_ = 0;
    QByteArray router_public_key_;

    // Peers settings.
    QString listen_interface_;
    QString peer_address_;
    uint16_t peer_port_ = 0;
    std::chrono::minutes peer_idle_timeout_;
    uint32_t max_peer_count_ = 0;
    bool statistics_enabled_ = false;
    std::chrono::seconds statistics_interval_;

    std::shared_ptr<base::TaskRunner> task_runner_;
    QTimer reconnect_timer_;
    std::unique_ptr<base::TcpChannel> channel_;
    std::unique_ptr<base::ClientAuthenticator> authenticator_;
    std::unique_ptr<SharedPool> shared_pool_;
    std::unique_ptr<SessionsWorker> sessions_worker_;

    std::unique_ptr<proto::RouterToRelay> incoming_message_;
    std::unique_ptr<proto::RelayToRouter> outgoing_message_;

    int session_count_ = 0;

    DISALLOW_COPY_AND_ASSIGN(Controller);
};

} // namespace relay

#endif // RELAY_CONTROLLER_H
