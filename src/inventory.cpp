// SPDX-License-Identifier: Apache-2.0
#include "neonstore/inventory.hpp"

namespace neonstore {


void Inventory::add_product(const Product& p) {
#ifdef NEONSTORE_THREADSAFE
  std::lock_guard<std::mutex> lk(mtx_);
#endif
  if (p.id.empty()) throw std::invalid_argument("product id is empty");
  if (products_.count(p.id)) throw std::invalid_argument("product id already exists");
  if (p.price < 0.0) throw std::invalid_argument("price negative");
  if (p.stock < 0) throw std::invalid_argument("stock negative");
  products_.emplace(p.id, p);
}

void Inventory::restock(const std::string& id, int qty) {
#ifdef NEONSTORE_THREADSAFE
  std::lock_guard<std::mutex> lk(mtx_);
#endif
  if (qty < 0) throw std::invalid_argument("qty negative");
  auto it = products_.find(id);
  if (it == products_.end()) throw std::invalid_argument("product not found");
  it->second.stock += qty;
}

Order Inventory::sell(const std::string& id, int qty) {
#ifdef NEONSTORE_THREADSAFE
  std::lock_guard<std::mutex> lk(mtx_);
#endif
  if (qty <= 0) throw std::invalid_argument("qty must be positive");
  auto it = products_.find(id);
  if (it == products_.end()) throw std::invalid_argument("product not found");
  if (it->second.stock < qty) throw std::runtime_error("insufficient stock");
  it->second.stock -= qty;

  Order o;
  o.id = "O-" + id; // simple local id
  o.items.push_back(OrderItem{.product_id = id, .quantity = qty, .price_each = it->second.price});
  return o;
}

std::vector<Product> Inventory::list_products() const {
  std::vector<Product> v;
  v.reserve(products_.size());
  for (const auto& kv : products_) v.push_back(kv.second);
  return v;
}

const Product* Inventory::find(const std::string& id) const {
  auto it = products_.find(id);
  return (it == products_.end()) ? nullptr : &it->second;
}

} // namespace neonstore


void Inventory::remove_product(const std::string& id) {
#ifdef NEONSTORE_THREADSAFE
  std::lock_guard<std::mutex> lk(mtx_);
#endif
  auto it = products_.find(id);
  if (it == products_.end()) throw std::invalid_argument("product not found");
  products_.erase(it);
}

void Inventory::set_price(const std::string& id, double price) {
#ifdef NEONSTORE_THREADSAFE
  std::lock_guard<std::mutex> lk(mtx_);
#endif
  if (price < 0.0) throw std::invalid_argument("price negative");
  auto it = products_.find(id);
  if (it == products_.end()) throw std::invalid_argument("product not found");
  it->second.price = price;
}
