/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char* id;
  const char* name;
  double price;
  int stock;
} ns_product_t;

/* Returns 0 on success */
int ns_inventory_create(void** out_handle);
int ns_inventory_destroy(void* handle);
int ns_inventory_add_product(void* handle, ns_product_t p);
/* Returns number of products; if out is not NULL, fills up to cap elements */
int ns_inventory_list_products(void* handle, ns_product_t* out, int cap);

#ifdef __cplusplus
} /* extern "C" */
#endif
