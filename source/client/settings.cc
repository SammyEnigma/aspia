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

#include "client/settings.h"

#include <QLocale>
#include <QVersionNumber>

#include <mutex>

#include "base/logging.h"
#include "base/xml_settings.h"
#include "build/version.h"
#include "client/config_factory.h"

namespace {

const QString kVersionParam = "version";
const QString kLocaleParam = "locale";
const QString kThemeParam = "theme";
const QString kSessionTypeParam = "session_type";
const QString kDesktopConfigParam = "desktop_config";
const QString kOneTimePasswordCheckedParam = "one_time_password_checked";
const QString kRouterManagerStateParam = "router_manager_state";
const QString kWindowGeometryParam = "window_geometry";
const QString kWindowStateParam = "window_state";
const QString kLargeIconsParam = "large_icons";
const QString kToolbarParam = "toolbar";
const QString kStatusbarParam = "statusbar";
const QString kOnlineCheckParam = "online_check";
const QString kTabStateParam = "tab_state";

} // namespace

//--------------------------------------------------------------------------------------------------
Settings::Settings()
    : settings_(XmlSettings::format(), QSettings::UserScope, "aspia", "client")
{
    static std::once_flag version_check_flag;
    std::call_once(version_check_flag, [this]()
    {
        QVersionNumber stored = QVersionNumber::fromString(settings_.value(kVersionParam).toString());
        QVersionNumber current = QVersionNumber::fromString(ASPIA_VERSION_SHORT_STRING);

        if (stored < current)
        {
            LOG(INFO) << "Settings version changed from" << stored.toString()
                      << "to" << current.toString();
            settings_.clear();
        }
    });
}

//--------------------------------------------------------------------------------------------------
Settings::~Settings()
{
    settings_.setValue(kVersionParam, QString::fromLatin1(ASPIA_VERSION_SHORT_STRING));
}

//--------------------------------------------------------------------------------------------------
QString Settings::locale() const
{
    return settings_.value(kLocaleParam, QLocale::system().bcp47Name()).toString();
}

//--------------------------------------------------------------------------------------------------
void Settings::setLocale(const QString& locale)
{
    settings_.setValue(kLocaleParam, locale);
}

//--------------------------------------------------------------------------------------------------
QString Settings::theme() const
{
    return settings_.value(kThemeParam, "auto").toString();
}

//--------------------------------------------------------------------------------------------------
void Settings::setTheme(const QString& theme)
{
    settings_.setValue(kThemeParam, theme);
}

//--------------------------------------------------------------------------------------------------
proto::peer::SessionType Settings::sessionType() const
{
    return static_cast<proto::peer::SessionType>(
        settings_.value(kSessionTypeParam, proto::peer::SESSION_TYPE_DESKTOP).toUInt());
}

//--------------------------------------------------------------------------------------------------
void Settings::setSessionType(proto::peer::SessionType session_type)
{
    settings_.setValue(kSessionTypeParam, static_cast<quint32>(session_type));
}

//--------------------------------------------------------------------------------------------------
proto::control::Config Settings::desktopConfig() const
{
    QByteArray buffer = settings_.value(kDesktopConfigParam).toByteArray();
    if (!buffer.isEmpty())
    {
        proto::control::Config config;
        if (config.ParseFromArray(buffer.data(), buffer.size()))
            return config;
    }

    return ConfigFactory::defaultDesktopConfig();
}

//--------------------------------------------------------------------------------------------------
void Settings::setDesktopConfig(const proto::control::Config& config)
{
    QByteArray buffer;
    buffer.resize(static_cast<int>(config.ByteSizeLong()));

    config.SerializeWithCachedSizesToArray(reinterpret_cast<quint8*>(buffer.data()));
    settings_.setValue(kDesktopConfigParam, buffer);
}

//--------------------------------------------------------------------------------------------------
bool Settings::isOneTimePasswordChecked() const
{
    return settings_.value(kOneTimePasswordCheckedParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setOneTimePasswordChecked(bool check)
{
    settings_.setValue(kOneTimePasswordCheckedParam, check);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::routerManagerState() const
{
    return settings_.value(kRouterManagerStateParam, false).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setRouterManagerState(const QByteArray& state)
{
    settings_.setValue(kRouterManagerStateParam, state);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::windowGeometry() const
{
    return settings_.value(kWindowGeometryParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setWindowGeometry(const QByteArray& geometry)
{
    settings_.setValue(kWindowGeometryParam, geometry);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::windowState() const
{
    return settings_.value(kWindowStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setWindowState(const QByteArray& state)
{
    settings_.setValue(kWindowStateParam, state);
}

//--------------------------------------------------------------------------------------------------
bool Settings::largeIcons() const
{
    return settings_.value(kLargeIconsParam).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setLargeIcons(bool enable)
{
    settings_.setValue(kLargeIconsParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool Settings::isToolBarEnabled() const
{
    return settings_.value(kToolbarParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setToolBarEnabled(bool enable)
{
    settings_.setValue(kToolbarParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool Settings::isStatusBarEnabled() const
{
    return settings_.value(kStatusbarParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setStatusBarEnabled(bool enable)
{
    settings_.setValue(kStatusbarParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool Settings::isOnlineCheckEnabled() const
{
    return settings_.value(kOnlineCheckParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setOnlineCheckEnabled(bool enable)
{
    settings_.setValue(kOnlineCheckParam, enable);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::tabState(const QString& name) const
{
    return settings_.value(kTabStateParam + "/" + name).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setTabState(const QString& name, const QByteArray& state)
{
    settings_.setValue(kTabStateParam + "/" + name, state);
}
