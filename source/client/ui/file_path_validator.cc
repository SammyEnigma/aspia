//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/file_path_validator.h"

#include "host/file_platform_util.h"

namespace aspia {

FilePathValidator::FilePathValidator(QObject* parent)
    : QValidator(parent)
{
    // Nothing
}

FilePathValidator::State FilePathValidator::validate(QString& input, int& pos) const
{
    if (!input.isEmpty())
    {
        const QList<QChar>& invalid_characters = FilePlatformUtil::invalidPathCharacters();

        for (const auto& character : input)
        {
            if (invalid_characters.contains(character))
            {
                emit invalidPathEntered();
                return Invalid;
            }
        }
    }

    return Acceptable;
}

void FilePathValidator::fixup(QString& input) const
{
    const QList<QChar>& invalid_characters =
        FilePlatformUtil::invalidPathCharacters();

    for (auto it = input.begin(); it != input.end(); ++it)
    {
        if (invalid_characters.contains(*it))
            input.remove(*it);
    }
}

} // namespace aspia
