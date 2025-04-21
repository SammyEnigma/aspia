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

#include "host/desktop_session_manager.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/task_runner.h"
#include "base/desktop/frame.h"
#include "base/ipc/ipc_channel.h"
#include "host/desktop_session_fake.h"
#include "host/desktop_session_ipc.h"
#include "host/desktop_session_process.h"
#include "host/desktop_session_proxy.h"

#if defined(OS_WIN)
#include "base/win/desktop.h"
#include "base/win/session_info.h"
#include "base/win/window_station.h"
#endif // defined(OS_WIN)

namespace host {

namespace {

const std::chrono::minutes kSessionAttachTimeout { 1 };

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopSessionManager::DesktopSessionManager(
    std::shared_ptr<base::TaskRunner> task_runner, DesktopSession::Delegate* delegate, QObject* parent)
    : QObject(parent),
      task_runner_(task_runner),
      session_proxy_(base::make_local_shared<DesktopSessionProxy>()),
      delegate_(delegate)
{
    LOG(LS_INFO) << "Ctor";

    session_attach_timer_.setSingleShot(true);

    connect(&session_attach_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(LS_ERROR) << "Session attach timeout (session_id=" << session_id_
                      << " timeout=" << kSessionAttachTimeout.count() << "min)";
        onErrorOccurred();
    });
}

//--------------------------------------------------------------------------------------------------
DesktopSessionManager::~DesktopSessionManager()
{
    LOG(LS_INFO) << "Dtor";

    setState(FROM_HERE, State::STOPPING);
    dettachSession(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::attachSession(
    const base::Location& location, base::SessionId session_id)
{
    if (state_ == State::ATTACHED)
    {
        LOG(LS_INFO) << "Already attached. Session ID: " << session_id << " (from="
                     << location.toString() << ")";
        return;
    }

    LOG(LS_INFO) << "Attach session with ID: " << session_id << " current state: "
                 << stateToString(state_) << " (from=" << location.toString() << ")";
    session_id_ = session_id;

    if (state_ == State::STOPPED)
        session_attach_timer_.start(kSessionAttachTimeout);

    setState(FROM_HERE, State::STARTING);

    LOG(LS_INFO) << "Starting desktop session";
    LOG(LS_INFO) << "#####################################################";

#if defined(OS_WIN)
    LOG(LS_INFO) << "# Active console session id: " << WTSGetActiveConsoleSessionId();
    for (const auto& window_station_name : base::WindowStation::windowStationList())
    {
        std::wstring desktops;

        base::WindowStation window_station = base::WindowStation::open(window_station_name.data());
        if (window_station.isValid())
        {
            std::vector<std::wstring> list = base::Desktop::desktopList(window_station.get());

            for (size_t i = 0; i < list.size(); ++i)
            {
                desktops += list[i];
                if ((i + 1) != list.size())
                    desktops += L", ";
            }
        }

        LOG(LS_INFO) << "# " << window_station_name << " (desktops: " << desktops << ")";
    }

    base::win::SessionInfo session_info(session_id);
    if (!session_info.isValid())
    {
        LOG(LS_ERROR) << "Unable to get session info (sid=" << session_id << ")";
        return;
    }

    LOG(LS_INFO) << "# Session info (sid=" << session_id
                 << " username='" << session_info.userName() << "'"
                 << " connect_state=" << base::win::SessionInfo::connectStateToString(session_info.connectState())
                 << " win_station='" << session_info.winStationName() << "'"
                 << " domain='" << session_info.domain() << "'"
                 << " locked=" << session_info.isUserLocked() << ")";
#endif // defined(OS_WIN)

    std::u16string channel_id = base::IpcServer::createUniqueId();

    LOG(LS_INFO) << "Starting IPC server for desktop session (channel_id=" << channel_id << ")";

    server_ = std::make_unique<base::IpcServer>();
    if (!server_->start(channel_id, this))
    {
        LOG(LS_ERROR) << "Failed to start IPC server (channel_id=" << channel_id << ")";

        onErrorOccurred();
        return;
    }

    LOG(LS_INFO) << "Starting desktop session process";

    std::unique_ptr<DesktopSessionProcess> process =
        DesktopSessionProcess::create(session_id, channel_id);
    if (!process)
    {
        LOG(LS_ERROR) << "Failed to create session process (sid=" << session_id
                      << " channel_id=" << channel_id << ")";

        onErrorOccurred();
        return;
    }

    LOG(LS_INFO) << "Desktop session process created (sid=" << session_id
                 << " channel_id=" << channel_id << ")";
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::dettachSession(const base::Location& location)
{
    if (state_ == State::STOPPED || state_ == State::DETACHED)
    {
        LOG(LS_INFO) << "Session already stopped or dettached (" << stateToString(state_) << ")";
        return;
    }

    LOG(LS_INFO) << "Dettach session (sid=" << session_id_ << " state=" << stateToString(state_)
                 << " from=" << location.toString() << ")";

    if (state_ != State::STOPPING)
        setState(FROM_HERE, State::DETACHED);

    session_attach_timer_.stop();
    session_proxy_->stopAndDettach();
    task_runner_->deleteSoon(std::move(session_));

    LOG(LS_INFO) << "Session process is detached (sid=" << session_id_ << ")";

    if (state_ == State::STOPPING)
        return;

    session_attach_timer_.start(kSessionAttachTimeout);

    // The real session process has ended. We create a temporary fake session.
    session_ = std::make_unique<DesktopSessionFake>(task_runner_, this);
    session_proxy_->attachAndStart(session_.get());
}

//--------------------------------------------------------------------------------------------------
base::local_shared_ptr<DesktopSessionProxy> DesktopSessionManager::sessionProxy() const
{
    return session_proxy_;
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::onNewConnection(std::unique_ptr<base::IpcChannel> channel)
{
    LOG(LS_INFO) << "Session process successfully connected (sid=" << session_id_ << ")";

    session_attach_timer_.stop();

    if (server_)
    {
        LOG(LS_INFO) << "IPC server already exists. Stop it (sid=" << session_id_ << ")";
        server_->stop();
        task_runner_->deleteSoon(std::move(server_));
    }

    session_ = std::make_unique<DesktopSessionIpc>(std::move(channel), this);

    setState(FROM_HERE, State::ATTACHED);
    session_proxy_->attachAndStart(session_.get());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::onErrorOccurred()
{
    if (state_ == State::STOPPED || state_ == State::STOPPING)
    {
        LOG(LS_INFO) << "Error skipped (state=" << stateToString(state_) << ")";
        return;
    }

    setState(FROM_HERE, State::STOPPING);
    dettachSession(FROM_HERE);
    setState(FROM_HERE, State::STOPPED);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::onDesktopSessionStarted()
{
    LOG(LS_INFO) << "Desktop session started";
    delegate_->onDesktopSessionStarted();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::onDesktopSessionStopped()
{
    LOG(LS_INFO) << "Desktop session stopped";
    dettachSession(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::onScreenCaptured(
    const base::Frame* frame, const base::MouseCursor* mouse_cursor)
{
    delegate_->onScreenCaptured(frame, mouse_cursor);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::onScreenCaptureError(proto::VideoErrorCode error_code)
{
    delegate_->onScreenCaptureError(error_code);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::onAudioCaptured(const proto::AudioPacket& audio_packet)
{
    delegate_->onAudioCaptured(audio_packet);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::onCursorPositionChanged(const proto::CursorPosition& cursor_position)
{
    delegate_->onCursorPositionChanged(cursor_position);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::onScreenListChanged(const proto::ScreenList& list)
{
    delegate_->onScreenListChanged(list);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::onScreenTypeChanged(const proto::ScreenType& type)
{
    delegate_->onScreenTypeChanged(type);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::onClipboardEvent(const proto::ClipboardEvent& event)
{
    delegate_->onClipboardEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::setState(const base::Location& location, State state)
{
    LOG(LS_INFO) << "State changed from " << stateToString(state_) << " to " << stateToString(state)
                 << " (from=" << location.toString() << ")";
    state_ = state;
}

//--------------------------------------------------------------------------------------------------
// static
const char* DesktopSessionManager::stateToString(State state)
{
    switch (state)
    {
        case State::STOPPED:
            return "STOPPED";
        case State::STARTING:
            return "STARTING";
        case State::STOPPING:
            return "STOPPING";
        case State::DETACHED:
            return "DETACHED";
        case State::ATTACHED:
            return "ATTACHED";
        default:
            return "Unknown";
    }
}

} // namespace host
