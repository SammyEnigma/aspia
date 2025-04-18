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

#ifndef BASE_CRYPTO_DATA_CRYPTOR_CHACHA20_POLY1305_H
#define BASE_CRYPTO_DATA_CRYPTOR_CHACHA20_POLY1305_H

#include "base/macros_magic.h"
#include "base/crypto/data_cryptor.h"

namespace base {

class DataCryptorChaCha20Poly1305 final : public DataCryptor
{
public:
    explicit DataCryptorChaCha20Poly1305(std::string_view key);
    ~DataCryptorChaCha20Poly1305() final;

    bool encrypt(std::string_view in, std::string* out) final;
    bool decrypt(std::string_view in, std::string* out) final;

private:
    std::string key_;

    DISALLOW_COPY_AND_ASSIGN(DataCryptorChaCha20Poly1305);
};

} // namespace base

#endif // BASE_CRYPTO_DATA_CRYPTOR_CHACHA20_POLY1305_H
