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

#include "client/ui/session_window.h"

//--------------------------------------------------------------------------------------------------
SessionTab::SessionTab(SessionWindow* session_window, QWidget* parent)
    : ClientTab(Type::SESSION, "session", parent),
      session_window_(session_window)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    if (session_window_)
    {
        session_window_->setParent(this);
        session_window_->installEventFilter(this);
        layout->addWidget(session_window_);
        session_window_->show();
    }
}

//--------------------------------------------------------------------------------------------------
SessionTab::~SessionTab()
{
    closing_ = true;

    if (session_window_)
        session_window_->close();
}

//--------------------------------------------------------------------------------------------------
SessionWindow* SessionTab::sessionWindow() const
{
    return session_window_;
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
void SessionTab::attach(QStatusBar* /* statusbar */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SessionTab::detach(QStatusBar* /* statusbar */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SessionTab::eventFilter(QObject* object, QEvent* event)
{
    if (object == session_window_)
    {
        switch (event->type())
        {
            case QEvent::WindowTitleChange:
                emit sig_titleChanged(session_window_->windowTitle());
                break;

            case QEvent::Close:
                if (!closing_)
                    emit sig_closeRequested();
                break;

            default:
                break;
        }
    }

    return ClientTab::eventFilter(object, event);
}
