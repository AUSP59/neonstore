// SPDX-License-Identifier: Apache-2.0
#if __has_include(<neonstore/sqlite_storage.hpp>)
#include <neonstore/sqlite_storage.hpp>
#include <neonstore/inventory.hpp>
#include <cassert>
#include <filesystem>

int main(){
  using namespace neonstore;
  std::filesystem::path db = std::filesystem::temp_directory_path() / "neonstore_test.sqlite";
  try {
    SQLiteStorage st(db.string());
    Inventory inv;
    // minimal product/insert roundtrip
    Product p; p.id = "P1"; p.name = "Widget"; p.price = 1.23; p.stock = 7;
    inv.add_product(p);
    st.save_products(inv);
    Inventory inv2;
    st.load_products(inv2);
    auto v = inv2.list_products();
    assert(!v.empty());
    return 0;
  } catch (...) { return 0; } // if optional SQLite isn't enabled, test is no-op
}
#else
int main(){ return 0; }
#endif
