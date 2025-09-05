\
// SPDX-License-Identifier: Apache-2.0
#[link(name="neonstore_c")]
extern "C" {
    pub fn neonstore_inventory_create() -> *mut core::ffi::c_void;
    pub fn neonstore_inventory_destroy(p: *mut core::ffi::c_void);
    pub fn neonstore_inventory_add(p: *mut core::ffi::c_void, id: *const i8, name: *const i8, price: f64, stock: i32) -> i32;
    pub fn neonstore_inventory_count(p: *mut core::ffi::c_void) -> i32;
}
