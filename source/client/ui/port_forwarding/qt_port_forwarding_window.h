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

#ifndef CLIENT_UI_PORT_FORWARDING_QT_PORT_FORWARDING_WINDOW_H
#define CLIENT_UI_PORT_FORWARDING_QT_PORT_FORWARDING_WINDOW_H

#include "base/macros_magic.h"
#include "client/port_forwarding_window_proxy.h"
#include "client/ui/session_window.h"
#include "proto/port_forwarding.pb.h"

namespace Ui {
class PortForwardingWindow;
} // namespace Ui

namespace client {

class QtPortForwardingWindow final
    : public SessionWindow,
      public PortForwardingWindow
{
    Q_OBJECT

public:
    explicit QtPortForwardingWindow(const proto::port_forwarding::Config& session_config,
                                    QWidget* parent = nullptr);
    ~QtPortForwardingWindow() final;

    // SessionWindow implementation.
    std::unique_ptr<Client> createClient() final;

    // PortForwardingWindow implementation.
    void start() final;
    void setStatistics(const Statistics& statistics) final;

protected:
    // SessionWindow implementation.
    void onInternalReset() final;

private:
    std::unique_ptr<Ui::PortForwardingWindow> ui;
    std::shared_ptr<PortForwardingWindowProxy> port_forwarding_window_proxy_;
    proto::port_forwarding::Config session_config_;

    DISALLOW_COPY_AND_ASSIGN(QtPortForwardingWindow);
};

} // namespace client

#endif // CLIENT_UI_PORT_FORWARDING_QT_PORT_FORWARDING_WINDOW_H
