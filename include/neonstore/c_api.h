
// SPDX-License-Identifier: Apache-2.0
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

typedef void* neonstore_inventory_t;

neonstore_inventory_t neonstore_inventory_create(void);
void neonstore_inventory_destroy(neonstore_inventory_t inv);
int  neonstore_inventory_add(neonstore_inventory_t inv, const char* id, const char* name, double price, int stock);
int  neonstore_inventory_count(neonstore_inventory_t inv);

#ifdef __cplusplus
}
#endif
