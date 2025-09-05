// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "neonstore/export.hpp"
#include <string>

namespace neonstore {

struct NEONSTORE_API Product {
  std::string id;
  std::string name;
  double price{0.0};
  int stock{0};
};

} // namespace neonstore
