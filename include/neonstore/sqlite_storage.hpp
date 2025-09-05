// SPDX-License-Identifier: Apache-2.0
#pragma once
#ifdef NEONSTORE_ENABLE_SQLITE
#include "neonstore/storage.hpp"
#include <string>

namespace neonstore {
std::unique_ptr<IStorage> make_sqlite_storage(const std::string& dsn);
} // namespace neonstore
#endif
