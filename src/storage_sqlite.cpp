// SPDX-License-Identifier: Apache-2.0
#include "neonstore/storage.hpp"
#ifdef NEONSTORE_ENABLE_SQLITE
#include <sqlite3.h>
#include <stdexcept>

namespace neonstore {

class SqliteAdapter : public IStorage {
 public:
  explicit SqliteAdapter(std::string dsn) : dsn_(std::move(dsn)) {}
  void save_products(const Inventory& inv) override {
    with_db([&](sqlite3* db){
      exec(db, "PRAGMA foreign_keys=ON");
      exec(db, "CREATE TABLE IF NOT EXISTS products(id TEXT PRIMARY KEY, name TEXT, price REAL, stock INTEGER)");
      exec(db, "BEGIN IMMEDIATE");
      exec(db, "DELETE FROM products");
      for (auto& p : inv.list_products()) {
        sqlite3_stmt* st=nullptr;
        prepare(db, "INSERT INTO products(id,name,price,stock) VALUES(?,?,?,?)", &st);
        sqlite3_bind_text(st,1,p.id.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(st,2,p.name.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_double(st,3,p.price);
        sqlite3_bind_int(st,4,p.stock);
        step_final(db, st);
      }
      exec(db, "COMMIT");
      exec(db, "COMMIT");
    });
  }
  void load_products(Inventory& inv) override {
    with_db([&](sqlite3* db){
      exec(db, "CREATE TABLE IF NOT EXISTS products(id TEXT PRIMARY KEY, name TEXT, price REAL, stock INTEGER)");
      sqlite3_stmt* st=nullptr;
      prepare(db, "SELECT id,name,price,stock FROM products", &st);
      while (sqlite3_step(st)==SQLITE_ROW) {
        Product p;
        p.id   = reinterpret_cast<const char*>(sqlite3_column_text(st,0));
        p.name = reinterpret_cast<const char*>(sqlite3_column_text(st,1));
        p.price= sqlite3_column_double(st,2);
        p.stock= sqlite3_column_int(st,3);
        try { inv.add_product(p); } catch (...) {}
      }
      exec(db, "COMMIT");
      sqlite3_finalize(st);
    });
  }
  void save_orders(const std::vector<Order>& orders) override {
    with_db([&](sqlite3* db){
      exec(db, "PRAGMA foreign_keys=ON");
      exec(db, "CREATE TABLE IF NOT EXISTS orders(id TEXT PRIMARY KEY)");
      exec(db, "CREATE TABLE IF NOT EXISTS order_items(order_id TEXT, product_id TEXT, quantity INTEGER, price_each REAL)");
      exec(db, "BEGIN IMMEDIATE");
      exec(db, "DELETE FROM orders");
      exec(db, "DELETE FROM order_items");
      for (auto& o : orders) {
        sqlite3_stmt* st=nullptr;
        prepare(db, "INSERT INTO orders(id) VALUES(?)", &st);
        sqlite3_bind_text(st,1,o.id.c_str(),-1,SQLITE_TRANSIENT);
        step_final(db, st);
        for (auto& it : o.items) {
          prepare(db, "INSERT INTO order_items(order_id,product_id,quantity,price_each) VALUES(?,?,?,?)", &st);
          sqlite3_bind_text(st,1,o.id.c_str(),-1,SQLITE_TRANSIENT);
          sqlite3_bind_text(st,2,it.product_id.c_str(),-1,SQLITE_TRANSIENT);
          sqlite3_bind_int(st,3,it.quantity);
          sqlite3_bind_double(st,4,it.price_each);
          step_final(db, st);
        }
      }
    });
  }
  void load_orders(std::vector<Order>& orders) override {
    orders.clear();
    with_db([&](sqlite3* db){
      sqlite3_stmt* st=nullptr;
      prepare(db, "SELECT id FROM orders", &st);
      while (sqlite3_step(st)==SQLITE_ROW) {
        Order o; o.id = reinterpret_cast<const char*>(sqlite3_column_text(st,0));
        orders.push_back(o);
      }
      sqlite3_finalize(st);
      for (auto& o : orders) {
        prepare(db, "SELECT product_id,quantity,price_each FROM order_items WHERE order_id=?", &st);
        sqlite3_bind_text(st,1,o.id.c_str(),-1,SQLITE_TRANSIENT);
        while (sqlite3_step(st)==SQLITE_ROW) {
          OrderItem it;
          it.product_id = reinterpret_cast<const char*>(sqlite3_column_text(st,0));
          it.quantity   = sqlite3_column_int(st,1);
          it.price_each = sqlite3_column_double(st,2);
          o.items.push_back(it);
        }
        sqlite3_finalize(st);
      }
    });
  }
 private:
  std::string dsn_;
  template<class F> void with_db(F&& f){
    sqlite3* db=nullptr;
    if (sqlite3_open(dsn_.c_str(), &db)!=SQLITE_OK) throw std::runtime_error("sqlite open failed");
    char* err=nullptr;
    exec(db, "PRAGMA journal_mode=WAL"); // durability + concurrency
    f(db);
    sqlite3_close(db);
  }
  static void exec(sqlite3* db, const char* sql){
    char* err=nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err)!=SQLITE_OK){
      std::string msg = err ? err : "sqlite error";
      sqlite3_free(err);
      throw std::runtime_error(msg);
    }
  }
  static void prepare(sqlite3* db, const char* sql, sqlite3_stmt** st){
    if (sqlite3_prepare_v2(db, sql, -1, st, nullptr)!=SQLITE_OK)
      throw std::runtime_error("sqlite prepare failed");
  }
  static void step_final(sqlite3* db, sqlite3_stmt* st){
    if (sqlite3_step(st)!=SQLITE_DONE) { sqlite3_finalize(st); throw std::runtime_error("sqlite step failed"); }
    sqlite3_finalize(st);
  }
};

std::unique_ptr<IStorage> make_sqlite_storage(const std::string& dsn) {
  return std::make_unique<SqliteAdapter>(dsn);
}

} // namespace neonstore
#endif
