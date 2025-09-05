// SPDX-License-Identifier: Apache-2.0
#include "neonstore/csv_storage.hpp"
#include <filesystem>
#include <fstream>

extern "C" int LLVMFuzzerTestOneInput(const unsigned char *Data, size_t Size) {
  // Interpret input as two CSVs separated by a delimiter |; write to temp dir and parse
  std::string s(reinterpret_cast<const char*>(Data), Size);
  auto pos = s.find('|');
  std::string a = s.substr(0, pos);
  std::string b = pos==std::string::npos ? std::string{} : s.substr(pos+1);

  std::filesystem::path dir = std::filesystem::temp_directory_path() / "nsfuzz";
  std::filesystem::create_directories(dir);
  std::ofstream(dir / "products.csv") << a;
  std::ofstream(dir / "orders.csv") << b;
  try {
    neonstore::CSVStorage st(dir.string());
    neonstore::Inventory inv;
    st.load_products(inv);
    st.load_orders(inv);
    auto all = inv.list_products();
    (void)all;
  } catch (...) {}
  return 0;
}
