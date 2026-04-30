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

#include "client/master_password.h"

#include "base/logging.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "client/database.h"

namespace {

const char kSaltPropertyName[] = "master_password_salt";
const char kVerifierPropertyName[] = "master_password_verifier";
const char kVersionPropertyName[] = "master_password_version";
const quint32 kCurrentVersion = 1;
const int kSaltSize = 32;
const int kVerifierPayloadSize = 32;

//--------------------------------------------------------------------------------------------------
QByteArray deriveKey(const QString& password, const QByteArray& salt)
{
    return base::PasswordHash::hash(base::PasswordHash::ARGON2ID, password, salt);
}

//--------------------------------------------------------------------------------------------------
std::optional<QByteArray> makeVerifier(const QByteArray& key)
{
    return base::DataCryptor(key).encrypt(base::Random::byteArray(kVerifierPayloadSize));
}

//--------------------------------------------------------------------------------------------------
bool checkVerifier(const QByteArray& key, const QByteArray& verifier)
{
    // ChaCha20-Poly1305 is an AEAD cipher: successful decryption (i.e. valid auth tag)
    // is itself the proof that the key is correct. The plaintext content does not matter.
    return base::DataCryptor(key).decrypt(verifier).has_value();
}

//--------------------------------------------------------------------------------------------------
bool changeKeyAndReencrypt(const QByteArray& new_key, const QByteArray& new_salt, const QByteArray& new_verifier)
{
    Database& db = Database::instance();
    if (!db.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    base::DataCryptor& cryptor = base::DataCryptor::instance();
    QByteArray old_key = cryptor.key();

    // Read decrypted data with the current key.
    QList<ComputerConfig> computers = db.allComputers();
    QList<RouterConfig> routers = db.routerList();

    // Switch the cryptor to the new key so writes are encrypted with it.
    cryptor.setKey(new_key);

    for (ComputerConfig computer : std::as_const(computers))
    {
        if (!db.modifyComputer(computer))
        {
            LOG(ERROR) << "Unable to re-encrypt computer:" << computer.id;
            cryptor.setKey(old_key);
            return false;
        }
    }

    for (const RouterConfig& router : std::as_const(routers))
    {
        if (!db.modifyRouter(router))
        {
            LOG(ERROR) << "Unable to re-encrypt router:" << router.router_id;
            cryptor.setKey(old_key);
            return false;
        }
    }

    bool ok;
    if (new_salt.isEmpty() && new_verifier.isEmpty())
    {
        ok = db.removeProperty(kSaltPropertyName) &&
             db.removeProperty(kVerifierPropertyName) &&
             db.removeProperty(kVersionPropertyName);
    }
    else
    {
        ok = db.setProperty(kSaltPropertyName, new_salt) &&
             db.setProperty(kVerifierPropertyName, new_verifier) &&
             db.setProperty(kVersionPropertyName, kCurrentVersion);
    }

    if (!ok)
    {
        cryptor.setKey(old_key);
        return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool MasterPassword::isSafePassword(const QString& password)
{
    qsizetype length = password.length();

    if (length < kSafePasswordLength)
        return false;

    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;

    for (qsizetype i = 0; i < length; ++i)
    {
        QChar character = password.at(i);

        if (character.isUpper())
            has_upper = true;

        if (character.isLower())
            has_lower = true;

        if (character.isDigit())
            has_digit = true;
    }

    return has_upper && has_lower && has_digit;
}

//--------------------------------------------------------------------------------------------------
// static
bool MasterPassword::isSet()
{
    Database& db = Database::instance();
    if (!db.isValid())
        return false;

    return db.hasProperty(kSaltPropertyName) &&
           db.hasProperty(kVerifierPropertyName) &&
           db.hasProperty(kVersionPropertyName);
}

//--------------------------------------------------------------------------------------------------
// static
bool MasterPassword::unlock(const QString& password)
{
    Database& db = Database::instance();
    if (!db.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QVariant salt_value = db.property(kSaltPropertyName);
    QVariant verifier_value = db.property(kVerifierPropertyName);
    QVariant version_value = db.property(kVersionPropertyName);

    QByteArray salt = salt_value.toByteArray();
    QByteArray verifier = verifier_value.toByteArray();

    if (salt.isEmpty() || verifier.isEmpty())
    {
        LOG(ERROR) << "Master password is not set";
        return false;
    }

    quint32 version = version_value.toUInt();
    if (version != kCurrentVersion)
    {
        LOG(ERROR) << "Unsupported master password version:" << version;
        return false;
    }

    QByteArray key = deriveKey(password, salt);
    if (!checkVerifier(key, verifier))
    {
        LOG(INFO) << "Invalid master password";
        return false;
    }

    base::DataCryptor::instance().setKey(key);
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool MasterPassword::setNew(const QString& new_password)
{
    if (new_password.isEmpty())
    {
        LOG(ERROR) << "Empty password";
        return false;
    }

    if (isSet())
    {
        LOG(ERROR) << "Master password is already set";
        return false;
    }

    QByteArray salt = base::Random::byteArray(kSaltSize);
    CHECK(!salt.isEmpty());

    QByteArray new_key = deriveKey(new_password, salt);
    std::optional<QByteArray> verifier = makeVerifier(new_key);
    if (!verifier.has_value())
        return false;

    return changeKeyAndReencrypt(new_key, salt, *verifier);
}

//--------------------------------------------------------------------------------------------------
// static
bool MasterPassword::change(const QString& current_password, const QString& new_password)
{
    if (new_password.isEmpty())
    {
        LOG(ERROR) << "Empty password";
        return false;
    }

    if (!unlock(current_password))
        return false;

    QByteArray salt = base::Random::byteArray(kSaltSize);
    CHECK(!salt.isEmpty());

    QByteArray new_key = deriveKey(new_password, salt);
    std::optional<QByteArray> verifier = makeVerifier(new_key);
    if (!verifier.has_value())
        return false;

    return changeKeyAndReencrypt(new_key, salt, *verifier);
}

//--------------------------------------------------------------------------------------------------
// static
bool MasterPassword::clear(const QString& current_password)
{
    if (!unlock(current_password))
        return false;

    return changeKeyAndReencrypt(QByteArray(), QByteArray(), QByteArray());
}
