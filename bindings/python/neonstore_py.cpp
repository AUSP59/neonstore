
// SPDX-License-Identifier: Apache-2.0
#include <pybind11/pybind11.h>
#include "neonstore/inventory.hpp"

namespace py = pybind11;
using namespace neonstore;

PYBIND11_MODULE(neonstore, m) {
  py::class_<Product>(m, "Product")
    .def(py::init<>())
    .def_readwrite("id", &Product::id)
    .def_readwrite("name", &Product::name)
    .def_readwrite("price", &Product::price)
    .def_readwrite("stock", &Product::stock);

  py::class_<Inventory>(m, "Inventory")
    .def(py::init<>())
    .def("add_product", [](Inventory& inv, const Product& p){ inv.add_product(p); })
    .def("list_products", &Inventory::list_products);
}
