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

#ifndef CLIENT_UI_HOSTS_HOSTS_TAB_H
#define CLIENT_UI_HOSTS_HOSTS_TAB_H

#include <QHash>

#include "client/config.h"
#include "client/router.h"
#include "client/ui/tab.h"
#include "proto/peer.h"
#include "ui_hosts_tab.h"

class ContentWidget;
class LocalGroupWidget;
class QTreeWidgetItem;
class RouterGroupWidget;
class RouterWidget;
class SearchWidget;
class User;

class HostsTab final : public Tab
{
    Q_OBJECT

public:
    explicit HostsTab(QWidget* parent = nullptr);
    ~HostsTab() final;

    // Tab implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    void activate(QStatusBar* statusbar) final;
    void deactivate(QStatusBar* statusbar) final;
    bool hasSearchField() const final;
    void onSearchTextChanged(const QString& text) final;

    void reloadRouters();

signals:
    void sig_connect(qint64 computer_id,
                     const ComputerConfig& computer,
                     proto::peer::SessionType session_type);

private slots:
    void onRouterStatusChanged(qint64 router_id, Router::Status status);
    void onSwitchContent(Sidebar::Item::Type type);
    void onSidebarContextMenu(Sidebar::Item::Type type, const QPoint& pos);
    void onCurrentComputerChanged(qint64 computer_id);
    void onConnectAction(QAction* action);
    void onLocalConnect(qint64 computer_id);
    void onLocalComputerContextMenu(qint64 computer_id, const QPoint& pos);
    void onUserContextMenu(qint64 router_id, const User& user, const QPoint& pos);
    void onHostContextMenu(qint64 router_id, const QPoint& pos, int column);
    void onRelayContextMenu(qint64 router_id, const QPoint& pos, int column);
    void onAddUserAction();
    void onEditUserAction();
    void onDeleteUserAction();
    void onImportOldBookAction();
    void onReloadAction();
    void onSaveAction();
    void onDisconnectAction();
    void onDisconnectAllAction();
    void onRemoveHostAction();
    void onOnlineCheckToggled(bool checked);

private:
    void switchContent(ContentWidget* new_widget);
    void updateActionsState();
    proto::peer::SessionType defaultSessionType() const;

    void destroyAllRouterWidgets();
    void destroyRouterWidget(qint64 router_id);
    RouterWidget* createRouterWidget(const RouterConfig& config);

    void editRouter(qint64 router_id);
    void deleteRouter(qint64 router_id);

    bool validateComputerForConnect(const ComputerConfig& computer);

    Ui::HostsTab ui;

    QAction* action_add_group_ = nullptr;
    QAction* action_delete_group_ = nullptr;
    QAction* action_edit_group_ = nullptr;

    QAction* action_add_computer_ = nullptr;
    QAction* action_delete_computer_ = nullptr;
    QAction* action_edit_computer_ = nullptr;
    QAction* action_copy_computer_ = nullptr;

    QAction* action_desktop_ = nullptr;
    QAction* action_file_transfer_ = nullptr;
    QAction* action_chat_ = nullptr;
    QAction* action_system_info_ = nullptr;

    QAction* action_desktop_connect_ = nullptr;
    QAction* action_file_transfer_connect_ = nullptr;
    QAction* action_chat_connect_ = nullptr;
    QAction* action_system_info_connect_ = nullptr;

    QAction* action_add_user_ = nullptr;
    QAction* action_edit_user_ = nullptr;
    QAction* action_delete_user_ = nullptr;

    QAction* action_save_ = nullptr;
    QAction* action_reload_ = nullptr;
    QAction* action_online_check_ = nullptr;
    QAction* action_import_old_book_ = nullptr;

    QAction* action_disconnect_ = nullptr;
    QAction* action_disconnect_all_ = nullptr;

    QAction* action_host_remove_ = nullptr;

    ContentWidget* current_content_ = nullptr;
    ContentWidget* previous_content_ = nullptr;

    QStatusBar* statusbar_ = nullptr;

    LocalGroupWidget* local_group_widget_ = nullptr;
    RouterGroupWidget* router_group_widget_ = nullptr;
    SearchWidget* search_widget_ = nullptr;

    QHash<qint64, RouterWidget*> router_widgets_;

    Q_DISABLE_COPY_MOVE(HostsTab)
};

#endif // CLIENT_UI_HOSTS_HOSTS_TAB_H
