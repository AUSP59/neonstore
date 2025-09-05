// SPDX-License-Identifier: Apache-2.0
#include <neonstore/csv_storage.hpp>
#include <neonstore/inventory.hpp>
#include <cassert>
#include <filesystem>
#include <fstream>

int main(){
  using namespace neonstore;
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "ns_crlf";
  std::filesystem::create_directories(dir);
  std::ofstream products(dir / "products.csv", std::ios::binary);
  products << "id,name,price,stock\r\n";
  products << "P1,Widget,9.99,5\r\n";
  products.close();

  std::ofstream orders(dir / "orders.csv", std::ios::binary);
  orders << "order_id,product_id,qty\r\n";
  orders << "O1,P1,2\r\n";
  orders.close();

  Inventory inv;
  CSVStorage st(dir.string());
  st.load_products(inv);
  auto v = inv.list_products();
  assert(!v.empty());
  return 0;
}
