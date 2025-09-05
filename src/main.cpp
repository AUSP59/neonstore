#include <locale>
#include <fstream>
#include <algorithm>
#include <random>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <array>
#include <cmath>
// SPDX-License-Identifier: Apache-2.0

#include "neonstore/csv_storage.hpp"
#include "neonstore/inventory.hpp"
#include "neonstore/storage.hpp"
#include "neonstore/config.hpp"
#include "neonstore/version.hpp"
#include "neonstore/log.hpp"
#include "neonstore/audit.hpp"
#include "neonstore/json.hpp"
#include "neonstore/sha256.hpp"
#include <cctype>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <chrono>
#include <regex>
#include <cstdio>
#ifdef _WIN32
 #include <io.h>
 #define isatty _isatty
#else
 #include <unistd.h>
#endif
#include <ctime>


using namespace neonstore;

// CRC32 (polynomial 0xEDB88320)
static uint32_t crc32_fast(const uint8_t* data, size_t len){
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i=0;i<len;i++){
    crc ^= data[i];
    for (int k=0;k<8;k++) crc = (crc>>1) ^ (0xEDB88320u & (-(int)(crc & 1)));
  }
  return ~crc;
}
static uint32_t crc32_str(const std::string& s){ return crc32_fast((const uint8_t*)s.data(), s.size()); }

// Tiny SHA-256 (public domain style, trimmed)
struct sha256_ctx { uint32_t h[8]; uint64_t len; uint8_t buf[64]; size_t idx; };
static uint32_t ROR(uint32_t x, uint32_t n){ return (x>>n)|(x<<(32-n)); }
static void sha256_init(sha256_ctx& c){
  c.h[0]=0x6a09e667; c.h[1]=0xbb67ae85; c.h[2]=0x3c6ef372; c.h[3]=0xa54ff53a;
  c.h[4]=0x510e527f; c.h[5]=0x9b05688c; c.h[6]=0x1f83d9ab; c.h[7]=0x5be0cd19;
  c.len=0; c.idx=0;
}
static void sha256_chunk(sha256_ctx& c, const uint8_t* p){
  static const uint32_t k[64]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
  uint32_t w[64]; for(int i=0;i<16;i++){ w[i]=(p[4*i]<<24)|(p[4*i+1]<<16)|(p[4*i+2]<<8)|p[4*i+3]; }
  for(int i=16;i<64;i++){ uint32_t s0=ROR(w[i-15],7)^ROR(w[i-15],18)^(w[i-15]>>3); uint32_t s1=ROR(w[i-2],17)^ROR(w[i-2],19)^(w[i-2]>>10); w[i]=w[i-16]+s0+w[i-7]+s1; }
  uint32_t a=c.h[0],b=c.h[1],c0=c.h[2],d=c.h[3],e=c.h[4],f=c.h[5],g=c.h[6],h=c.h[7];
  for(int i=0;i<64;i++){
    uint32_t S1=ROR(e,6)^ROR(e,11)^ROR(e,25);
    uint32_t ch=(e&f)^( (~e)&g );
    uint32_t temp1=h+S1+ch+k[i]+w[i];
    uint32_t S0=ROR(a,2)^ROR(a,13)^ROR(a,22);
    uint32_t maj=(a&b)^(a&c0)^(b&c0);
    uint32_t temp2=S0+maj;
    h=g; g=f; f=e; e=d+temp1; d=c0; c0=b; b=a; a=temp1+temp2;
  }
  c.h[0]+=a; c.h[1]+=b; c.h[2]+=c0; c.h[3]+=d; c.h[4]+=e; c.h[5]+=f; c.h[6]+=g; c.h[7]+=h;
}
static void sha256_update(sha256_ctx& c, const uint8_t* data, size_t len){
  c.len += len;
  while(len>0){
    size_t n = std::min((size_t)64 - c.idx, len);
    std::memcpy(c.buf + c.idx, data, n);
    c.idx += n; data += n; len -= n;
    if (c.idx==64){ sha256_chunk(c, c.buf); c.idx=0; }
  }
}
static std::string sha256_hex(const std::string& s){
  sha256_ctx c; sha256_init(c); sha256_update(c, (const uint8_t*)s.data(), s.size());
  // padding
  uint64_t bitlen = c.len * 8;
  uint8_t one = 0x80; sha256_update(c, &one, 1);
  uint8_t z = 0; while(c.idx != 56){ sha256_update(c, &z, 1); }
  uint8_t lenb[8]; for(int i=0;i<8;i++) lenb[7-i] = (uint8_t)((bitlen >> (8*i)) & 0xFF);
  sha256_update(c, lenb, 8);
  if (c.idx) sha256_chunk(c, c.buf);
  std::ostringstream os; os<<std::hex<<std::nouppercase;
  for(int i=0;i<8;i++){ uint32_t v=c.h[i]; os<<std::setw(8)<<std::setfill('0')<<v; }
  return os.str();
}


static std::string now_iso(){
  std::time_t t = std::time(nullptr);
  std::tm *tm = std::gmtime(&t);
  char buf[32]; std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
  return std::string(buf);
}
static long long parse_iso_utc_epoch(const std::string& iso){
  // very small parser: YYYY-MM-DDThh:mm:ssZ (UTC)
  if (iso.size() < 20) return 0;
  int Y=std::stoi(iso.substr(0,4)), m=std::stoi(iso.substr(5,2)), d=std::stoi(iso.substr(8,2));
  int H=std::stoi(iso.substr(11,2)), M=std::stoi(iso.substr(14,2)), S=std::stoi(iso.substr(17,2));
  std::tm tm{}; tm.tm_year=Y-1900; tm.tm_mon=m-1; tm.tm_mday=d; tm.tm_hour=H; tm.tm_min=M; tm.tm_sec=S;
  auto tt = timegm(&tm); return (long long)tt;
}
static bool worm_active(const std::string& dir){
  std::error_code ec;
  std::filesystem::path p = std::filesystem::path(dir)/".neonstore"/"worm.ini";
  if (!std::filesystem::exists(p, ec)) return false;
  std::ifstream f(p.string()); std::string s; long long until=0;
  while(std::getline(f,s)){ if(s.rfind("until=",0)==0){ try{ until = std::stoll(s.substr(6)); }catch(...){ } } }
  std::time_t now = std::time(nullptr);
  return (until>0 && now < until);
}
static bool atomic_write_raw(const std::string& dir, const std::string& prod_csv, const std::string& ord_csv){
  std::error_code ec;
  auto d = std::filesystem::path(dir);
  auto pnew = (d/"products.csv.new").string();
  auto onew = (d/"orders.csv.new").string();
  { std::ofstream f(pnew, std::ios::binary|std::ios::trunc); f<<prod_csv; f.flush(); f.close(); }
  { std::ofstream f(onew, std::ios::binary|std::ios::trunc); f<<ord_csv; f.flush(); f.close(); }
  auto j = (d/"txn_journal.json");
  { std::ofstream jf(j.string(), std::ios::binary|std::ios::trunc);
    jf << "{\"state\":\"prepared\",\"products\":\"products.csv.new\",\"orders\":\"orders.csv.new\"}\n";
  }
  std::filesystem::rename(d/"products.csv.new", d/"products.csv", ec); if (ec) return false;
  std::filesystem::rename(d/"orders.csv.new", d/"orders.csv", ec); if (ec) return false;
  std::filesystem::remove(j, ec);
  return true;
}
// TAR helpers (ustar)
static void tar_write_header(std::ofstream& out, const std::string& name, size_t size){
  char h[512]; std::memset(h, 0, sizeof(h));
  auto put = [&](size_t off, const std::string& v){ std::memcpy(h+off, v.c_str(), std::min(v.size(), sizeof(h)-off)); };
  put(0, name);
  std::snprintf(h+100, 8, "%07o", 0644);
  std::snprintf(h+108, 8, "%07o", 0);
  std::snprintf(h+116, 8, "%07o", 0);
  std::snprintf(h+124, 12, "%011o", (unsigned int)size);
  std::snprintf(h+136, 12, "%011o", (unsigned int)std::time(nullptr));
  std::memset(h+148, ' ', 8);
  h[156] = '0';
  std::memcpy(h+257, "ustar", 5);
  unsigned int sum=0; for (int i=0;i<512;i++) sum += (unsigned char)h[i];
  std::snprintf(h+148, 8, "%07o", sum);
  out.write(h, 512);
}
static void tar_write_file(std::ofstream& out, const std::string& name, const std::string& data){
  tar_write_header(out, name, data.size());
  out.write(data.data(), (std::streamsize)data.size());
  size_t pad = (512 - (data.size() % 512)) % 512;
  if (pad){ std::string z(pad, '\0'); out.write(z.data(), (std::streamsize)z.size()); }
}
static void tar_write_end(std::ofstream& out){
  char z[1024]; std::memset(z, 0, sizeof(z)); out.write(z, 1024);
}
static std::map<std::string,std::string> tar_read_all(const std::string& path){
  std::map<std::string,std::string> files;
  std::ifstream in(path, std::ios::binary); if(!in) return files;
  while (true){
    char h[512]; in.read(h,512); if (!in) break;
    bool empty=true; for(int i=0;i<512;i++){ if(h[i]!=0){ empty=false; break; } }
    if (empty) break;
    std::string name(h, h+100); name = name.c_str();
    auto parse_oct = [&](const char* p, int n)->size_t{ size_t v=0; for(int i=0;i<n && p[i]; ++i){ if(p[i]<'0'||p[i]>'7') break; v = (v<<3) + (p[i]-'0'); } return v; };
    size_t size = parse_oct(h+124, 12);
    std::string data; data.resize(size);
    in.read(&data[0], (std::streamsize)size);
    size_t pad = (512 - (size % 512)) % 512; if (pad) in.ignore(pad);
    files[name]=data;
  }
  return files;
}

namespace neonstore { bool atomic_write_dataset(const Inventory&, const std::vector<Order>&, const std::string&); }

static double policy_unit_price_delta_pct(const std::string& dir){
  for (auto p : { std::filesystem::path("policy.ini"), std::filesystem::path(".neonstore")/"policy.ini", std::filesystem::path(dir)/"policy.ini" }){
    std::error_code ec; if(!std::filesystem::exists(p, ec)) continue;
    std::ifstream f(p.string()); std::string s;
    while(std::getline(f,s)){ if(s.rfind("unit_price_delta_pct=",0)==0){ try{ return std::stod(s.substr(21)); }catch(...){ } } }
  }
  const char* e = std::getenv("NEONSTORE_UNIT_PRICE_DELTA_PCT");
  if (e && *e){ try{ return std::stod(e); }catch(...){ } }
  return 10.0; // default tolerance 10%
}


static uint64_t fnv1a64(const void* data, size_t len){
  const uint8_t* p = (const uint8_t*)data;
  uint64_t h = 1469598103934665603ULL;
  for (size_t i=0;i<len;i++){ h ^= p[i]; h *= 1099511628211ULL; }
  return h;
}
static uint64_t fnv1a64_str(const std::string& s){ return fnv1a64(s.data(), s.size()); }


static double policy_price_change_max_pct(const std::string& dir){
  for (auto p : { std::filesystem::path("policy.ini"), std::filesystem::path(".neonstore")/"policy.ini", std::filesystem::path(dir)/"policy.ini" }){
    std::error_code ec; if(!std::filesystem::exists(p, ec)) continue;
    std::ifstream f(p.string()); std::string s;
    while(std::getline(f,s)){
      if(s.rfind("price_change_max_pct=",0)==0){ try{ return std::stod(s.substr(21)); }catch(...){ } }
    }
  }
  const char* e = std::getenv("NEONSTORE_PRICE_CHANGE_MAX_PCT");
  if (e && *e){ try{ return std::stod(e); }catch(...){ } }
  return -1.0;
}

static std::string trim(const std::string& s){ size_t a=0,b=s.size(); while(a<b && std::isspace((unsigned char)s[a])) ++a; while(b>a && std::isspace((unsigned char)s[b-1])) --b; return s.substr(a,b-a);}
static std::string uuid4(){
  std::random_device rd; std::mt19937_64 gen(((uint64_t)rd()<<32) ^ rd());
  std::array<unsigned char,16> u{};
  for(int i=0;i<16;i++){ u[i] = (unsigned char)(gen() & 0xFF); }
  u[6] = (unsigned char)((u[6] & 0x0F) | 0x40); // version 4
  u[8] = (unsigned char)((u[8] & 0x3F) | 0x80); // variant 10
  auto hx=[&](unsigned char b){ const char* d="0123456789abcdef"; std::string s; s.push_back(d[(b>>4)&0xF]); s.push_back(d[b&0xF]); return s; };
  std::ostringstream os; os<<std::nouppercase;
  for(int i=0;i<16;i++){
    os << hx(u[i]);
    if(i==3||i==5||i==7||i==9) os << "-";
  }
  return os.str();
}

static std::vector<std::string> read_lines(const std::string& path){ std::vector<std::string> v; std::ifstream f(path, std::ios::binary); std::string s; while(std::getline(f,s)) v.push_back(s); return v; }
static void write_text(const std::string& path, const std::string& s){ std::ofstream o(path, std::ios::binary|std::ios::trunc); o<<s; }
static std::string default_data_dir(){
  const char* env = std::getenv("NEONSTORE_DIR"); if (env && *env) return std::string(env);
  // project/local config
  for (auto p : { std::string("neonstore.ini"), std::string(".neonstore/config.ini") }){
    if (std::filesystem::exists(p)){ std::ifstream f(p); std::string line; while(std::getline(f,line)){ if(line.rfind("dir=",0)==0) return line.substr(4); } }
  }
  // XDG
  const char* xdg = std::getenv("XDG_CONFIG_HOME"); if (xdg){ std::filesystem::path p = std::filesystem::path(xdg)/"neonstore"/"config.ini"; if (std::filesystem::exists(p)){ std::ifstream f(p); std::string line; while(std::getline(f,line)){ if(line.rfind("dir=",0)==0) return line.substr(4); } } }
  return std::string("data");
}

static std::string mktemp_dir(const std::string& base){
  std::string t = base + "/.tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  std::error_code ec; std::filesystem::create_directories(t, ec); return t;
}

static std::string slurp(const std::string& path){ std::ifstream f(path, std::ios::binary); if(!f) return std::string(); return std::string((std::istreambuf_iterator<char>(f)), {});}

static std::string gen_metrics_prom(const std::string& dir){
  Inventory invX; std::vector<Order> oo; make_csv_storage(dir)->load_products(invX); make_csv_storage(dir)->load_orders(oo);
  auto v = invX.list_products(); size_t products=v.size(); size_t orders_n=oo.size();
  double inventory_value=0.0; for (auto&p: v) inventory_value += p.price * p.stock;
  double revenue=0.0; for (auto&o: oo) revenue += o.total();
  std::string out;
  out += "# HELP neonstore_products_total Number of products\n# TYPE neonstore_products_total gauge\nneonstore_products_total " + std::to_string(products) + "\n";
  out += "# HELP neonstore_orders_total Number of orders\n# TYPE neonstore_orders_total gauge\nneonstore_orders_total " + std::to_string(orders_n) + "\n";
  out += "# HELP neonstore_inventory_value_total Total inventory value\n# TYPE neonstore_inventory_value_total gauge\nneonstore_inventory_value_total " + std::to_string(inventory_value) + "\n";
  out += "# HELP neonstore_revenue_total Total revenue\n# TYPE neonstore_revenue_total counter\nneonstore_revenue_total " + std::to_string(revenue) + "\n";
  return out;
}

static size_t levenshtein(const std::string& a, const std::string& b){
  const size_t n=a.size(), m=b.size();
  std::vector<size_t> dp(m+1); for(size_t j=0;j<=m;++j) dp[j]=j;
  for(size_t i=1;i<=n;++i){ size_t prev=dp[0]++; size_t cur; for(size_t j=1;j<=m;++j){ cur=std::min({ dp[j]+1, dp[j-1]+1, prev + (a[i-1]==b[j-1]?0:1) }); prev=dp[j]; dp[j]=cur; } }
  return dp[m]; }
static std::string lower(const std::string& s){ std::string r=s; for(char&c:r) c=(char)std::tolower((unsigned char)c); return r; }
static std::string upper_copy(const std::string& s){ std::string r=s; for(char&c:r) c=(char)std::toupper((unsigned char)c); return r; }
static void tar_write_header(std::ofstream& out, const std::string& name, size_t size){
  char h[512]; std::memset(h, 0, 512);
  auto w = [&](int off, const std::string& s){ std::memcpy(h+off, s.c_str(), std::min((int)s.size(), 512-off)); };
  w(0, name.substr(0,100));
  std::snprintf(h+100, 8, "%07o", 0644);
  std::snprintf(h+108, 8, "%07o", 0); // uid
  std::snprintf(h+116, 8, "%07o", 0); // gid
  std::snprintf(h+124, 12, "%011o", (unsigned int)size);
  std::snprintf(h+136, 12, "%011o", (unsigned int)std::time(nullptr));
  std::memcpy(h+257, "ustar", 5); std::memcpy(h+263, "00", 2);
  h[156] = '0';
  std::memset(h+148, ' ', 8);
  unsigned int sum=0; for (int i=0;i<512;i++) sum += (unsigned char)h[i];
  std::snprintf(h+148, 8, "%06o\0 ", sum);
  out.write(h, 512);
}
static void tar_write_file(std::ofstream& out, const std::string& name, const std::string& path){
  std::ifstream f(path, std::ios::binary); if(!f) return; std::string data((std::istreambuf_iterator<char>(f)), {});
  tar_write_header(out, name, data.size()); out.write(data.data(), (std::streamsize)data.size());
  size_t pad = (512 - (data.size()%512))%512; if (pad){ static const char z[512]={0}; out.write(z, pad); }
}
static void tar_write_eof(std::ofstream& out){ static const char z[1024]={0}; out.write(z, 1024); }

static std::string compute_fp_all(const std::string& dir){ Inventory invX; std::vector<Order> oo; make_csv_storage(dir)->load_products(invX); make_csv_storage(dir)->load_orders(oo); std::string canon; { auto v = invX.list_products(); std::sort(v.begin(), v.end(), [](auto&a,auto&b){return a.id<b.id;}); canon += "P|"; for (auto& p:v){ canon += p.id + "|" + p.name + "|" + std::to_string(p.price) + "|" + std::to_string(p.stock) + "\n"; } } { std::sort(oo.begin(), oo.end(), [](const Order&a,const Order&b){return a.id<b.id;}); canon += "O|"; for (auto& o:oo){ canon += o.id + "|" + std::to_string(o.total()) + "\n"; } } return Sha256::of_string(canon);}
static std::string newid_ulid(){
  static const char* A = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"; // Crockford base32
  // 48-bit time (ms) + 80-bit randomness -> 26 chars
  uint64_t tms = (uint64_t) std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  std::random_device rd; std::mt19937_64 gen((uint64_t)rd() ^ ((uint64_t)rd()<<32));
  uint64_t r1 = gen();
  uint64_t r2 = gen();
  unsigned char data[16];
  // time 6 bytes big-endian
  data[0]=(tms>>40)&0xFF; data[1]=(tms>>32)&0xFF; data[2]=(tms>>24)&0xFF;
  data[3]=(tms>>16)&0xFF; data[4]=(tms>>8)&0xFF; data[5]=(tms)&0xFF;
  // randomness 10 bytes
  for(int i=0;i<8;++i) data[6+i]=(r1>>(56-8*i))&0xFF;
  data[14]=(r2>>56)&0xFF; data[15]=(r2>>48)&0xFF;
  // Encode 26 chars
  unsigned int bits=0; int bitbuf=0; std::string out; out.reserve(26);
  for(int i=0;i<16;++i){
    bitbuf=(bitbuf<<8)|data[i]; bits+=8;
    while(bits>=5){ unsigned int idx=(bitbuf>>(bits-5)) & 31u; out.push_back(A[idx]); bits-=5; }
  }
  if(bits>0){ unsigned int idx=(bitbuf<<(5-bits)) & 31u; out.push_back(A[idx]); }
  while(out.size()<26) out.push_back('0');
  if(out.size()>26) out.resize(26);
  return out;
}

auto jkv=[](const std::string& k, const std::string& v){ return std::string("\"")+k+"\":\"" + json_escape(v) + "\""; };
auto tick=[](){ return std::chrono::steady_clock::now(); };
auto ms=[&](auto a, auto b){ return std::chrono::duration_cast<std::chrono::milliseconds>(b-a).count(); };
static bool must_allow_write(){ if (read_only){ std::cerr << "read-only mode\n"; return false; } return true; }
static bool cas_guard(const std::map<std::string,std::string>& args, const std::string& dir){ auto it=args.find("--expect-fingerprint"); if(it==args.end()) return true; std::string cur = compute_fp_all(dir); if (cur!=it->second){ std::cerr << "fingerprint mismatch (concurrent change?)\n"; return false; } return true; }

static const char* tr(const char* en, const char* es){ return (lang=="es" ? es : en); }

static bool no_color=false; static bool verbose=false; static bool quiet=false; static bool dry_run=false; static bool profile=false; static bool read_only=false; static std::string lang="en";
static void print_help() {
  std::cout << "NeonStoreSystem v" << NEONSTORE_VERSION << "\n"
            << "Commands:\n"
            << "  add-product --id ID --name NAME --price PRICE [--stock QTY]\n"
            << "  list-products [--output text|json|jsonl|tsv|yaml] [--limit N] [--offset N] [--sort key1[,key2...]] [--desc] [--filter SUBSTR|--filter-regex REGEX] [--price-gt V] [--price-lt V] [--stock-gt N] [--stock-lt N]\n"
            << "  restock --id ID --qty QTY\n"
            << "  remove-product --id ID\n"
            << "  set-price --id ID --price PRICE\n"
            << "  find-product --id ID [--output text|json]\n"
            << "  export --format json|jsonl|csv|tsv --out FILE\n"
            << "  import --format csv --file FILE (use "-" for stdin)\n"
            << "  migrate --from-backend csv|sqlite --from-dir DIR --from-dsn DSN --to-backend csv|sqlite --to-dir DIR --to-dsn DSN\n"
            << "  backup [--max-total-bytes BYTES] --dir PATH --out DIR\n"            << "  backup verify --dir PATH\n"            << "  quota set --max-bytes N [--dir PATH] | status [--dir PATH] | unset [--dir PATH]\n"
            << "  restore --dir PATH --from DIR|latest\n"
            << "  sanitize --dir PATH [--upper-ids] [--dry-run]\n"
            << "  benchmark [--iters N]\n"
            << "  seed --products N [--prefix P]\n"
            << "  diff --from-dir A --to-dir B [--output text|json]\n"
            << "  fingerprint [--dir PATH] [--which products|orders|all]\n"
            << "  trash list|purge|undelete --id ID\n"
            << "  snapshot create|list|restore <TS>|label <TS> --label L|prune --keep N [--age-days D] [--dir PATH]\n"
            << "  dataset version show|set --value V [--dir PATH]\n"            << "  migrate plan|apply --from V --to V [--dir PATH]\n"
            << "  redact --dir PATH --out PATH [--mode pseudonymize|strip] [--ids keep|hash]\n"
            << "  metrics [--format prometheus|json] [--dir PATH]\n"            << "  catalog generate [--dir PATH] [--output json|text]\n"            << "  serve --port N [--bind HOST]\n"
            << "  report sales [--top N] [--by revenue|quantity] [--output text|json]\n"
            << "  compact --dir PATH\n"
            << "  archive create --out FILE [--dir PATH] | extract --file FILE [--dir PATH]\n"
            << "  diff --left DIR1 --right DIR2 [--format text|json|yaml|patch] [--orders] [--orders-detail] [--fail-on-diff]\n"
            << "  patch apply --file FILE [--dir PATH]\n"            << "  jsonrpc --stdio\n"            << "  license check\n"            << "  perms check|fix [--dir PATH] [--output text|json]\n"            << "  verify-all [--dir PATH] [--output json|text]\n"            << "  report generate --out DIR [--dir PATH]\n"            << "  health [--dir PATH] [--output json|text]\n"            << "  doctor fix [--dir PATH]\n"            << "  lint refs|csv [--dir PATH]\n"            << "  worm enable --until ISO8601Z | status | disable [--dir PATH]\n"            << "  products stats [--dir PATH] [--format json|text]\n"            << "  products set-price --id ID --price NUM [--dir PATH]\n"            << "  products set-stock --id ID --stock NUM [--dir PATH]\n"            << "  search products --name SUBSTR [--dir PATH]\n"            << "  search products --regex PATTERN [--dir PATH]\n"            << "  seal create|verify [--dir PATH] [--output json|text]\n"            << "  buildinfo [--format text|json]\n"            << "  config get [KEY] | set KEY=VALUE [--global]\n"            << "  schema show|check [--dir PATH]\n"            << "  schema export [--format json|md]\n"            << "  index build|check|drop|lookup --id ID [--dir PATH]\n"            << "  dataset hash [--dir PATH] [--format json|text]\n"            << "  dataset diff --dir2 PATH [--dir PATH] [--output json|text]\n"            << "  env [--dir PATH]\n"            << "  lock status|on|off [--dir PATH]\n"            << "  export products|orders --format jsonl|csv [--dir PATH]\n"            << "  snapshot write --out DIR [--dir PATH]\n            << "  backup create --out DIR [--dir PATH]\n            backup list --dir PATH\n            backup verify --file FILE.tar\n            backup restore --file FILE.tar [--dir PATH]\n            backup prune --dir DIR --keep N\n""            << "  snapshot restore --from DIR [--dir PATH]\n"            << "  orders summarize [--dir PATH] [--format json|text] [--top N]\n"            << "  orders compact [--dir PATH]\n"            << "  search orders --product-id ID [--dir PATH]\n"            << "  orders anomalies [--dir PATH] [--format json|text] [--delta-pct D]\n"            << "  import products --from FILE.jsonl [--dir PATH]\n"            << "  replicate push --to DIR [--dir PATH]\n"            << "  lint ids [--dir PATH]\n"            << "  sbom generate [--dir PATH] [--output json|text]\n"            << "  fs readonly [--dir PATH]\n"            << "  policy init [--dir PATH]\n"            << "  why [--dir PATH]\n"
            << "  tx begin|status|run -- <args...> | commit | abort [--dir PATH]\n"
            << "  shell\n"            << "  newid [--format ulid|uuid4]\n"
            << "  list-orders [--output text|json|tsv|yaml] [--min-total V] [--max-total V] [--filter SUBSTR]\n"
            << "  doctor --dir PATH\n"
            << "  repair-integrity --dir PATH\n"
            << "  audit verify --dir PATH\n"
            << "  audit show --dir PATH\n"
            << "  audit rotate --dir PATH\n"
            << "  sell --id ID --qty QTY\n"
            << "  save --dir PATH\n"
            << "  stats\n"
            << "  health\n"
            << "  validate [--dir PATH]\n"
            << "  schema\n"
            << "  config generate [--out FILE]\n"
            << "  load --dir PATH\n"
            << "Options:\n"
            << "  --help, --version\n";
}

static std::string arg_or(const std::map<std::string,std::string>& args, const std::string& k, const std::string& d="") {
  auto it = args.find(k);
  return it==args.end() ? d : it->second;
}

static std::map<std::string,std::string> parse_kv(int argc, char** argv) {

  std::map<std::string,std::string> m;
  for (int i=2;i<argc;++i) {
    std::string a = argv[i];
    // --key=value support
    auto pos_eq = a.find('=');
    if (a.rfind("--",0)==0 && pos_eq!=std::string::npos) {
      std::string k=a.substr(0,pos_eq); std::string v=a.substr(pos_eq+1);
      if (!v.empty() && v.front()=='"' && v.back()=='"' && v.size()>=2) { v=v.substr(1,v.size()-2);} 
      m[k]=v; continue; }
    
    std::string a = argv[i];
    if (a.rfind("--",0)==0) {
      std::string k = a;
      std::string v = "1";
      if (i+1<argc) {
        std::string n = argv[i+1];
        if (n.rfind("--",0)!=0) { v=n; ++i; }
      }
      m[k]=v;
    }
  }
  return m;
}

static void merge_kv_file(std::map<std::string,std::string>& m, const std::string& path){
  std::ifstream f(path);
  if(!f.good()) return;
  std::string line;
  while(std::getline(f,line)){
    if(line.empty() || line[0]=='#') continue;
    auto eq=line.find('=');
    if(eq==std::string::npos) continue;
    std::string k=line.substr(0,eq), v=line.substr(eq+1);
    auto trim=[&](std::string&s){ while(!s.empty() && (s.back()=='\r'||s.back()=='\n'||s.back()==' '||s.back()=='\t')) s.pop_back(); while(!s.empty() && (s.front()==' '||s.front()=='\t')) s.erase(s.begin()); };
    trim(k); trim(v);
    if(k=="BACKEND") m.emplace("--backend", v);
    else if(k=="DATA_DIR") m.emplace("--dir", v);
    else if(k=="DSN") m.emplace("--dsn", v);
    else if(k=="LANG") m.emplace("--lang", v);
    else if(k=="INTEGRITY") m.emplace("--integrity", v);
  }
}
static void load_config_into(std::map<std::string,std::string>& m){
  // explicit --config
  auto it = m.find("--config");
  if (it != m.end() && !it->second.empty()) { merge_kv_file(m, it->second); return; }
  // env NEONSTORE_CONFIG
  if (const char* e = std::getenv("NEONSTORE_CONFIG"); e && *e){ merge_kv_file(m, e); return; }
  // XDG
  std::string home = std::getenv("HOME")? std::getenv("HOME"):std::string("");
  std::string xdg = std::getenv("XDG_CONFIG_HOME")? std::getenv("XDG_CONFIG_HOME"): (home.empty()? std::string("") : (home+"/.config"));
  if(!xdg.empty()) merge_kv_file(m, xdg+"/neonstore/neonstore.conf");
}


#include <clocale>
int main(int argc, char** argv) {
  std::setlocale(LC_ALL, "C");

  std::string prog = argv[0];
  std::setlocale(LC_ALL,"C");
  if (argc<=1) { print_help(); return 1; }
  std::string cmd = argv[1];
  /* LOCALE_INIT */
  try {
    std::locale::global(std::locale::classic());
    std::cout.imbue(std::locale::classic());
    std::cerr.imbue(std::locale::classic());
  } catch(...) {}

  /* AUDIT START */
  try{
    auto m0 = parse_kv(argc, argv);
    std::string dir0 = arg_or(m0, "--dir", data_dir);
    std::filesystem::path flag = std::filesystem::path(dir0)/".neonstore"/"audit.on";
    std::error_code ec;
    if (std::filesystem::exists(flag, ec)){
      std::filesystem::path logp = std::filesystem::path(dir0)/".neonstore"/"audit.log";
      std::ofstream out(logp.string(), std::ios::app);
      out<<now_iso()<<" cmd="; for(int i=0;i<argc;i++){ if(i) out<<' '; out<<argv[i]; } out<<"\n";
    }
  } catch(...) { /* ignore */ }

  /* WORM GUARD */
  {
    // determine dataset dir if provided
    auto m0 = parse_kv(argc, argv);
    std::string dir0 = arg_or(m0, "--dir", data_dir);
    bool worm = worm_active(dir0);
    // commands allowed during WORM
    std::set<std::string> allow = {"help","version","why","env","schema","dataset","verify-all","orders","products","sbom","search","fs","config","lock","worm"};
    if (worm && !allow.count(cmd)){
      std::cerr<<"worm_active\n"; return 13;
    }
  }

  if (cmd=="--help" || cmd=="help") { print_help(); return 0; }
  if (cmd=="--version" || cmd=="version") { std::cout << NEONSTORE_VERSION << "\n"; return 0; }

  Inventory inv;
  std::vector<Order> orders;
  // global args
  auto args = parse_kv(argc, argv);
  load_config_into(args);
  if (args.count("--no-color")) no_color=true; if (args.count("--quiet")) quiet=true; if (args.count("--dry-run")) dry_run=true; if (args.count("--profile")) profile=true; if (args.count("--read-only")) read_only=true;
  if (args.count("--lang")) lang = args.at("--lang");
  if (args.count("--verbose")) verbose=true;
  // env overrides
  auto getenv_s = [](const char* k){ const char* v = std::getenv(k); return v?std::string(v):std::string(); };
  if(lang=="en"){ auto L=getenv_s("LANG"); if(!L.empty() && (L.rfind("es",0)==0 || L.rfind("es_",0)==0)) lang="es"; }
  if (!args.count("--backend")) { auto v=getenv_s("NEONSTORE_BACKEND"); if(!v.empty()) args["--backend"]=v; }
  if (!args.count("--dir"))     { auto v=getenv_s("NEONSTORE_DIR");     if(!v.empty()) args["--dir"]=v; }
  if (!args.count("--dsn"))     { auto v=getenv_s("NEONSTORE_DSN");     if(!v.empty()) args["--dsn"]=v; }
  // config
  if (args.count("--config")) {
    auto cfg = load_config(args["--config"]);
    if (!args.count("--backend")) args["--backend"]=cfg.backend;
    if (!args.count("--dir")) args["--dir"]=cfg.data_dir;
    if (!args.count("--dsn")) args["--dsn"]=cfg.dsn;
  }
  // integrity
  // Policy settings (from args/config/env)
  const std::string pol_id = arg_or(args,"--policy-id-regex", getenv_s("NEONSTORE_POLICY_ID_REGEX"));
  const std::string pol_pmin = arg_or(args,"--policy-price-min", getenv_s("NEONSTORE_POLICY_PRICE_MIN"));
  const std::string pol_pmax = arg_or(args,"--policy-price-max", getenv_s("NEONSTORE_POLICY_PRICE_MAX"));
  const std::string pol_smin = arg_or(args,"--policy-stock-min", getenv_s("NEONSTORE_POLICY_STOCK_MIN"));
  const std::string pol_smax = arg_or(args,"--policy-stock-max", getenv_s("NEONSTORE_POLICY_STOCK_MAX"));
  auto enforce_id=[&](const std::string& id){ if(!pol_id.empty()){ try{ std::regex re(pol_id); if(!std::regex_match(id,re)){ throw std::runtime_error("ID violates policy regex"); } } catch(const std::exception& e){ throw; } } };
  auto enforce_price=[&](double pr){ if(!pol_pmin.empty()){ try{ if(pr < std::stod(pol_pmin)) throw std::runtime_error("price below policy min"); }catch(...){ throw; } } if(!pol_pmax.empty()){ try{ if(pr > std::stod(pol_pmax)) throw std::runtime_error("price above policy max"); }catch(...){ throw; } } };
  auto enforce_stock=[&](int st){ if(!pol_smin.empty()){ try{ if(st < std::stoi(pol_smin)) throw std::runtime_error("stock below policy min"); }catch(...){ throw; } } if(!pol_smax.empty()){ try{ if(st > std::stoi(pol_smax)) throw std::runtime_error("stock above policy max"); }catch(...){ throw; } } };

  auto im = arg_or(args,"--integrity","lenient");
  if (im=="off") CsvIntegrity::set(Integrity::Off); else if (im=="strict") CsvIntegrity::set(Integrity::Strict); else CsvIntegrity::set(Integrity::Lenient);
  std::string data_dir = arg_or(args,"--dir","data");
  // integrity algo
  auto ialgo = arg_or(args,"--integrity-algo","crc32");
  if (ialgo=="sha256") CsvIntegrity::set_algo(IntegrityAlgo::SHA256); else CsvIntegrity::set_algo(IntegrityAlgo::CRC32);
  // lock mode
  auto lkm = arg_or(args,"--lock-mode","dir"); if(lkm=="os") CsvLock::set_mode(LockMode::OS); else CsvLock::set_mode(LockMode::Dir);
  // storage factory
  std::unique_ptr<IStorage> storage;
  if(quiet) std::cout.setstate(std::ios_base::failbit);
  std::string backend = arg_or(args,"--backend","csv");
#ifdef NEONSTORE_ENABLE_SQLITE
  if (backend=="sqlite") storage = make_sqlite_storage(arg_or(args,"--dsn","neonstore.db"));
  else storage = make_csv_storage(arg_or(args,"--dir","data"));
#else
  if (backend!="csv") { std::cerr << "SQLite backend not enabled at build time\n"; return 3; }
  storage = make_csv_storage(arg_or(args,"--dir","data"));
#endif

  try {
    auto args = parse_kv(argc, argv);
  load_config_into(args);

    if (cmd=="add-product") {
      Product p;
      p.id = arg_or(args,"--id");
      p.name = arg_or(args,"--name");
      p.price = std::stod(arg_or(args,"--price","0"));
      p.stock = std::stoi(arg_or(args,"--stock","0"));
      if(!dry_run) inv.add_product(p); auto t1=tick(); if(!dry_run) audit_append(data_dir, "add-product", std::string("{")+jkv("id",id)+","+jkv("name",name)+"}"); if(profile) std::cerr << "[profile] add-product ms=" << ms(t0,t1) << "\n";
      std::cout << tr("Added product: ","Producto agregado: ") << p.id << "\n";
    } else if (cmd=="list-products") {
      auto fmt = arg_or(args,"--output","text");
      auto v = inv.list_products();
      size_t off = 0, lim = v.size();
      try { auto s=arg_or(args,"--offset","0"); off = (size_t)std::stoull(s);} catch(...) {}
      try { auto s=arg_or(args,"--limit", std::to_string(v.size())); lim = (size_t)std::stoull(s);} catch(...) {}
      size_t end = std::min(v.size(), off+lim);
      std::vector<Product> sv(v.begin() + std::min(off, v.size()), v.begin() + end);
      if (fmt=="json") {
        std::cout << "[\n";
        for (size_t i=0;i<sv.size();++i) {
          const auto& p = sv[i];
          std::cout << "  {\"id\":\"" << json_escape(p.id) << "\",\"name\":\"" << json_escape(p.name)
                    << "\",\"price\":" << p.price << ",\"stock\":" << p.stock << "}";
          if (i+1<sv.size()) std::cout << ","; std::cout << "\n";
        }
        std::cout << "]\n";
      } else if (fmt=="yaml") {
        for (const auto& p : work) { std::cout << "- id: " << p.id << "\n  name: " << p.name << "\n  price: " << p.price << "\n  stock: " << p.stock << "\n"; }
      } else if (fmt=="jsonl") {
        for (const auto& p : sv) {
          std::cout << "{\"id\":\"" << json_escape(p.id) << "\",\"name\":\"" << json_escape(p.name)
                    << "\",\"price\":" << p.price << ",\"stock\":" << p.stock << "}\n";
        }
      } else if (fmt=="tsv") {
        std::cout << "[\n";
        for (size_t i=0;i<sv.size();++i) {
          const auto& p = sv[i];
          std::cout << "  {\"id\":\"" << json_escape(p.id) << "\",\"name\":\"" << json_escape(p.name)
                    << "\",\"price\":" << p.price << ",\"stock\":" << p.stock << "}";
          if (i+1<v.size()) std::cout << ",";
          std::cout << "\n";
        }
        std::cout << "]\n";
      } else if (fmt=="tsv") {
        std::cout << "id\tname\tprice\tstock\n";
        for (const auto& p : sv) { std::cout << p.id << "\t" << p.name << "\t" << p.price << "\t" << p.stock << "\n"; }
        if (v.empty()) std::cout << tr("(no products)","(sin productos)") << "\n";
      } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; }
        std::cout << "[\n";
        for (size_t i=0;i<sv.size();++i) {
          const auto& p = sv[i];
          std::cout << "  {\"id\":\"" << json_escape(p.id) << "\",\"name\":\"" << json_escape(p.name)
                    << "\",\"price\":" << p.price << ",\"stock\":" << p.stock << "}";
          if (i+1<v.size()) std::cout << ",";
          std::cout << "\n";
        }
        std::cout << "]\n";
      } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; }
        for (const auto& p : sv) {
          std::cout << p.id << " | " << p.name << " | $" << p.price << " | stock=" << p.stock << "\n";
        }
        if (v.empty()) std::cout << "(no products)\n";
      }
    } else if (cmd=="restock") {
      if(!cas_guard(args, data_dir)) return 6;
enforce_stock(qty);
      if(!must_allow_write()) return 4;
      auto id = arg_or(args,"--id");
      int qty = std::stoi(arg_or(args,"--qty","0"));
      auto t0=tick(); if(!dry_run) inv.restock(id, qty); auto t1=tick(); if(!dry_run) audit_append(data_dir, "restock", std::string("{")+jkv("id",id)+",\"qty\":"+std::to_string(qty)+"}"); if(profile) std::cerr << "[profile] restock ms=" << ms(t0,t1) << "\n";
      std::cout << tr("Restocked ","Reabastecido ") << id << " by " << qty << "\n";
    } else if (cmd=="sell") {
enforce_stock(qty);
      auto id = arg_or(args,"--id");
      int qty = std::stoi(arg_or(args,"--qty","0"));
      auto o = inv.sell(id, qty);
      if (o.id.empty()) { o.id = "order-" + std::to_string(static_cast<unsigned long long>(std::time(nullptr))); }
      orders.push_back(o);
      std::cout << tr("Sold ","Vendido ") << qty << " of " << id << "; total=" << o.total() << "\n";
    } else if (cmd=="save") {
      auto dir = arg_or(args,"--dir","data");
      storage->save_products(inv);
      storage->save_orders(orders);
      std::cout << tr("Saved to ","Guardado en ") << dir << "\n";
    } else if (cmd=="load") {
      auto dir = arg_or(args,"--dir","data");
      storage->load_products(inv);
      storage->load_orders(orders);
      std::cout << tr("Loaded from ","Cargado desde ") << dir << "\n";
    } else if (cmd=="stats") {
      auto v = inv.list_products();
      size_t n = v.size(); long long stock=0; double value=0.0, minp=1e300, maxp=0.0;
      for (const auto& p : v){ stock += p.stock; value += p.price * p.stock; if(p.price<minp)minp=p.price; if(p.price>maxp)maxp=p.price; }
      double avg = n? (value/ (double)stock) : 0.0;
      std::cout << tr("products=","productos=") << n << "\n"
                << tr("total_stock=","stock_total=") << stock << "\n"
                << tr("inventory_value=","valor_inventario=") << value << "\n"
                << tr("avg_price=","precio_promedio=") << avg << "\n"
                << tr("min_price=","precio_min=") << (n?minp:0.0) << "\n"
                << tr("max_price=","precio_max=") << (n?maxp:0.0) << "\n";
    } else if (cmd=="health") {
      std::cout << "version=" << NEONSTORE_VERSION;
#ifdef NEONSTORE_GIT_SHA
      std::cout << " git=" << NEONSTORE_GIT_SHA;
#endif
      std::cout << "\nfeatures=" << (no_color?"no-color ":"") << (verbose?"verbose ":"");
#ifdef NEONSTORE_ENABLE_SQLITE
      std::cout << "sqlite ";
#endif
#ifdef NEONSTORE_THREADSAFE
      std::cout << "threadsafe ";
#endif
      std::cout << "\nOK\n";
    } else if (cmd=="validate") {
      // Optionally load from directory first
      auto dir = arg_or(args,"--dir","");
      if (!dir.empty()) { storage->load_products(inv); storage->load_orders(orders); }
      // Validate products
      std::map<std::string,int> ids;
      std::vector<std::string> issues;
      for (const auto& p : inv.list_products()) {
        if (p.id.empty()) issues.push_back("empty_id");
        if (p.price < 0.0) issues.push_back("negative_price:" + p.id);
        if (p.stock < 0) issues.push_back("negative_stock:" + p.id);
        ids[p.id]++;
      }
      for (auto& kv : ids) if (kv.second>1) issues.push_back(std::string("duplicate_id:")+kv.first);
      std::cout << "{\"ok\":" << (issues.empty()?"true":"false") << ",\"issues\":["";
      for (size_t i=0;i<issues.size();++i){ std::cout << issues[i]; if (i+1<issues.size()) std::cout << "\",\""; }
      std::cout << ""]}\n";
      std::cout << "version=" << NEONSTORE_VERSION;
#ifdef NEONSTORE_GIT_SHA
      std::cout << " git=" << NEONSTORE_GIT_SHA;
#endif
      std::cout << "\nfeatures=" << (no_color?"no-color ":"") << (verbose?"verbose ":"");
#ifdef NEONSTORE_ENABLE_SQLITE
      std::cout << "sqlite ";
#endif
#ifdef NEONSTORE_THREADSAFE
      std::cout << "threadsafe ";
#endif
      std::cout << "\nOK\n";
      auto dir = arg_or(args,"--dir","data");
      storage->load_products(inv);
      storage->load_orders(orders);
      std::cout << tr("Loaded from ","Cargado desde ") << dir << "\n";
    } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; }
      std::cerr << tr("Unknown command: ","Comando desconocido: ") << cmd << "\n";
      print_help();
      return 1;
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 2;
  }

  return 0;
}


    // separate parsing for migrate (use argv directly to not conflict with earlier map)
    else if (cmd=="migrate") {
      std::map<std::string,std::string> m = parse_kv(argc, argv);
      auto fb = m.count("--from-backend")? m["--from-backend"] : "csv";
      auto tb = m.count("--to-backend")? m["--to-backend"] : "csv";
      std::unique_ptr<IStorage> src, dst;
#ifdef NEONSTORE_ENABLE_SQLITE
      src = (fb=="sqlite") ? make_sqlite_storage(m.count("--from-dsn")?m["--from-dsn"]:"neonstore.db")
                           : make_csv_storage(m.count("--from-dir")?m["--from-dir"]:"data");
      dst = (tb=="sqlite") ? make_sqlite_storage(m.count("--to-dsn")?m["--to-dsn"]:"neonstore.db")
                           : make_csv_storage(m.count("--to-dir")?m["--to-dir"]:"data");
#else
      if (fb=="sqlite" || tb=="sqlite") { std::cerr << "SQLite backend not enabled at build time\n"; return 3; }
      src = make_csv_storage(m.count("--from-dir")?m["--from-dir"]:"data");
      dst = make_csv_storage(m.count("--to-dir")?m["--to-dir"]:"data");
#endif
      Inventory inv2; std::vector<Order> ord2;
      src->load_products(inv2);
      src->load_orders(ord2);
      dst->save_products(inv2);
      dst->save_orders(ord2);
      std::cout << "migrated" << "\n"; if(!dry_run) audit_append(data_dir, "migrate", std::string("{")+jkv("from",fb)+","+jkv("to",tb)+"}");
    }


else if (cmd=="backup") {
  auto dir = arg_or(args,"--dir","data");
  auto out = arg_or(args,"--out","backup"); auto keep = arg_or(args,"--keep","");
  std::error_code ec; std::filesystem::create_directories(out, ec);
  auto ts = std::to_string(std::time(nullptr));
  auto cp = [&](const char* fn){ std::filesystem::copy_file(dir + std::string("/") + fn, out + std::string("/") + fn + std::string("." ) + ts, std::filesystem::copy_options::overwrite_existing, ec); std::filesystem::copy_file(dir + std::string("/") + fn + std::string(".crc32"), out + std::string("/") + fn + std::string("." ) + ts + std::string(".crc32"), std::filesystem::copy_options::overwrite_existing, ec); };
  cp("products.csv"); cp("orders.csv");
  std::cout << tr("Backup created in ","Respaldo creado en ") << out << "\n";
      /* WRITE_MANIFEST */
      {
        std::vector<std::string> files; for (auto& e: std::filesystem::directory_iterator(out)) if(e.is_regular_file()) files.push_back(e.path().string());
        std::sort(files.begin(), files.end());
        std::ofstream mf((std::filesystem::path(out)/"MANIFEST.txt").string(), std::ios::binary|std::ios::trunc);
        auto shasum=[&](const std::string& p){ std::ifstream f(p, std::ios::binary); std::string s((std::istreambuf_iterator<char>(f)),{}); unsigned char h[32];
          #ifndef _WIN32
          // simple portable SHA256 via our header
          std::string hex = Sha256::of_string(s);
          return hex;
          #else
          std::string hex = Sha256::of_string(s); return hex; #endif };
        for (auto& fpath: files){ mf << shasum(fpath) << "  " << std::filesystem::path(fpath).filename().string() << "\n"; }
      }
      /* PRUNE_BACKUPS */
      if(!keep.empty()) { try { int k=std::stoi(keep); if(k>=0){ std::vector<std::string> files; for (auto& e: std::filesystem::directory_iterator(out)) if(e.is_regular_file()) files.push_back(e.path().string()); std::sort(files.begin(), files.end()); if ((int)files.size()>k) { for (size_t i=0;i<files.size()-k;++i) { std::error_code ec; std::filesystem::remove(files[i], ec);} } } } catch(...) {} } if(!dry_run) audit_append(data_dir, "backup", std::string("{")+jkv("out",out)+"}");
}
else if (cmd=="restore") {
  auto dir = arg_or(args,"--dir","data");
  auto src = arg_or(args,"--from","");
  if (src.empty()) { std::cerr << "missing --from" << "\n"; return 1; }
  std::error_code ec; std::filesystem::create_directories(dir, ec);
  auto mv = [&](const char* base){ std::filesystem::copy_file(src + std::string("/") + base, dir + std::string("/") + base, std::filesystem::copy_options::overwrite_existing, ec); if(!std::filesystem::exists(src + std::string("/") + base + std::string(".crc32"))) return; std::filesystem::copy_file(src + std::string("/") + base + std::string(".crc32"), dir + std::string("/") + base + std::string(".crc32"), std::filesystem::copy_options::overwrite_existing, ec); };
  mv("products.csv"); mv("orders.csv");
  std::cout << tr("Restored from ","Restaurado desde ") << src << "\n"; if(!dry_run) audit_append(data_dir, "restore", std::string("{")+jkv("from",src)+"}");
}
else if (cmd=="benchmark") {
  int iters = 10000; try { iters = std::stoi(arg_or(args,"--iters","10000")); } catch(...) {}
  auto t0 = std::chrono::steady_clock::now();
  for (int i=0;i<iters;++i){ Product p{.id="B"+std::to_string(i), .name="N", .price=1.0, .stock=1}; try{ if(!dry_run) inv.add_product(p); auto t1=tick(); if(!dry_run) audit_append(data_dir, "add-product", std::string("{")+jkv("id",id)+","+jkv("name",name)+"}"); if(profile) std::cerr << "[profile] add-product ms=" << ms(t0,t1) << "\n";}catch(...){} }
  auto t1 = std::chrono::steady_clock::now();
  auto d_add = std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
  auto v = inv.list_products();
  auto t2 = std::chrono::steady_clock::now();
  long long stock=0; for (const auto& p : v) stock += p.stock;
  auto t3 = std::chrono::steady_clock::now();
  auto d_list = std::chrono::duration_cast<std::chrono::milliseconds>(t3-t2).count();
  std::cout << "metric\top\tms\n" << "add\t" << iters << "\t" << d_add << "\n" << "sum\t" << v.size() << "\t" << d_list << "\n";
}


else if (cmd=="list-orders") {
  auto fmt = arg_or(args,"--output","text");
  if (fmt=="json") {
    std::cout << "[\n";
    for (size_t i=0;i<orders.size();++i) {
      const auto& o = orders[i];
      std::cout << "  {\"id\":\"" << json_escape(o.id) << "\",\"items\":" << o.items.size() << ",\"total\":" << o.total() << "}";
      if (i+1<orders.size()) std::cout << ","; std::cout << "\n";
    }
    std::cout << "]\n";
  } else if (fmt=="tsv") {
    std::cout << "id\titems\ttotal\n";
    for (const auto& o : orders) std::cout << o.id << "\t" << o.items.size() << "\t" << o.total() << "\n";
  } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; }
    for (const auto& o : orders) std::cout << o.id << " items=" << o.items.size() << " total=" << o.total() << "\n";
  }
}


else if (cmd=="doctor") {

      if (argc>2 && std::string(argv[2])=="checkup"){
        auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir); bool jout = arg_or(m,"--output","json")=="json";
        Inventory inv; std::vector<Order> oo; auto st=make_csv_storage(dir); st->load_products(inv); st->load_orders(oo);
        std::set<std::string> ids; for (auto&p: inv.list_products()) ids.insert(p.id);
        int missing=0; for (auto&o: oo){ for (auto &it: o.items){ if (!ids.count(it.product_id)) missing++; } }
        int bad=0; for (auto &p: inv.list_products()){ if (p.price<0 || p.stock<0) bad++; }
        for (auto &o: oo){ for (auto &it: o.items){ if (it.quantity<=0 || it.unit_price<0) bad++; } }
        std::error_code ec; std::filesystem::path neond = std::filesystem::path(dir)/".neonstore";
        bool seal_ok=false, ledger_ok=false;
        auto sp = neond/"seal.json"; if (std::filesystem::exists(sp, ec)){
          std::ifstream f(sp.string()); std::string s; std::getline(f,s);
          auto getv=[&](const std::string& k)->std::string{ auto p=s.find("\""+k+"\":\""); if(p==std::string::npos) return ""; size_t q=s.find("\"", p+k.size()+4); return s.substr(p+k.size()+4, q-(p+k.size()+4)); };
          auto rd=[&](const std::string& f)->std::string{ std::ifstream in((std::filesystem::path(dir)/f).string(), std::ios::binary); std::ostringstream ss; ss<<in.rdbuf(); std::string s=ss.str(); std::string out; out.reserve(s.size()); for(size_t i=0;i<s.size();++i){ char c=s[i]; if(c=='\r'){ if(i+1<s.size()&&s[i+1]=='\n'){ continue; } else continue; } else out.push_back(c);} return out; };
          auto hash = getv("hash"); auto prod = rd("products.csv"); auto ord = rd("orders.csv"); if (!hash.empty()){ seal_ok = (sha256_hex(prod+ord)==hash); }
        }
        auto lp = neond/"ledger.log"; if (std::filesystem::exists(lp, ec)){
          std::ifstream in(lp.string()); std::string line, prev(64,'0'); bool ok=true;
          while(std::getline(in,line)){
            auto getv=[&](const std::string& key)->std::string{ auto p=line.find("\""+key+"\":\""); if(p==std::string::npos) return ""; size_t q=line.find("\"", p+key.size()+4); return line.substr(p+key.size()+4, q-(p+key.size()+4)); };
            std::string ds = getv("dataset"), ts = getv("ts"), pr = getv("prev"), en = getv("entry");
            if (pr!=prev){ ok=false; break; }
            std::string recompute = sha256_hex(prev + ds + ts);
            if (recompute!=en){ ok=false; break; }
            prev = en;
          }
          ledger_ok = ok;
        }
        if (jout){
          std::cout<<"{\"missing_refs\":"<<missing<<",\"bad_rows\":"<<bad<<",\"seal_ok\":"<<(seal_ok?"true":"false")<<",\"ledger_ok\":"<<(ledger_ok?"true":"false")<<"}\n";
        } else {
          std::cout<<"missing_refs="<<missing<<"\nbad_rows="<<bad<<"\nseal_ok="<<(seal_ok?"true":"false")<<"\nledger_ok="<<(ledger_ok?"true":"false")<<"\n";
        }
        return (missing==0 && bad==0 && (!std::filesystem::exists(sp,ec) || seal_ok) && (!std::filesystem::exists(lp,ec) || ledger_ok))?0:2;
      }

  auto dir = arg_or(args,"--dir","data");
  std::vector<std::string> issues;
  // dir exists?
  if (!std::filesystem::exists(dir)) issues.push_back("missing_data_dir");
  // verify CRCs (if present)
  auto chk=[&](const char* base){ std::string reason; std::ifstream side(std::string(dir)+"/"+base+".crc32"); if(side.good()){ /* provoke loader to check */ try{ if(std::string(base)=="products.csv"){ Inventory tmp; CsvStorage::load_products(tmp, dir); } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; } std::vector<Order> oo; CsvStorage::load_orders(oo, dir);} } catch(const std::exception& e){ issues.push_back(std::string("integrity_")+base+":"+e.what()); } } };
  chk("products.csv"); chk("orders.csv");
  // duplicates/negatives
  std::map<std::string,int> ids;
  for (const auto& p : inv.list_products()){ ids[p.id]++; if(p.price<0) issues.push_back("negative_price:"+p.id); if(p.stock<0) issues.push_back("negative_stock:"+p.id); }
  for (auto& kv: ids) if (kv.second>1) issues.push_back(std::string("duplicate_id:")+kv.first);
  // report
  std::cout << "{\"ok\":" << (issues.empty()?"true":"false") << ",\"issues\":["";
  for (size_t i=0;i<issues.size();++i){ std::cout << issues[i]; if (i+1<issues.size()) std::cout << "\",\""; }
  std::cout << ""]}\n";
}


else if (cmd=="repair-integrity") {
  auto dir = arg_or(args,"--dir","data");
  // Force-write CRC sidecars by re-saving (no data change)
  CsvStorage::save_products(inv, dir);
  CsvStorage::save_orders(orders, dir);
  std::cout << tr("Integrity sidecars rebuilt in ","Integridad reconstruida en ") << dir << "\n";
}


else if (cmd=="audit") {

      if (argc>2 && std::string(argv[2])=="prune"){
        auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir",".neonstore"); int keep=5; try{ keep=std::stoi(arg_or(m,"--keep","5")); }catch(...){}
        std::vector<std::filesystem::directory_entry> files;
        std::error_code ec;
        for (auto &e: std::filesystem::directory_iterator(std::filesystem::path(dir), ec)){
          auto fn = e.path().filename().string();
          if (e.is_regular_file() && (fn.rfind("audit-",0)==0 || fn.rfind("audit.",0)==0 || fn=="audit.log")) files.push_back(e);
        }
        std::sort(files.begin(), files.end(), [](auto&a,auto&b){ return a.path().filename().string() < b.path().filename().string(); });
        int removed=0; while ((int)files.size() > keep){ auto p = files.front().path(); files.erase(files.begin()); std::filesystem::remove(p, ec); ++removed; }
        std::cout<<"{\"kept\":"<<files.size()<<",\"removed\":"<<removed<<"}\n"; return 0;
      }


// subcommands: export|tail|grep
if (argc>2 && (std::string(argv[2])=="export"||std::string(argv[2])=="tail"||std::string(argv[2])=="grep")){
  auto m = parse_kv(argc, argv); auto dir = arg_or(m,"--dir", data_dir); std::string out = arg_or(m,"--out",""); std::string fmt = arg_or(m,"--format","text"); int n=100; try{ n=std::stoi(arg_or(m,"--n","100")); }catch(...){ }
  std::string contains = arg_or(m,"--contains","");
  auto al = (std::filesystem::path(dir)/"audit.log").string(); auto lines = read_lines(al);
  std::vector<std::string> sel = lines;
  if (std::string(argv[2])=="tail" && (int)lines.size()>n) sel.assign(lines.end()-n, lines.end());
  if (!contains.empty()){ std::vector<std::string> tmp; for (auto& s: sel) if (s.find(contains)!=std::string::npos) tmp.push_back(s); sel.swap(tmp); }
  std::string buf;
  if (fmt=="jsonl"){ for (auto& s: sel){ buf += std::string("{\"line\":\"")+json_escape(s)+"\"}\n"; } }
  else { for (auto& s: sel){ buf += s + "\n"; } }
  if (out.empty()) std::cout << buf; else write_text(out, buf);
  return 0;
}

  std::string sub = argc>2 ? std::string(argv[2]) : std::string();
  auto m = parse_kv(argc, argv);
  auto dir = m.count("--dir")? m["--dir"] : "data";
  if (sub=="verify") {
    std::string reason; bool ok = audit_verify(dir, &reason);
    std::cout << "{\"ok\":" << (ok? "true":"false") << ",\"reason\":\"" << json_escape(reason) << "\"}\n";
  } else if (sub=="show") {
    std::ifstream f(audit_log_path(dir), std::ios::binary); std::string line; while (std::getline(f,line)) std::cout << line << "\n";
  } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; }
    std::cerr << "Usage: audit verify|show --dir PATH\n"; return 1;
  }
}


else if (cmd=="audit" && argc>2 && std::string(argv[2])=="rotate") {
  auto m = parse_kv(argc, argv);
  auto dir = m.count("--dir")? m["--dir"] : "data";
  std::string src = audit_log_path(dir);
  if (!std::filesystem::exists(src)) { std::cout << "no audit log\n"; }
  else {
    std::string dst = (std::filesystem::path(dir) / (std::string("audit-") + iso8601_utc() + ".log")).string();
    std::error_code ec; std::filesystem::rename(src, dst, ec);
    std::cout << "rotated to " << dst << "\n";
  }
}


else if (cmd=="seed") {
  int n=1000; try { n = std::stoi(arg_or(args,"--products","1000")); } catch(...) {}
  std::string prefix = arg_or(args,"--prefix","P");
  for (int i=0;i<n;++i) {
    Product p{.id=prefix+std::to_string(i), .name="Item "+std::to_string(i), .price=(i%100)*0.5 + 0.99, .stock=(i%50)};
    try { if(!dry_run) inv.add_product(p); } catch(...) {}
  }
  if(!dry_run) audit_append(data_dir, "seed", std::string("{")+ "\"products\":" + std::to_string(n) + "}");
  std::cout << "seeded " << n << " products\n";
}


else if (cmd=="diff") {
  auto A = arg_or(args,"--from-dir","");
  auto B = arg_or(args,"--to-dir","");
  auto fmt = arg_or(args,"--output","text");
  if (A.empty() || B.empty()) { std::cerr << "missing --from-dir/--to-dir\n"; return 1; }
  Inventory invA, invB; std::vector<Order> ordA, ordB;
  make_csv_storage(A)->load_products(invA); make_csv_storage(B)->load_products(invB);
  auto a = invA.list_products(); auto b = invB.list_products();
  std::map<std::string,Product> ma, mb; for (auto& p:a) ma[p.id]=p; for (auto& p:b) mb[p.id]=p;
  std::vector<std::string> added, removed, changed;
  for (auto& [id,p]: mb){ if (!ma.count(id)) added.push_back(id); else { const auto& q=ma[id]; if (q.name!=p.name || q.price!=p.price || q.stock!=p.stock) changed.push_back(id);} }
  for (auto& [id,p]: ma){ if (!mb.count(id)) removed.push_back(id); }
  if (fmt=="json") {
    std::cout << "{\"added\":[";
    for (size_t i=0;i<added.size();++i){ std::cout << "\"" << json_escape(added[i]) << "\""; if (i+1<added.size()) std::cout << ","; }
    std::cout << "],\"removed\":[";
    for (size_t i=0;i<removed.size();++i){ std::cout << "\"" << json_escape(removed[i]) << "\""; if (i+1<removed.size()) std::cout << ","; }
    std::cout << "],\"changed\":[";
    for (size_t i=0;i<changed.size();++i){ std::cout << "\"" << json_escape(changed[i]) << "\""; if (i+1<changed.size()) std::cout << ","; }
    std::cout << "]}\n";
  } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; }
    for (auto& id: added) std::cout << "+ " << id << "\n";
    for (auto& id: removed) std::cout << "- " << id << "\n";
    for (auto& id: changed) std::cout << "~ " << id << "\n";
  }
}


else if (cmd=="fingerprint") {
  auto dir = arg_or(args,"--dir","data");
  auto which = arg_or(args,"--which","all");
  std::string canon;
  if (which=="products" || which=="all") {
    Inventory invX; make_csv_storage(dir)->load_products(invX);
    auto v = invX.list_products(); std::sort(v.begin(), v.end(), [](const Product& a,const Product& b){return a.id<b.id;});
    canon += "P|"; for (auto& p:v){ canon += p.id + "|" + p.name + "|" + std::to_string(p.price) + "|" + std::to_string(p.stock) + "\n"; }
  }
  if (which=="orders" || which=="all") {
    std::vector<Order> oo; make_csv_storage(dir)->load_orders(oo);
    std::sort(oo.begin(), oo.end(), [](const Order& a,const Order& b){return a.id<b.id;});
    canon += "O|"; for (auto& o:oo){ canon += o.id + "|" + std::to_string(o.total()) + "\n"; }
  }
  std::string h = Sha256::of_string(canon);
  std::cout << h << "\n";
}


else if (cmd=="sanitize") {
  auto dir = arg_or(args,"--dir","data");
  bool up = args.count("--upper-ids");
  bool dry = args.count("--dry-run") || dry_run;
  Inventory invX; std::vector<Order> ordX;
  make_csv_storage(dir)->load_products(invX);
  make_csv_storage(dir)->load_orders(ordX);
  auto v = invX.list_products();
  int changed_rows=0, trimmed=0, ctrl_removed=0, uppered=0;
  auto clean_str=[&](std::string s){ std::string out; out.reserve(s.size()); for (unsigned char c : s){ if (c<32 && c!=9 && c!=10 && c!=13){ ++ctrl_removed; continue;} out.push_back(c);} // trim
    while(!out.empty() && (out.back()==' '||out.back()=='\t'||out.back()=='\r'||out.back()=='\n')){ out.pop_back(); ++trimmed; }
    size_t i=0; while(i<out.size() && (out[i]==' '||out[i]=='\t'||out[i]=='\r'||out[i]=='\n')){ out.erase(out.begin()+i); ++trimmed; } return out; };
  for (auto& p : v){
    std::string nid = clean_str(p.id);
    std::string nname = clean_str(p.name);
    if (up){ std::string tmp; tmp.reserve(nid.size()); for(char c : nid){ tmp.push_back((char)std::toupper((unsigned char)c)); } if (tmp!=nid){ nid=tmp; ++uppered; } }
    if (nid!=p.id || nname!=p.name){ ++changed_rows; }
    p.id = nid; p.name = nname;
  }
  if (!dry){
    Inventory out; for (auto& p : v) out.add_product(p);
    CsvStorage::save_products(out, dir);
    CsvStorage::save_orders(ordX, dir);
  }
  std::cout << "{\"changed\":" << changed_rows << ",\"trimmed\":" << trimmed << ",\"ctrl_removed\":" << ctrl_removed << ",\"uppered\":" << uppered << ",\"dry_run\":" << (dry? "true":"false") << "}\n";
  if (!dry) { audit_append(dir, "sanitize", "{\"rows\":" + std::to_string(changed_rows) + "}"); }
}


else if (cmd=="shell") {
  std::cout << "neonstore shell (type commands without the program name; Ctrl-D to exit)\n";
  std::string line;
  while (true) {
    std::cout << "> "; std::cout.flush();
    if (!std::getline(std::cin, line)) break;
    if (line.empty()) continue;
    std::string cmdline = std::string(""") + prog + "" " + line;
    int rc = std::system(cmdline.c_str());
    if (rc == -1) std::cerr << "failed to spawn command\n";
  }
  return 0;
}

else if (cmd=="newid") {
  std::cout << newid_ulid() << "\n";
}


else if (cmd=="trash") {
  std::string sub = argc>2 ? std::string(argv[2]) : std::string();
  auto m = parse_kv(argc, argv);
  auto dir = m.count("--dir")? m["--dir"] : data_dir;
  auto path = (std::filesystem::path(dir)/".trash.csv").string();
  if (sub=="list") {
    std::ifstream f(path, std::ios::binary); std::string line; while (std::getline(f,line)) std::cout << line << "\n";
  } else if (sub=="purge") {
    if(!must_allow_write()) return 4;
    std::error_code ec; std::filesystem::remove(path, ec); std::cout << "trash purged\n";
  } else if (sub=="undelete") {
    if(!must_allow_write()) return 4;
    auto id = m.count("--id")? m["--id"] : "";
    if (id.empty()) { std::cerr << "missing --id\n"; return 1; }
    std::ifstream f(path, std::ios::binary); std::string line; bool done=false;
    std::vector<std::string> keep;
    while (std::getline(f,line)) {
      if (line.rfind(id+",",0)==0 && !done) { // restore first match
        std::stringstream ss(line); std::string a,b,c,d;
        std::getline(ss,a,','); std::getline(ss,b,','); std::getline(ss,c,','); std::getline(ss,d,',');
        try { Product p{.id=a, .name=b, .price=std::stod(c), .stock=std::stoi(d)}; inv.add_product(p); done=true; }
        catch(...) { }
      } else keep.push_back(line);
    }
    // rewrite trash file without restored item
    { std::ofstream o(path, std::ios::binary|std::ios::trunc); for (auto& l: keep) o << l << "\n"; }
    std::cout << (done? "undeleted\n":"not found\n");
  } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; }
    std::cerr << "Usage: trash list|purge|undelete --id ID\n"; return 1;
  }
}


else if (cmd=="snapshot") {
      auto labels_path = [&](const std::string& dir){ return (std::filesystem::path(dir)/"snapshots"/"labels.tsv").string(); };
  std::string sub = argc>2 ? std::string(argv[2]) : std::string();
  auto m = parse_kv(argc, argv); auto dir = m.count("--dir")? m["--dir"] : data_dir;
  auto snaps = std::filesystem::path(dir) / ".snapshots";
  std::error_code ec; std::filesystem::create_directories(snaps, ec);
  if (sub=="create") {
    if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6;
    auto ts = iso8601_utc(); for(char&c:ts){ if(c==':'||c=='T') c='-'; }
    auto d = snaps / ts; std::filesystem::create_directories(d, ec);
    for (auto f : {"products.csv","orders.csv","products.csv.crc32","orders.csv.crc32","products.csv.sha256","orders.csv.sha256","audit.log"}) {
      std::filesystem::path src = std::filesystem::path(dir)/f;
      if (std::filesystem::exists(src)) { std::filesystem::copy_file(src, d/f, std::filesystem::copy_options::overwrite_existing, ec); }
    }
    std::cout << "snapshot " << ts << " created\n";
  } else if (sub=="list") {
        std::map<std::string,std::string> lab; { std::ifstream lf(labels_path(data_dir)); std::string line; while(std::getline(lf,line)){ auto p=line.find('\t'); if(p!=std::string::npos) lab[line.substr(0,p)]=line.substr(p+1); } }
    for (auto& e : std::filesystem::directory_iterator(snaps)) if (e.is_directory()) auto nm=e.path().filename().string(); auto it=lab.find(nm); if(it!=lab.end()) std::cout << nm << "  " << it->second << "\n"; else std::cout << nm << "\n";
  } else if (sub=="restore") {
    if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6;
    std::string ts = argc>3? std::string(argv[3]) : std::string();
    if (ts.empty()){ std::cerr << "missing timestamp\n"; return 1; }
    auto d = snaps / ts; if (!std::filesystem::exists(d)){ std::cerr << "snapshot not found\n"; return 1; }
    for (auto f : {"products.csv","orders.csv","products.csv.crc32","orders.csv.crc32","products.csv.sha256","orders.csv.sha256"}) {
      std::filesystem::path src = d/f; std::filesystem::path dst = std::filesystem::path(dir)/f;
      if (std::filesystem::exists(src)) { std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec); }
    }
    std::cout << "snapshot restored\n";
  } else if (sub=="prune") {
    if(!must_allow_write()) return 4;
    int keep=5; try{ keep=std::stoi(m["--keep"]); }catch(...){}
    std::vector<std::string> names; for (auto& e : std::filesystem::directory_iterator(snaps)) if (e.is_directory()) names.push_back(e.path().filename().string());
    std::sort(names.begin(), names.end()); if ((int)names.size()>keep){ for (size_t i=0;i<names.size()-keep;++i) { std::filesystem::remove_all(snaps/names[i], ec);} }
    std::cout << "snapshots pruned\n";
  } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; }
    std::cerr << "Usage: snapshot create|list|restore <TS>|prune --keep N\n"; return 1;
  }
}


else if (cmd=="metrics") {
  auto m = parse_kv(argc, argv); auto dir = m.count("--dir")? m["--dir"] : data_dir;
  std::string fmt = m.count("--format")? m["--format"] : "prometheus";
  Inventory invX; std::vector<Order> oo; make_csv_storage(dir)->load_products(invX); make_csv_storage(dir)->load_orders(oo);
  auto v = invX.list_products();
  size_t products=v.size(); size_t orders_n=oo.size();
  double inventory_value=0.0; for (auto&p: v) inventory_value += p.price * p.stock;
  double revenue=0.0; for (auto&o: oo) revenue += o.total();
  std::string audit_reason; bool audit_ok = audit_verify(dir, &audit_reason);
  if (fmt=="json") {
    std::cout << "{\"products\":" << products << ",\"orders\":" << orders_n << ",\"inventory_value\":" << inventory_value << ",\"revenue_total\":" << revenue << ",\"audit_ok\":" << (audit_ok? "true":"false") << ",\"audit_reason\":\"" << json_escape(audit_reason) << "\"}\n";
  } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; }
    std::cout << "# HELP neonstore_products_total Number of products\n# TYPE neonstore_products_total gauge\nneonstore_products_total " << products << "\n";
    std::cout << "# HELP neonstore_orders_total Number of orders\n# TYPE neonstore_orders_total gauge\nneonstore_orders_total " << orders_n << "\n";
    std::cout << "# HELP neonstore_inventory_value_total Total inventory value\n# TYPE neonstore_inventory_value_total gauge\nneonstore_inventory_value_total " << inventory_value << "\n";
    std::cout << "# HELP neonstore_revenue_total Total revenue\n# TYPE neonstore_revenue_total counter\nneonstore_revenue_total " << revenue << "\n";
    std::cout << "# HELP neonstore_audit_ok Audit verification (1 ok, 0 fail)\n# TYPE neonstore_audit_ok gauge\nneonstore_audit_ok " << (audit_ok? 1:0) << "\n";
  }
}


else if (cmd=="report") {
  std::string sub = argc>2 ? std::string(argv[2]) : std::string();
  if (sub=="sales") {
    auto m = parse_kv(argc, argv);
    int top=10; try{ top=std::stoi(arg_or(m,"--top","10")); }catch(...){}
    std::string by = arg_or(m,"--by","revenue");
    std::string out = arg_or(m,"--output","text");
    std::map<std::string, std::pair<int,double>> agg; // id -> (qty,revenue)
    for (auto& o : orders){ for (auto& it : o.items){ agg[it.product_id].first += it.quantity; agg[it.product_id].second += it.quantity * it.price; } }
    std::vector<std::tuple<std::string,int,double>> rows;
    for (auto& kv : agg) rows.emplace_back(kv.first, kv.second.first, kv.second.second);
    std::sort(rows.begin(), rows.end(), [&](const auto& A, const auto& B){ if (by=="quantity") return std::get<1>(A) > std::get<1>(B); else return std::get<2>(A) > std::get<2>(B); });
    if ((int)rows.size()>top) rows.resize(top);
    if (out=="json"){ std::cout << "[\n"; for (size_t i=0;i<rows.size();++i){ auto& r=rows[i]; std::cout << "  {\"id\":\"" << json_escape(std::get<0>(r)) << "\",\"qty\":" << std::get<1>(r) << ",\"revenue\":" << std::get<2>(r) << "}"; if(i+1<rows.size()) std::cout << ","; std::cout << "\n"; } std::cout << "]\n"; }
    else { for (auto& r: rows) std::cout << std::get<0>(r) << " qty=" << std::get<1>(r) << " revenue=" << std::get<2>(r) << "\n"; }
  } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; } std::cerr << "Usage: report sales [--top N] [--by revenue|quantity] [--output text|json]\n"; return 1; }
}


else if (cmd=="compact") {
  auto m = parse_kv(argc, argv); auto dir = m.count("--dir")? m["--dir"] : data_dir;
  if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6;
  Inventory invX; make_csv_storage(dir)->load_products(invX);
  auto v = invX.list_products();
  std::stable_sort(v.begin(), v.end(), [](const Product&a,const Product&b){return a.id<b.id;});
  std::vector<Product> outv; outv.reserve(v.size());
  std::string last; for (auto& p : v){ if(p.id!=last){ outv.push_back(p); last=p.id; } }
  Inventory out; for (auto& p: outv) out.add_product(p);
  CsvStorage::save_products(out, dir);
  std::cout << "compacted " << v.size() - outv.size() << " duplicates removed\n";
}


else if (cmd=="tx") {
  std::string sub = argc>2 ? std::string(argv[2]) : std::string();
  auto m = parse_kv(argc, argv);
  auto dir = m.count("--dir")? m["--dir"] : data_dir;
  auto tdir = (std::filesystem::path(dir)/".tx").string();
  if (sub=="begin") {
    if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6;
    if (std::filesystem::exists(tdir)) { std::cerr << "tx already active\n"; return 1; }
    std::error_code ec; std::filesystem::create_directories(tdir, ec);
    for (auto f : {"products.csv","orders.csv","products.csv.crc32","orders.csv.crc32","products.csv.sha256","orders.csv.sha256","audit.log"}) {
      std::filesystem::path src = std::filesystem::path(dir)/f;
      if (std::filesystem::exists(src)) std::filesystem::copy_file(src, std::filesystem::path(tdir)/f, std::filesystem::copy_options::overwrite_existing, ec);
    }
    std::cout << "tx started in " << tdir << "\n";
  } else if (sub=="status") {
    std::cout << (std::filesystem::exists(tdir) ? "active\n" : "inactive\n");
  } else if (sub=="run") {
    if (!std::filesystem::exists(tdir)) { std::cerr << "no active tx\n"; return 1; }
    int sep=-1; for (int i=0;i<argc;i++){ if(std::string(argv[i])=="--"){ sep=i; break; } }
    if (sep<0 || sep+1>=argc) { std::cerr << "Usage: tx run -- <neonstore args>\n"; return 1; }
    std::string cmdline = std::string(""") + argv[0] + "" ";
    for (int i=sep+1;i<argc;i++){ cmdline += std::string(argv[i]) + " "; }
    cmdline += std::string("--dir "") + tdir + """;
    int rc = std::system(cmdline.c_str());
    return (rc==-1)? 1 : (rc>>8);
  } else if (sub=="commit") {
    if(!must_allow_write()) return 4; if(!std::filesystem::exists(tdir)){ std::cerr << "no active tx\n"; return 1; }
    std::error_code ec;
    for (auto f : {"products.csv","orders.csv","products.csv.crc32","orders.csv.crc32","products.csv.sha256","orders.csv.sha256"}) {
      std::filesystem::path src = std::filesystem::path(tdir)/f;
      std::filesystem::path dst = std::filesystem::path(dir)/f;
      if (std::filesystem::exists(src)) std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    }
    std::filesystem::remove_all(tdir, ec);
    audit_append(dir, "tx-commit", "{}");
    std::cout << "tx committed\n";
  } else if (sub=="abort") {
    std::error_code ec; std::filesystem::remove_all(tdir, ec); std::cout << "tx aborted\n";
  } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; }
    std::cerr << "Usage: tx begin|status|run -- <args...> | commit | abort [--dir PATH]\n"; return 1;
  }
}


else if (cmd=="jsonrpc") {
  auto m = parse_kv(argc, argv);
  bool stdio = m.count("--stdio");
  if (!stdio) { std::cerr << "only --stdio supported" << "\n"; return 1; }
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) continue;
    auto method_pos = line.find("\"method\"");
    auto params_pos = line.find("\"params\"");
    auto id_pos = line.find("\"id\"");
    std::string method, params="{}", id="null";
    if (method_pos!=std::string::npos) { auto q1=line.find('"', method_pos+8); auto q2=line.find('"', q1+1); method=line.substr(q1+1, q2-q1-1); }
    if (params_pos!=std::string::npos) { auto b1=line.find('{', params_pos); auto b2=line.rfind('}'); if (b1!=std::string::npos && b2!=std::string::npos && b2>b1) params=line.substr(b1, b2-b1+1); }
    if (id_pos!=std::string::npos) { auto c=line.find(':', id_pos); if (c!=std::string::npos) id=line.substr(c+1); }
    std::string result="{\"ok\":true}"; int code=0;
    try {
      if (method=="ping") { result="{\"pong\":true}"; }
      else if (method=="listProducts") {
        auto v = inv.list_products();
        std::ostringstream oss; oss << "["; for (size_t i=0;i<v.size();++i){ const auto& p=v[i]; oss << "{\"id\":\"" << json_escape(p.id) << "\",\"name\":\"" << json_escape(p.name) << "\",\"price\":" << p.price << ",\"stock\":" << p.stock << "}"; if(i+1<v.size()) oss << ","; } oss << "]";
        result = oss.str();
      } else if (method=="fingerprint") {
        result = std::string("{\"fp\":\"") + compute_fp_all(data_dir) + "\"}";
      } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; } code=1; result="{\"error\":\"unknown method\"}"; }
    } catch(...) { code=2; result="{\"error\":\"exception\"}"; }
    std::cout << "{\"jsonrpc\":\"2.0\",\"id\":" << id << ",\"result\":" << result << "}" << std::endl;
  }
  return 0;
}


else if (cmd=="license") {
  std::string sub = argc>2 ? std::string(argv[2]) : std::string();
  if (sub!="check") { std::cerr << "Usage: license check\n"; return 1; }
  size_t files=0, with_spdx=0;
  for (auto& p : std::filesystem::recursive_directory_iterator(".")) {
    if (!p.is_regular_file()) continue;
    auto path=p.path().string();
    if (path.find("/build/")!=std::string::npos) continue;
    if (!(path.ends_with(".cpp")||path.ends_with(".hpp")||path.ends_with(".c")||path.ends_with(".h")||path.ends_with(".py")||path.ends_with(".md"))) continue;
    ++files;
    std::ifstream f(path, std::ios::binary);
    std::string s((std::istreambuf_iterator<char>(f)), {});
    if (s.find("SPDX-License-Identifier:")!=std::string::npos) ++with_spdx;
  }
  std::cout << "{\"files\":" << files << ",\"with_spdx\":" << with_spdx << ",\"coverage\":" << (files? (100.0*with_spdx/files):100.0) << "}\n";
}


    else if (cmd=="archive") {
      if (argc<3){ std::cerr << "Usage: archive create --out FILE [--dir PATH] | extract --file FILE [--dir PATH]\n"; return 1; }
      std::string sub = argv[2];
      auto m = parse_kv(argc, argv);
      auto dir = m.count("--dir")? m["--dir"] : data_dir;
      if (sub=="create") {
        if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6;
        std::string outp = arg_or(m, "--out", "");
        if (outp.empty()){ std::cerr << "missing --out\n"; return 1; }
        std::ofstream out(outp, std::ios::binary|std::ios::trunc);
        if(!out){ std::cerr << "cannot write " << outp << "\n"; return 1; }
        for (auto f : { "products.csv", "orders.csv", "products.csv.crc32", "orders.csv.crc32", "products.csv.sha256", "orders.csv.sha256", "audit.log" }) {
          auto p = (std::filesystem::path(dir)/f).string(); if (std::filesystem::exists(p)) tar_write_file(out, f, p);
        }
        tar_write_eof(out);
        std::cout << "archive written " << outp << "\n";
      } else if (sub=="extract") {
        if(!must_allow_write()) return 4;
        std::string file = arg_or(m, "--file", "");
        if (file.empty()){ std::cerr << "missing --file\n"; return 1; }
        std::ifstream in(file, std::ios::binary);
        if(!in){ std::cerr << "cannot read " << file << "\n"; return 1; }
        while (true){
          char h[512]; in.read(h,512); if(!in) break;
          bool allz=true; for(int i=0;i<512;i++){ if(h[i]!=0){ allz=false; break; } }
          if(allz){ char h2[512]; in.read(h2,512); break; }
          std::string name(h, h+100); name = name.c_str();
          std::string size_str(h+124, h+124+12); size_t sz = std::strtoul(size_str.c_str(), nullptr, 8);
          std::vector<char> buf(sz); in.read(buf.data(), (std::streamsize)sz);
          auto outp = (std::filesystem::path(dir)/name).string();
          std::filesystem::create_directories(std::filesystem::path(outp).parent_path());
          std::ofstream o(outp, std::ios::binary|std::ios::trunc); o.write(buf.data(), (std::streamsize)sz);
          size_t pad = (512 - (sz%512))%512; if (pad) in.ignore((std::streamsize)pad);
        }
        std::cout << "archive extracted to " << dir << "\n";
      } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; }
        std::cerr << "Usage: archive create --out FILE [--dir PATH] | extract --file FILE [--dir PATH]\n"; return 1;
      }
    }


    else if (cmd=="dataset") {
      auto lockp = (std::filesystem::path(data_dir)/"LOCKED"); if (argc>2) { std::string sub=argv[2]; if (sub=="lock"){ std::ofstream f(lockp.string()); f << "locked at " << iso8601_utc() << "\n"; std::cout << "locked\n"; return 0; } else if (sub=="unlock"){ std::error_code ec; std::filesystem::remove(lockp, ec); std::cout << "unlocked\n"; return 0; } else if (sub=="status"){ std::cout << (std::filesystem::exists(lockp)? "locked":"unlocked") << "\n"; return 0; } }
      std::string sub = argc>2? std::string(argv[2]) : std::string();
      if (sub=="label") { auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir",data_dir); if(argc<4){ std::cerr<<"Usage: snapshot label <TS> --label L [--dir PATH]\n"; return 1;} std::string ts=argv[3]; std::string L=arg_or(m,"--label",""); if(L.empty()){ std::cerr<<"missing --label\n"; return 1;} std::error_code ec; std::filesystem::create_directories(std::filesystem::path(dir)/"snapshots", ec); std::ofstream f(labels_path(dir), std::ios::app); f<<ts<<"\t"<<L<<"\n"; std::cout<<"labeled "<<ts<<" as "<<L<<"\n"; return 0; }
      auto m = parse_kv(argc, argv);
      auto dir = m.count("--dir")? m["--dir"] : data_dir;
      auto vfile = (std::filesystem::path(dir)/"DATASET_VERSION").string();
      if (sub=="version") {
        std::string sub2 = argc>3? std::string(argv[3]) : std::string();
        if (sub2=="show") {
          std::ifstream f(vfile); std::string v; std::getline(f,v); if(v.empty()) v="0"; std::cout << v << "\n";
        } else if (sub2=="set") {
          if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6;
          std::string val = arg_or(m,"--value","");
          if (val.empty()){ std::cerr << "missing --value\n"; return 1; }
          std::ofstream f(vfile, std::ios::binary|std::ios::trunc); f << val << "\n";
          audit_append(dir, "dataset-version", std::string("{\"value\":\"")+val+"\"}");
          std::cout << "dataset version set to " << val << "\n";
        } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; } std::cerr << "Usage: dataset version show|set --value V [--dir PATH]\n"; return 1; }
      } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; } std::cerr << "Usage: dataset version show|set --value V [--dir PATH]\n"; return 1; }
    }


else if (cmd=="buildinfo") {
  auto m = parse_kv(argc, argv); std::string fmt = arg_or(m,"--format","text"); bool includeOrders = m.count("--orders");
  const char* ts = NEONSTORE_BUILD_TS;
  const char* cid = NEONSTORE_COMPILER_ID;
  const char* cbt = NEONSTORE_BUILD_TYPE;
  const char* ver = NEONSTORE_VERSION; const char* sde = NEONSTORE_SOURCE_DATE_EPOCH;
  const char* sha = NEONSTORE_GIT_SHA;
  
if (fmt=="patch"){
        for (auto& id: added){ const auto& p = MR[id]; std::cout << "+ " << rfc4180_write_row({p.id,p.name,std::to_string(p.price),std::to_string(p.stock)}) << "\n"; }
        for (auto& id: removed){ const auto& p = ML[id]; std::cout << "- " << rfc4180_write_row({p.id,p.name,std::to_string(p.price),std::to_string(p.stock)}) << "\n"; }
        for (auto& id: changed){ const auto& p = MR[id]; std::cout << "~ " << rfc4180_write_row({p.id,p.name,std::to_string(p.price),std::to_string(p.stock)}) << "\n"; }
      } else if (fmt=="json"){
        std::cout << "{\"added\":["; for(size_t i=0;i<added.size();++i){ const auto& id=added[i]; std::cout << dump(id, MR[id]); if(i+1<added.size()) std::cout << ","; } std::cout << "],\"removed\":["; for(size_t i=0;i<removed.size();++i){ const auto& id=removed[i]; std::cout << dump(id, ML[id]); if(i+1<removed.size()) std::cout << ","; } std::cout << "],\"changed\":["; for(size_t i=0;i<changed.size();++i){ const auto& id=changed[i]; std::cout << "{\"id\":\"" << json_escape(id) << "\",\"left\":" << dump(id, ML[id]) << ",\"right\":" << dump(id, MR[id]) << "}"; if(i+1<changed.size()) std::cout << ","; } std::cout << "]"; if(includeOrdersDetail){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); std::vector<std::string> oa, ob; for(auto&id: ro) if(!lo.count(id)) oa.push_back(id); for(auto&id: lo) if(!ro.count(id)) ob.push_back(id); std::cout << ",\"orders\":{\"added\":["; for(size_t i=0;i<oa.size();++i){ std::cout << "\"" << json_escape(oa[i]) << "\""; if(i+1<oa.size()) std::cout << ","; } std::cout << "],\"removed\":["; for(size_t i=0;i<ob.size();++i){ std::cout << "\"" << json_escape(ob[i]) << "\""; if(i+1<ob.size()) std::cout << ","; } std::cout << "]}" ; } std::cout << "}\n";
        std::string sde_s = (sde && *sde)? std::string(",\"source_date_epoch\":\"")+sde+"\"" : std::string();

    std::cout << "{\"version\":\"" << ver << "\",\"git\":\"" << sha << "\",\"build_ts\":\"" << ts << "\",\"compiler\":\"" << cid << "\",\"build_type\":\"" << cbt << "\"" << sde_s << "}\n";
  } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; }
    std::cout << "version: " << ver << "\n" << "git: " << sha << "\n" << "built: " << ts << "\n" << "compiler: " << cid << "\n" << "type: " << cbt << "\n";
  }
}


else if (cmd=="redact") {
  auto m = parse_kv(argc, argv);
  std::string dir = arg_or(m,"--dir","");
  std::string out = arg_or(m,"--out","");
  std::string mode = arg_or(m,"--mode","pseudonymize");
  std::string ids = arg_or(m,"--ids","keep");
  if (dir.empty()||out.empty()){ std::cerr << "Usage: redact --dir DIR --out OUT [--mode pseudonymize|strip] [--ids keep|hash]\n"; return 1; }
  Inventory inv; std::vector<Order> orders; auto storage = make_csv_storage(dir); storage->load_products(inv); storage->load_orders(orders);
  std::map<std::string,std::string> idmap;
  Inventory outInv;
  for (auto p : inv.list_products()){
    if (mode=="strip"){ p.name=""; }
    else { // pseudonymize
      p.name = "Item-" + Sha256::of_string(p.name).substr(0,12);
    }
    if (ids=="hash"){
      std::string old = p.id; p.id = "ID-" + Sha256::of_string(old).substr(0,10);
      idmap[old]=p.id;
    }
    outInv.add_product(p);
  }
  for (auto& o : orders){
    for (auto& it : o.items){ if(ids=="hash"){ auto it2=idmap.find(it.product_id); if (it2!=idmap.end()) it.product_id=it2->second; } }
  }
  std::error_code ec; std::filesystem::create_directories(out, ec);
  CsvStorage::save_products(outInv, out);
  CsvStorage::save_orders(orders, out);
  std::cout << "redacted dataset written to " << out << "\n";
}

    else if (cmd=="patch") {
      if (argc<3 || std::string(argv[2])!="apply"){ std::cerr << "Usage: patch apply --file FILE [--dir PATH]\n"; return 1; }
      auto m = parse_kv(argc, argv);
      auto dir = arg_or(m,"--dir", data_dir);
      std::string file = arg_or(m,"--file","");
      if (file.empty()){ std::cerr << "missing --file\n"; return 1; }
      if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6;
      Inventory inv; std::vector<Order> oo; auto st = make_csv_storage(dir); st->load_products(inv); st->load_orders(oo);
      std::ifstream f(file, std::ios::binary); if(!f){ std::cerr << "cannot read patch file\n"; return 1; }
      std::string line; size_t adds=0, dels=0, mods=0, bad=0;
      while (std::getline(f,line)){
        if (line.empty()) continue;
        char op = line[0];
        if (!(op=='+'||op=='-'||op=='~')){ ++bad; continue; }
        size_t sp = line.find(' '); if (sp==std::string::npos){ ++bad; continue; }
        std::string row = line.substr(sp+1);
        auto cols = rfc4180_parse_line(row);
        if (cols.size()<1){ ++bad; continue; }
        if (op=='-'){ inv.remove_product(cols[0]); ++dels; continue; }
        if (cols.size()!=4){ ++bad; continue; }
        Product p; p.id=cols[0]; p.name=cols[1];
        try{ p.price = std::stod(cols[2]); p.stock = std::stoi(cols[3]); }catch(...){ ++bad; continue; }
        if (op=='+'){ inv.add_product(p); ++adds; }
        else { inv.update_product(p); ++mods; }
      }
      CsvStorage::save_products(inv, dir);
      std::cout << "applied patch adds=" << adds << " dels=" << dels << " mods=" << mods << " bad=" << bad << "\n";
    }


else if (cmd=="search-products") {
  auto m = parse_kv(argc, argv);
  std::string q = arg_or(m,"--query","");
  int mx=20; try{ mx = std::stoi(arg_or(m,"--max","20")); }catch(...){}
  bool fuzzy = m.count("--fuzzy");
  if (q.empty()){ std::cerr << "missing --query\n"; return 1; }
  auto all = inv.list_products();
  std::vector<std::tuple<int,const Product*>> scored;
  std::string ql = lower(q);
  for (const auto& p : all){
    std::string name = lower(p.name);
    int score = 1000000;
    if (!fuzzy){
      auto pos = name.find(ql);
      if (pos!=std::string::npos) score = (int)pos;
    } else {
        if(includeOrders){ std::set<std::string> lo, ro; for(auto&o:ol) lo.insert(o.id); for(auto&o:orr) ro.insert(o.id); for(auto&id: ro) if(!lo.count(id)) std::cout << "+ order " << id << "\n"; for(auto&id: lo) if(!ro.count(id)) std::cout << "- order " << id << "\n"; }
      score = (int)levenshtein(name, ql);
    }
    if (score<1000000) scored.emplace_back(score, &p);
  }
  std::sort(scored.begin(), scored.end(), [](auto&a,auto&b){ return std::get<0>(a) < std::get<0>(b); });
  for (int i=0;i<(int)scored.size() && i<mx; ++i){
    auto* p = std::get<1>(scored[i]);
    std::cout << p->id << "  " << p->name << "  price=" << p->price << " stock=" << p->stock << "\n";
  }
}


    else if (cmd=="serve") {
      auto m = parse_kv(argc, argv);
      int port=0; try{ port=std::stoi(arg_or(m,"--port","0")); }catch(...){}
      std::string bind = arg_or(m,"--bind","127.0.0.1");
      if (port<=0){ std::cerr << "missing/invalid --port\n"; return 1; }
#ifdef _WIN32
      WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
#endif
      int s = (int)socket(AF_INET, SOCK_STREAM, 0);
      if (s<0){ std::perror("socket"); return 1; }
      sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons((uint16_t)port); addr.sin_addr.s_addr = inet_addr(bind.c_str());
      int opt=1;
#ifndef _WIN32
      setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
      if (bind.c_str()==nullptr || addr.sin_addr.s_addr==INADDR_NONE){ std::cerr << "bad bind address\n"; return 1; }
      if (::bind(s, (sockaddr*)&addr, sizeof(addr))<0){ std::perror("bind"); return 1; }
      if (listen(s, 16)<0){ std::perror("listen"); return 1; }
      std::cout << "serving http on " << bind << ":" << port << "\\n";
      for(;;){
        sockaddr_in ca{}; socklen_t calen=sizeof(ca);
        int c = (int)accept(s, (sockaddr*)&ca, &calen);
        if (c<0) continue;
        std::string req; char buf[1024]; int n=recv(c, buf, sizeof(buf), 0); if(n>0) req.assign(buf, buf+n);
        std::string path="/";
        auto sp = req.find(' '); if(sp!=std::string::npos){ auto sp2=req.find(' ', sp+1); if(sp2!=std::string::npos) path=req.substr(sp+1, sp2-sp-1); }
        std::string body, ct="text/plain";
        if (path=="/metrics"){ body = gen_metrics_prom(data_dir); }
        else if (path=="/healthz"){ body = "ok\n"; }
        else { body = "not found\n"; }
        std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: "+ct+"\r\nContent-Length: "+std::to_string(body.size())+"\r\nConnection: close\r\n\r\n"+body;
        send(c, resp.c_str(), (int)resp.size(), 0);
#ifdef _WIN32
        closesocket(c);
#else
        close(c);
#endif
      }
    }


static bool read_first_token(const std::string& s, std::string* out){
  size_t i=0; while (i<s.size() && std::isspace((unsigned char)s[i])) ++i;
  size_t j=i; while (j<s.size() && !std::isspace((unsigned char)s[j])) ++j;
  if (j>i){ *out = s.substr(i, j-i); return true; } return false;
}
static bool verify_sidecar_pair(const std::string& dir, const char* base, bool* crc_ok, bool* sha_ok){
  std::string data = slurp((std::filesystem::path(dir)/base).string());
  if (data.empty()){ *crc_ok=false; *sha_ok=false; return false; }
  // compute crc32 and sha256 from data
  uint32_t crc = crc32_of(data); std::string crc_hex = hex_u32(crc);
  std::string sha = Sha256::of_string(data);
  std::string crc_file = slurp((std::filesystem::path(dir)/(std::string(base)+".crc32")).string());
  std::string sha_file = slurp((std::filesystem::path(dir)/(std::string(base)+".sha256")).string());
  std::string crc_tok, sha_tok; bool c1=read_first_token(crc_file,&crc_tok), s1=read_first_token(sha_file,&sha_tok);
  *crc_ok = c1 && (lower(crc_tok)==lower(crc_hex));
  *sha_ok = s1 && (lower(sha_tok)==lower(sha));
  return true;
}


else if (cmd=="catalog") {
  auto m = parse_kv(argc, argv);
  std::string sub = argc>2? std::string(argv[2]) : std::string();
      if (sub=="label") { auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir",data_dir); if(argc<4){ std::cerr<<"Usage: snapshot label <TS> --label L [--dir PATH]\n"; return 1;} std::string ts=argv[3]; std::string L=arg_or(m,"--label",""); if(L.empty()){ std::cerr<<"missing --label\n"; return 1;} std::error_code ec; std::filesystem::create_directories(std::filesystem::path(dir)/"snapshots", ec); std::ofstream f(labels_path(dir), std::ios::app); f<<ts<<"\t"<<L<<"\n"; std::cout<<"labeled "<<ts<<" as "<<L<<"\n"; return 0; }
  if (sub!="generate"){ std::cerr << "Usage: catalog generate [--dir PATH] [--output json|text]\n"; return 1; }
  auto dir = arg_or(m,"--dir", data_dir);
  std::string outfmt = arg_or(m,"--output","json");
  Inventory invX; std::vector<Order> oo; make_csv_storage(dir)->load_products(invX); make_csv_storage(dir)->load_orders(oo);
  auto v = invX.list_products(); size_t products=v.size(); size_t orders_n=oo.size();
  double inventory_value=0.0; for (auto&p: v) inventory_value += p.price * p.stock;
  double revenue=0.0; for (auto&o: oo) revenue += o.total();
  std::string fp = compute_fp_all(dir);
  bool c1=false,s1=false,c2=false,s2=false; verify_sidecar_pair(dir, "products.csv", &c1,&s1); verify_sidecar_pair(dir, "orders.csv", &c2,&s2);
  std::string ver; { std::ifstream f((std::filesystem::path(dir)/"DATASET_VERSION").string()); std::getline(f, ver); }
  auto statf=[&](const char* f)->uintmax_t{ std::error_code ec; auto p=(std::filesystem::path(dir)/f); return std::filesystem::exists(p,ec)? std::filesystem::file_size(p,ec):0; };
  std::string audit_reason; bool audit_ok = audit_verify(dir, &audit_reason);
  if (outfmt=="json"){
    std::cout << "{\"products\":"<<products<<",\"orders\":"<<orders_n<<",\"inventory_value\":"<<inventory_value<<",\"revenue_total\":"<<revenue<<",\"fingerprint\":\""<<fp<<"\",\"dataset_version\":\""<<json_escape(ver)<<"\",\"sizes\":{\"products.csv\":"<<statf("products.csv")<<",\"orders.csv\":"<<statf("orders.csv")<<",\"audit.log\":"<<statf("audit.log")<<"},\"integrity\":{\"products\":{\"crc32\":"<<(c1?"true":"false")<<",\"sha256\":"<<(s1?"true":"false")<<"},\"orders\":{\"crc32\":"<<(c2?"true":"false")<<",\"sha256\":"<<(s2?"true":"false")<<"}},\"audit_ok\":"<<(audit_ok?"true":"false")<<",\"audit_reason\":\""<<json_escape(audit_reason)<<"\"}\n";
  } else {
    std::cout << "products="<<products<<" orders="<<orders_n<<" inv_value="<<inventory_value<<" revenue="<<revenue<<"\n";
    std::cout << "fingerprint="<<fp<<" dataset_version="<<ver<<"\n";
    std::cout << "sizes: products.csv="<<statf("products.csv")<<" orders.csv="<<statf("orders.csv")<<" audit.log="<<statf("audit.log")<<"\n";
    std::cout << "integrity: products(crc="<<(c1?"ok":"bad")<<",sha="<<(s1?"ok":"bad")<<") orders(crc="<<(c2?"ok":"bad")<<",sha="<<(s2?"ok":"bad")<<")\n";
    std::cout << "audit_ok="<<(audit_ok?"true":"false")<<" reason="<<audit_reason<<"\n";
  }
}


else if (cmd=="verify-all") {
  auto m = parse_kv(argc, argv);
  auto dir = arg_or(m,"--dir", data_dir);
  std::string outfmt = arg_or(m,"--output","text");
  bool c1=false,s1=false,c2=false,s2=false; bool ok1=verify_sidecar_pair(dir, "products.csv", &c1,&s1); bool ok2=verify_sidecar_pair(dir, "orders.csv", &c2,&s2);
  std::string reason; bool audit_ok = audit_verify(dir, &reason);
  bool referential=true;
  { Inventory inv; std::vector<Order> oo; make_csv_storage(dir)->load_products(inv); make_csv_storage(dir)->load_orders(oo); std::set<std::string> ids; for(auto&p: inv.list_products()) ids.insert(p.id); for(auto&o: oo){ for(auto& it: o.items){ if(!ids.count(it.product_id)) referential=false; } } }
  bool overall = (ok1 && ok2 && c1 && s1 && c2 && s2 && audit_ok && referential);
  if (outfmt=="json"){
    std::cout << "{\"sidecars\":{\"products\":{\"crc32\":"<<(c1?"true":"false")<<",\"sha256\":"<<(s1?"true":"false")<<"},\"orders\":{\"crc32\":"<<(c2?"true":"false")<<",\"sha256\":"<<(s2?"true":"false")<<"}},\"audit_ok\":"<<(audit_ok?"true":"false")<<",\"referential_ok\":"<<(referential?"true":"false")<<",\"ok\":"<<(overall?"true":"false")<<",\"audit_reason\":\""<<json_escape(reason)<<"\"}\n";
  } else {
    std::cout << "products: crc="<<(c1?"ok":"bad")<<" sha="<<(s1?"ok":"bad")<<"\n";
    std::cout << "orders:   crc="<<(c2?"ok":"bad")<<" sha="<<(s2?"ok":"bad")<<"\n";
    std::cout << "audit:    "<<(audit_ok?"ok":"bad")<<" ("<<reason<<")\n";
    std::cout << "refs:     "<<(referential?"ok":"bad")<<"\n";
    std::cout << "overall:  "<<(overall?"OK":"FAIL")<<"\n";
  }
  return overall? 0 : 2;
}


else if (cmd=="env") {
  auto m = parse_kv(argc, argv);
  auto dir = arg_or(m,"--dir", data_dir);
  std::cout << "NEONSTORE_DIR=" << dir << "\n";
  const char* nh = std::getenv("NEONSTORE_NO_HOOKS"); std::cout << "NEONSTORE_NO_HOOKS=" << (nh? nh : "") << "\n";
  std::cout << "build.version=" << NEONSTORE_VERSION << " built=" << NEONSTORE_BUILD_TS << " compiler=" << NEONSTORE_COMPILER_ID << " type=" << NEONSTORE_BUILD_TYPE << "\n";
}


else if (cmd=="bench") {
  auto m = parse_kv(argc, argv);
  int ops=1000; try{ ops = std::stoi(arg_or(m,"--ops","1000")); }catch(...){}
  auto dir = arg_or(m,"--dir", data_dir);
  bool keep = m.count("--keep");
  std::string tmp = mktemp_dir(dir);
  // Copy current dataset
  for (auto f : {"products.csv","orders.csv","products.csv.crc32","orders.csv.crc32","products.csv.sha256","orders.csv.sha256","audit.log","DATASET_VERSION"}) {
    std::filesystem::path src = std::filesystem::path(dir)/f; if (std::filesystem::exists(src)) std::filesystem::copy_file(src, std::filesystem::path(tmp)/f, std::filesystem::copy_options::overwrite_existing);
  }
  auto st = make_csv_storage(tmp);
  Inventory inv; std::vector<Order> orders;
  auto t0 = std::chrono::high_resolution_clock::now();
  st->load_products(inv);
  auto t1 = std::chrono::high_resolution_clock::now();
  st->load_orders(orders);
  auto t2 = std::chrono::high_resolution_clock::now();
  auto products = inv.list_products();
  auto t3 = std::chrono::high_resolution_clock::now();
  // mutate prices
  std::mt19937_64 gen(12345);
  std::uniform_real_distribution<double> ud(-0.5, 0.5);
  int n = std::min<int>(ops, (int)products.size());
  for (int i=0;i<n;i++){ auto p = products[i]; p.price = std::max(0.0, p.price * (1.0 + ud(gen))); inv.update_product(p); }
  auto t4 = std::chrono::high_resolution_clock::now();
  CsvStorage::save_products(inv, tmp);
  auto t5 = std::chrono::high_resolution_clock::now();
  // report
  auto ms = [&](auto a, auto b){ return std::chrono::duration_cast<std::chrono::milliseconds>(b-a).count(); };
  std::cout << "{\"load_products_ms\":"<<ms(t0,t1)<<",\"load_orders_ms\":"<<ms(t1,t2)<<",\"list_products_ms\":"<<ms(t2,t3)<<",\"mutate_ms\":"<<ms(t3,t4)<<",\"save_products_ms\":"<<ms(t4,t5)<<",\"ops\":"<<n<<"}\n";
  if(!keep){ std::error_code ec; std::filesystem::remove_all(tmp, ec); }
}


else if (cmd=="selftest") {
  auto m = parse_kv(argc, argv);
  auto dir = arg_or(m,"--dir", data_dir);
  std::string tmp = mktemp_dir(dir);
  Inventory inv; Product p1; p1.id="SELFTEST-P1"; p1.name="Self Product"; p1.price=12.34; p1.stock=10; inv.add_product(p1);
  std::vector<Order> oo; Order o; o.id="SELFTEST-O1"; OrderItem it; it.product_id=p1.id; it.quantity=2; it.unit_price=p1.price; o.items.push_back(it); oo.push_back(o);
  CsvStorage::save_products(inv, tmp); CsvStorage::save_orders(oo, tmp);
  // reload & verify referential integrity and totals
  Inventory inv2; std::vector<Order> oo2; make_csv_storage(tmp)->load_products(inv2); make_csv_storage(tmp)->load_orders(oo2);
  bool ok_ids=true; std::set<std::string> ids; for(auto&p: inv2.list_products()) ids.insert(p.id); for (auto& ox: oo2){ for(auto& itx: ox.items){ if(!ids.count(itx.product_id)) ok_ids=false; } }
  std::string reason; bool audit_ok = audit_verify(tmp, &reason);
  bool ok = ok_ids && audit_ok;
  std::cout << "{\"referential_ok\":"<<(ok_ids?"true":"false")<<",\"audit_ok\":"<<(audit_ok?"true":"false")<<",\"ok\":"<<(ok?"true":"false")<<"}\n";
  // cleanup
  std::error_code ec; std::filesystem::remove_all(tmp, ec);
  return ok? 0 : 2;
}


else if (cmd=="lint") {
  auto m = parse_kv(argc, argv);
  auto dir = arg_or(m,"--dir", data_dir);
  std::string out = arg_or(m,"--output","text");
  Inventory inv; std::vector<Order> oo; make_csv_storage(dir)->load_products(inv); make_csv_storage(dir)->load_orders(oo);
  auto v = inv.list_products();
  std::map<std::string, std::string> name_to_id; std::vector<std::string> issues;
  auto lowerx = [](const std::string&s){ std::string r=s; for(char&c:r) c=(char)std::tolower((unsigned char)c); return r; };
  for (auto& p: v){
    std::string k = lowerx(p.name);
    if (!p.name.empty()){ auto it=name_to_id.find(k); if (it!=name_to_id.end()) issues.push_back(std::string("duplicate_name:")+p.id+":"+p.name); else name_to_id[k]=p.id; }
    if (p.price<0) issues.push_back(std::string("negative_price:")+p.id);
    if (p.stock<0) issues.push_back(std::string("negative_stock:")+p.id);
    if (p.price>10000000.0) issues.push_back(std::string("price_too_large:")+p.id);
    if (p.name.size()>256) issues.push_back(std::string("name_too_long:")+p.id);
  }
  if (out=="json"){
    std::cout << "{\"count\":"<<issues.size()<<",\"issues\":[";
    for (size_t i=0;i<issues.size();++i){ std::cout << "\"" << json_escape(issues[i]) << "\""; if(i+1<issues.size()) std::cout << ","; }
    std::cout << "]}\n";
  } else {
    if (issues.empty()) std::cout << "ok\n"; else { for(auto&s: issues) std::cout << s << "\n"; }
  }
  return issues.empty()? 0 : 2;
}


else if (cmd=="generate") {
  if (argc<3 || std::string(argv[2])!="demo"){ std::cerr << "Usage: generate demo --products N --orders M [--dir PATH]\n"; return 1; }
  auto m = parse_kv(argc, argv);
  int np=100, no=50; try{ np=std::stoi(arg_or(m,"--products","100")); }catch(...){}
  try{ no=std::stoi(arg_or(m,"--orders","50")); }catch(...){}
  auto dir = arg_or(m,"--dir", data_dir);
  if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6;
  unsigned long long seed=42; try{ seed=std::stoull(arg_or(m,"--seed","42")); }catch(...){ seed=42; } std::mt19937_64 gen(seed);
  std::uniform_real_distribution<double> price(1.0, 500.0);
  std::uniform_int_distribution<int> stock(0, 500);
  Inventory inv;
  for (int i=0;i<np;++i){ Product p; p.id="P"+std::to_string(i+1); p.name="Item "+std::to_string(i+1); p.price=std::round(price(gen)*100)/100.0; p.stock=stock(gen); inv.add_product(p); }
  std::vector<Order> oo;
  std::uniform_int_distribution<int> pick(0, np-1);
  for (int i=0;i<no;++i){ Order o; o.id=newid_ulid(); int items = 1 + (gen()%4); for(int k=0;k<items;++k){ OrderItem it; it.product_id="P"+std::to_string(1+pick(gen)); it.quantity=1+(gen()%5); it.unit_price=inv.get_product(it.product_id).price; o.items.push_back(it);} oo.push_back(o); }
  CsvStorage::save_products(inv, dir); CsvStorage::save_orders(oo, dir); audit_append(dir, "generate-demo", "{}");
  std::cout << "demo dataset generated: products="<<np<<" orders="<<no<<"\n";
}


else if (cmd=="schema") {
  auto m = parse_kv(argc, argv);
  auto dir = arg_or(m,"--dir", data_dir);
  std::string p = (std::filesystem::path(dir)/"products.csv").string();
  std::string o = (std::filesystem::path(dir)/"orders.csv").string();
  std::string expectedP = "id,name,price,stock";
  std::string expectedO = "order_id,product_id,quantity,unit_price";
  auto check = [&](const std::string& path, const std::string& expected)->bool{ std::ifstream f(path, std::ios::binary); if(!f) return false; std::string hdr; std::getline(f,hdr); return lower(hdr)==lower(expected); };
  std::cout << "Products: " << expectedP << "\nOrders:   " << expectedO << "\n";
  if (argc>2){ // assume schema check
    bool okP = check(p, expectedP);
    bool okO = check(o, expectedO);
    std::cout << "check: products=" << (okP?"ok":"bad") << " orders=" << (okO?"ok":"bad") << "\n";
    if(!(okP && okO)) return 2;
  }
}


else if (cmd=="config") {
  auto m = parse_kv(argc, argv);
  bool global = m.count("--global");
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  std::filesystem::path cfg = global? (std::filesystem::path(xdg?xdg:"")/"neonstore"/"config.ini") : std::filesystem::path("neonstore.ini");
  std::error_code ec; if (global) std::filesystem::create_directories(cfg.parent_path(), ec);
  std::map<std::string,std::string> kv;
  if (std::filesystem::exists(cfg)){ for (auto& s: read_lines(cfg.string())){ auto p=s.find('='); if(p!=std::string::npos) kv[s.substr(0,p)]=s.substr(p+1); } }
  if (argc>2 && std::string(argv[2])=="get"){ std::string key = argc>3? argv[3] : ""; if(key.empty()){ for (auto& it: kv) std::cout << it.first << "=" << it.second << "\n"; } else { auto it=kv.find(key); if(it!=kv.end()) std::cout << it->second << "\n"; } }
  else if (argc>2 && std::string(argv[2])=="set"){ if (argc<4){ std::cerr << "Usage: config set KEY=VALUE [--global]\n"; return 1; } std::string arg=argv[3]; auto p=arg.find('='); if(p==std::string::npos){ std::cerr << "expect KEY=VALUE\n"; return 1; } kv[arg.substr(0,p)]=arg.substr(p+1); std::string out; for (auto& it: kv) out += it.first + "=" + it.second + "\n"; write_text(cfg.string(), out); std::cout << "wrote " << cfg.string() << "\n"; }
  else { std::cerr << "Usage: config get [KEY] | set KEY=VALUE [--global]\n"; return 1; }
}


else if (cmd=="stress") {
  auto m = parse_kv(argc, argv);
  int threads=4; try{ threads=std::stoi(arg_or(m,"--threads","4")); }catch(...){}
  int seconds=5; try{ seconds=std::stoi(arg_or(m,"--seconds","5")); }catch(...){}
  auto dir = arg_or(m,"--dir", data_dir);
  bool keep = m.count("--keep");
  std::string tmp = mktemp_dir(dir);
  for (auto f : {"products.csv","orders.csv","products.csv.crc32","orders.csv.crc32","products.csv.sha256","orders.csv.sha256","audit.log","DATASET_VERSION"}) {
    std::filesystem::path src = std::filesystem::path(dir)/f; if (std::filesystem::exists(src)) std::filesystem::copy_file(src, std::filesystem::path(tmp)/f, std::filesystem::copy_options::overwrite_existing);
  }
  std::atomic<bool> stop{false};
  auto worker = [&](int id){ auto st=make_csv_storage(tmp); std::mt19937_64 gen(1234+id); while(!stop){ Inventory inv; std::vector<Order> oo; st->load_products(inv); auto v = inv.list_products(); if(!v.empty()){ auto p=v[gen()%v.size()]; p.price = std::max(0.0, p.price * (1.0 + ((int)(gen()%21)-10)/100.0)); inv.update_product(p); CsvStorage::save_products(inv, tmp);} } };
  std::vector<std::thread> th; for (int i=0;i<threads;i++) th.emplace_back(worker, i);
  std::this_thread::sleep_for(std::chrono::seconds(seconds)); stop=true; for(auto& t: th) t.join();
  std::cout << "stress done in " << tmp << "\n";
  if(!keep){ std::error_code ec; std::filesystem::remove_all(tmp, ec); }
}


else if (cmd=="transform-products") {
  auto m = parse_kv(argc, argv);
  auto dir = arg_or(m,"--dir", data_dir);
  std::string pref = arg_or(m,"--where-id-prefix","");
  std::string sname = arg_or(m,"--where-name-contains","");
  double pmul=1.0; try{ pmul = std::stod(arg_or(m,"--price-mul","1.0")); }catch(...){}
  double padd=0.0; try{ padd = std::stod(arg_or(m,"--price-add","0.0")); }catch(...){}
  int sadd=0; try{ sadd = std::stoi(arg_or(m,"--stock-add","0")); }catch(...){}
  bool has_set = m.count("--stock-set");
  int sset=0; if (has_set){ try{ sset = std::stoi(m["--stock-set"]); }catch(...){ has_set=false; } }
  if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6;
  Inventory inv; make_csv_storage(dir)->load_products(inv);
  auto v = inv.list_products();
  int touched=0;
  auto match = [&](const Product& p)->bool{
    if (!pref.empty() && p.id.rfind(pref,0)!=0) return false;
    if (!sname.empty()) { auto L=lower(p.name); if (L.find(lower(sname))==std::string::npos) return false; }
    return true;
  };
  for (auto& p : v){
    if (!match(p)) continue;
    double newp = std::max(0.0, p.price * pmul + padd); if (pround>=0 && pround<=6){ double s=std::pow(10.0, pround); newp = std::round(newp * s)/s; }
    int news = has_set? sset : (p.stock + sadd);
    if (newp!=p.price || news!=p.stock){
      p.price = std::round(newp * 100.0) / 100.0;
      p.stock = news;
      inv.update_product(p);
      ++touched;
    }
  }
  CsvStorage::save_products(inv, dir);
  std::cout << "{\"updated\":" << touched << ",\"total\":" << v.size() << "}\n";
}


else if (cmd=="csv") {

      if (argc>2 && std::string(argv[2])=="canonicalize"){
        auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
        Inventory inv; std::vector<Order> oo; auto st=make_csv_storage(dir); st->load_products(inv); st->load_orders(oo);
        auto vp = inv.list_products(); std::sort(vp.begin(), vp.end(), [](const Product&a,const Product&b){ return a.id<b.id; });
        Inventory out; for (auto &p: vp){ if(out.exists(p.id)) out.update_product(p); else out.add_product(p); }
        std::sort(oo.begin(), oo.end(), [](const Order&a,const Order&b){ return a.id<b.id; });
        for (auto &o: oo){ std::sort(o.items.begin(), o.items.end(), [](const OrderItem&a,const OrderItem&b){ return a.product_id<b.product_id; }); }
        if(!atomic_write_dataset(out, oo, dir)){ std::cerr<<"atomic_write_failed\n"; return 1; }
        std::cout<<"ok\n"; return 0;
      }

      if (argc>2 && std::string(argv[2])=="canonicalize"){ auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir); if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6; Inventory inv; std::vector<Order> oo; auto st=make_csv_storage(dir); st->load_products(inv); st->load_orders(oo); auto vp=inv.list_products(); std::sort(vp.begin(), vp.end(), [](const Product&a,const Product&b){ return a.id<b.id; }); Inventory inv2; for(auto&p:vp) inv2.add_product(p); CsvStorage::save_products(inv2, dir); CsvStorage::save_orders(oo, dir); std::cout << "canonicalized\n"; return 0; }
  if (argc<3 || std::string(argv[2])!="validate"){ std::cerr << "Usage: csv validate [--file products|orders|all] [--max-errors N] [--dir PATH]\n"; return 1; }
  auto m = parse_kv(argc, argv);
  auto dir = arg_or(m,"--dir", data_dir);
  std::string which = arg_or(m,"--file","all");
  int maxerr=50; try{ maxerr=std::stoi(arg_or(m,"--max-errors","50")); }catch(...){}
  struct Err { std::string file; int line; std::string msg; };
  std::vector<Err> errs;
  auto check_file = [&](const std::string& fname, int expect_cols){
    auto path = (std::filesystem::path(dir)/fname).string();
    std::ifstream f(path, std::ios::binary); if(!f){ errs.push_back({fname,0,"cannot_open"}); return; }
    std::string line; int ln=0;
    while (std::getline(f,line)){
      ++ln;
      auto cols = rfc4180_parse_line(line);
      if (ln==1){ /* header allowed */ continue; }
      if ((int)cols.size()!=expect_cols){ errs.push_back({fname,ln,"wrong_columns"}); if((int)errs.size()>=maxerr) return; }
      if (fname=="products.csv"){
        if (cols.size()>=4){ try{ (void)std::stod(cols[2]); }catch(...){ errs.push_back({fname,ln,"bad_price"}); }
          try{ (void)std::stoi(cols[3]); }catch(...){ errs.push_back({fname,ln,"bad_stock"}); } }
      } else if (fname=="orders.csv"){
        if (cols.size()>=4){ try{ (void)std::stoi(cols[2]); }catch(...){ errs.push_back({fname,ln,"bad_quantity"}); }
          try{ (void)std::stod(cols[3]); }catch(...){ errs.push_back({fname,ln,"bad_unit_price"}); } }
      }
      if((int)errs.size()>=maxerr) return;
    }
  };
  if (which=="all" || which=="products") check_file("products.csv", 4);
  if (which=="all" || which=="orders")   check_file("orders.csv", 4);
  if (errs.empty()){ std::cout << "{\"ok\":true}\n"; return 0; }
  std::cout << "{\"ok\":false,\"errors\":[";
  for (size_t i=0;i<errs.size();++i){ auto&e=errs[i]; std::cout << "{\"file\":\"" << e.file << "\",\"line\":" << e.line << ",\"msg\":\"" << e.msg << "\"}"; if(i+1<errs.size()) std::cout << ","; }
  std::cout << "]}\n";
  return 2;
}


    else if (cmd=="seal") {
      auto m = parse_kv(argc, argv); auto dir = arg_or(m,"--dir", data_dir); std::string outfmt = arg_or(m,"--output","text");
      std::string sealp = (std::filesystem::path(dir)/"SEAL").string();
      std::string sub = argc>2? std::string(argv[2]) : std::string();
      if (sub=="create"){
        std::string fp = compute_fp_all(dir);
        std::string body = std::string("fingerprint=")+fp+"\ncreated_at="+iso8601_utc()+"\n";
        std::ofstream f(sealp, std::ios::binary|std::ios::trunc); f << body;
#if !defined(_WIN32)
        using std::filesystem::perms; std::error_code ec; std::filesystem::permissions(sealp, perms::owner_read|perms::owner_write|perms::group_read, ec);
#endif
        if (outfmt=="json") std::cout << "{\"ok\":true,\"fingerprint\":\"" << fp << "\"}\n"; else std::cout << "sealed\n";
      } else if (sub=="verify"){
        std::ifstream f(sealp); std::string line, fpfile;
        while(std::getline(f,line)){ if(line.rfind("fingerprint=",0)==0) { fpfile=line.substr(12); break; } }
        std::string fp = compute_fp_all(dir);
        bool ok = !fpfile.empty() && (fpfile==fp);
        if (outfmt=="json") std::cout << "{\"ok\":" << (ok?"true":"false") << ",\"fingerprint\":\"" << fp << "\",\"expected\":\"" << fpfile << "\"}\n";
        else std::cout << (ok? "OK":"FAIL") << "\n";
        return ok? 0 : 2;
      } else {
        std::cerr << "Usage: seal create|verify [--dir PATH] [--output json|text]\n"; return 1;
      }
    }


    else if (cmd=="perms") {
      auto m = parse_kv(argc, argv); auto dir = arg_or(m,"--dir", data_dir); std::string outfmt = arg_or(m,"--output","text");
#if defined(_WIN32)
      if (outfmt=="json"){ std::cout << "{\"ok\":true,\"note\":\"n/a on Windows\"}\n"; } else { std::cout << "n/a on Windows\n"; }
      return 0;
#else
      using std::filesystem::perms;
      auto mode_file_ok = [&](const std::filesystem::path& p){ std::error_code ec; auto st = std::filesystem::status(p, ec); if(ec) return false; auto pr = st.permissions(); bool ok = ((pr & perms::owner_all) == (perms::owner_read|perms::owner_write)) && ((pr & perms::group_read)==perms::group_read); return ok; };
      auto mode_dir_ok  = [&](const std::filesystem::path& p){ std::error_code ec; auto st = std::filesystem::status(p, ec); if(ec) return false; auto pr = st.permissions(); bool ok = ((pr & perms::owner_all) == (perms::owner_read|perms::owner_write|perms::owner_exec)) && ((pr & perms::group_exec)==perms::group_exec); return ok; };
      std::vector<std::string> bad;
      auto check = [&](const std::string& f, bool is_dir){ auto p=std::filesystem::path(dir)/f; if(std::filesystem::exists(p)){ bool ok = is_dir? mode_dir_ok(p): mode_file_ok(p); if(!ok) bad.push_back(f);} };
      check(".", true);
      check("products.csv", false); check("orders.csv", false); check("audit.log", false);
      if (argc>2 && std::string(argv[2])=="fix"){
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::path(dir))){ std::filesystem::permissions(std::filesystem::path(dir), perms::owner_all|perms::group_exec, ec); }
        for (auto f : { "products.csv","orders.csv","audit.log","SEAL" }){
          auto p = (std::filesystem::path(dir)/f); if (std::filesystem::exists(p)) std::filesystem::permissions(p, perms::owner_read|perms::owner_write|perms::group_read, ec);
        }
        if (outfmt=="json"){ std::cout << "{\"ok\":true}\n"; } else { std::cout << "fixed\n"; }
        return 0;
      }
      if (bad.empty()){ if (outfmt=="json") std::cout << "{\"ok\":true}\n"; else std::cout << "ok\n"; return 0; }
      if (outfmt=="json"){ std::cout << "{\"ok\":false,\"bad\":["; for(size_t i=0;i<bad.size();++i){ std::cout << "\"" << bad[i] << "\""; if(i+1<bad.size()) std::cout << ","; } std::cout << "]}\n"; } else { for(auto&s:bad) std::cout << s << "\n"; }
      return 2;
#endif
    }


else if (cmd=="newid-uuid4") {
  std::cout << uuid4() << "\n";
}


else if (cmd=="quota") {
  auto m = parse_kv(argc, argv);
  auto dir = arg_or(m,"--dir", data_dir);
  std::filesystem::path q = std::filesystem::path(dir)/".neonstore"/"quota.ini";
  if (argc>2 && std::string(argv[2])=="set"){
    uint64_t mb=0; try{ mb = std::stoull(arg_or(m,"--max-bytes","0")); }catch(...){ }
    if (mb==0){ std::cerr << "missing --max-bytes\n"; return 1; }
    std::error_code ec; std::filesystem::create_directories(q.parent_path(), ec);
    std::ofstream f(q.string()); f << "max_bytes=" << mb << "\n"; std::cout << "ok\n";
  } else if (argc>2 && std::string(argv[2])=="status"){
    std::error_code ec; if (!std::filesystem::exists(q, ec)){ std::cout << "{\"enabled\":false}\n"; }
    else {
      std::ifstream f(q.string()); std::string s; uint64_t lim=0; while(std::getline(f,s)){ if(s.rfind("max_bytes=",0)==0){ try{ lim=std::stoull(s.substr(10)); }catch(...){}}}
      uint64_t used=0; for (auto& e: std::filesystem::recursive_directory_iterator(dir, ec)) if(!ec && e.is_regular_file(ec)) used += (uint64_t)std::filesystem::file_size(e, ec);
      std::cout << "{\"enabled\":true,\"used\":"<<used<<",\"max_bytes\":"<<lim<<"}\n";
    }
  } else if (argc>2 && std::string(argv[2])=="unset"){
    std::error_code ec; std::filesystem::remove(q, ec); std::cout << "ok\n";
  } else {
    std::cerr << "Usage: quota set --max-bytes N | status | unset [--dir PATH]\n"; return 1;
  }
}


else if (cmd=="sidecars") {

if (argc>2 && std::string(argv[2])=="check"){
  auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
  int missing=0, orphan=0;
  auto check = [&](const std::string& f){
    std::filesystem::path p = std::filesystem::path(dir)/f;
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return;
    std::string base = p.filename().string();
    for (auto ext : {".crc32",".sha256"}){
      auto s = p.string()+ext;
      if (!std::filesystem::exists(s, ec)) { ++missing; std::cout << "missing " << (base+ext) << "\n"; }
    }
  };
  check("products.csv"); check("orders.csv");
  // Orphans: sidecars without main file
  for (auto &e: std::filesystem::directory_iterator(std::filesystem::path(dir))){
    auto nm = e.path().filename().string();
    if (nm=="products.csv.crc32" || nm=="products.csv.sha256" || nm=="orders.csv.crc32" || nm=="orders.csv.sha256"){
      std::string mainf = nm.substr(0, nm.find_last_of('.'));
      if (!std::filesystem::exists(std::filesystem::path(dir)/mainf)) { ++orphan; std::cout << "orphan " << nm << "\n"; }
    }
  }
  std::cout << "{\"missing\":" << missing << ",\"orphan\":" << orphan << "}\n";
  return (missing||orphan)? 2:0;
} else if (argc>2 && std::string(argv[2])=="scrub"){
  auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
  if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6;
  int removed=0; std::error_code ec;
  for (auto &e: std::filesystem::directory_iterator(std::filesystem::path(dir))){
    auto nm = e.path().filename().string();
    if (nm=="products.csv.crc32" || nm=="products.csv.sha256" || nm=="orders.csv.crc32" || nm=="orders.csv.sha256"){
      std::string mainf = nm.substr(0, nm.find_last_of('.'));
      if (!std::filesystem::exists(std::filesystem::path(dir)/mainf)) { std::filesystem::remove(e, ec); if(!ec) ++removed; }
    }
  }
  std::cout << "{\"removed\":" << removed << "}\n"; return 0;
}

  auto m = parse_kv(argc, argv);
  auto dir = arg_or(m,"--dir", data_dir);
  CsvStorage::refresh_sidecars(dir);
  std::cout << "refreshed\n";
}


else if (cmd=="set-price-cas") {
  auto m = parse_kv(argc, argv); auto dir = arg_or(m,"--dir", data_dir);
  std::string id = arg_or(m,"--id",""); double ifc=std::stod(arg_or(m,"--if-current","-1")); double np=std::stod(arg_or(m,"--price","-1"));
  if(id.empty()||ifc<0||np<0){ std::cerr<<"usage: set-price-cas --id ID --if-current P1 --price P2\n"; return 1; }
  if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6;
  Inventory inv; make_csv_storage(dir)->load_products(inv);
  auto p = inv.get_product(id);
  if (std::fabs(p.price - ifc) > 1e-9){ std::cerr<<"cas_mismatch\n"; return 3; }
  p.price = np; inv.update_product(p); CsvStorage::save_products(inv, dir); std::cout<<"ok\n";
}


else if (cmd=="why") {
  auto dir = data_dir;
  std::filesystem::path lockp = std::filesystem::path(dir)/"LOCKED";
  bool locked = std::filesystem::exists(lockp) || std::getenv("NEONSTORE_LOCK");
  std::cout << "locked=" << (locked? "true":"false") << "\n";
  // quota
  std::filesystem::path q = std::filesystem::path(dir)/".neonstore"/"quota.ini";
  std::error_code ec; bool qon = std::filesystem::exists(q, ec);
  if (qon){ std::ifstream f(q.string()); std::string s; while(std::getline(f,s)){ if(s.rfind("max_bytes=",0)==0){ std::cout << s << "\n"; } } } else { std::cout << "quota=disabled\n"; }
  // policy
  bool pol=false; for (auto p : { std::filesystem::path("policy.ini"), std::filesystem::path(".neonstore")/"policy.ini", std::filesystem::path(dir)/"policy.ini" }){ if(std::filesystem::exists(p, ec)){ pol=true; std::cout << "policy="<<p.string()<<"\n"; } }
  if(!pol) std::cout << "policy=none\n";
}

    else if (cmd=="rename-products") {
      auto m = parse_kv(argc, argv);
      auto dir = arg_or(m,"--dir", data_dir);
      std::string rx = arg_or(m,"--match-regex","");
      std::string rep = arg_or(m,"--replace","");
      if (rx.empty()){ std::cerr<<"Usage: rename-products --match-regex R --replace STR [--dir PATH]\n"; return 1; }
      if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6;
      std::regex R; try{ R = std::regex(rx); }catch(...){ std::cerr<<"bad_regex\n"; return 1; }
      Inventory inv; make_csv_storage(dir)->load_products(inv);
      auto v = inv.list_products(); int n=0;
      for (auto &p : v){ std::string nn = std::regex_replace(p.name, R, rep); if (nn!=p.name){ p.name=nn; inv.update_product(p); ++n; } }
      CsvStorage::save_products(inv, dir);
      std::cout << "{\"renamed\":" << n << "}\n";
    }


else if (cmd=="policy") {

      else if (argc>2 && std::string(argv[2])=="enforce"){
        auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir); bool dry=m.count("--dry-run");
        double floor=0.0, ceil=0.0; int maxqty=0;
        for (auto p : { std::filesystem::path("policy.ini"), std::filesystem::path(".neonstore")/"policy.ini", std::filesystem::path(dir)/"policy.ini" }){
          std::error_code ec; if(!std::filesystem::exists(p, ec)) continue; std::ifstream f(p.string()); std::string s;
          while(std::getline(f,s)){
            if(s.rfind("price_floor=",0)==0){ try{ floor=std::stod(s.substr(12)); }catch(...){ } }
            else if(s.rfind("price_ceiling=",0)==0){ try{ ceil=std::stod(s.substr(14)); }catch(...){ } }
            else if(s.rfind("max_order_item_qty=",0)==0){ try{ maxqty=std::stoi(s.substr(20)); }catch(...){ } }
          }
        }
        Inventory inv; std::vector<Order> oo; auto st=make_csv_storage(dir); st->load_products(inv); st->load_orders(oo);
        int price_adj=0, qty_adj=0;
        // clamp prices
        for (auto &p: inv.list_products()){ auto x=p; bool ch=false; if (floor>0 && x.price<floor){ x.price=floor; ch=true; } if (ceil>0 && x.price>ceil){ x.price=ceil; ch=true; } if(ch){ inv.update_product(x); ++price_adj; } }
        // clamp quantities
        if (maxqty>0){
          for (auto &o: oo){ for (auto &it: o.items){ if (it.quantity>maxqty){ it.quantity=maxqty; ++qty_adj; } } }
        }
        if (!dry) { if(!atomic_write_dataset(inv, oo, dir)){ std::cerr<<"atomic_write_failed\n"; return 1; } }
        std::cout<<"{\"price_adjusted\":"<<price_adj<<",\"qty_adjusted\":"<<qty_adj<<",\"dry_run\":"<<(dry? "true":"false")<<"}\n";
        return 0;
      }


      else if (argc>2 && std::string(argv[2])=="check"){
        auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
        double floor=0.0, ceil=0.0; int maxqty=0;
        for (auto p : { std::filesystem::path("policy.ini"), std::filesystem::path(".neonstore")/"policy.ini", std::filesystem::path(dir)/"policy.ini" }){
          std::error_code ec; if(!std::filesystem::exists(p, ec)) continue; std::ifstream f(p.string()); std::string s;
          while(std::getline(f,s)){
            if(s.rfind("price_floor=",0)==0){ try{ floor=std::stod(s.substr(12)); }catch(...){ } }
            else if(s.rfind("price_ceiling=",0)==0){ try{ ceil=std::stod(s.substr(14)); }catch(...){ } }
            else if(s.rfind("max_order_item_qty=",0)==0){ try{ maxqty=std::stoi(s.substr(20)); }catch(...){ } }
          }
        }
        Inventory inv; std::vector<Order> oo; auto st=make_csv_storage(dir); st->load_products(inv); st->load_orders(oo);
        int v_price=0, v_qty=0;
        for (auto &p: inv.list_products()){ if (floor>0 && p.price<floor) ++v_price; if (ceil>0 && p.price>ceil) ++v_price; }
        if (maxqty>0){ for (auto &o: oo){ for (auto &it: o.items){ if (it.quantity>maxqty) ++v_qty; } } }
        bool ok = (v_price==0 && v_qty==0);
        std::cout<<"{\"ok\":"<<(ok? "true":"false")<<",\"price_violations\":"<<v_price<<",\"qty_violations\":"<<v_qty<<"}\n";
        return ok?0:2;
      }

  if (argc>2 && std::string(argv[2])=="init"){
    auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
    std::filesystem::path p = std::filesystem::path(dir)/"policy.ini";
    std::string tpl = "# NeonStore runtime policy\n# deny=<command> to block it at runtime\n# Hard constraints for writes (enforced in storage layer)\n# price_min=0.00\n# price_max=100000.00\n# stock_max=100000\n# quantity_max=10000\n# name_forbid_regex=forbidden|illegal\n";
    std::ofstream f(p.string(), std::ios::binary|std::ios::trunc); f<<tpl;
    std::cout<<"wrote "<<p.string()<<"\n"; return 0;
  }
  std::cerr << "Usage: policy init [--dir PATH]\n"; return 1;
}


else if (cmd=="batch") {
  if (argc<3 || std::string(argv[2])!="apply"){ std::cerr<<"Usage: batch apply --file FILE [--dir PATH]\n"; return 1; }
  auto m = parse_kv(argc, argv);
  auto dir = arg_or(m,"--dir", data_dir);
  std::string file = arg_or(m,"--file","");
  if (file.empty()){ std::cerr<<"missing --file\n"; return 1; }
  if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6;
  std::ifstream in(file); if(!in){ std::cerr<<"cannot_open\n"; return 1; }
      std::set<std::string> applied; std::filesystem::path logp = std::filesystem::path(dir)/".neonstore"/"applied.log"; { std::error_code ec; std::filesystem::create_directories(logp.parent_path(), ec); std::ifstream ap(logp.string()); std::string s; while(std::getline(ap,s)){ if(!s.empty()) applied.insert(s); } }
  Inventory inv; make_csv_storage(dir)->load_products(inv);
  std::string line; int n=0; int ln=0;
  auto toks = [&](const std::string& s){ std::vector<std::string> v; std::string cur; for(size_t i=0;i<=s.size();++i){ char c = (i<s.size()? s[i] : ' '); if(c==' '||c=='\t'||i==s.size()){ if(!cur.empty()){ v.push_back(cur); cur.clear(); } } else cur.push_back(c);} return v; };
  std::vector<std::string> newops;
      while (std::getline(in, line)){
    ++ln; std::string s = trim(line); if (s.empty()||s[0]=='#') continue;
    auto v = toks(s); if (v.empty()) continue;
    std::string op = v[0]; std::map<std::string,std::string> kv; std::string op_id;
    for (size_t i=1;i<v.size();++i){ auto p=v[i].find('='); if(p!=std::string::npos){ auto k=v[i].substr(0,p); auto val=v[i].substr(p+1); if(k=="op_id") op_id=val; else kv[k]=val; } }
    if (!op_id.empty() && applied.count(op_id)) { continue; }
        if (op=="set-price"){ auto id=kv["id"]; double p=std::stod(kv["price"]); auto x=inv.get_product(id); x.price=p; inv.update_product(x); ++n; }
    else if (op=="set-stock"){ auto id=kv["id"]; int st=std::stoi(kv["stock"]); auto x=inv.get_product(id); x.stock=st; inv.update_product(x); ++n; }
    else if (op=="add-product"){ Product p; p.id=kv["id"]; p.name=kv["name"]; p.price=std::stod(kv["price"]); p.stock=std::stoi(kv["stock"]); inv.add_product(p); ++n; }
    else { std::cerr<<"unknown_op at line "<<ln<<"\n"; }
        if (!op_id.empty()) newops.push_back(op_id);
  }
  CsvStorage::save_products(inv, dir);
  std::cout << "{\"applied\":" << n << "}\n";
}


else if (cmd=="fs") {
  if (argc>2 && std::string(argv[2])=="readonly"){
    auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
    std::filesystem::path p = std::filesystem::path(dir)/".__neonstore_rwcheck";
    std::error_code ec; std::ofstream f(p.string(), std::ios::app);
    bool ro = !f.good();
    if (!ro){ f.close(); std::filesystem::remove(p, ec); }
    std::cout << (ro? "readonly":"writable") << "\n";
    return ro? 2:0;
  }
  std::cerr << "Usage: fs readonly [--dir PATH]\n"; return 1;
}


else if (cmd=="retention") {
  if (argc<3 || std::string(argv[2])!="enforce"){ std::cerr<<"Usage: retention enforce --backups|--audit --days D [--dir PATH]\n"; return 1; }
  auto m = parse_kv(argc, argv);
  bool backups = m.count("--backups"); bool audit = m.count("--audit");
  int days=30; try{ days=std::stoi(arg_or(m,"--days","30")); }catch(...){}
  if (!backups && !audit){ std::cerr<<"missing target (--backups or --audit)\n"; return 1; }
  auto dir = arg_or(m,"--dir", backups? "backups" : data_dir);
  int removed=0; std::error_code ec;
  auto now = std::chrono::system_clock::now();
  for (auto &e: std::filesystem::directory_iterator(std::filesystem::path(dir), ec)){
    auto nm = e.path().filename().string();
    if (audit && nm.rfind("audit-",0)==0 && nm.find(".log")!=std::string::npos){
      auto ft = std::filesystem::last_write_time(e, ec);
      if (ec) continue;
      auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ft - decltype(ft)::clock::now() + std::chrono::system_clock::now());
      auto age_days = std::chrono::duration_cast<std::chrono::hours>(now - sctp).count()/24;
      if (age_days > days){ std::filesystem::remove(e, ec); if(!ec) ++removed; }
    } else if (backups && (nm.find(".tar")!=std::string::npos || nm.find(".tgz")!=std::string::npos || nm.find(".zip")!=std::string::npos || nm=="MANIFEST.txt")){
      if (nm=="MANIFEST.txt") continue;
      auto ft = std::filesystem::last_write_time(e, ec);
      if (ec) continue;
      auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ft - decltype(ft)::clock::now() + std::chrono::system_clock::now());
      auto age_days = std::chrono::duration_cast<std::chrono::hours>(now - sctp).count()/24;
      if (age_days > days){ std::filesystem::remove(e, ec); if(!ec) ++removed; }
    }
  }
  std::cout << "{\"removed\":" << removed << "}\n"; return 0;
}


else if (cmd=="remove-product") {
  auto m = parse_kv(argc, argv); auto dir = arg_or(m,"--dir", data_dir);
  bool dry = m.count("--dry-run"); std::string id = arg_or(m,"--id",""); std::string cascade = arg_or(m,"--cascade","no");
  if (id.empty()){ std::cerr<<"Usage: remove-product --id ID [--cascade yes|no] [--dry-run] [--dir PATH]\n"; return 1; }
  if(!must_allow_write() && !dry) return 4; if(!dry && !cas_guard(m, dir)) return 6;
  Inventory inv; std::vector<Order> oo; auto st=make_csv_storage(dir); st->load_products(inv); st->load_orders(oo);
  bool used=false; int affected=0;
  for (auto &o: oo){ for (auto &it: o.items){ if (it.product_id==id){ used=true; ++affected; } } }
  if (used && cascade!="yes"){ std::cerr<<"in_use\\n"; return 2; }
  if (used && cascade=="yes"){ for (auto &o: oo){ o.items.erase(std::remove_if(o.items.begin(), o.items.end(), [&](const OrderItem& it){ return it.product_id==id; }), o.items.end()); } }
  inv.remove_product(id);
  if(!dry){ CsvStorage::save_products(inv, dir); CsvStorage::save_orders(oo, dir); }
  std::cout << "{\"removed_product\":true,\"removed_items\":" << (cascade=="yes"? affected:0) << ",\"dry_run\":" << (dry? "true":"false") << "}\n";
}


else if (cmd=="export") {
  if (argc<3){ std::cerr<<"Usage: export products|orders --format jsonl|csv [--dir PATH]\n"; return 1; }
  std::string what = argv[2]; auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir); std::string fmt = arg_or(m,"--format","jsonl");
  Inventory inv; std::vector<Order> oo; auto st=make_csv_storage(dir); st->load_products(inv); st->load_orders(oo);
  if (what=="products"){
    if (fmt=="csv"){ std::cout<<"id,name,price,stock\n"; for (auto&p: inv.list_products()){ std::cout<<p.id<<","<<escape_csv(p.name)<<","<<p.price<<","<<p.stock<<"\n"; } }
    else { for (auto&p: inv.list_products()){ std::cout << "{\"id\":\""<<escape_json(p.id)<<"\",\"name\":\""<<escape_json(p.name)<<"\",\"price\":"<<p.price<<",\"stock\":"<<p.stock<<"}\n"; } }
  } else if (what=="orders"){
    if (fmt=="csv"){ std::cout<<"order_id,product_id,quantity,unit_price\n"; for (auto&o: oo){ for (auto &it: o.items){ std::cout<<o.id<<","<<it.product_id<<","<<it.quantity<<","<<it.unit_price<<"\n"; } } }
    else { for (auto&o: oo){ for (auto &it: o.items){ std::cout << "{\"order_id\":\""<<escape_json(o.id)<<"\",\"product_id\":\""<<escape_json(it.product_id)<<"\",\"quantity\":"<<it.quantity<<",\"unit_price\":"<<it.unit_price<<"}\n"; } } }
  } else { std::cerr<<"unknown_what\n"; return 1; }
}


else if (cmd=="import") {
  if (argc<3 || std::string(argv[2])!="products"){ std::cerr<<"Usage: import products --from FILE.jsonl [--dir PATH]\n"; return 1; }
  auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir); std::string path = arg_or(m,"--from","");
  if (path.empty()){ std::cerr<<"missing --from\n"; return 1; }
  if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6;
  std::ifstream in(path); if(!in){ std::cerr<<"cannot_open\n"; return 1; }
  Inventory inv; make_csv_storage(dir)->load_products(inv);
  std::string line; int n=0; while(std::getline(in, line)){
    if(line.empty()) continue;
    // naive JSONL: {"id":"..","name":"..","price":..,"stock":..}
    auto val = [&](const std::string& k)->std::string{ auto pos=line.find("\""+k+"\""); if(pos==std::string::npos) return ""; pos=line.find(":", pos); if(pos==std::string::npos) return ""; size_t i=pos+1; while(i<line.size() && (line[i]==' ')) ++i; if(i<line.size() && line[i]=='\"'){ size_t j=line.find("\"", i+1); return (j==std::string::npos? "" : line.substr(i+1, j-(i+1))); } else { size_t j=i; while(j<line.size() && (std::isdigit((unsigned char)line[j]) || line[j]=='.')) ++j; return line.substr(i, j-i); } };
    Product p; p.id = val("id"); p.name = val("name"); try{ p.price = std::stod(val("price")); }catch(...){ p.price=0; } try{ p.stock = std::stoi(val("stock")); }catch(...){ p.stock=0; }
    if (!p.id.empty()){ if(inv.exists(p.id)) inv.update_product(p); else inv.add_product(p); ++n; }
  }
  CsvStorage::save_products(inv, dir); std::cout << "{\"imported\":"<<n<<"}\n";
}


    else if (cmd=="lock") {
      auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
      std::filesystem::path p = std::filesystem::path(dir)/"LOCKED"; std::error_code ec;
      if (argc>2 && std::string(argv[2])=="status"){ bool on = std::filesystem::exists(p, ec) || std::getenv("NEONSTORE_LOCK"); std::cout<<(on? "on":"off")<<"\n"; return on? 0:2; }
      if (argc>2 && std::string(argv[2])=="on"){ std::ofstream f(p.string(), std::ios::app); std::cout<<"on\n"; return 0; }
      if (argc>2 && std::string(argv[2])=="off"){ std::filesystem::remove(p, ec); std::cout<<"off\n"; return 0; }
      std::cerr<<"Usage: lock status|on|off [--dir PATH]\n"; return 1;
    }


    else if (cmd=="index") {
      if (argc<3){ std::cerr<<"Usage: index build|check|drop|lookup --id ID [--dir PATH]\n"; return 1; }
      auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
      std::filesystem::path idxp = std::filesystem::path(dir)/".neonstore"/"products.idx"; std::error_code ec;
      std::string sub = argv[2];
      if (sub=="build"){
        std::filesystem::create_directories(idxp.parent_path(), ec);
        std::filesystem::path p = std::filesystem::path(dir)/"products.csv";
        std::ifstream in(p.string(), std::ios::binary); if(!in){ std::cerr<<"no_products\n"; return 1; }
        std::ofstream out(idxp.string(), std::ios::binary|std::ios::trunc);
        std::string line; uint64_t off=0; bool header=true;
        while (true){
          uint64_t cur = (uint64_t)in.tellg();
          if(!std::getline(in, line)) break; if (header){ header=false; continue; }
          // id is until first comma
          size_t c = line.find(','); std::string id = (c==std::string::npos? line : line.substr(0,c));
          out << id << "," << cur << "\n";
          off = (uint64_t)in.tellg();
        }
        std::cout<<"built\n"; return 0;
      } else if (sub=="check"){
        bool ok=true; std::ifstream idx(idxp.string()); std::string row;
        while(std::getline(idx, row)){ size_t c=row.find(','); if(c==std::string::npos) continue; std::string id=row.substr(0,c); uint64_t off=0; try{ off=std::stoull(row.substr(c+1)); }catch(...){ ok=false; break; }
          std::ifstream in((std::filesystem::path(dir)/"products.csv").string(), std::ios::binary); in.seekg((std::streamoff)off); std::string line; std::getline(in,line); size_t cc=line.find(','); std::string check=(cc==std::string::npos? line:line.substr(0,cc)); if (check!=id) { ok=false; break; } }
        std::cout<<(ok? "ok":"bad")<<"\n"; return ok?0:2;
      } else if (sub=="drop"){
        std::filesystem::remove(idxp, ec); std::cout<<"dropped\n"; return 0;
      } else if (sub=="lookup"){
        std::string id = arg_or(m,"--id",""); if(id.empty()){ std::cerr<<"missing --id\n"; return 1; }
        std::ifstream idx(idxp.string()); if(!idx){ std::cerr<<"no_index\n"; return 1; } std::string row; while(std::getline(idx,row)){ size_t c=row.find(','); if(c==std::string::npos) continue; if(row.substr(0,c)==id){ std::cout<<row.substr(c+1)<<"\n"; return 0; } } std::cerr<<"not_found\n"; return 2;
      } else {
        std::cerr<<"Usage: index build|check|drop|lookup --id ID [--dir PATH]\n"; return 1;
      }
    }


    else if (cmd=="txn") {
      auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
      std::filesystem::path j = std::filesystem::path(dir)/"txn_journal.json"; std::error_code ec;
      if (argc>2 && std::string(argv[2])=="status"){
        bool exists = std::filesystem::exists(j, ec);
        std::cout << (exists? "pending":"none") << "\n"; return exists? 2:0;
      } else if (argc>2 && std::string(argv[2])=="recover"){
        if (!std::filesystem::exists(j, ec)){ std::cout<<"nothing\n"; return 0; }
        // If .new files exist, try to complete renames
        auto d = std::filesystem::path(dir);
        if (std::filesystem::exists(d/"products.csv.new", ec)) std::filesystem::rename(d/"products.csv.new", d/"products.csv", ec);
        if (std::filesystem::exists(d/"orders.csv.new", ec))   std::filesystem::rename(d/"orders.csv.new",   d/"orders.csv",   ec);
        std::filesystem::remove(j, ec);
        std::cout<<"recovered\n"; return 0;
      }
      std::cerr<<"Usage: txn status|recover [--dir PATH]\n"; return 1;
    }


    else if (cmd=="sbom") {
      if (argc>2 && std::string(argv[2])=="generate"){
        auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir","."); bool json = arg_or(m,"--output","json")=="json"; std::string hopt = arg_or(m,"--hash","none");
        std::vector<std::tuple<std::string, unsigned long long, unsigned long long>> items;
        std::error_code ec;
        for (auto &e: std::filesystem::recursive_directory_iterator(std::filesystem::path(dir), ec)){
          if (e.is_directory()) continue;
          auto p = e.path(); auto rel = std::filesystem::relative(p, dir, ec);
          unsigned long long sz = (unsigned long long)std::filesystem::file_size(p, ec);
          auto ft = std::filesystem::last_write_time(p, ec);
          auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ft - decltype(ft)::clock::now() + std::chrono::system_clock::now());
          auto epoch = std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
          std::string hx; if (hopt=="sha256"){ std::ifstream fin(p, std::ios::binary); std::ostringstream ss; ss<<fin.rdbuf(); hx = sha256_hex(ss.str()); }
          items.push_back({ rel.string(), sz, (unsigned long long)epoch });
        }
        if (json){
          std::cout<<"{\"files\":[";
          for (size_t i=0;i<items.size();++i){ if(i) std::cout<<","; auto &t=items[i]; std::cout<<"{\"path\":\""<<escape_json(std::get<0>(t))<<"\",\"size\":"<<std::get<1>(t)<<",\"mtime\":"<<std::get<2>(t); if(hopt==\"sha256\"){ std::cout<<",\"sha256\":\""<<sha256_hex(std::string())<<"\""; } std::cout<<"}"; }
          std::cout<<"]}\n";
        } else {
          for (auto &t: items){ std::string hx=""; if(hopt=="sha256"){ std::ifstream fin(std::filesystem::path(dir)/std::get<0>(t), std::ios::binary); std::ostringstream ss; ss<<fin.rdbuf(); hx = sha256_hex(ss.str()); }
            std::cout<<std::get<0>(t)<<","<<std::get<1>(t)<<","<<std::get<2>(t); if(!hx.empty()) std::cout<<","<<hx; std::cout<<"\n"; }
        }
        return 0;
      }
      std::cerr<<"Usage: sbom generate [--dir PATH] [--output json|text]\n"; return 1;
    }


    else if (cmd=="replicate") {
      if (argc>2 && std::string(argv[2])=="push"){
        auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir); auto to=arg_or(m,"--to","");
        if (to.empty()){ std::cerr<<"missing --to\n"; return 1; }
        Inventory inv; std::vector<Order> oo; auto st=make_csv_storage(dir); st->load_products(inv); st->load_orders(oo);
        if(!atomic_write_dataset(inv, oo, to)){ std::cerr<<"atomic_write_failed\n"; return 1; }
        std::cout<<"ok\n"; return 0;
      }
      std::cerr<<"Usage: replicate push --to DIR [--dir PATH]\n"; return 1;
    }


    else if (cmd=="worm") {
      auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
      std::filesystem::path p = std::filesystem::path(dir)/".neonstore"/"worm.ini"; std::error_code ec; std::filesystem::create_directories(p.parent_path(), ec);
      if (argc>2 && std::string(argv[2])=="status"){
        bool on = worm_active(dir); std::cout<<(on? "on":"off")<<"\n"; return on?0:2;
      } else if (argc>2 && std::string(argv[2])=="enable"){
        std::string until_iso = arg_or(m,"--until",""); if (until_iso.empty()){ std::cerr<<"missing --until\n"; return 1; }
        long long epoch = parse_iso_utc_epoch(until_iso); if (epoch<=0){ std::cerr<<"bad_until\n"; return 1; }
        std::ofstream f(p.string(), std::ios::binary|std::ios::trunc); f<<"until="<<epoch<<"\n"; std::cout<<"on\n"; return 0;
      } else if (argc>2 && std::string(argv[2])=="disable"){
        std::filesystem::remove(p, ec); std::cout<<"off\n"; return 0;
      }
      std::cerr<<"Usage: worm enable --until YYYY-MM-DDThh:mm:ssZ | status | disable [--dir PATH]\n"; return 1;
    }


    else if (cmd=="utf8") {
      if (argc>2 && std::string(argv[2])=="validate"){
        auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
        Inventory inv; std::vector<Order> oo; auto st=make_csv_storage(dir); st->load_products(inv); st->load_orders(oo);
        int bad=0;
        auto is_valid_utf8 = [&](const std::string& s)->bool{
          const unsigned char* p = (const unsigned char*)s.data(); size_t i=0,n=s.size();
          while (i<n){
            unsigned char c=p[i];
            if (c<0x80){ i++; continue; }
            else if ((c>>5)==0x6 && i+1<n && (p[i+1]>>6)==0x2){ i+=2; }
            else if ((c>>4)==0xE && i+2<n && (p[i+1]>>6)==0x2 && (p[i+2]>>6)==0x2){ i+=3; }
            else if ((c>>3)==0x1E && i+3<n && (p[i+1]>>6)==0x2 && (p[i+2]>>6)==0x2 && (p[i+3]>>6)==0x2){ i+=4; }
            else { return false; }
          }
          return true;
        };
        for (auto &p: inv.list_products()){ if (!is_valid_utf8(p.name)) { std::cout<<"bad_name "<<p.id<<"\n"; ++bad; } }
        return bad? 2:0;
      } else if (argc>2 && std::string(argv[2])=="scrub"){
        auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
        if(!must_allow_write()) return 4; if(!cas_guard(m, dir)) return 6;
        Inventory inv; make_csv_storage(dir)->load_products(inv);
        auto is_valid_utf8 = [&](const std::string& s)->bool{
          const unsigned char* p = (const unsigned char*)s.data(); size_t i=0,n=s.size();
          while (i<n){
            unsigned char c=p[i];
            if (c<0x80){ i++; continue; }
            else if ((c>>5)==0x6 && i+1<n && (p[i+1]>>6)==0x2){ i+=2; }
            else if ((c>>4)==0xE && i+2<n && (p[i+1]>>6)==0x2 && (p[i+2]>>6)==0x2){ i+=3; }
            else if ((c>>3)==0x1E && i+3<n && (p[i+1]>>6)==0x2 && (p[i+2]>>6)==0x2 && (p[i+3]>>6)==0x2){ i+=4; }
            else { return false; }
          }
          return true;
        };
        auto fix = [&](const std::string& s)->std::string{
          std::string out; out.reserve(s.size());
          const unsigned char* p = (const unsigned char*)s.data(); size_t i=0,n=s.size();
          while (i<n){
            unsigned char c=p[i];
            if (c<0x80){ out.push_back((char)c); i++; continue; }
            else if ((c>>5)==0x6 && i+1<n && (p[i+1]>>6)==0x2){ out.append((const char*)p+i,2); i+=2; }
            else if ((c>>4)==0xE && i+2<n && (p[i+1]>>6)==0x2 && (p[i+2]>>6)==0x2){ out.append((const char*)p+i,3); i+=3; }
            else if ((c>>3)==0x1E && i+3<n && (p[i+1]>>6)==0x2 && (p[i+2]>>6)==0x2 && (p[i+3]>>6)==0x2){ out.append((const char*)p+i,4); i+=4; }
            else { out.push_back('?'); i++; }
          }
          return out;
        };
        int fixed=0; for (auto &p: inv.list_products()){ if(!is_valid_utf8(p.name)){ auto x=p; x.name=fix(x.name); inv.update_product(x); ++fixed; } }
        CsvStorage::save_products(inv, dir); std::cout<<"fixed="<<fixed<<"\n"; return 0;
      }
      std::cerr<<"Usage: utf8 validate|scrub [--dir PATH]\n"; return 1;
    }


    else if (cmd=="vdiff") {
      // verbose dataset diff: vdiff --dir2 PATH [--dir PATH] [--output json|text]
      auto m=parse_kv(argc,argv); auto dir1=arg_or(m,"--dir", data_dir); auto dir2=arg_or(m,"--dir2",""); bool json = arg_or(m,"--output","json")=="json";
      if (dir2.empty()){ std::cerr<<"missing --dir2\n"; return 1; }
      Inventory a1; std::vector<Order> o1; auto s1=make_csv_storage(dir1); s1->load_products(a1); s1->load_orders(o1);
      Inventory a2; std::vector<Order> o2; auto s2=make_csv_storage(dir2); s2->load_products(a2); s2->load_orders(o2);
      std::set<std::string> ids1, ids2; for (auto&p: a1.list_products()) ids1.insert(p.id); for (auto&p: a2.list_products()) ids2.insert(p.id);
      std::vector<std::string> added, removed, price_changed, stock_changed;
      for (auto&id: ids2){ if (!ids1.count(id)) added.push_back(id); }
      for (auto&id: ids1){ if (!ids2.count(id)) removed.push_back(id); }
      std::map<std::string, Product> M1; for (auto&p: a1.list_products()) M1[p.id]=p;
      for (auto&p: a2.list_products()){ auto it=M1.find(p.id); if (it!=M1.end()){ if (p.price!=it->second.price) price_changed.push_back(p.id); if (p.stock!=it->second.stock) stock_changed.push_back(p.id); } }
      auto cnt_items = [](const std::vector<Order>& oo){ size_t n=0; for (auto&o: oo) n+=o.items.size(); return n; };
      long long orders_delta = (long long)o2.size() - (long long)o1.size();
      long long items_delta  = (long long)cnt_items(o2) - (long long)cnt_items(o1);
      if (json){
        std::cout<<"{"
          << "\"products_added\":"<<added.size()
          << ",\"products_removed\":"<<removed.size()
          << ",\"price_changed\":"<<price_changed.size()
          << ",\"stock_changed\":"<<stock_changed.size()
          << ",\"orders_delta\":"<<orders_delta
          << ",\"items_delta\":"<<items_delta
          << ",\"added_ids\":["; for(size_t i=0;i<added.size();++i){ if(i) std::cout<<","; std::cout<<"\""<<added[i]<<"\""; }
        std::cout<<"],\"removed_ids\":["; for(size_t i=0;i<removed.size();++i){ if(i) std::cout<<","; std::cout<<"\""<<removed[i]<<"\""; }
        std::cout<<"],\"price_changed_ids\":["; for(size_t i=0;i<price_changed.size();++i){ if(i) std::cout<<","; std::cout<<"\""<<price_changed[i]<<"\""; }
        std::cout<<"],\"stock_changed_ids\":["; for(size_t i=0;i<stock_changed.size();++i){ if(i) std::cout<<","; std::cout<<"\""<<stock_changed[i]<<"\""; }
        std::cout<<"]}\n";
      } else {
        for (auto &x: added)         std::cout<<"added "<<x<<"\n";
        for (auto &x: removed)       std::cout<<"removed "<<x<<"\n";
        for (auto &x: price_changed) std::cout<<"price_changed "<<x<<"\n";
        for (auto &x: stock_changed) std::cout<<"stock_changed "<<x<<"\n";
        std::cout<<"orders_delta="<<orders_delta<<"\nitems_delta="<<items_delta<<"\n";
      }
      return 0;
    }

    else if (cmd=="merge") {
      // merge --dir2 PATH [--dir PATH] [--strategy prefer-dir1|prefer-dir2|safest] [--dry-run]
      auto m=parse_kv(argc,argv); auto dir1=arg_or(m,"--dir", data_dir); auto dir2=arg_or(m,"--dir2","");
      bool dry = m.count("--dry-run"); std::string strat = arg_or(m,"--strategy","safest");
      if (dir2.empty()){ std::cerr<<"missing --dir2\n"; return 1; }
      Inventory a1; std::vector<Order> o1; auto s1=make_csv_storage(dir1); s1->load_products(a1); s1->load_orders(o1);
      Inventory a2; std::vector<Order> o2; auto s2=make_csv_storage(dir2); s2->load_products(a2); s2->load_orders(o2);
      Inventory out = a1; int prod_created=0, prod_updated=0;
      for (auto &p2 : a2.list_products()){
        if (!out.exists(p2.id)){ out.add_product(p2); ++prod_created; }
        else {
          auto p1 = out.get_product(p2.id); Product merged = p1;
          if (strat=="prefer-dir2"){ merged = p2; }
          else if (strat=="prefer-dir1"){ merged = p1; }
          else { merged.stock = std::max(p1.stock, p2.stock); merged.price = std::min(p1.price, p2.price); if (p2.name.size()>p1.name.size()) merged.name=p2.name; }
          if (merged.name!=p1.name || merged.price!=p1.price || merged.stock!=p1.stock){ out.update_product(merged); ++prod_updated; }
        }
      }
      std::map<std::string, Order> M; for (auto &o: o1) M[o.id]=o;
      for (auto &o: o2){
        auto it=M.find(o.id);
        if (it==M.end()){ M[o.id]=o; }
        else {
          std::map<std::string, OrderItem> items; for (auto &it1: it->second.items) items[it1.product_id]=it1;
          for (auto &it2: o.items){ auto &slot = items[it2.product_id]; if (slot.product_id.empty()) slot=it2; else slot.quantity += it2.quantity; }
          it->second.items.clear(); for (auto &kv: items) it->second.items.push_back(kv.second);
        }
      }
      std::vector<Order> out_orders; out_orders.reserve(M.size()); for (auto &kv: M) out_orders.push_back(kv.second);
      if (!dry){ if(!atomic_write_dataset(out, out_orders, dir1)){ std::cerr<<"atomic_write_failed\n"; return 1; } }
      std::cout<<"{\"products_created\":"<<prod_created<<",\"products_updated\":"<<prod_updated<<",\"orders_total\":"<<out_orders.size()<<",\"dry_run\":"<<(dry? "true":"false")<<"}\n";
      return 0;
    }

    else if (cmd=="schema-lint") {
      // schema-lint [--dir PATH]
      auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
      auto check = [&](const std::string& f, const std::string& expect)->int{
        std::ifstream in((std::filesystem::path(dir)/f).string(), std::ios::binary); if(!in) return 1;
        std::string header; std::getline(in, header); while(!header.empty() && (header.back()=='\r'||header.back()=='\n')) header.pop_back();
        return header==expect? 0:2;
      };
      int e1 = check("products.csv","id,name,price,stock");
      int e2 = check("orders.csv","order_id,product_id,quantity,unit_price");
      std::cout<<"{\"products_header_ok\":"<<(e1==0? "true":"false")<<",\"orders_header_ok\":"<<(e2==0? "true":"false")<<"}\n";
      return (e1||e2)?2:0;
    }

    else if (cmd=="normalize") {
      // normalize [--trim] [--collapse-spaces] [--lower|--upper] [--dry-run] [--dir PATH]
      auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
      bool trim = m.count("--trim"); bool collapse = m.count("--collapse-spaces"); bool lower = m.count("--lower"); bool upper = m.count("--upper"); bool dry = m.count("--dry-run");
      if (lower && upper){ std::cerr<<"choose one of --lower or --upper\n"; return 1; }
      Inventory inv; make_csv_storage(dir)->load_products(inv);
      auto norm = [&](std::string s)->std::string{
        if (trim){ while(!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin()); while(!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back(); }
        if (collapse){ std::string out; bool in_space=false; for (char c: s){ if (std::isspace((unsigned char)c)){ if(!in_space){ out.push_back(' '); in_space=true; } } else { out.push_back(c); in_space=false; } } s=out; }
        if (lower){ std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); }); }
        if (upper){ std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::toupper(c); }); }
        return s;
      };
      int changed_n=0; for (auto &p: inv.list_products()){ auto newname = norm(p.name); if (newname!=p.name){ auto x=p; x.name=newname; inv.update_product(x); ++changed_n; } }
      if (!dry) CsvStorage::save_products(inv, dir);
      std::cout<<"{\"updated\":"<<changed_n<<",\"dry_run\":"<<(dry? "true":"false")<<"}\n"; return 0;
    }


    else if (cmd=="fuzz") {
      if (argc>2 && std::string(argv[2])=="run"){
        auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
        int ops=1000; try{ ops=std::stoi(arg_or(m,"--ops","1000")); }catch(...){}
        unsigned int seed=12345; try{ seed=(unsigned int)std::stoul(arg_or(m,"--seed","12345")); }catch(...){}
        std::mt19937 rng(seed);
        Inventory inv; std::vector<Order> oo; auto st=make_csv_storage(dir); st->load_products(inv); st->load_orders(oo);
        auto ids = inv.list_products();
        auto ids_list = inv.list_products();
        std::vector<std::string> pids; for (auto &p: inv.list_products()) pids.push_back(p.id);
        std::uniform_int_distribution<int> pick(0, (int)std::max<size_t>(1,pids.size())-1);
        int price_changes=0, stock_changes=0, guard_blocks=0;
        for (int i=0;i<ops;i++){
          if (pids.empty()) break;
          auto id = pids[pick(rng)];
          double delta = (rng()%200 - 100) / 10.0; // -10..+10
          if (delta!=0){
            auto p = inv.get_product(id);
            double newp = p.price + delta; if (newp<0) newp=0;
            // apply price_change_max_pct policy if present
            double maxpct = policy_unit_price_delta_pct(dir);
            if (p.price>0 && std::fabs((newp-p.price)/p.price*100.0) > maxpct){ guard_blocks++; continue; }
            p.price = newp; inv.update_product(p); price_changes++;
          }
          int sdelta = (rng()%11) - 5; // -5..+5
          auto p = inv.get_product(id); int ns = p.stock + sdelta; if (ns<0) ns=0; if (ns!=p.stock){ p.stock=ns; inv.update_product(p); stock_changes++; }
        }
        // Save and run a doctor-like validation
        if(!CsvStorage::save_products(inv, dir)){ std::cerr<<"save_fail\n"; return 1; }
        int doc_err=0; { std::set<std::string> ids; for (auto &p: inv.list_products()){ if(!ids.insert(p.id).second) ++doc_err; if (p.price<0||p.stock<0) ++doc_err; } }
        bool ok = (doc_err==0);
        std::cout<<"{\"ok\":"<<(ok? "true":"false")<<",\"ops\":"<<ops<<",\"price_changes\":"<<price_changes<<",\"stock_changes\":"<<stock_changes<<",\"policy_blocks\":"<<guard_blocks<<"}\n";
        return ok?0:2;
      }
      std::cerr<<"Usage: fuzz run [--ops N] [--seed S] [--dir PATH]\n"; return 1;
    }

    else if (cmd=="ledger") {

      if (argc>2 && std::string(argv[2])=="prune"){
        auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir",".neonstore"); int keep=5; try{ keep=std::stoi(arg_or(m,"--keep","5")); }catch(...){}
        std::vector<std::filesystem::directory_entry> files;
        std::error_code ec;
        for (auto &e: std::filesystem::directory_iterator(std::filesystem::path(dir), ec)){
          auto fn = e.path().filename().string();
          if (e.is_regular_file() && (fn.rfind("ledger-",0)==0 || fn.rfind("ledger.",0)==0 || fn=="ledger.log")) files.push_back(e);
        }
        std::sort(files.begin(), files.end(), [](auto&a,auto&b){ return a.path().filename().string() < b.path().filename().string(); });
        int removed=0; while ((int)files.size() > keep){ auto p = files.front().path(); files.erase(files.begin()); std::filesystem::remove(p, ec); ++removed; }
        std::cout<<"{\"kept\":"<<files.size()<<",\"removed\":"<<removed<<"}\n"; return 0;
      }


      if (argc>2 && std::string(argv[2])=="rotate"){
        auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
        std::filesystem::path neond = std::filesystem::path(dir)/".neonstore"; std::error_code ec; std::filesystem::create_directories(neond, ec);
        auto logp = neond/"ledger.log"; auto rot = neond/("ledger-" + now_iso() + ".log");
        if (std::filesystem::exists(logp, ec)){ std::filesystem::rename(logp, rot, ec); }
        std::cout<<"ok\n"; return 0;
      }

      auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
      std::filesystem::path neond = std::filesystem::path(dir)/".neonstore"; std::error_code ec; std::filesystem::create_directories(neond, ec);
      auto on = neond/"ledger.on"; auto logp = neond/"ledger.log";
      if (argc>2 && std::string(argv[2])=="enable"){ std::ofstream(on.string()).put('1'); std::cout<<"on\n"; return 0; }
      if (argc>2 && std::string(argv[2])=="disable"){ std::filesystem::remove(on, ec); std::cout<<"off\n"; return 0; }
      if (argc>2 && std::string(argv[2])=="status"){ bool st = std::filesystem::exists(on, ec); std::cout<<(st? "on":"off")<<"\n"; return st?0:2; }
      if (argc>2 && std::string(argv[2])=="tail"){ std::ifstream in(logp.string()); std::string s; while(std::getline(in,s)) std::cout<<s<<"\n"; return 0; }
      if (argc>2 && std::string(argv[2])=="verify"){
        std::ifstream in(logp.string()); std::string line, prev(64,'0'); bool ok=true;
        while(std::getline(in,line)){
          auto getv=[&](const std::string& key)->std::string{ auto p=line.find("\""+key+"\":\""); if(p==std::string::npos) return ""; size_t q=line.find("\"", p+key.size()+4); return line.substr(p+key.size()+4, q-(p+key.size()+4)); };
          std::string ds = getv("dataset"), ts = getv("ts"), pr = getv("prev"), en = getv("entry");
          if (pr!=prev){ ok=false; break; }
          std::string recompute = sha256_hex(prev + ds + ts);
          if (recompute!=en){ ok=false; break; }
          prev = en;
        }
        std::cout<<(ok? "ok":"bad")<<"\n"; return ok?0:2;
      }
      std::cerr<<"Usage: ledger enable|disable|status|tail|verify [--dir PATH]\n"; return 1;
    }

    else if (cmd=="export") {
      if (argc>2 && std::string(argv[2])=="jsonl"){
        auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir); auto outdir=arg_or(m,"--out","export");
        std::error_code ec; std::filesystem::create_directories(outdir, ec);
        Inventory inv; std::vector<Order> oo; auto st=make_csv_storage(dir); st->load_products(inv); st->load_orders(oo);
        auto esc=[&](const std::string& s)->std::string{ std::string o; for(char c: s){ if(c=='"'||c=='\\') o.push_back('\\'); if(c=='\n'){ o+="\\n"; continue; } o.push_back(c); } return o; };
        { std::ofstream f((std::filesystem::path(outdir)/"products.jsonl").string(), std::ios::binary|std::ios::trunc);
          for (auto &p: inv.list_products()){ f<<"{\"id\":\""<<esc(p.id)<<"\",\"name\":\""<<esc(p.name)<<"\",\"price\":"<<p.price<<",\"stock\":"<<p.stock<<"}\n"; }
        }
        { std::ofstream f((std::filesystem::path(outdir)/"orders.jsonl").string(), std::ios::binary|std::ios::trunc);
          for (auto &o: oo){ for (auto &it: o.items){ f<<"{\"order_id\":\""<<esc(o.id)<<"\",\"product_id\":\""<<esc(it.product_id)<<"\",\"quantity\":"<<it.quantity<<",\"unit_price\":"<<it.unit_price<<"}\n"; } }
        }
        std::cout<<"ok\n"; return 0;
      }
      std::cerr<<"Usage: export jsonl --out DIR [--dir PATH]\n"; return 1;
    }


    else if (cmd=="dedupe") {
      auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir); bool dry = m.count("--dry-run");
      if (argc>2 && std::string(argv[2])=="products"){
        Inventory inv; make_csv_storage(dir)->load_products(inv);
        std::set<std::string> seen; int removed=0;
        Inventory out;
        for (auto &p: inv.list_products()){
          if (seen.insert(p.id).second){ if(out.exists(p.id)) out.update_product(p); else out.add_product(p); }
          else { ++removed; }
        }
        if (!dry){ if(!CsvStorage::save_products(out, dir)){ std::cerr<<"save_fail\n"; return 1; } }
        std::cout<<"{\"duplicates_removed\":"<<removed<<",\"dry_run\":"<<(dry? "true":"false")<<"}\n"; return 0;
      } else if (argc>2 && std::string(argv[2])=="orders"){
        std::vector<Order> oo; make_csv_storage(dir)->load_orders(oo);
        std::map<std::string, std::map<std::string, OrderItem>> M; // order_id -> product_id -> item
        for (auto &o: oo){
          auto &mp = M[o.id];
          for (auto &it: o.items){ auto &slot = mp[it.product_id]; if (slot.product_id.empty()) slot=it; else slot.quantity += it.quantity; }
        }
        std::vector<Order> out; out.reserve(M.size());
        for (auto &kv: M){ Order o; o.id=kv.first; for (auto &kv2: kv.second) o.items.push_back(kv2.second); out.push_back(o); }
        if (!dry){ Inventory inv; make_csv_storage(dir)->load_products(inv); if(!atomic_write_dataset(inv, out, dir)){ std::cerr<<"atomic_write_failed\n"; return 1; } }
        std::cout<<"{\"orders_deduped\":"<<M.size()<<",\"dry_run\":"<<(dry? "true":"false")<<"}\n"; return 0;
      }
      std::cerr<<"Usage: dedupe products|orders [--dir PATH] [--dry-run]\n"; return 1;
    }


    else if (cmd=="analytics") {
      if (argc>2 && std::string(argv[2])=="summary"){
        auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir); bool json = arg_or(m,"--output","json")=="json";
        int topn=10; try{ topn=std::stoi(arg_or(m,"--top","10")); }catch(...){}
        int low=3; try{ low=std::stoi(arg_or(m,"--low","3")); }catch(...){}
        Inventory inv; std::vector<Order> oo; auto st=make_csv_storage(dir); st->load_products(inv); st->load_orders(oo);
        size_t nprod = inv.list_products().size(); size_t norders = oo.size();
        // inventory valuation
        double valuation=0.0; for (auto &p: inv.list_products()) valuation += p.price * p.stock;
        // revenue by product
        std::map<std::string,double> rev; size_t items=0;
        for (auto &o: oo){ for (auto &it: o.items){ rev[it.product_id] += it.unit_price * it.quantity; ++items; } }
        std::vector<std::pair<std::string,double>> top(rev.begin(), rev.end());
        std::sort(top.begin(), top.end(), [](auto&a,auto&b){ return a.second>b.second; });
        if ((int)top.size()>topn) top.resize(topn);
        // low stock count
        size_t low_count=0; for (auto &p: inv.list_products()){ if (p.stock<=low) low_count++; }
        if (json){
          std::cout<<"{\"products\":"<<nprod<<",\"orders\":"<<norders<<",\"order_items\":"<<items<<",\"inventory_value\":"<<valuation<<",\"low_stock_threshold\":"<<low<<",\"low_stock\":"<<low_count<<",\"top\":[";
          for (size_t i=0;i<top.size();++i){ if(i) std::cout<<","; std::cout<<"{\"product_id\":\""<<top[i].first<<"\",\"revenue\":"<<top[i].second<<"}"; }
          std::cout<<"]}\n";
        } else {
          std::cout<<"products="<<nprod<<"\norders="<<norders<<"\norder_items="<<items<<"\ninventory_value="<<valuation<<"\nlow_stock_threshold="<<low<<"\nlow_stock="<<low_count<<"\n";
          for (auto &kv: top){ std::cout<<"top "<<kv.first<<" "<<kv.second<<"\n"; }
        }
        return 0;
      }
      std::cerr<<"Usage: analytics summary [--dir PATH] [--output json|text] [--top N] [--low N]\n"; return 1;
    }


    else if (cmd=="safemode") {
      auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir);
      std::filesystem::path neond = std::filesystem::path(dir)/".neonstore"; std::error_code ec; std::filesystem::create_directories(neond, ec);
      auto ro = neond/"readonly.on";
      if (argc>2 && std::string(argv[2])=="enable"){ std::ofstream(ro.string()).put('1'); std::cout<<"on\n"; return 0; }
      if (argc>2 && std::string(argv[2])=="disable"){ std::filesystem::remove(ro, ec); std::cout<<"off\n"; return 0; }
      if (argc>2 && std::string(argv[2])=="status"){ bool st=std::filesystem::exists(ro, ec); std::cout<<(st? "on":"off")<<"\n"; return st?0:2; }
      std::cerr<<"Usage: safemode enable|disable|status [--dir PATH]\n"; return 1;
    }


    else if (cmd=="mask") {
      auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir); auto out=arg_or(m,"--out","masked"); auto strat=arg_or(m,"--strategy","redact");
      std::error_code ec; std::filesystem::create_directories(out, ec);
      Inventory inv; std::vector<Order> oo; auto st=make_csv_storage(dir); st->load_products(inv); st->load_orders(oo);
      auto hex2=[&](uint64_t x){ std::ostringstream s; s<<std::hex<<std::nouppercase<<x; return s.str(); };
      auto fnv=[&](const std::string& s){ uint64_t h=1469598103934665603ull; for (unsigned char c: s){ h^=c; h*=1099511628211ull;} return h; };
      {
        std::ofstream p((std::filesystem::path(out)/"products.csv").string(), std::ios::binary|std::ios::trunc);
        p<<"id,name,price,stock\n";
        for (auto &x: inv.list_products()){
          std::string name = x.name;
          if (strat=="redact"){ name = "REDACTED-" + hex2(fnv(x.id)); }
          else if (strat=="hash"){ name = "PSEUDO-" + hex2(fnv(x.name)); }
          p<<x.id<<","<<name<<","<<x.price<<","<<x.stock<<"\n";
        }
      }
      {
        std::ifstream in((std::filesystem::path(dir)/"orders.csv").string(), std::ios::binary); std::ofstream outp((std::filesystem::path(out)/"orders.csv").string(), std::ios::binary|std::ios::trunc);
        outp<<in.rdbuf();
      }
      std::cout<<"ok\n"; return 0;
    }


    else if (cmd=="query") {
      auto m=parse_kv(argc,argv); auto dir=arg_or(m,"--dir", data_dir); bool json = arg_or(m,"--output","json")=="json";
      auto where = arg_or(m,"--where",""); auto order = arg_or(m,"--order",""); int lim=0; try{ lim=std::stoi(arg_or(m,"--limit","0")); }catch(...){}
      auto parse_cond = [&](const std::string& expr)->std::vector<std::tuple<std::string,std::string,std::string>>{
        std::vector<std::tuple<std::string,std::string,std::string>> v;
        size_t start=0;
        while (true){
          size_t pos = expr.find("&&", start);
          std::string term = expr.substr(start, pos==std::string::npos? std::string::npos : pos-start);
          std::string t = term;
          auto l = t.find_first_not_of(" \t"); auto r = t.find_last_not_of(" \t");
          if (l==std::string::npos){ if (pos==std::string::npos) break; start=pos+2; continue; }
          t=t.substr(l, r-l+1);
          std::regex rx(R"(^([a-zA-Z_][a-zA-Z0-9_]*)\s*(==|!=|>=|<=|>|<)\s*(.+)$)");
          std::smatch mm;
          if (std::regex_match(t, mm, rx)){
            std::string val=mm[3];
            if (val.size()>=2 && val.front()=='"' && val.back()=='"'){ val=val.substr(1, val.size()-2); }
            v.emplace_back(mm[1], mm[2], val);
          }
          if (pos==std::string::npos) break; start=pos+2;
        }
        return v;
      };
      auto cmpnum=[&](double a, const std::string& op, double b)->bool{
        if(op=="==") return a==b; if(op=="!=") return a!=b; if(op==">") return a>b; if(op=="<") return a<b; if(op==">=") return a>=b; if(op=="<=") return a<=b; return false;
      };
      auto cmpstr=[&](const std::string&a,const std::string&op,const std::string&b)->bool{
        if(op=="==") return a==b; if(op=="!=") return a!=b; if(op==">") return a>b; if(op=="<") return a<b; if(op==">=") return a>=b; if(op=="<=") return a<=b; return false;
      };
      if (argc>2 && std::string(argv[2])=="products"){
        Inventory inv; make_csv_storage(dir)->load_products(inv);
        auto conds = parse_cond(where);
        std::vector<Product> out;
        for (auto &p: inv.list_products()){
          bool ok=true;
          for (auto &c: conds){
            auto f=std::get<0>(c),op=std::get<1>(c),val=std::get<2>(c);
            if (f=="id"||f=="name"){ ok = ok && cmpstr((f=="id"?p.id:p.name), op, val); }
            else if (f=="price"||f=="stock"){
              try{ double v=std::stod(val); ok = ok && cmpnum((f=="price"?p.price:(double)p.stock), op, v); }catch(...){ ok=false; }
            }
          }
          if (ok) out.push_back(p);
        }
        if (!order.empty()){
          std::istringstream ss(order); std::string field, dirn="asc"; ss>>field>>dirn;
          std::sort(out.begin(), out.end(), [&](const Product&a,const Product&b){
            if (field=="id"||field=="name"){
              std::string sa=(field=="id"?a.id:a.name), sb=(field=="id"?b.id:b.name);
              return (dirn=="desc")? sa>sb: sa<sb;
            }
            double ka = (field=="price"?a.price:(field=="stock"?a.stock:0));
            double kb = (field=="price"?b.price:(field=="stock"?b.stock:0));
            return (dirn=="desc")? ka>kb: ka<kb;
          });
        }
        if (lim>0 && (int)out.size()>lim) out.resize(lim);
        if (json){
          std::cout<<"[";
          for (size_t i=0;i<out.size();++i){ if(i) std::cout<<","; std::cout<<"{\"id\":\""<<out[i].id<<"\",\"name\":\""<<out[i].name<<"\",\"price\":"<<out[i].price<<",\"stock\":"<<out[i].stock<<"}"; }
          std::cout<<"]\n";
        } else { for (auto &p: out){ std::cout<<p.id<<","<<p.name<<","<<p.price<<","<<p.stock<<"\n"; } }
        return 0;
      } else if (argc>2 && std::string(argv[2])=="orders"){
        std::vector<Order> oo; make_csv_storage(dir)->load_orders(oo);
        auto conds = parse_cond(where);
        struct Row{ std::string order_id, product_id; int quantity; double unit_price; };
        std::vector<Row> rows;
        for (auto &o: oo){
          for (auto &it: o.items){
            bool ok=true;
            for (auto &c: conds){
              auto f=std::get<0>(c),op=std::get<1>(c),val=std::get<2>(c);
              if (f=="order_id"||f=="product_id"){ ok = ok && cmpstr((f=="order_id"?o.id:it.product_id), op, val); }
              else if (f=="quantity"||f=="unit_price"){
                try{ double v=std::stod(val); ok = ok && cmpnum((f=="quantity"? (double)it.quantity : it.unit_price), op, v); }catch(...){ ok=false; }
              }
            }
            if (ok) rows.push_back({o.id,it.product_id,it.quantity,it.unit_price});
          }
        }
        if (!order.empty()){
          std::istringstream ss(order); std::string field, dirn="asc"; ss>>field>>dirn;
          std::sort(rows.begin(), rows.end(), [&](const Row&a,const Row&b){
            if (field=="order_id") return (dirn=="desc")? a.order_id>b.order_id: a.order_id<b.order_id;
            if (field=="product_id") return (dirn=="desc")? a.product_id>b.product_id: a.product_id<b.product_id;
            if (field=="quantity") return (dirn=="desc")? a.quantity>b.quantity: a.quantity<b.quantity;
            if (field=="unit_price") return (dirn=="desc")? a.unit_price>b.unit_price: a.unit_price<b.unit_price;
            return a.order_id<b.order_id;
          });
        }
        if (lim>0 && (int)rows.size()>lim) rows.resize(lim);
        if (json){
          std::cout<<"[";
          for (size_t i=0;i<rows.size();++i){ if(i) std::cout<<","; auto&r=rows[i]; std::cout<<"{\"order_id\":\""<<r.order_id<<"\",\"product_id\":\""<<r.product_id<<"\",\"quantity\":"<<r.quantity<<",\"unit_price\":"<<r.unit_price<<"}"; }
          std::cout<<"]\n";
        } else { for (auto &r: rows){ std::cout<<r.order_id<<","<<r.product_id<<","<<r.quantity<<","<<r.unit_price<<"\n"; } }
        return 0;
      }
      std::cerr<<"Usage: query products|orders --where EXPR [--order FIELD [asc|desc]] [--limit N] [--dir PATH] [--output json|text]\n"; return 1;
    }
