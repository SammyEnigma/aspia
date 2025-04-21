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

#ifndef COMMON_CLIPBOARD_MONITOR_H
#define COMMON_CLIPBOARD_MONITOR_H

#include "base/threading/asio_thread.h"
#include "common/clipboard.h"

namespace common {

class ClipboardMonitor final
    : public QObject,
      public base::AsioThread::Delegate
{
    Q_OBJECT

public:
    explicit ClipboardMonitor(QObject* parent = nullptr);
    ~ClipboardMonitor() final;

    void start();

    void injectClipboardEvent(const proto::ClipboardEvent& event);
    void clearClipboard();

signals:
    void sig_clipboardEvent(const proto::ClipboardEvent& event);
    void sig_injectClipboardEventPrivate(const proto::ClipboardEvent& event);
    void sig_clearClipboardPrivate();

protected:
    // base::AsioThread::Delegate implementation.
    void onBeforeThreadRunning() final;
    void onAfterThreadRunning() final;

private:
    std::unique_ptr<base::AsioThread> thread_;
    std::unique_ptr<common::Clipboard> clipboard_;

    DISALLOW_COPY_AND_ASSIGN(ClipboardMonitor);
};

} // namespace common

#endif // COMMON_CLIPBOARD_MONITOR_H
