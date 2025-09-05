// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "neonstore/export.hpp"
#include "neonstore/inventory.hpp"
#include "neonstore/order.hpp"
#include <memory>
#include <string>

namespace neonstore {

struct IStorage {
  virtual ~IStorage() = default;
  virtual void save_products(const Inventory& inv) = 0;
  virtual void load_products(Inventory& inv) = 0;
  virtual void save_orders(const std::vector<Order>& orders) = 0;
  virtual void load_orders(std::vector<Order>& orders) = 0;
};

NEONSTORE_API std::unique_ptr<IStorage> make_csv_storage(const std::string& dir);

#ifdef NEONSTORE_ENABLE_SQLITE
NEONSTORE_API std::unique_ptr<IStorage> make_sqlite_storage(const std::string& dsn);
#endif

} // namespace neonstore
