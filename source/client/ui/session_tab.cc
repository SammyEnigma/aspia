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

#include "client/ui/session_tab.h"

#include <QEvent>
#include <QVBoxLayout>

#include "client/ui/client_window.h"

//--------------------------------------------------------------------------------------------------
SessionTab::SessionTab(ClientWindow* client_window, QWidget* parent)
    : Tab(Type::SESSION, "session", parent),
      client_window_(client_window)
{
    CHECK(client_window);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    client_window_->setParent(this);
    client_window_->installEventFilter(this);
    layout->addWidget(client_window_);
    client_window_->show();

    connect(client_window_, &ClientWindow::sig_dragMove, this, &Tab::sig_dragMove);
    connect(client_window_, &ClientWindow::sig_dragFinished, this, &Tab::sig_dragFinished);
    connect(client_window_, &ClientWindow::sig_fullscreenRequested, this, &Tab::sig_fullscreenRequested);
    connect(client_window_, &ClientWindow::sig_minimizeRequested, this, &Tab::sig_minimizeRequested);
    connect(client_window_, &ClientWindow::sig_showRequested, this, &Tab::sig_showRequested);
}

//--------------------------------------------------------------------------------------------------
SessionTab::~SessionTab()
{
    closing_ = true;

    if (client_window_)
        client_window_->close();
}

//--------------------------------------------------------------------------------------------------
ClientWindow* SessionTab::clientWindow() const
{
    return client_window_;
}

//--------------------------------------------------------------------------------------------------
bool SessionTab::isDetachable() const
{
    return true;
}

//--------------------------------------------------------------------------------------------------
bool SessionTab::isDetached() const
{
    return client_window_ && client_window_->isWindow();
}

//--------------------------------------------------------------------------------------------------
void SessionTab::detachToWindow()
{
    if (!client_window_ || isDetached())
        return;

    layout()->removeWidget(client_window_);
    client_window_->setParent(nullptr, Qt::Window);
    client_window_->show();
    client_window_->setSessionPaused(false);
}

//--------------------------------------------------------------------------------------------------
void SessionTab::attachToTab()
{
    if (!client_window_ || !isDetached())
        return;

    client_window_->setParent(this);
    layout()->addWidget(client_window_);
    client_window_->show();
}

//--------------------------------------------------------------------------------------------------
QWidget* SessionTab::detachedWindow() const
{
    return isDetached() ? client_window_.get() : nullptr;
}

//--------------------------------------------------------------------------------------------------
QByteArray SessionTab::saveState()
{
    return QByteArray();
}

//--------------------------------------------------------------------------------------------------
void SessionTab::restoreState(const QByteArray& /* state */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SessionTab::activate(QStatusBar* /* statusbar */)
{
    if (client_window_)
        client_window_->setSessionPaused(false);
}

//--------------------------------------------------------------------------------------------------
void SessionTab::deactivate(QStatusBar* /* statusbar */)
{
    if (client_window_)
        client_window_->setSessionPaused(true);
}

//--------------------------------------------------------------------------------------------------
bool SessionTab::hasStatusBar() const
{
    return false;
}

//--------------------------------------------------------------------------------------------------
bool SessionTab::eventFilter(QObject* object, QEvent* event)
{
    if (object == client_window_ && event->type() == QEvent::Close && !closing_)
        emit sig_closeRequested();

    return Tab::eventFilter(object, event);
}
