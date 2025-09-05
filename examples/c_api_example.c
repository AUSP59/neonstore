/* SPDX-License-Identifier: Apache-2.0 */
#include <stdio.h>
#include "neonstore/capi.h"
int main() {
  void* h = NULL;
  if (ns_inventory_create(&h) != 0) return 1;
  ns_product_t p = { "P1", "Widget", 9.99, 5 };
  ns_inventory_add_product(h, p);
  int n = ns_inventory_list_products(h, NULL, 0);
  printf("Products: %d\n", n);
  return ns_inventory_destroy(h);
}
