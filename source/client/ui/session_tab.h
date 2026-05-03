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

#ifndef CLIENT_UI_SESSION_TAB_H
#define CLIENT_UI_SESSION_TAB_H

#include "base/scoped_qpointer.h"
#include "client/ui/tab.h"

class QStatusBar;
class SessionWindow;

class SessionTab final : public Tab
{
    Q_OBJECT

public:
    explicit SessionTab(SessionWindow* session_window, QWidget* parent = nullptr);
    ~SessionTab() final;

    SessionWindow* sessionWindow() const;

    // Switches the embedded SessionWindow into a top-level window. The session widget is
    // reparented away from this tab and shown. The caller is responsible for positioning and
    // sizing the new window (and calling startSystemMove if a drag is in progress).
    void detachToWindow();

    // Switches the SessionWindow back into the tab as a child widget.
    void attachToTab();

    // Tab implementation.
    bool isDetachable() const final;
    bool isDetached() const final;
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    void activate(QStatusBar* statusbar) final;
    void deactivate(QStatusBar* statusbar) final;

protected:
    // QObject implementation.
    bool eventFilter(QObject* object, QEvent* event) final;

private:
    ScopedQPointer<SessionWindow> session_window_;
    bool closing_ = false;

    Q_DISABLE_COPY_MOVE(SessionTab)
};

#endif // CLIENT_UI_SESSION_TAB_H
