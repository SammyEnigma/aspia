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

#include "host/service.h"

#include "base/logging.h"
#include "base/win/safe_mode_util.h"
#include "base/win/session_status.h"
#include "host/host_storage.h"
#include "host/service_constants.h"

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#endif // defined(Q_OS_WINDOWS)

namespace host {

namespace {

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
QString powerEventToString(quint32 event)
{
    const char* name;

    switch (event)
    {
        case PBT_APMPOWERSTATUSCHANGE:
            name = "PBT_APMPOWERSTATUSCHANGE";
            break;

        case PBT_APMRESUMEAUTOMATIC:
            name = "PBT_APMRESUMEAUTOMATIC";
            break;

        case PBT_APMRESUMESUSPEND:
            name = "PBT_APMRESUMESUSPEND";
            break;

        case PBT_APMSUSPEND:
            name = "PBT_APMSUSPEND";
            break;

        case PBT_POWERSETTINGCHANGE:
            name = "PBT_POWERSETTINGCHANGE";
            break;

        default:
            name = "UNKNOWN";
            break;
    }

    return QString("%1 (%2)").arg(name).arg(static_cast<int>(event));
}
#endif // defined(Q_OS_WINDOWS)

} // namespace

//--------------------------------------------------------------------------------------------------
Service::Service(QObject* parent)
    : base::Service(kHostServiceName, parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
Service::~Service()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void Service::onStart()
{
    LOG(INFO) << "Service is started";

#if defined(Q_OS_WINDOWS)
    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
    {
        PLOG(ERROR) << "SetPriorityClass failed";
    }

    HostStorage storage;
    if (storage.isBootToSafeMode())
    {
        storage.setBootToSafeMode(false);

        if (!base::SafeModeUtil::setSafeMode(false))
        {
            LOG(ERROR) << "Failed to turn off boot in safe mode";
        }
        else
        {
            LOG(INFO) << "Safe mode is disabled";
        }

        if (!base::SafeModeUtil::setSafeModeService(kHostServiceName, false))
        {
            LOG(ERROR) << "Failed to remove service from boot in Safe Mode";
        }
        else
        {
            LOG(INFO) << "Service removed from safe mode loading";
        }
    }
#endif // defined(Q_OS_WINDOWS)

    server_ = new Server(this);
    server_->start();
}

//--------------------------------------------------------------------------------------------------
void Service::onStop()
{
    LOG(INFO) << "Service stopping...";
    delete server_;
    LOG(INFO) << "Service is stopped";
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void Service::onSessionEvent(base::SessionStatus status, base::SessionId session_id)
{
    LOG(INFO) << "Session event detected (status:" << base::sessionStatusToString(status)
              << ", session_id:" << session_id << ")";

    if (server_)
    {
        server_->setSessionEvent(status, session_id);
    }
    else
    {
        LOG(ERROR) << "No server instance";
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onPowerEvent(quint32 event)
{
    LOG(INFO) << "Power event detected:" << powerEventToString(event);

    if (server_)
    {
        server_->setPowerEvent(event);
    }
    else
    {
        LOG(ERROR) << "No server instance";
    }
}
#endif // defined(Q_OS_WINDOWS)

} // namespace host
