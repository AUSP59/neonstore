// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "neonstore/export.hpp"
#include <string>
#include <vector>

namespace neonstore {

struct NEONSTORE_API OrderItem {
  std::string product_id;
  int quantity{0};
  double price_each{0.0};
};

struct NEONSTORE_API Order {
  std::string id;
  std::vector<OrderItem> items;

  double total() const {
    double t = 0.0;
    for (const auto& it : items) t += it.price_each * static_cast<double>(it.quantity);
    return t;
  }
};

} // namespace neonstore
