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

#ifndef CLIENT_UI_TAB_H
#define CLIENT_UI_TAB_H

#include <QAction>
#include <QList>
#include <QPair>
#include <QWidget>

class QStatusBar;

class Tab : public QWidget
{
    Q_OBJECT

public:
    enum class Type { HOSTS, SESSION };
    enum class ActionRole { FILE, EDIT, VIEW, SESSION_TYPE, HELP };

    // Property name. If set on a QAction to true, the action will be added only to the menu and
    // skipped on the toolbar (e.g. checkable items without an icon).
    static inline constexpr const char* kMenuOnlyProperty = "aspiaMenuOnly";

    explicit Tab(Type type, const QString& object_name, QWidget* parent = nullptr);
    ~Tab() override;

    Type tabType() const;
    bool isClosable() const;

    virtual bool isDetachable() const;
    virtual bool isDetached() const;
    virtual QByteArray saveState() = 0;
    virtual void restoreState(const QByteArray& state) = 0;
    virtual void activate(QStatusBar* statusbar) = 0;
    virtual void deactivate(QStatusBar* statusbar) = 0;
    virtual bool hasSearchField() const;
    virtual bool hasStatusBar() const;
    virtual void onSearchTextChanged(const QString& text);

    using ActionGroupEntry = QPair<ActionRole, QList<QAction*>>;
    const QList<ActionGroupEntry>& actionGroups() const;

signals:
    void sig_titleChanged(const QString& title);
    void sig_closeRequested();

    // Emitted while the user drags a detached tab as a top-level window with the left mouse
    // button held. The owner uses global_pos to update visual hints (e.g. previewing the drop
    // target in the main tab bar). Detachable subclasses are responsible for emitting these.
    void sig_dragMove(const QPoint& global_pos);
    void sig_dragFinished(const QPoint& global_release_pos);

protected:
    void addActions(ActionRole group, const QList<QAction*>& actions);

private:
    Type type_;
    QList<ActionGroupEntry> action_groups_;

    Q_DISABLE_COPY_MOVE(Tab)
};

#endif // CLIENT_UI_TAB_H
