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

#include "base/ipc/ipc_channel_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"

namespace base {

//--------------------------------------------------------------------------------------------------
IpcChannelProxy::IpcChannelProxy(std::shared_ptr<TaskRunner> task_runner, IpcChannel* channel)
    : task_runner_(std::move(task_runner)),
      channel_(channel)
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(task_runner_ && channel_);
}

//--------------------------------------------------------------------------------------------------
IpcChannelProxy::~IpcChannelProxy()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void IpcChannelProxy::send(QByteArray&& buffer)
{
    bool schedule_write;

    {
        std::scoped_lock lock(incoming_queue_lock_);

        schedule_write = incoming_queue_.empty();
        incoming_queue_.emplace(std::move(buffer));
    }

    if (!schedule_write)
        return;

    task_runner_->postTask(std::bind(&IpcChannelProxy::scheduleWrite, shared_from_this()));
}

//--------------------------------------------------------------------------------------------------
void IpcChannelProxy::willDestroyCurrentChannel()
{
    LOG(LS_INFO) << "Ipc channel destroyed";
    channel_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void IpcChannelProxy::scheduleWrite()
{
    if (!channel_)
        return;

    if (!reloadWriteQueue(&channel_->write_queue_))
        return;

    channel_->doWriteSize();
}

//--------------------------------------------------------------------------------------------------
bool IpcChannelProxy::reloadWriteQueue(std::queue<QByteArray>* work_queue)
{
    if (!work_queue->empty())
        return false;

    std::scoped_lock lock(incoming_queue_lock_);

    if (incoming_queue_.empty())
        return false;

    incoming_queue_.swap(*work_queue);
    DCHECK(incoming_queue_.empty());

    return true;
}

} // namespace base
