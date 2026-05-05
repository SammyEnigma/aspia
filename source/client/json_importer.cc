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

#include "client/json_importer.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <memory>
#include <optional>

#include "base/logging.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/secure_memory.h"
#include "client/database.h"
#include "client/ui/unlock_dialog.h"
#include "common/ui/msg_box.h"

namespace {

constexpr int kSupportedVersion = 1;
constexpr int kMaxNameLength = 64;
constexpr int kMaxCommentLength = 2048;

//--------------------------------------------------------------------------------------------------
struct ImportCounters
{
    int routers = 0;
    int routers_skipped = 0;
    int groups = 0;
    int computers = 0;
    int computers_skipped = 0;
};

//--------------------------------------------------------------------------------------------------
QString sanitizedName(const QString& name)
{
    return name.left(kMaxNameLength);
}

//--------------------------------------------------------------------------------------------------
QString sanitizedComment(const QString& comment)
{
    return comment.left(kMaxCommentLength);
}

//--------------------------------------------------------------------------------------------------
QString decryptFromHex(DataCryptor& cryptor, const QJsonValue& value)
{
    if (!value.isString())
        return QString();

    QByteArray encrypted = QByteArray::fromHex(value.toString().toLatin1());
    if (encrypted.isEmpty())
        return QString();

    std::optional<QByteArray> decrypted = cryptor.decrypt(encrypted);
    if (!decrypted.has_value())
        return QString();

    QString result = QString::fromUtf8(*decrypted);
    memZero(&*decrypted);
    return result;
}

//--------------------------------------------------------------------------------------------------
void readCredentials(const QJsonObject& object, DataCryptor* cryptor, QString* username, QString* password)
{
    if (!cryptor)
    {
        username->clear();
        password->clear();
        return;
    }

    *username = decryptFromHex(*cryptor, object.value("username"));
    *password = decryptFromHex(*cryptor, object.value("password"));
}

//--------------------------------------------------------------------------------------------------
qint64 importRouter(const QJsonObject& router_object, DataCryptor* cryptor, ImportCounters* counters)
{
    QString display_name = sanitizedName(router_object.value("display_name").toString());
    QString address = router_object.value("address").toString();
    int session_type = router_object.value("session_type").toInt(proto::router::SESSION_TYPE_CLIENT);

    if (address.isEmpty())
    {
        ++counters->routers_skipped;
        return 0;
    }

    QString username;
    QString password;
    readCredentials(router_object, cryptor, &username, &password);

    if (username.isEmpty() || password.isEmpty())
    {
        ++counters->routers_skipped;
        return 0;
    }

    Database& db = Database::instance();
    const QList<RouterConfig> existing = db.routerList();
    for (const RouterConfig& router : std::as_const(existing))
    {
        if (router.address == address && router.username == username)
            return router.router_id;
    }

    RouterConfig config;
    config.display_name = display_name.isEmpty() ? address : display_name;
    config.address = address;
    config.username = username;
    config.password = password;
    config.session_type = static_cast<proto::router::SessionType>(session_type);

    if (!db.addRouter(config))
    {
        LOG(ERROR) << "Unable to add router during import";
        ++counters->routers_skipped;
        return 0;
    }

    ++counters->routers;
    return config.router_id;
}

//--------------------------------------------------------------------------------------------------
bool readGroupIds(const QJsonObject& object, qint64* id, qint64* parent_id)
{
    if (!object.contains("id"))
        return false;

    *id = object.value("id").toInteger();
    *parent_id = object.value("parent_id").toInteger(0);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool importGroups(const QJsonArray& groups_array, QHash<qint64, qint64>* group_id_map, ImportCounters* counters)
{
    Database& db = Database::instance();

    QHash<qint64, QList<QJsonObject>> children;
    for (const QJsonValue& value : std::as_const(groups_array))
    {
        if (!value.isObject())
            continue;

        QJsonObject group_object = value.toObject();
        qint64 id = 0;
        qint64 parent_id = 0;
        if (!readGroupIds(group_object, &id, &parent_id))
            continue;

        children[parent_id].append(group_object);
    }

    group_id_map->insert(0, 0);

    // BFS from root so a child group is never imported before its parent.
    QList<qint64> queue;
    queue.append(0);

    while (!queue.isEmpty())
    {
        qint64 current_old_parent = queue.takeFirst();
        qint64 current_new_parent = group_id_map->value(current_old_parent, 0);

        const QList<QJsonObject>& list = children.value(current_old_parent);
        for (const QJsonObject& group_object : std::as_const(list))
        {
            qint64 old_id = 0;
            qint64 old_parent_id = 0;
            if (!readGroupIds(group_object, &old_id, &old_parent_id))
                continue;

            QString name = sanitizedName(group_object.value("name").toString());
            if (name.isEmpty())
                continue;

            GroupConfig group_config;
            group_config.parent_id = current_new_parent;
            group_config.name = name;
            group_config.comment = sanitizedComment(group_object.value("comment").toString());

            if (!db.addGroup(group_config))
            {
                LOG(ERROR) << "Unable to add group during import";
                continue;
            }

            group_id_map->insert(old_id, group_config.id);
            ++counters->groups;
            queue.append(old_id);
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void importComputers(const QJsonArray& computers_array,
                     const QHash<qint64, qint64>& group_id_map,
                     const QHash<qint64, qint64>& router_id_map,
                     DataCryptor* cryptor,
                     ImportCounters* counters)
{
    Database& db = Database::instance();

    for (const QJsonValue& value : std::as_const(computers_array))
    {
        if (!value.isObject())
            continue;

        QJsonObject object = value.toObject();

        QString name = sanitizedName(object.value("name").toString());
        if (name.isEmpty())
        {
            ++counters->computers_skipped;
            continue;
        }

        QString address = object.value("address").toString();
        if (address.isEmpty())
        {
            ++counters->computers_skipped;
            continue;
        }

        QString username;
        QString password;
        readCredentials(object, cryptor, &username, &password);

        qint64 old_group_id = object.value("group_id").toInteger(0);
        qint64 old_router_id = object.value("router_id").toInteger(0);

        ComputerConfig config;
        config.group_id = group_id_map.value(old_group_id, 0);
        config.router_id = router_id_map.value(old_router_id, 0);
        config.name = name;
        config.comment = sanitizedComment(object.value("comment").toString());
        config.address = address;
        config.username = username;
        config.password = password;

        if (!db.addComputer(config))
        {
            LOG(ERROR) << "Unable to add computer during import";
            ++counters->computers_skipped;
            continue;
        }

        ++counters->computers;
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool JsonImporter::importFromFile(QWidget* parent, const QString& file_path)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        MsgBox::warning(parent, tr("Unable to open file \"%1\": %2").arg(file_path, file.errorString()));
        return false;
    }

    QByteArray buffer = file.readAll();
    file.close();

    if (buffer.isEmpty())
    {
        MsgBox::warning(parent, tr("Selected file is empty."));
        return false;
    }

    QJsonParseError parse_error;
    QJsonDocument document = QJsonDocument::fromJson(buffer, &parse_error);
    if (document.isNull() || !document.isObject())
    {
        MsgBox::warning(parent, tr("The file is not a valid JSON document: %1").arg(parse_error.errorString()));
        return false;
    }

    QJsonObject root = document.object();

    int version = root.value("version").toInt(0);
    if (version != kSupportedVersion)
    {
        MsgBox::warning(parent, tr("Unsupported file format version: %1").arg(version));
        return false;
    }

    QByteArray salt = QByteArray::fromHex(root.value("salt").toString().toLatin1());
    QByteArray verifier = QByteArray::fromHex(root.value("verifier").toString().toLatin1());
    std::unique_ptr<DataCryptor> cryptor;

    if (!verifier.isEmpty())
    {
        if (salt.isEmpty())
        {
            MsgBox::warning(parent, tr("Encrypted file is missing salt."));
            return false;
        }

        int answer = MsgBox::question(parent,
            tr("The file contains encrypted usernames and passwords. To import them, you need to "
               "enter a password (if no password is entered, they will be imported without them). "
               "Do you want to enter the password?"),
            MsgBox::Yes | MsgBox::No | MsgBox::Cancel);

        if (answer == MsgBox::Cancel)
            return false;

        if (answer == MsgBox::Yes)
        {
            UnlockDialog dialog(parent, file_path, tr("ChaCha20 + Poly1305 (256-bit key)"));
            if (dialog.exec() != QDialog::Accepted)
                return false;

            QByteArray key = PasswordHash::hash(PasswordHash::ARGON2ID, dialog.password(), salt);
            cryptor = std::make_unique<DataCryptor>(key);
            memZero(&key);

            if (!cryptor->decrypt(verifier).has_value())
            {
                MsgBox::warning(parent, tr("Unable to decrypt the file with the specified password."));
                return false;
            }
        }
    }

    Database& db = Database::instance();
    if (!db.isValid())
    {
        MsgBox::warning(parent, tr("Address book database is not available."));
        return false;
    }

    ImportCounters counters;

    QHash<qint64, qint64> router_id_map;
    QJsonArray routers_array = root.value("routers").toArray();
    for (const QJsonValue& value : std::as_const(routers_array))
    {
        if (!value.isObject())
            continue;

        QJsonObject router_object = value.toObject();
        qint64 old_id = router_object.value("id").toInteger(0);
        qint64 new_id = importRouter(router_object, cryptor.get(), &counters);
        if (new_id != 0)
            router_id_map.insert(old_id, new_id);
    }

    QHash<qint64, qint64> group_id_map;
    QJsonArray groups_array = root.value("groups").toArray();
    importGroups(groups_array, &group_id_map, &counters);

    QJsonArray computers_array = root.value("computers").toArray();
    importComputers(computers_array, group_id_map, router_id_map, cryptor.get(), &counters);

    cryptor.reset();

    if (counters.routers == 0 && counters.groups == 0 && counters.computers == 0)
    {
        MsgBox::information(parent, tr("Nothing was imported."));
        return false;
    }

    MsgBox::information(parent,
        tr("Import completed successfully.\n"
           "Routers added: %1\n"
           "Routers skipped: %2\n"
           "Groups added: %3\n"
           "Computers added: %4\n"
           "Computers skipped: %5")
            .arg(counters.routers)
            .arg(counters.routers_skipped)
            .arg(counters.groups)
            .arg(counters.computers)
            .arg(counters.computers_skipped));

    return true;
}
