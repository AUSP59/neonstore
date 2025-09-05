\
# SPDX-License-Identifier: Apache-2.0
# Build with: cmake -DENABLE_PYTHON=ON ...
import neonstore as ns
inv = ns.Inventory()
p = ns.Product()
p.id = "P1"; p.name = "Widget"; p.price = 9.99; p.stock = 5
inv.add_product(p)
print(len(inv.list_products()))
