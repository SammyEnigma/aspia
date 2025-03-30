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

#ifndef BASE_CODEC_SCOPED_ZSTD_STREAM_H
#define BASE_CODEC_SCOPED_ZSTD_STREAM_H

#include <zstd.h>

#include <memory>

namespace base {

struct ZstdCStreamDeleter
{
    void operator()(ZSTD_CStream* cstream);
};

struct ZstdDStreamDeleter
{
    void operator()(ZSTD_DStream* dstream);
};

using ScopedZstdCStream = std::unique_ptr<ZSTD_CStream, ZstdCStreamDeleter>;
using ScopedZstdDStream = std::unique_ptr<ZSTD_DStream, ZstdDStreamDeleter>;

} // namespace base

#endif // BASE_CODEC_SCOPED_ZSTD_STREAM_H
