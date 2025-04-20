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

#ifndef BASE_AUDIO_AUDIO_CAPTURER_WRAPPER_H
#define BASE_AUDIO_AUDIO_CAPTURER_WRAPPER_H

#include "base/threading/asio_thread.h"
#include "proto/desktop_internal.pb.h"

namespace base {

class AudioCapturer;
class IpcChannelProxy;

class AudioCapturerWrapper final : public AsioThread::Delegate
{
public:
    explicit AudioCapturerWrapper(std::shared_ptr<IpcChannelProxy> channel_proxy);
    ~AudioCapturerWrapper() final;

    void start();

protected:
    // AsioThread::Delegate implementation.
    void onBeforeThreadRunning() final;
    void onAfterThreadRunning() final;

private:
    std::shared_ptr<IpcChannelProxy> channel_proxy_;
    std::unique_ptr<AsioThread> thread_;
    std::unique_ptr<AudioCapturer> capturer_;
    proto::internal::DesktopToService outgoing_message_;

    DISALLOW_COPY_AND_ASSIGN(AudioCapturerWrapper);
};

} // namespace base

#endif // BASE_AUDIO_AUDIO_CAPTURER_WRAPPER_H
