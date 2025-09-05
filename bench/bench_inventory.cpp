// SPDX-License-Identifier: Apache-2.0
#include "neonstore/inventory.hpp"
#include <chrono>
#include <iostream>
#include <random>

using namespace neonstore;
int main(){
  Inventory inv;
  std::mt19937 rng(123);
  std::uniform_real_distribution<double> price(1.0, 100.0);
  const int N = 100000;
  auto start = std::chrono::steady_clock::now();
  for (int i=0;i<N;++i){
    Product p; p.id = "P"+std::to_string(i); p.name="Item"+std::to_string(i);
    p.price=price(rng); p.stock = i%100;
    inv.add_product(p);
  }
  auto mid = std::chrono::steady_clock::now();
  auto all = inv.list_products();
  volatile size_t sink = all.size();
  auto end = std::chrono::steady_clock::now();
  auto t_insert = std::chrono::duration_cast<std::chrono::milliseconds>(mid-start).count();
  auto t_list   = std::chrono::duration_cast<std::chrono::milliseconds>(end-mid).count();
  std::cout << "Inserted: " << N << " in " << t_insert << " ms, list in " << t_list << " ms\n";
  std::cout << "Total: " << sink << "\n";
  return 0;
}
