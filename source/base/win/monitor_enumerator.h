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

#ifndef BASE_WIN_MONITOR_ENUMERATOR_H
#define BASE_WIN_MONITOR_ENUMERATOR_H

#include "base/win/device_enumerator.h"
#include "base/edid.h"

#include <memory>

namespace base::win {

class MonitorEnumerator final : public DeviceEnumerator
{
public:
    MonitorEnumerator();
    ~MonitorEnumerator() final = default;

    std::unique_ptr<Edid> edid() const;

private:
    DISALLOW_COPY_AND_ASSIGN(MonitorEnumerator);
};

} // namespace base::win

#endif // BASE_WIN_MONITOR_ENUMERATOR_H
