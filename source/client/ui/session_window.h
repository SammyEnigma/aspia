//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/client.h"
#include "client/config.h"
#include "client/session_state.h"
#include "proto/peer.h"

#include <QWidget>

class QTimer;
class StatusDialog;

class SessionWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SessionWindow(proto::peer::SessionType session_type, QWidget* parent = nullptr);
    virtual ~SessionWindow() override;

    // Connects to a host.
    // If the username and/or password are not specified in the connection parameters, the
    // authorization dialog will be displayed.
    bool connectToHost(ComputerConfig computer, const QString& display_name);

    std::shared_ptr<SessionState> sessionState() { return session_state_; }

signals:
    void sig_start();
    void sig_stop();

    // Emitted on each drag-poll tick while the user is dragging this widget as a top-level
    // window with the left mouse button held. The owner uses global_pos to update visual hints
    // (e.g. previewing the destination in the main tab bar).
    void sig_dragMove(const QPoint& global_pos);

    // Emitted when the user finishes a native window-drag of this widget while it is top-level
    // (i.e. detached from a tab). The owner inspects global_release_pos to decide whether to
    // re-attach the session into the main tab bar.
    void sig_dragFinished(const QPoint& global_release_pos);

protected:
    virtual Client* createClient() = 0;
    virtual void onInternalReset() = 0;

    // QWidget implementation.
    void closeEvent(QCloseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;

public slots:
    void onStatusChanged(Client::Status status, const QVariant& data);

private slots:
    void onDragPoll();

private:
    void setClientTitle(const ComputerConfig& computer, proto::peer::SessionType session_type);
    void onErrorOccurred(const QString& message);

    const proto::peer::SessionType session_type_;
    std::shared_ptr<SessionState> session_state_;
    StatusDialog* status_dialog_ = nullptr;
    QTimer* drag_poll_timer_ = nullptr;
};

#endif // CLIENT_UI_SESSION_WINDOW_H
