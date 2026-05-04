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

#include "client/database.h"

#include "base/crypto/data_cryptor.h"
#include "base/logging.h"
#include "base/files/base_paths.h"
#include "build/build_config.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

const char kConnectionName[] = "client";

constexpr auto kSettingDisplayName    = "display_name";
constexpr auto kSettingCheckUpdates   = "check_updates";
constexpr auto kSettingUpdateServer   = "update_server";
constexpr auto kSettingSalt           = "master_password_salt";
constexpr auto kSettingVerifier       = "master_password_verifier";
constexpr auto kSettingVersion        = "master_password_version";

//--------------------------------------------------------------------------------------------------
ComputerConfig readComputer(const QSqlQuery& query)
{
    DataCryptor& cryptor = DataCryptor::instance();

    ComputerConfig computer;
    computer.id = query.value(0).toLongLong();
    computer.group_id = query.value(1).toLongLong();
    computer.router_id = query.value(2).toLongLong();
    computer.name = query.value(3).toString();
    computer.comment = query.value(4).toString();
    computer.address = query.value(5).toString();
    computer.username = query.value(6).toString();
    computer.password = QString::fromUtf8(
        cryptor.decrypt(query.value(7).toByteArray()).value_or(QByteArray()));
    computer.create_time = query.value(8).toLongLong();
    computer.modify_time = query.value(9).toLongLong();
    computer.connect_time = query.value(10).toLongLong();
    computer.data = query.value(11).toByteArray();

    return computer;
}

//--------------------------------------------------------------------------------------------------
GroupConfig readGroup(const QSqlQuery& query)
{
    GroupConfig group;
    group.id = query.value(0).toLongLong();
    group.parent_id = query.value(1).toLongLong();
    group.name = query.value(2).toString();
    group.comment = query.value(3).toString();
    return group;
}

//--------------------------------------------------------------------------------------------------
RouterConfig readRouter(const QSqlQuery& query)
{
    DataCryptor& cryptor = DataCryptor::instance();

    RouterConfig router;
    router.router_id = query.value(0).toLongLong();
    router.display_name = query.value(1).toString();
    router.address = query.value(2).toString();
    router.session_type = static_cast<proto::router::SessionType>(query.value(3).toUInt());
    router.username = query.value(4).toString();
    router.password = QString::fromUtf8(
        cryptor.decrypt(query.value(5).toByteArray()).value_or(QByteArray()));

    return router;
}

//--------------------------------------------------------------------------------------------------
bool createTables(QSqlDatabase& db)
{
    QSqlQuery query(db);

    if (!query.exec("CREATE TABLE IF NOT EXISTS \"groups\" ("
                    "\"id\" INTEGER UNIQUE,"
                    "\"parent_id\" INTEGER NOT NULL DEFAULT 0,"
                    "\"name\" TEXT NOT NULL DEFAULT '',"
                    "\"comment\" TEXT NOT NULL DEFAULT '',"
                    "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create groups table:" << query.lastError();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS \"computers\" ("
                    "\"id\" INTEGER UNIQUE,"
                    "\"group_id\" INTEGER NOT NULL DEFAULT 0,"
                    "\"router_id\" INTEGER NOT NULL DEFAULT 0,"
                    "\"name\" TEXT NOT NULL DEFAULT '',"
                    "\"comment\" TEXT DEFAULT '',"
                    "\"address\" TEXT NOT NULL DEFAULT '',"
                    "\"username\" TEXT DEFAULT '',"
                    "\"password\" BLOB DEFAULT X'',"
                    "\"create_time\" INTEGER NOT NULL DEFAULT 0,"
                    "\"modify_time\" INTEGER NOT NULL DEFAULT 0,"
                    "\"connect_time\" INTEGER NOT NULL DEFAULT 0,"
                    "\"data\" BLOB DEFAULT X'',"
                    "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create computers table:" << query.lastError();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS \"routers\" ("
                    "\"id\" INTEGER UNIQUE,"
                    "\"name\" TEXT NOT NULL DEFAULT '',"
                    "\"address\" TEXT NOT NULL DEFAULT '',"
                    "\"session_type\" INTEGER NOT NULL DEFAULT 0,"
                    "\"username\" TEXT NOT NULL DEFAULT '',"
                    "\"password\" BLOB NOT NULL DEFAULT X'',"
                    "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create routers table:" << query.lastError();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS \"settings\" ("
                    "\"name\" TEXT PRIMARY KEY NOT NULL,"
                    "\"value\" TEXT NOT NULL)"))
    {
        LOG(ERROR) << "Unable to create settings table:" << query.lastError();
        return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
Database& Database::instance()
{
    static thread_local Database database;

    if (!database.valid_)
        database.valid_ = database.openDatabase();

    return database;
}

//--------------------------------------------------------------------------------------------------
// static
QString Database::filePath()
{
    QString dir_path = BasePaths::appUserDataDir();
    if (dir_path.isEmpty())
        return QString();

    return dir_path + "/client.db3";
}

//--------------------------------------------------------------------------------------------------
bool Database::isValid() const
{
    return valid_;
}

//--------------------------------------------------------------------------------------------------
QList<ComputerConfig> Database::computerList(qint64 group_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, group_id, router_id, name, comment, address, username, password, "
                  "create_time, modify_time, connect_time, data "
                  "FROM computers WHERE group_id=?");
    query.addBindValue(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get computer list:" << query.lastError();
        return {};
    }

    QList<ComputerConfig> computers;
    while (query.next())
        computers.append(readComputer(query));

    return computers;
}

//--------------------------------------------------------------------------------------------------
QList<ComputerConfig> Database::allComputers() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, group_id, router_id, name, comment, address, username, password, "
                  "create_time, modify_time, connect_time, data "
                  "FROM computers");

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get computer list:" << query.lastError();
        return {};
    }

    QList<ComputerConfig> computers;
    while (query.next())
        computers.append(readComputer(query));

    return computers;
}

//--------------------------------------------------------------------------------------------------
bool Database::addComputer(ComputerConfig& computer)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (computer.name.isEmpty() || computer.address.isEmpty() || computer.group_id < 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    DataCryptor& cryptor = DataCryptor::instance();

    const qint64 current_time = QDateTime::currentSecsSinceEpoch();
    computer.create_time = current_time;
    computer.modify_time = current_time;
    computer.connect_time = 0;

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("INSERT INTO computers (id, group_id, router_id, name, comment, address, username, password, "
                  "create_time, modify_time, connect_time, data) "
                  "VALUES (NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(computer.group_id);
    query.addBindValue(computer.router_id);
    query.addBindValue(computer.name);
    query.addBindValue(computer.comment);
    query.addBindValue(computer.address);
    query.addBindValue(computer.username);
    query.addBindValue(cryptor.encrypt(computer.password.toUtf8()).value_or(QByteArray()));
    query.addBindValue(computer.create_time);
    query.addBindValue(computer.modify_time);
    query.addBindValue(computer.connect_time);
    query.addBindValue(computer.data);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    computer.id = query.lastInsertId().toLongLong();
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyComputer(ComputerConfig& computer)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    DataCryptor& cryptor = DataCryptor::instance();

    computer.modify_time = QDateTime::currentSecsSinceEpoch();

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("UPDATE computers SET group_id=?, router_id=?, name=?, comment=?, address=?, username=?, "
                  "password=?, modify_time=?, data=? WHERE id=?");
    query.addBindValue(computer.group_id);
    query.addBindValue(computer.router_id);
    query.addBindValue(computer.name);
    query.addBindValue(computer.comment);
    query.addBindValue(computer.address);
    query.addBindValue(computer.username);
    query.addBindValue(cryptor.encrypt(computer.password.toUtf8()).value_or(QByteArray()));
    query.addBindValue(computer.modify_time);
    query.addBindValue(computer.data);
    query.addBindValue(computer.id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::removeComputer(qint64 computer_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("DELETE FROM computers WHERE id=?");
    query.addBindValue(computer_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::setConnectTime(qint64 computer_id, qint64 connect_time)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("UPDATE computers SET connect_time=? WHERE id=?");
    query.addBindValue(connect_time);
    query.addBindValue(computer_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
std::optional<ComputerConfig> Database::findComputer(qint64 computer_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return std::nullopt;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, group_id, router_id, name, comment, address, username, password, "
                  "create_time, modify_time, connect_time, data "
                  "FROM computers WHERE id=?");
    query.addBindValue(computer_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return readComputer(query);
}

//--------------------------------------------------------------------------------------------------
QList<ComputerConfig> Database::searchComputers(const QString& query_text) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, group_id, router_id, name, comment, address, username, password, "
                  "create_time, modify_time, connect_time, data FROM computers "
                  "WHERE name LIKE ? OR address LIKE ? OR comment LIKE ?");

    QString pattern = QString("%%1%").arg(query_text);
    query.addBindValue(pattern);
    query.addBindValue(pattern);
    query.addBindValue(pattern);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute search query:" << query.lastError();
        return {};
    }

    QList<ComputerConfig> computers;
    while (query.next())
        computers.append(readComputer(query));

    return computers;
}

//--------------------------------------------------------------------------------------------------
QList<GroupConfig> Database::groupList(qint64 parent_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, parent_id, name, comment FROM groups WHERE parent_id=?");
    query.addBindValue(parent_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get group list:" << query.lastError();
        return {};
    }

    QList<GroupConfig> groups;
    while (query.next())
        groups.append(readGroup(query));

    return groups;
}

//--------------------------------------------------------------------------------------------------
QList<GroupConfig> Database::allGroups() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    if (!query.exec("SELECT id, parent_id, name, comment FROM groups"))
    {
        LOG(ERROR) << "Unable to get all groups:" << query.lastError();
        return {};
    }

    QList<GroupConfig> groups;
    while (query.next())
        groups.append(readGroup(query));

    return groups;
}

//--------------------------------------------------------------------------------------------------
bool Database::addGroup(GroupConfig& group)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("INSERT INTO groups (id, parent_id, name, comment) "
                  "VALUES (NULL, ?, ?, ?)");
    query.addBindValue(group.parent_id);
    query.addBindValue(group.name);
    query.addBindValue(group.comment);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    group.id = query.lastInsertId().toLongLong();
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyGroup(const GroupConfig& group)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("UPDATE groups SET parent_id=?, name=?, comment=? WHERE id=?");
    query.addBindValue(group.parent_id);
    query.addBindValue(group.name);
    query.addBindValue(group.comment);
    query.addBindValue(group.id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::moveGroup(qint64 group_id, qint64 new_parent_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("UPDATE groups SET parent_id=? WHERE id=?");
    query.addBindValue(new_parent_id);
    query.addBindValue(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::removeGroup(qint64 group_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("DELETE FROM groups WHERE id=?");
    query.addBindValue(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
std::optional<GroupConfig> Database::findGroup(qint64 group_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return std::nullopt;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, parent_id, name, comment FROM groups WHERE id=?");
    query.addBindValue(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return readGroup(query);
}

//--------------------------------------------------------------------------------------------------
QList<RouterConfig> Database::routerList() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    if (!query.exec("SELECT id, name, address, session_type, username, password FROM routers"))
    {
        LOG(ERROR) << "Unable to get router list:" << query.lastError();
        return {};
    }

    QList<RouterConfig> routers;
    while (query.next())
        routers.append(readRouter(query));

    return routers;
}

//--------------------------------------------------------------------------------------------------
bool Database::addRouter(RouterConfig& router)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (router.address.isEmpty() || router.username.isEmpty())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    DataCryptor& cryptor = DataCryptor::instance();

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("INSERT INTO routers (id, name, address, session_type, username, password) "
                  "VALUES (NULL, ?, ?, ?, ?, ?)");
    query.addBindValue(router.display_name);
    query.addBindValue(router.address);
    query.addBindValue(static_cast<quint32>(router.session_type));
    query.addBindValue(router.username);
    query.addBindValue(cryptor.encrypt(router.password.toUtf8()).value_or(QByteArray()));

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    router.router_id = query.lastInsertId().toLongLong();
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyRouter(const RouterConfig& router)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    DataCryptor& cryptor = DataCryptor::instance();

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("UPDATE routers SET name=?, address=?, session_type=?, username=?, password=? "
                  "WHERE id=?");
    query.addBindValue(router.display_name);
    query.addBindValue(router.address);
    query.addBindValue(static_cast<quint32>(router.session_type));
    query.addBindValue(router.username);
    query.addBindValue(cryptor.encrypt(router.password.toUtf8()).value_or(QByteArray()));
    query.addBindValue(router.router_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::removeRouter(qint64 router_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("DELETE FROM routers WHERE id=?");
    query.addBindValue(router_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
std::optional<RouterConfig> Database::findRouter(qint64 router_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return std::nullopt;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, name, address, session_type, username, password "
                  "FROM routers WHERE id=?");
    query.addBindValue(router_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return readRouter(query);
}

//--------------------------------------------------------------------------------------------------
QString Database::displayName() const
{
    return readSetting(kSettingDisplayName);
}

//--------------------------------------------------------------------------------------------------
bool Database::setDisplayName(const QString& name)
{
    return writeSetting(kSettingDisplayName, name);
}

//--------------------------------------------------------------------------------------------------
bool Database::isCheckUpdatesEnabled() const
{
    QString value = readSetting(kSettingCheckUpdates);
    if (value.isEmpty())
        return true;
    return value == "1";
}

//--------------------------------------------------------------------------------------------------
bool Database::setCheckUpdatesEnabled(bool enable)
{
    return writeSetting(kSettingCheckUpdates, enable ? "1" : "0");
}

//--------------------------------------------------------------------------------------------------
QString Database::updateServer() const
{
    QString value = readSetting(kSettingUpdateServer);
    if (value.isEmpty())
        value = QString::fromLatin1(DEFAULT_UPDATE_SERVER);
    return value.toLower();
}

//--------------------------------------------------------------------------------------------------
bool Database::setUpdateServer(const QString& server)
{
    return writeSetting(kSettingUpdateServer, server);
}

//--------------------------------------------------------------------------------------------------
bool Database::isMasterPasswordSet() const
{
    return !readSetting(kSettingSalt).isEmpty() && !readSetting(kSettingVerifier).isEmpty();
}

//--------------------------------------------------------------------------------------------------
bool Database::setMasterPassword(const QByteArray& salt, const QByteArray& verifier, quint32 version)
{
    return writeSetting(kSettingSalt, QString::fromLatin1(salt.toBase64())) &&
           writeSetting(kSettingVerifier, QString::fromLatin1(verifier.toBase64())) &&
           writeSetting(kSettingVersion, QString::number(version));
}

//--------------------------------------------------------------------------------------------------
QByteArray Database::masterPasswordSalt() const
{
    return QByteArray::fromBase64(readSetting(kSettingSalt).toLatin1());
}

//--------------------------------------------------------------------------------------------------
QByteArray Database::masterPasswordVerifier() const
{
    return QByteArray::fromBase64(readSetting(kSettingVerifier).toLatin1());
}

//--------------------------------------------------------------------------------------------------
quint32 Database::masterPasswordVersion() const
{
    return readSetting(kSettingVersion).toUInt();
}

//--------------------------------------------------------------------------------------------------
bool Database::openDatabase()
{
    QString dir_path = BasePaths::appUserDataDir();
    if (dir_path.isEmpty())
    {
        LOG(ERROR) << "Invalid directory path";
        return false;
    }

    // Ensure directory exists.
    QFileInfo dir_info(dir_path);
    if (dir_info.exists())
    {
        if (!dir_info.isDir())
        {
            LOG(ERROR) << "Unable to create directory for database. Need to delete file:" << dir_path;
            return false;
        }
    }
    else
    {
        if (!QDir().mkpath(dir_path))
        {
            LOG(ERROR) << "Unable to create directory for database";
            return false;
        }
    }

    QString file_path = filePath();
    if (file_path.isEmpty())
    {
        LOG(ERROR) << "Invalid file path";
        return false;
    }

    LOG(INFO) << (!QFileInfo::exists(file_path) ? "Creating" : "Opening") << "book database:" << file_path;

    QSqlDatabase db = QSqlDatabase::database(kConnectionName, false);
    if (!db.isValid())
    {
        db = QSqlDatabase::addDatabase("QSQLITE", kConnectionName);
        db.setDatabaseName(file_path);
    }

    if (!db.isOpen() && !db.open())
    {
        LOG(ERROR) << "QSqlDatabase::open failed:" << db.lastError();
        return false;
    }

    if (!createTables(db))
    {
        db.close();
        QSqlDatabase::removeDatabase(kConnectionName);
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QString Database::readSetting(const QString& name) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return QString();
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT value FROM settings WHERE name=?");
    query.addBindValue(name);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return QString();
    }

    if (!query.next())
        return QString();

    return query.value(0).toString();
}

//--------------------------------------------------------------------------------------------------
bool Database::writeSetting(const QString& name, const QString& value)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES (?, ?)");
    query.addBindValue(name);
    query.addBindValue(value);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}
