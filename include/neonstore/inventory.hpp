// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "neonstore/export.hpp"
#include "neonstore/product.hpp"
#include "neonstore/order.hpp"
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace neonstore {

class NEONSTORE_API Inventory {
 public:
  // Throws if id already exists
  void add_product(const Product& p);
  // Increases stock; throws if product not found or qty < 0
  void restock(const std::string& id, int qty);
  void remove_product(const std::string& id);
  void set_price(const std::string& id, double price);
  // Decreases stock and returns an Order with a single item; throws if insufficient
  Order sell(const std::string& id, int qty);
  // Returns copy of products
  std::vector<Product> list_products() const;
  // Find product pointer or nullptr
  const Product* find(const std::string& id) const;

 private:
#ifdef NEONSTORE_THREADSAFE
  mutable std::mutex mtx_{};
#endif
  std::unordered_map<std::string, Product> products_;
};

} // namespace neonstore
