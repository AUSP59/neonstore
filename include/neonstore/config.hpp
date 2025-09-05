// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <map>
#include <string>

namespace neonstore {
struct Config {
  std::string backend = "csv";
  std::string data_dir = "data";
  std::string dsn = "neonstore.db";
};

Config load_config(const std::string& path);
} // namespace neonstore
