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

#include "client/ui/main_window.h"

#include <QDesktopServices>
#include <QLineEdit>
#include <QTabBar>
#include <QTimer>
#include <QUrl>
#include <QWindow>

#include "base/gui_application.h"
#include "common/ui/msg_box.h"
#include "base/logging.h"
#include "base/version_constants.h"
#include "base/peer/host_id.h"
#include "build/build_config.h"
#include "client/database.h"
#include "client/settings.h"
#include "client/ui/settings_dialog.h"
#include "client/ui/tab.h"
#include "client/ui/session_tab.h"
#include "client/ui/session_window.h"
#include "client/ui/tab_bar.h"
#include "client/ui/tab_widget.h"
#include "client/ui/chat/chat_session_window.h"
#include "client/ui/hosts/hosts_tab.h"
#include "client/ui/desktop/desktop_session_window.h"
#include "client/ui/file_transfer/file_transfer_session_window.h"
#include "client/ui/sys_info/system_info_session_window.h"
#include "common/ui/session_type.h"
#include "common/update_checker.h"
#include "common/update_info.h"
#include "common/ui/about_dialog.h"
#include "common/ui/update_dialog.h"

//--------------------------------------------------------------------------------------------------
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    LOG(INFO) << "Ctor";

    Settings settings;

    ui.setupUi(this);

    connect(ui.tabs->tabBar(), &TabBar::sig_tabDetachRequested, this, &MainWindow::onTabDetachRequested);

    // Create search field at the far right of the toolbar.
    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui.toolbar->addWidget(spacer);

    search_field_ = new QLineEdit(this);
    search_field_->setPlaceholderText(tr("Search..."));
    search_field_->setClearButtonEnabled(true);
    search_field_->setMaximumWidth(250);
    search_action_ = ui.toolbar->addWidget(search_field_);
    search_action_->setVisible(false);

    restoreGeometry(settings.windowGeometry());
    restoreState(settings.windowState());

    ui.action_toolbar->setChecked(settings.isToolBarEnabled());
    ui.action_statusbar->setChecked(settings.isStatusBarEnabled());
    ui.statusbar->setVisible(settings.isStatusBarEnabled());

    bool large_icons = settings.largeIcons();
    ui.toolbar->setIconSize(large_icons ? QSize(32, 32) : QSize(24, 24));
    ui.action_large_icons->setChecked(large_icons);

    ui.action_sessions_in_tabs->setChecked(settings.openSessionsInTabs());

    connect(ui.action_settings, &QAction::triggered, this, &MainWindow::onSettings);
    connect(ui.action_help, &QAction::triggered, this, &MainWindow::onHelp);
    connect(ui.action_about, &QAction::triggered, this, &MainWindow::onAbout);
    connect(ui.action_exit, &QAction::triggered, this, &MainWindow::close);
    connect(ui.toolbar, &QToolBar::visibilityChanged, ui.action_toolbar, &QAction::setChecked);
    connect(ui.action_toolbar, &QAction::toggled, ui.toolbar, &QToolBar::setVisible);
    connect(ui.action_statusbar, &QAction::toggled, ui.statusbar, &QStatusBar::setVisible);
    connect(ui.action_large_icons, &QAction::toggled, this, [this](bool enable)
    {
        ui.toolbar->setIconSize(enable ? QSize(32, 32) : QSize(24, 24));
    });

    // Tab management.
    connect(ui.tabs, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
    connect(ui.tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onCloseTab);

    // Search field.
    connect(search_field_, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);

#if defined(Q_OS_WINDOWS)
    Database& db = Database::instance();
    if (db.isCheckUpdatesEnabled())
    {
        QString update_server = db.updateServer();
        update_checker_ = std::make_unique<UpdateChecker>(update_server, "client");

        connect(update_checker_.get(), &UpdateChecker::sig_checkedFinished,
                this, &MainWindow::onUpdateCheckedFinished);

        LOG(INFO) << "Start update checker";
        update_checker_->start();
    }
#endif

    connect(GuiApplication::instance(), &GuiApplication::sig_themeChanged,
            this, &MainWindow::onAfterThemeChanged);
    onAfterThemeChanged();

    // Hide dynamic menus until a tab populates them.
    ui.menu_edit->menuAction()->setVisible(false);
    ui.menu_session_type->menuAction()->setVisible(false);

    HostsTab* hosts = new HostsTab(this);
    connect(hosts, &HostsTab::sig_connect, this, &MainWindow::onConnect);

    // Create default tabs.
    addTab(hosts, tr("Hosts"), QIcon(":/img/computer.svg"));
}

//--------------------------------------------------------------------------------------------------
MainWindow::~MainWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void MainWindow::showAndActivate()
{
    LOG(INFO) << "Show and activate window";

    show();
    raise();
    activateWindow();
    setFocus();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::closeEvent(QCloseEvent* /* event */)
{
    LOG(INFO) << "Close event detected";

    Settings settings;
    settings.setWindowGeometry(saveGeometry());
    settings.setWindowState(saveState());
    settings.setToolBarEnabled(ui.action_toolbar->isChecked());
    settings.setStatusBarEnabled(ui.action_statusbar->isChecked());
    settings.setLargeIcons(ui.action_large_icons->isChecked());
    settings.setOpenSessionsInTabs(ui.action_sessions_in_tabs->isChecked());

    for (int i = 0; i < ui.tabs->count(); ++i)
    {
        Tab* tab = dynamic_cast<Tab*>(ui.tabs->widget(i));
        if (tab)
            settings.setTabState(tab->objectName(), tab->saveState());
    }

    QApplication::quit();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onUpdateCheckedFinished(const QByteArray& result)
{
    if (result.isEmpty())
    {
        LOG(ERROR) << "Error while retrieving update information";
    }
    else
    {
        UpdateInfo update_info = UpdateInfo::fromXml(result);
        if (!update_info.isValid())
        {
            LOG(INFO) << "No updates available";
        }
        else
        {
            const QVersionNumber& current_version = kCurrentVersion;
            const QVersionNumber& update_version = update_info.version();

            if (update_version > current_version)
            {
                LOG(INFO) << "New version available:" << update_version.toString();
                UpdateDialog(update_info, this).exec();
            }
        }
    }

    QTimer::singleShot(0, this, [this]()
    {
        LOG(INFO) << "Destroy update checker";
        update_checker_.reset();
    });
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onSettings()
{
    LOG(INFO) << "[ACTION] Settings clicked";
    if (SettingsDialog(this).exec() == QDialog::Accepted)
    {
        ui.retranslateUi(this);

        for (int i = 0; i < ui.tabs->count(); ++i)
        {
            HostsTab* hosts = dynamic_cast<HostsTab*>(ui.tabs->widget(i));
            if (hosts)
                hosts->reloadRouters();
        }
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onHelp()
{
    LOG(INFO) << "[ACTION] Help clicked";
    QDesktopServices::openUrl(QUrl("https://aspia.org/help"));
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onAbout()
{
    LOG(INFO) << "[ACTION] About clicked";
    AboutDialog(tr("Aspia Client"), this).exec();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onAfterThemeChanged()
{

}

//--------------------------------------------------------------------------------------------------
void MainWindow::onCurrentTabChanged(int index)
{
    if (active_tab_)
    {
        removeTabActions();
        active_tab_->deactivate(ui.statusbar);
        active_tab_ = nullptr;
    }

    if (index == -1)
        return;

    Tab* tab = tabAt(index);
    if (!tab)
        return;

    active_tab_ = tab;
    installTabActions(active_tab_);
    active_tab_->activate(ui.statusbar);
    updateSearchFieldVisibility();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onCloseTab(int index)
{
    Tab* tab = tabAt(index);
    if (!tab || !tab->isClosable())
        return;

    if (tab == active_tab_)
    {
        removeTabActions();
        tab->deactivate(ui.statusbar);
        active_tab_ = nullptr;
    }

    Settings settings;
    settings.setTabState(tab->objectName(), tab->saveState());

    ui.tabs->removeTab(index);
    delete tab;
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onSearchTextChanged(const QString& text)
{
    if (active_tab_)
        active_tab_->onSearchTextChanged(text);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onConnect(qint64 /* computer_id */,
                           const ComputerConfig& computer,
                           proto::peer::SessionType session_type)
{
    if (isHostId(computer.address) && computer.router_id <= 0)
    {
        MsgBox::warning(this,
            tr("Connection by ID is specified in the properties of the computer, "
               "but the router is not configured. Check the parameters of the "
               "router in the properties of the address book."));
        return;
    }

    SessionWindow* session_window = nullptr;

    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP:
            session_window = new DesktopSessionWindow(Settings().desktopConfig());
            break;

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            session_window = new FileTransferSessionWindow();
            break;

        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
            session_window = new SystemInfoSessionWindow();
            break;

        case proto::peer::SESSION_TYPE_TEXT_CHAT:
            session_window = new ChatSessionWindow();
            break;

        default:
            NOTREACHED();
            break;
    }

    if (!session_window)
        return;

    QString display_name = Database::instance().displayName();
    QString computer_name = computer.name.isEmpty() ? computer.address : computer.name;
    QString title = QString("%1 - %2").arg(computer_name, sessionName(session_type));
    QIcon icon = sessionIcon(session_type);

    session_window->setWindowIcon(icon);

    SessionTab* session_tab = new SessionTab(session_window);
    addTab(session_tab, title, icon);

    if (!ui.action_sessions_in_tabs->isChecked())
    {
        int index = ui.tabs->indexOf(session_tab);
        ui.tabs->tabBar()->setTabVisible(index, false);
        session_tab->detachToWindow();
    }

    if (!session_window->connectToHost(computer, display_name))
    {
        int index = ui.tabs->indexOf(session_tab);
        if (index != -1)
            onCloseTab(index);
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onTabDetachRequested(int index, const QPoint& global_pos)
{
    SessionTab* session_tab = dynamic_cast<SessionTab*>(tabAt(index));
    if (!session_tab || !session_tab->isDetachable() || session_tab->isDetached())
        return;

    QSize new_size = session_tab->size() / 2;

    session_tab->detachToWindow();
    ui.tabs->tabBar()->setTabVisible(index, false);

    SessionWindow* session_window = session_tab->sessionWindow();
    if (!session_window)
        return;

    session_window->resize(new_size);
    session_window->move(global_pos - QPoint(new_size.width() / 2, 15));
    session_window->raise();
    session_window->activateWindow();

    if (QWindow* handle = session_window->windowHandle())
        handle->startSystemMove();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onTabDragMove(const QPoint& global_pos)
{
    SessionTab* session_tab = qobject_cast<SessionTab*>(sender());
    if (!session_tab || !session_tab->isDetached())
        return;

    int index = ui.tabs->indexOf(session_tab);
    if (index == -1)
        return;

    TabBar* tabbar = ui.tabs->tabBar();
    bool over = tabBarHitTest(global_pos);
    if (tabbar->isTabVisible(index) != over)
        tabbar->setTabVisible(index, over);
    tabbar->setDropTarget(over ? index : -1);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onTabDragFinished(const QPoint& global_pos)
{
    SessionTab* session_tab = qobject_cast<SessionTab*>(sender());
    if (!session_tab || !session_tab->isDetached())
        return;

    int index = ui.tabs->indexOf(session_tab);
    if (index == -1)
        return;

    TabBar* tabbar = ui.tabs->tabBar();
    tabbar->setDropTarget(-1);

    if (tabBarHitTest(global_pos))
    {
        session_tab->attachToTab();
        tabbar->setTabVisible(index, true);
        ui.tabs->setCurrentIndex(index);
    }
    else if (tabbar->isTabVisible(index))
    {
        // Drop missed the tab bar; clean up the preview state we may have set during drag.
        tabbar->setTabVisible(index, false);
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::addTab(Tab* tab, const QString& title, const QIcon& icon)
{
    int index = ui.tabs->addTab(tab, icon, title);

    if (!tab->isClosable())
        hideCloseButtonForTab(index);

    Settings settings;
    tab->restoreState(settings.tabState(tab->objectName()));

    connect(tab, &Tab::sig_titleChanged, this, [this, tab](const QString& new_title)
    {
        int tab_index = ui.tabs->indexOf(tab);
        if (tab_index != -1)
            ui.tabs->setTabText(tab_index, new_title);
    });

    connect(tab, &Tab::sig_closeRequested, this, [this, tab]()
    {
        int tab_index = ui.tabs->indexOf(tab);
        if (tab_index != -1)
            onCloseTab(tab_index);
    }, Qt::QueuedConnection);

    connect(tab, &Tab::sig_dragMove, this, &MainWindow::onTabDragMove);
    connect(tab, &Tab::sig_dragFinished, this, &MainWindow::onTabDragFinished);

    ui.tabs->setCurrentIndex(index);
}

//--------------------------------------------------------------------------------------------------
bool MainWindow::tabBarHitTest(const QPoint& global_pos) const
{
    QTabBar* tabbar = ui.tabs->tabBar();
    if (!tabbar->isVisible())
        return false;

    return tabbar->rect().contains(tabbar->mapFromGlobal(global_pos));
}

//--------------------------------------------------------------------------------------------------
void MainWindow::hideCloseButtonForTab(int index)
{
    // Hide close button on both sides for cross-platform compatibility.
    QWidget* close_button = ui.tabs->tabBar()->tabButton(index, QTabBar::RightSide);
    if (close_button)
        close_button->hide();

    close_button = ui.tabs->tabBar()->tabButton(index, QTabBar::LeftSide);
    if (close_button)
        close_button->hide();
}

//--------------------------------------------------------------------------------------------------
Tab* MainWindow::tabAt(int index)
{
    return dynamic_cast<Tab*>(ui.tabs->widget(index));
}

//--------------------------------------------------------------------------------------------------
void MainWindow::updateSearchFieldVisibility()
{
    bool show_search = active_tab_ && active_tab_->hasSearchField();

    if (!show_search && search_field_->isVisible())
        search_field_->clear();

    search_action_->setVisible(show_search);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::installTabActions(Tab* tab)
{
    const QList<Tab::ActionGroupEntry>& groups = tab->actionGroups();

    // Find the first static toolbar action to insert before it.
    QAction* before = nullptr;
    QList<QAction*> toolbar_actions = ui.toolbar->actions();
    if (!toolbar_actions.isEmpty())
        before = toolbar_actions.first();

    for (int i = 0; i < groups.size(); ++i)
    {
        const Tab::ActionGroupEntry& entry = groups[i];

        // Add actions to toolbar and connect visibility tracking. Actions marked with
        // kMenuOnlyProperty go only to the menu and are skipped here.
        bool any_in_toolbar = false;
        for (QAction* action : entry.second)
        {
            if (action->property(Tab::kMenuOnlyProperty).toBool())
                continue;

            ui.toolbar->insertAction(before, action);
            tab_toolbar_actions_.append(action);

            connect(action, &QAction::changed, this, &MainWindow::updateSeparatorVisibility);
            any_in_toolbar = true;
        }

        // Add separator after each group (only if at least one action was added to toolbar).
        if (any_in_toolbar)
        {
            QAction* separator = new QAction(this);
            separator->setSeparator(true);
            ui.toolbar->insertAction(before, separator);
            tab_toolbar_actions_.append(separator);
        }

        // Add actions to corresponding menu.
        QMenu* menu = menuForActionGroup(entry.first);
        if (menu)
        {
            // Find the first static action (the first action that is not one of our
            // previously inserted dynamic items).
            QAction* anchor = nullptr;
            const QList<QAction*> existing = menu->actions();
            for (QAction* action : existing)
            {
                bool is_ours = false;
                for (const auto& [m, items] : std::as_const(tab_menu_actions_))
                {
                    if (m == menu && items.contains(action))
                    {
                        is_ours = true;
                        break;
                    }
                }
                if (!is_ours)
                {
                    anchor = action;
                    break;
                }
            }

            QList<QAction*> inserted_items;
            for (QAction* action : entry.second)
            {
                if (anchor)
                    menu->insertAction(anchor, action);
                else
                    menu->addAction(action);
                inserted_items.append(action);
            }

            // Separator between this dynamic group and the following items (static or next group).
            QAction* separator = anchor ? menu->insertSeparator(anchor) : menu->addSeparator();
            inserted_items.append(separator);

            tab_menu_actions_.append({ menu, inserted_items });
            menu->menuAction()->setVisible(true);
        }
    }

    updateSeparatorVisibility();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::removeTabActions()
{
    // Remove from toolbar.
    for (QAction* action : std::as_const(tab_toolbar_actions_))
    {
        if (!action->isSeparator())
            disconnect(action, &QAction::changed, this, &MainWindow::updateSeparatorVisibility);

        ui.toolbar->removeAction(action);

        if (action->isSeparator())
            delete action;
    }

    tab_toolbar_actions_.clear();

    // Remove from menus.
    for (const auto& [menu, actions] : std::as_const(tab_menu_actions_))
    {
        for (QAction* action : actions)
        {
            menu->removeAction(action);
            if (action->isSeparator())
                delete action;
        }

        // Hide menu if it has no actions left.
        if (menu->isEmpty())
            menu->menuAction()->setVisible(false);
    }

    tab_menu_actions_.clear();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::updateSeparatorVisibility()
{
    // Update toolbar separator visibility.
    for (int i = 0; i < tab_toolbar_actions_.size(); ++i)
    {
        if (!tab_toolbar_actions_[i]->isSeparator())
            continue;

        // Separator is visible if there is at least one visible action in the group before it.
        bool has_visible = false;
        for (int j = i - 1; j >= 0; --j)
        {
            if (tab_toolbar_actions_[j]->isSeparator())
                break;
            if (tab_toolbar_actions_[j]->isVisible())
            {
                has_visible = true;
                break;
            }
        }

        tab_toolbar_actions_[i]->setVisible(has_visible);
    }

    // Update menu visibility: hide menu if none of its tab actions are visible.
    QSet<QMenu*> menus;
    for (const auto& [menu, actions] : std::as_const(tab_menu_actions_))
        menus.insert(menu);

    for (QMenu* menu : std::as_const(menus))
    {
        bool has_visible = false;
        const QList<QAction*> menu_actions = menu->actions();
        for (QAction* action : menu_actions)
        {
            if (!action->isSeparator() && action->isVisible())
            {
                has_visible = true;
                break;
            }
        }

        menu->menuAction()->setVisible(has_visible);
    }
}

//--------------------------------------------------------------------------------------------------
QMenu* MainWindow::menuForActionGroup(Tab::ActionRole group) const
{
    switch (group)
    {
        case Tab::ActionRole::FILE:
            return ui.menu_file;

        case Tab::ActionRole::EDIT:
            return ui.menu_edit;

        case Tab::ActionRole::VIEW:
            return ui.menu_view;

        case Tab::ActionRole::SESSION_TYPE:
            return ui.menu_session_type;

        case Tab::ActionRole::HELP:
            return ui.menu_help;

        default:
            return nullptr;
    }
}
