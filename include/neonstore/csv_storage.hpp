// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <string>

#include <filesystem>

#include "neonstore/export.hpp"
#include "neonstore/inventory.hpp"
#include <string>

namespace neonstore {

// Very small CSV persistence (no external deps).
// Products CSV: id,name,price,stock
// Orders CSV: id,product_id,quantity,price_each (one row per item)
struct NEONSTORE_API CsvStorage {
  static void save_products(const Inventory& inv, const std::string& dir);
  static void load_products(Inventory& inv, const std::string& dir);
  static void save_orders(const std::vector<Order>& orders, const std::string& dir);
  static void load_orders(std::vector<Order>& orders, const std::string& dir);
};

} // namespace neonstore


namespace neonstore {
enum class Integrity { Off, Lenient, Strict };
enum class IntegrityAlgo { CRC32, SHA256 };
struct CsvIntegrity {
  static void set_algo(IntegrityAlgo a);
  static IntegrityAlgo get_algo();
  static void set(Integrity mode);
  static Integrity get();
};
} // namespace neonstore


namespace neonstore {
enum class LockMode { Dir, OS };
struct CsvLock {
  static void set_mode(LockMode m);
  static LockMode get_mode();
};
} // namespace neonstore
