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

#include "client/ui/tab_bar.h"

#include <QApplication>
#include <QMouseEvent>

namespace {

// Vertical distance the cursor must travel beyond the tab bar bounds while dragging a tab to
// trigger detachment. Horizontal movement keeps the tab in the bar (reorder).
constexpr int kDetachThresholdPx = 30;

} // namespace

//--------------------------------------------------------------------------------------------------
TabBar::TabBar(QWidget* parent)
    : QTabBar(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
TabBar::~TabBar() = default;

//--------------------------------------------------------------------------------------------------
void TabBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        pressed_tab_index_ = tabAt(event->pos());

    QTabBar::mousePressEvent(event);
}

//--------------------------------------------------------------------------------------------------
void TabBar::mouseMoveEvent(QMouseEvent* event)
{
    QTabBar::mouseMoveEvent(event);

    if (!(event->buttons() & Qt::LeftButton) || pressed_tab_index_ < 0)
        return;

    int y = event->position().toPoint().y();
    if (y >= -kDetachThresholdPx && y <= height() + kDetachThresholdPx)
        return;

    int detach_index = pressed_tab_index_;
    pressed_tab_index_ = -1;

    // Cancel QTabBar's internal drag state so the synthetic release leaves it in a clean state.
    QMouseEvent release(QEvent::MouseButtonRelease,
                        event->position(),
                        event->scenePosition(),
                        event->globalPosition(),
                        Qt::LeftButton,
                        Qt::NoButton,
                        event->modifiers());
    QTabBar::mouseReleaseEvent(&release);

    emit sig_tabDetachRequested(detach_index, event->globalPosition().toPoint());
}

//--------------------------------------------------------------------------------------------------
void TabBar::mouseReleaseEvent(QMouseEvent* event)
{
    pressed_tab_index_ = -1;
    QTabBar::mouseReleaseEvent(event);
}
