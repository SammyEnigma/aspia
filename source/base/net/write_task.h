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

#ifndef BASE_NET_WRITE_TASK_H
#define BASE_NET_WRITE_TASK_H

#include <QByteArray>

#include <queue>
#include <vector>

namespace base {

class WriteTask
{
public:
    enum class Type { SERVICE_DATA, USER_DATA };

    enum class Priority : int
    {
        REAL_TIME = 0,
        HIGH      = 1,
        NORMAL    = 2,
        LOW       = 3,
        IDLE      = 4
    };

    WriteTask(Type type, Priority priority, int sequence_num, uint8_t channel_id, QByteArray&& data)
        : type_(type),
          priority_(priority),
          sequence_num_(sequence_num),
          channel_id_(channel_id),
          data_(std::move(data))
    {
        // Nothing
    }

    WriteTask(const WriteTask& other) = default;
    WriteTask& operator=(const WriteTask& other) = default;

    Type type() const { return type_; }
    Priority priority() const { return priority_; }
    int sequenceNum() const { return sequence_num_; }
    uint8_t channelId() const { return channel_id_; }
    const QByteArray& data() const { return data_; }
    QByteArray& data() { return data_; }

private:
    Type type_;
    Priority priority_;
    int sequence_num_;
    uint8_t channel_id_;
    QByteArray data_;
};

struct WriteTaskCompare
{
    // Used to support sorting.
    bool operator()(const WriteTask& first, const WriteTask& second)
    {
        if (first.priority() < second.priority())
            return false;

        if (first.priority() > second.priority())
            return true;

        return (first.sequenceNum() - second.sequenceNum()) > 0;
    }
};

using WriteQueue = std::priority_queue<WriteTask, std::vector<WriteTask>, WriteTaskCompare>;

} // namespace base

#endif // BASE_NET_WRITE_TASK_H
