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

#include "common/clipboard_monitor.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "common/clipboard_win.h"
#elif defined(OS_LINUX)
#include "common/clipboard_x11.h"
#elif defined(OS_MAC)
#include "common/clipboard_mac.h"
#else
#error Not implemented
#endif

namespace common {

namespace {

#if defined(Q_OS_WIN)
const base::AsioThread::EventDispatcher kEventDispatcher = base::AsioThread::EventDispatcher::QT;
#else
const base::AsioThread::EventDispatcher kEventDispatcher = base::AsioThread::EventDispatcher::ASIO;
#endif

} // namespace

//--------------------------------------------------------------------------------------------------
ClipboardMonitor::ClipboardMonitor()
    : thread_(std::make_unique<base::AsioThread>(kEventDispatcher, this))
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClipboardMonitor::~ClipboardMonitor()
{
    LOG(LS_INFO) << "Dtor";
    thread_->stop();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::start(std::shared_ptr<base::TaskRunner> caller_task_runner,
                             common::Clipboard::Delegate* delegate)
{
    LOG(LS_INFO) << "Starting clipboard monitor";

    caller_task_runner_ = std::move(caller_task_runner);
    delegate_ = delegate;

    DCHECK(caller_task_runner_);
    DCHECK(delegate_);

    thread_->start();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::injectClipboardEvent(const proto::ClipboardEvent& event)
{
    if (!self_task_runner_)
        return;

    if (!self_task_runner_->belongsToCurrentThread())
    {
        self_task_runner_->postTask(
            std::bind(&ClipboardMonitor::injectClipboardEvent, this, event));
        return;
    }

    if (clipboard_)
        clipboard_->injectClipboardEvent(event);
    else
        LOG(LS_ERROR) << "No clipboard instance";
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::clearClipboard()
{
    if (!self_task_runner_)
        return;

    if (!self_task_runner_->belongsToCurrentThread())
    {
        self_task_runner_->postTask(std::bind(&ClipboardMonitor::clearClipboard, this));
        return;
    }

    if (clipboard_)
        clipboard_->clearClipboard();
    else
        LOG(LS_ERROR) << "No clipboard instance";
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::onBeforeThreadRunning()
{
    LOG(LS_INFO) << "Thread starting";

    self_task_runner_ = thread_->taskRunner();
    DCHECK(self_task_runner_);

#if defined(OS_WIN)
    clipboard_ = std::make_unique<common::ClipboardWin>();
#elif defined(OS_LINUX)
    clipboard_ = std::make_unique<common::ClipboardX11>();
#elif defined(OS_MAC)
    clipboard_ = std::make_unique<common::ClipboardMac>();
#else
#error Not implemented
#endif
    clipboard_->start(this);
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::onAfterThreadRunning()
{
    LOG(LS_INFO) << "Thread stopping";
    clipboard_.reset();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::onClipboardEvent(const proto::ClipboardEvent& event)
{
    if (!caller_task_runner_->belongsToCurrentThread())
    {
        caller_task_runner_->postTask(std::bind(&ClipboardMonitor::onClipboardEvent, this, event));
        return;
    }

    if (delegate_)
        delegate_->onClipboardEvent(event);
    else
        LOG(LS_ERROR) << "Invalid delegate";
}

} // namespace common
