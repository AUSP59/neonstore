// SPDX-License-Identifier: Apache-2.0
#include "neonstore/storage.hpp"
#include "neonstore/csv_storage.hpp"

namespace neonstore {
class CsvAdapter : public IStorage {
 public:
  explicit CsvAdapter(std::string dir) : dir_(std::move(dir)) {}
  void save_products(const Inventory& inv) override { CsvStorage::save_products(inv, dir_); }
  void load_products(Inventory& inv) override { CsvStorage::load_products(inv, dir_); }
  void save_orders(const std::vector<Order>& o) override { CsvStorage::save_orders(o, dir_); }
  void load_orders(std::vector<Order>& o) override { CsvStorage::load_orders(o, dir_); }
 private:
  std::string dir_;
};

std::unique_ptr<IStorage> make_csv_storage(const std::string& dir) {
  return std::make_unique<CsvAdapter>(dir);
}

} // namespace neonstore
