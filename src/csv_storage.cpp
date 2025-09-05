// SPDX-License-Identifier: Apache-2.0

#include "neonstore/csv_storage.hpp"
#include "neonstore/crc32.hpp"
#include "neonstore/sha256.hpp"
#include <fstream>
#include <random>
#include <filesystem>
#include <iomanip>

static std::string now_iso_csv(){
  std::time_t t = std::time(nullptr);
  std::tm *tm = std::gmtime(&t);
  char buf[32]; std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
  return std::string(buf);
}
// FNV-1a 64
static uint64_t fnv1a64_csv(const void* d, size_t n){
  const unsigned char* p = (const unsigned char*)d; uint64_t h = 1469598103934665603ull;
  for (size_t i=0;i<n;i++){ h ^= p[i]; h *= 1099511628211ull; }
  return h;
}
// Tiny SHA-256 (compact)
static uint32_t RORcsv(uint32_t x, uint32_t n){ return (x>>n)|(x<<(32-n)); }
struct sha256_ctx_csv { uint32_t h[8]; uint64_t len; uint8_t buf[64]; size_t idx; };
static void sha256_init_csv(sha256_ctx_csv& c){
  c.h[0]=0x6a09e667; c.h[1]=0xbb67ae85; c.h[2]=0x3c6ef372; c.h[3]=0xa54ff53a;
  c.h[4]=0x510e527f; c.h[5]=0x9b05688c; c.h[6]=0x1f83d9ab; c.h[7]=0x5be0cd19;
  c.len=0; c.idx=0;
}
static void sha256_chunk_csv(sha256_ctx_csv& c, const uint8_t* p){
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
  for(int i=16;i<64;i++){ uint32_t s0=RORcsv(w[i-15],7)^RORcsv(w[i-15],18)^(w[i-15]>>3); uint32_t s1=RORcsv(w[i-2],17)^RORcsv(w[i-2],19)^(w[i-2]>>10); w[i]=w[i-16]+s0+w[i-7]+s1; }
  uint32_t a=c.h[0],b=c.h[1],c0=c.h[2],d=c.h[3],e=c.h[4],f=c.h[5],g=c.h[6],h=c.h[7];
  for(int i=0;i<64;i++){
    uint32_t S1=RORcsv(e,6)^RORcsv(e,11)^RORcsv(e,25);
    uint32_t ch=(e&f)^( (~e)&g );
    uint32_t temp1=h+S1+ch+k[i]+w[i];
    uint32_t S0=RORcsv(a,2)^RORcsv(a,13)^RORcsv(a,22);
    uint32_t maj=(a&b)^(a&c0)^(b&c0);
    uint32_t temp2=S0+maj;
    h=g; g=f; f=e; e=d+temp1; d=c0; c0=b; b=a; a=temp1+temp2;
  }
  c.h[0]+=a; c.h[1]+=b; c.h[2]+=c0; c.h[3]+=d; c.h[4]+=e; c.h[5]+=f; c.h[6]+=g; c.h[7]+=h;
}
static void sha256_update_csv(sha256_ctx_csv& c, const uint8_t* data, size_t len){
  c.len += len;
  while(len>0){
    size_t n = std::min((size_t)64 - c.idx, len);
    std::memcpy(c.buf + c.idx, data, n);
    c.idx += n; data += n; len -= n;
    if (c.idx==64){ sha256_chunk_csv(c, c.buf); c.idx=0; }
  }
}
static std::string sha256_hex_csv(const std::string& s){
  sha256_ctx_csv c; sha256_init_csv(c); sha256_update_csv(c, (const uint8_t*)s.data(), s.size());
  uint64_t bitlen = c.len * 8;
  uint8_t one = 0x80; sha256_update_csv(c, &one, 1);
  uint8_t z = 0; while(c.idx != 56){ sha256_update_csv(c, &z, 1); }
  uint8_t lenb[8]; for(int i=0;i<8;i++) lenb[7-i] = (uint8_t)((bitlen >> (8*i)) & 0xFF);
  sha256_update_csv(c, lenb, 8);
  if (c.idx) sha256_chunk_csv(c, c.buf);
  std::ostringstream os; os<<std::hex<<std::nouppercase;
  for(int i=0;i<8;i++){ uint32_t v=c.h[i]; os<<std::setw(8)<<std::setfill('0')<<v; }
  return os.str();
}

#include <regex>
#include <cstdlib>
#include <cstdio>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif
#ifndef _WIN32
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <windows.h>
#endif
#include <iostream>
#include <cstdio>
#ifdef _WIN32
 #include <io.h>
 #define fsync _commit
#else
 #include <unistd.h>
#endif

#include <filesystem>
#include <iomanip>

static std::string now_iso_csv(){
  std::time_t t = std::time(nullptr);
  std::tm *tm = std::gmtime(&t);
  char buf[32]; std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
  return std::string(buf);
}
// FNV-1a 64
static uint64_t fnv1a64_csv(const void* d, size_t n){
  const unsigned char* p = (const unsigned char*)d; uint64_t h = 1469598103934665603ull;
  for (size_t i=0;i<n;i++){ h ^= p[i]; h *= 1099511628211ull; }
  return h;
}
// Tiny SHA-256 (compact)
static uint32_t RORcsv(uint32_t x, uint32_t n){ return (x>>n)|(x<<(32-n)); }
struct sha256_ctx_csv { uint32_t h[8]; uint64_t len; uint8_t buf[64]; size_t idx; };
static void sha256_init_csv(sha256_ctx_csv& c){
  c.h[0]=0x6a09e667; c.h[1]=0xbb67ae85; c.h[2]=0x3c6ef372; c.h[3]=0xa54ff53a;
  c.h[4]=0x510e527f; c.h[5]=0x9b05688c; c.h[6]=0x1f83d9ab; c.h[7]=0x5be0cd19;
  c.len=0; c.idx=0;
}
static void sha256_chunk_csv(sha256_ctx_csv& c, const uint8_t* p){
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
  for(int i=16;i<64;i++){ uint32_t s0=RORcsv(w[i-15],7)^RORcsv(w[i-15],18)^(w[i-15]>>3); uint32_t s1=RORcsv(w[i-2],17)^RORcsv(w[i-2],19)^(w[i-2]>>10); w[i]=w[i-16]+s0+w[i-7]+s1; }
  uint32_t a=c.h[0],b=c.h[1],c0=c.h[2],d=c.h[3],e=c.h[4],f=c.h[5],g=c.h[6],h=c.h[7];
  for(int i=0;i<64;i++){
    uint32_t S1=RORcsv(e,6)^RORcsv(e,11)^RORcsv(e,25);
    uint32_t ch=(e&f)^( (~e)&g );
    uint32_t temp1=h+S1+ch+k[i]+w[i];
    uint32_t S0=RORcsv(a,2)^RORcsv(a,13)^RORcsv(a,22);
    uint32_t maj=(a&b)^(a&c0)^(b&c0);
    uint32_t temp2=S0+maj;
    h=g; g=f; f=e; e=d+temp1; d=c0; c0=b; b=a; a=temp1+temp2;
  }
  c.h[0]+=a; c.h[1]+=b; c.h[2]+=c0; c.h[3]+=d; c.h[4]+=e; c.h[5]+=f; c.h[6]+=g; c.h[7]+=h;
}
static void sha256_update_csv(sha256_ctx_csv& c, const uint8_t* data, size_t len){
  c.len += len;
  while(len>0){
    size_t n = std::min((size_t)64 - c.idx, len);
    std::memcpy(c.buf + c.idx, data, n);
    c.idx += n; data += n; len -= n;
    if (c.idx==64){ sha256_chunk_csv(c, c.buf); c.idx=0; }
  }
}
static std::string sha256_hex_csv(const std::string& s){
  sha256_ctx_csv c; sha256_init_csv(c); sha256_update_csv(c, (const uint8_t*)s.data(), s.size());
  uint64_t bitlen = c.len * 8;
  uint8_t one = 0x80; sha256_update_csv(c, &one, 1);
  uint8_t z = 0; while(c.idx != 56){ sha256_update_csv(c, &z, 1); }
  uint8_t lenb[8]; for(int i=0;i<8;i++) lenb[7-i] = (uint8_t)((bitlen >> (8*i)) & 0xFF);
  sha256_update_csv(c, lenb, 8);
  if (c.idx) sha256_chunk_csv(c, c.buf);
  std::ostringstream os; os<<std::hex<<std::nouppercase;
  for(int i=0;i<8;i++){ uint32_t v=c.h[i]; os<<std::setw(8)<<std::setfill('0')<<v; }
  return os.str();
}

#include <regex>
#include <fstream>
#include <random>
#include <filesystem>
#include <iomanip>

static std::string now_iso_csv(){
  std::time_t t = std::time(nullptr);
  std::tm *tm = std::gmtime(&t);
  char buf[32]; std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
  return std::string(buf);
}
// FNV-1a 64
static uint64_t fnv1a64_csv(const void* d, size_t n){
  const unsigned char* p = (const unsigned char*)d; uint64_t h = 1469598103934665603ull;
  for (size_t i=0;i<n;i++){ h ^= p[i]; h *= 1099511628211ull; }
  return h;
}
// Tiny SHA-256 (compact)
static uint32_t RORcsv(uint32_t x, uint32_t n){ return (x>>n)|(x<<(32-n)); }
struct sha256_ctx_csv { uint32_t h[8]; uint64_t len; uint8_t buf[64]; size_t idx; };
static void sha256_init_csv(sha256_ctx_csv& c){
  c.h[0]=0x6a09e667; c.h[1]=0xbb67ae85; c.h[2]=0x3c6ef372; c.h[3]=0xa54ff53a;
  c.h[4]=0x510e527f; c.h[5]=0x9b05688c; c.h[6]=0x1f83d9ab; c.h[7]=0x5be0cd19;
  c.len=0; c.idx=0;
}
static void sha256_chunk_csv(sha256_ctx_csv& c, const uint8_t* p){
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
  for(int i=16;i<64;i++){ uint32_t s0=RORcsv(w[i-15],7)^RORcsv(w[i-15],18)^(w[i-15]>>3); uint32_t s1=RORcsv(w[i-2],17)^RORcsv(w[i-2],19)^(w[i-2]>>10); w[i]=w[i-16]+s0+w[i-7]+s1; }
  uint32_t a=c.h[0],b=c.h[1],c0=c.h[2],d=c.h[3],e=c.h[4],f=c.h[5],g=c.h[6],h=c.h[7];
  for(int i=0;i<64;i++){
    uint32_t S1=RORcsv(e,6)^RORcsv(e,11)^RORcsv(e,25);
    uint32_t ch=(e&f)^( (~e)&g );
    uint32_t temp1=h+S1+ch+k[i]+w[i];
    uint32_t S0=RORcsv(a,2)^RORcsv(a,13)^RORcsv(a,22);
    uint32_t maj=(a&b)^(a&c0)^(b&c0);
    uint32_t temp2=S0+maj;
    h=g; g=f; f=e; e=d+temp1; d=c0; c0=b; b=a; a=temp1+temp2;
  }
  c.h[0]+=a; c.h[1]+=b; c.h[2]+=c0; c.h[3]+=d; c.h[4]+=e; c.h[5]+=f; c.h[6]+=g; c.h[7]+=h;
}
static void sha256_update_csv(sha256_ctx_csv& c, const uint8_t* data, size_t len){
  c.len += len;
  while(len>0){
    size_t n = std::min((size_t)64 - c.idx, len);
    std::memcpy(c.buf + c.idx, data, n);
    c.idx += n; data += n; len -= n;
    if (c.idx==64){ sha256_chunk_csv(c, c.buf); c.idx=0; }
  }
}
static std::string sha256_hex_csv(const std::string& s){
  sha256_ctx_csv c; sha256_init_csv(c); sha256_update_csv(c, (const uint8_t*)s.data(), s.size());
  uint64_t bitlen = c.len * 8;
  uint8_t one = 0x80; sha256_update_csv(c, &one, 1);
  uint8_t z = 0; while(c.idx != 56){ sha256_update_csv(c, &z, 1); }
  uint8_t lenb[8]; for(int i=0;i<8;i++) lenb[7-i] = (uint8_t)((bitlen >> (8*i)) & 0xFF);
  sha256_update_csv(c, lenb, 8);
  if (c.idx) sha256_chunk_csv(c, c.buf);
  std::ostringstream os; os<<std::hex<<std::nouppercase;
  for(int i=0;i<8;i++){ uint32_t v=c.h[i]; os<<std::setw(8)<<std::setfill('0')<<v; }
  return os.str();
}

#include <regex>
#include <cstdlib>
#include <cstdio>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif
#ifndef _WIN32
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <windows.h>
#endif
#include <iostream>
#include <cstdio>
#ifdef _WIN32
 #include <io.h>
 #define fsync _commit
#else
 #include <unistd.h>
#endif

#include <sstream>
#include <unordered_map>

namespace fs = std::filesystem;
namespace neonstore {

struct PolicyLimits {
  bool has_price_min=false, has_price_max=false, has_stock_max=false, has_quantity_max=false;
  double price_min=0.0, price_max=0.0; int stock_max=0, quantity_max=0;
  std::string name_forbid_regex;
};
static PolicyLimits load_policy_limits(const std::string& dir){
  PolicyLimits L; std::error_code ec;
  for (auto p : { std::filesystem::path("policy.ini"), std::filesystem::path(".neonstore")/"policy.ini", std::filesystem::path(dir)/"policy.ini" }){
    if (!std::filesystem::exists(p, ec)) continue;
    std::ifstream f(p.string()); std::string s;
    while (std::getline(f,s)){
      if (s.size()==0 || s[0]=='#' || s[0]==';') continue;
      auto pos=s.find('='); if (pos==std::string::npos) continue;
      auto k=s.substr(0,pos), v=s.substr(pos+1);
      auto lowerk = lower(k);
      try{
        if (lowerk=="price_min"){ L.price_min=std::stod(v); L.has_price_min=true; }
        else if (lowerk=="price_max"){ L.price_max=std::stod(v); L.has_price_max=true; }
        else if (lowerk=="stock_max"){ L.stock_max=std::stoi(v); L.has_stock_max=true; }
        else if (lowerk=="quantity_max"){ L.quantity_max=std::stoi(v); L.has_quantity_max=true; }
        else if (lowerk=="name_forbid_regex"){ L.name_forbid_regex=v; }
      }catch(...){}
    }
  }
  return L;
}
static bool chaos_inject(){
  const char* p = std::getenv("NEONSTORE_CHAOS_P");
  if (!p || !*p) cleanup();
  return false;
  double prob=0.0; try{ prob = std::stod(p); }catch(...){ prob=0.0; }
  if (prob<=0.0) cleanup();
  return false;
  static std::random_device rd; static std::mt19937 gen(rd()); static std::uniform_real_distribution<double> U(0.0,1.0);
  return U(gen) < prob;
}


static uint64_t dir_size_bytes(const std::string& dir){
  uint64_t tot=0; std::error_code ec;
  for (auto& e: std::filesystem::recursive_directory_iterator(dir, ec)){
    if (!ec && e.is_regular_file(ec)) { tot += (uint64_t)std::filesystem::file_size(e, ec); }
  }
  return tot;
}
static uint64_t quota_max_bytes(const std::string& dir){
  const char* env = std::getenv("NEONSTORE_MAX_BYTES"); if (env && *env){ try{ return std::stoull(env); }catch(...){ } }
  for (auto p : { std::filesystem::path(dir)/".neonstore"/"quota.ini", std::filesystem::path("neonstore.ini"), std::filesystem::path(".neonstore")/"config.ini" }){
    std::error_code ec; if (std::filesystem::exists(p, ec)){ std::ifstream f(p.string()); std::string line; while(std::getline(f,line)){ if(line.rfind("max_bytes=",0)==0){ try{ return std::stoull(line.substr(10)); }catch(...){ } } } }
  }
  return 0;
}
static bool quota_allow_write(const std::string& dir, uint64_t safety=0){
  uint64_t q = quota_max_bytes(dir); if (!q) cleanup();

  // Append tamper-evident ledger entry if enabled
  try{
    std::filesystem::path neond = std::filesystem::path(dir)/".neonstore";
    std::error_code ec;
    if (std::filesystem::exists(neond/"ledger.on", ec)){
      auto rd=[&](const std::string& f)->std::string{
        std::ifstream in((std::filesystem::path(dir)/f).string(), std::ios::binary); std::ostringstream ss; ss<<in.rdbuf();
        std::string s=ss.str(); std::string out; out.reserve(s.size());
        for(size_t i=0;i<s.size();++i){ char c=s[i]; if(c=='\r'){ if(i+1<s.size()&&s[i+1]=='\n'){ continue; } else continue; } else out.push_back(c); } return out;
      };
      auto prod = rd("products.csv"); auto ord = rd("orders.csv");
      uint64_t h=0; h=fnv1a64_csv(prod.data(), prod.size()); h = fnv1a64_csv(&h, sizeof(h)) ^ fnv1a64_csv(ord.data(), ord.size());
      std::ostringstream ds; ds<<std::hex<<std::nouppercase<<h;
      std::filesystem::create_directories(neond, ec);
      std::filesystem::path logp = neond/"ledger.log";
      std::string prev="";
      { std::ifstream in(logp.string()); std::string line, last; while(std::getline(in,line)) if(!line.empty()) last=line;
        if(!last.empty()){ auto p=last.find("\"entry\":\""); if(p!=std::string::npos){ size_t q=last.find("\"", p+9); if(q!=std::string::npos) prev = last.substr(p+9, q-(p+9)); } } }
      if (prev.empty()) prev = std::string(64,'0');
      std::string ts = now_iso_csv();
      std::string entry = sha256_hex_csv(prev + ds.str() + ts);
      std::ofstream out((logp).string(), std::ios::app);
      out<<"{\"ts\":\"" << ts << "\",\"dataset\":\"" << ds.str() << "\",\"prev\":\"" << prev << "\",\"entry\":\"" << entry << "\"}\n";
    }
  } catch(...) { /* ignore ledger errors */ }

  return true;
  uint64_t used = dir_size_bytes(dir);
  return used + safety <= q;
}

static void secure_set_perms(const std::string& path){
#if !defined(_WIN32)
  using std::filesystem::perms;
  std::error_code ec;
  std::filesystem::permissions(path, perms::owner_read|perms::owner_write|perms::group_read, ec);
#endif
}

static void run_hook_prepost(const std::string& dir, const char* when, const char* target){
  try{
    const char* nohooks = std::getenv("NEONSTORE_NO_HOOKS");
    if (nohooks && (*nohooks=='1'||*nohooks=='t'||*nohooks=='T'||*nohooks=='y'||*nohooks=='Y')) return;
    std::filesystem::path hook = std::filesystem::path(dir)/".hooks"/when;
    if (std::filesystem::exists(hook)){
      std::string cmd = hook.string();
#ifdef _WIN32
      cmd = """ + cmd + """;
#endif
      // Set environment variables
      std::string envdir = "NEONSTORE_DIR=" + std::filesystem::path(dir).string();
#ifdef _WIN32
      _putenv(envdir.c_str());
#else
      setenv("NEONSTORE_DIR", std::filesystem::path(dir).c_str(), 1);
#endif
      std::string envt = std::string("NEONSTORE_HOOK_TARGET=") + target;
#ifdef _WIN32
      _putenv(envt.c_str());
#else
      setenv("NEONSTORE_HOOK_TARGET", target, 1);
#endif
      std::system(cmd.c_str());
    }
  }catch(...) { /* ignore hook errors */ }
}

#ifndef _WIN32
static void fsync_dir_after(const std::string& path){
  auto d = std::filesystem::path(path).parent_path().string();
  int fd = ::open(d.c_str(), O_DIRECTORY|O_RDONLY);
  if (fd>=0){ ::fsync(fd); ::close(fd);} }
#else
static void fsync_dir_after(const std::string&){ }
#endif

static LockMode g_lockmode = LockMode::Dir;
void CsvLock::set_mode(LockMode m){ g_lockmode=m; }
LockMode CsvLock::get_mode(){ return g_lockmode; }

static Integrity g_integrity = Integrity::Lenient;
static IntegrityAlgo g_algo = IntegrityAlgo::CRC32;
void CsvIntegrity::set_algo(IntegrityAlgo a){ g_algo=a; }
IntegrityAlgo CsvIntegrity::get_algo(){ return g_algo; }
void CsvIntegrity::set(Integrity m){ g_integrity=m; }
Integrity CsvIntegrity::get(){ return g_integrity; }


namespace {
class LockDir { public:
  explicit LockDir(const std::filesystem::path& dir) : p_(dir/".lock") {
    std::error_code ec; std::filesystem::create_directory(p_, ec);
    if (ec) throw std::runtime_error("lock busy: " + p_.string());
  }
  ~LockDir(){ std::error_code ec; std::filesystem::remove(p_, ec); }
 private: std::filesystem::path p_; };

class OsLock { public:
  explicit OsLock(const std::filesystem::path& dir){
    path_ = (dir/".lockf").string();
#ifdef _WIN32
    fh_ = CreateFileA(path_.c_str(), GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fh_==INVALID_HANDLE_VALUE) throw std::runtime_error("cannot open lock file");
    OVERLAPPED ov{};
    if (!LockFileEx(fh_, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &ov)) { CloseHandle(fh_); throw std::runtime_error("lock busy"); }
#else
    fd_ = ::open(path_.c_str(), O_CREAT|O_RDWR, 0600);
    if (fd_<0) throw std::runtime_error("cannot open lock file");
    struct flock fl{}; fl.l_type=F_WRLCK; fl.l_whence=SEEK_SET; fl.l_start=0; fl.l_len=0;
    if (fcntl(fd_, F_SETLK, &fl) < 0) { ::close(fd_); throw std::runtime_error("lock busy"); }
#endif
  }
  ~OsLock(){
#ifdef _WIN32
    if (fh_!=INVALID_HANDLE_VALUE){ OVERLAPPED ov{}; UnlockFileEx(fh_, 0, MAXDWORD, MAXDWORD, &ov); CloseHandle(fh_); }
#else
    if (fd_>=0){ struct flock fl{}; fl.l_type=F_UNLCK; fl.l_whence=SEEK_SET; fl.l_start=0; fl.l_len=0; fcntl(fd_, F_SETLK, &fl); ::close(fd_); }
#endif
  }
 private:
  std::string path_;
#ifdef _WIN32
  HANDLE fh_{INVALID_HANDLE_VALUE};
#else
  int fd_{-1};
#endif
}; } // anon

  explicit LockDir(const std::filesystem::path& dir) : p_(dir/".lock") {
    std::error_code ec; std::filesystem::create_directory(p_, ec);
    if (ec) throw std::runtime_error("lock busy: " + p_.string()); }
  ~LockDir(){ std::error_code ec; std::filesystem::remove(p_, ec); }
 private: std::filesystem::path p_; }; } // anon


namespace {
// Escape per RFC4180-ish: quote if contains comma, quote, CR, or LF; double quotes inside
static std::string csv_escape(const std::string& s) {
  bool need = s.find(',') != std::string::npos || s.find('"') != std::string::npos
           || s.find('\n') != std::string::npos || s.find('\r') != std::string::npos;
  if (!need) return s;
  std::string out;
  out.reserve(s.size() + 2);
  out.push_back('"');
  for (char c : s) {
    if (c == '"') out.push_back('"');
    out.push_back(c);
  }
  out.push_back('"');
  return out;
}

// Basic CSV parse for a single line; returns vector of fields.
static std::vector<std::string> csv_parse_line(const std::string& line) {
  std::vector<std::string> fields;
  std::string cur;
  bool in_quotes = false;
  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];
    if (in_quotes) {
      if (c == '"') {
        if (i + 1 < line.size() && line[i+1] == '"') { // escaped quote
          cur.push_back('"'); ++i;
        } else {
          in_quotes = false;
        }
      } else {
        cur.push_back(c);
      }
    } else {
      if (c == ',') {
        fields.push_back(cur); cur.clear();
      } else if (c == '"') {
        in_quotes = true;
      } else {
        cur.push_back(c);
      }
    }
  }
  fields.push_back(cur);
  return fields;
}

// Atomically write text to file: write tmp then rename
static void write_atomic(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  auto tmp = path; tmp += ".tmp";
  {
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    ofs << text;
    ofs.flush();
    if (!ofs) throw std::runtime_error("failed to write: " + tmp.string());
  }
  fs::rename(tmp, path);
}

} // namespace

void CsvStorage::save_products(const Inventory& inv, const std::string& dir) {
  if (chaos_inject()){ std::cerr<<"chaos_injected products\n"; return; }
  auto PL = load_policy_limits(dir);
  if (!quota_allow_write(dir)) { std::cerr << "quota_exceeded: refusing to write products.csv\n"; return; }
  const char* dry=getenv("NEONSTORE_DRYRUN"); if(dry && (*dry=='1'||*dry=='y'||*dry=='Y'||*dry=='t'||*dry=='T')){ std::cerr<<"[DRYRUN] skip save_products\n"; return; }
  const char* dry=getenv("NEONSTORE_DRYRUN"); if(dry && (*dry=='1'||*dry=='y'||*dry=='Y'||*dry=='t'||*dry=='T')){ std::cerr<<"[DRYRUN] skip save_products\n"; return; }
  run_hook_prepost(dir, "pre-save", "products");
  auto _lk = (g_lockmode==LockMode::OS)? nullptr : nullptr; if (g_lockmode==LockMode::OS) { OsLock ol(dir); } else { LockDir ld(dir); }
  fs::create_directories(dir);
  std::ostringstream oss;
  oss << "id,name,price,stock\n";
  for (const auto& p : inv.list_products()) {
    oss << csv_escape(p.id) << ','
        << csv_escape(p.name) << ','
        << p.price << ','
        << p.stock << '\n';
  }
  write_atomic(fs::path(dir) / "products.csv", oss.str());
}

void CsvStorage::load_products(Inventory& inv, const std::string& dir) {
  std::string reason; if (g_algo==IntegrityAlgo::CRC32){ if (!verify_crc_sidecar(dir + "/products.csv", &reason)) { if (g_integrity==Integrity::Strict) throw std::runtime_error("integrity fail: products.csv: " + reason); } } else { if (!verify_sha256_sidecar(dir + "/products.csv", &reason)) { if (g_integrity==Integrity::Strict) throw std::runtime_error("integrity fail: products.csv: " + reason); } }
  LockDir lk(dir);
  fs::path file = fs::path(dir) / "products.csv";
  if (!fs::exists(file)) return;
  std::ifstream ifs(file, std::ios::binary);
  if (!ifs) throw std::runtime_error("cannot open products.csv");
  std::string line;
  bool first = true;
  while (std::getline(ifs, line)) {
    if (first) { first = false; continue; } // skip header
    if (line.empty()) continue;
    auto fields = csv_parse_line(line);
    if (fields.size() != 4) continue;
    Product p;
    p.id = fields[0];
    p.name = fields[1];
    try {
      p.price = std::stod(fields[2]);
      p.stock = std::stoi(fields[3]);
    } catch (...) { continue; }
    inv.add_product(p);
  }
}

void CsvStorage::save_orders(const std::vector<Order>& orders, const std::string& dir) {
  if (chaos_inject()){ std::cerr<<"chaos_injected orders\n"; return; }
  auto PL = load_policy_limits(dir);
  if (!quota_allow_write(dir)) { std::cerr << "quota_exceeded: refusing to write orders.csv\n"; return; }
  const char* dry=getenv("NEONSTORE_DRYRUN"); if(dry && (*dry=='1'||*dry=='y'||*dry=='Y'||*dry=='t'||*dry=='T')){ std::cerr<<"[DRYRUN] skip save_orders\n"; return; }
  run_hook_prepost(dir, "pre-save", "orders");
  if (g_lockmode==LockMode::OS) { OsLock ol(dir); } else { LockDir ld(dir); }
  fs::create_directories(dir);
  std::ostringstream oss;
  oss << "id,product_id,quantity,price_each\n";
  for (const auto& o : orders) {
    for (const auto& it : o.items) {
      oss << csv_escape(o.id) << ','
          << csv_escape(it.product_id) << ','
          << it.quantity << ','
          << it.price_each << '\n';
    }
  }
  write_atomic(fs::path(dir) / "orders.csv", oss.str());
}

void CsvStorage::load_orders(std::vector<Order>& orders, const std::string& dir) {
  std::string reason; if (g_algo==IntegrityAlgo::CRC32){ if (!verify_crc_sidecar(dir + "/orders.csv", &reason)) { if (g_integrity==Integrity::Strict) throw std::runtime_error("integrity fail: orders.csv: " + reason); } } else { if (!verify_sha256_sidecar(dir + "/orders.csv", &reason)) { if (g_integrity==Integrity::Strict) throw std::runtime_error("integrity fail: orders.csv: " + reason); } }
  LockDir lk(dir);
  fs::path file = fs::path(dir) / "orders.csv";
  if (!fs::exists(file)) return;
  std::ifstream ifs(file, std::ios::binary);
  if (!ifs) throw std::runtime_error("cannot open orders.csv");
  std::unordered_map<std::string, Order> by_id;
  std::string line;
  bool first = true;
  while (std::getline(ifs, line)) {
    if (first) { first = false; continue; }
    if (line.empty()) continue;
    auto fields = csv_parse_line(line);
    if (fields.size() != 4) continue;
    OrderItem item;
    item.product_id = fields[1];
    try {
      item.quantity = std::stoi(fields[2]);
      item.price_each = std::stod(fields[3]);
    } catch (...) { continue; }
    auto& o = by_id[fields[0]];
    o.id = fields[0];
    o.items.push_back(item);
  }
  orders.clear();
  orders.reserve(by_id.size());
  for (auto& kv : by_id) orders.push_back(std::move(kv.second));
}

static void write_crc_sidecar(const std::string& path){
  std::ifstream f(path, std::ios::binary);
  std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  uint32_t c = crc32_of_string(content);
  char buf[16]; std::snprintf(buf, sizeof(buf), "%08X", c);
  std::ofstream(path + ".crc32", std::ios::binary) << buf << "\n";
}
static bool verify_crc_sidecar(const std::string& path, std::string* reason){
  std::ifstream s(path + ".crc32", std::ios::binary);
  if (!s) cleanup();

  // Append tamper-evident ledger entry if enabled
  try{
    std::filesystem::path neond = std::filesystem::path(dir)/".neonstore";
    std::error_code ec;
    if (std::filesystem::exists(neond/"ledger.on", ec)){
      auto rd=[&](const std::string& f)->std::string{
        std::ifstream in((std::filesystem::path(dir)/f).string(), std::ios::binary); std::ostringstream ss; ss<<in.rdbuf();
        std::string s=ss.str(); std::string out; out.reserve(s.size());
        for(size_t i=0;i<s.size();++i){ char c=s[i]; if(c=='\r'){ if(i+1<s.size()&&s[i+1]=='\n'){ continue; } else continue; } else out.push_back(c); } return out;
      };
      auto prod = rd("products.csv"); auto ord = rd("orders.csv");
      uint64_t h=0; h=fnv1a64_csv(prod.data(), prod.size()); h = fnv1a64_csv(&h, sizeof(h)) ^ fnv1a64_csv(ord.data(), ord.size());
      std::ostringstream ds; ds<<std::hex<<std::nouppercase<<h;
      std::filesystem::create_directories(neond, ec);
      std::filesystem::path logp = neond/"ledger.log";
      std::string prev="";
      { std::ifstream in(logp.string()); std::string line, last; while(std::getline(in,line)) if(!line.empty()) last=line;
        if(!last.empty()){ auto p=last.find("\"entry\":\""); if(p!=std::string::npos){ size_t q=last.find("\"", p+9); if(q!=std::string::npos) prev = last.substr(p+9, q-(p+9)); } } }
      if (prev.empty()) prev = std::string(64,'0');
      std::string ts = now_iso_csv();
      std::string entry = sha256_hex_csv(prev + ds.str() + ts);
      std::ofstream out((logp).string(), std::ios::app);
      out<<"{\"ts\":\"" << ts << "\",\"dataset\":\"" << ds.str() << "\",\"prev\":\"" << prev << "\",\"entry\":\"" << entry << "\"}\n";
    }
  } catch(...) { /* ignore ledger errors */ }

  return true; // sidecar not present → skip
  std::string ref; std::getline(s, ref);
  std::ifstream f(path, std::ios::binary);
  if (!f) { if(reason) *reason="missing data file"; cleanup();
  return false; }
  std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  uint32_t c = crc32_of_string(content);
  char buf[16]; std::snprintf(buf, sizeof(buf), "%08X", c);
  if (ref != buf) { if(reason) *reason="crc32 mismatch"; cleanup();
  return false; }
  cleanup();

  // Append tamper-evident ledger entry if enabled
  try{
    std::filesystem::path neond = std::filesystem::path(dir)/".neonstore";
    std::error_code ec;
    if (std::filesystem::exists(neond/"ledger.on", ec)){
      auto rd=[&](const std::string& f)->std::string{
        std::ifstream in((std::filesystem::path(dir)/f).string(), std::ios::binary); std::ostringstream ss; ss<<in.rdbuf();
        std::string s=ss.str(); std::string out; out.reserve(s.size());
        for(size_t i=0;i<s.size();++i){ char c=s[i]; if(c=='\r'){ if(i+1<s.size()&&s[i+1]=='\n'){ continue; } else continue; } else out.push_back(c); } return out;
      };
      auto prod = rd("products.csv"); auto ord = rd("orders.csv");
      uint64_t h=0; h=fnv1a64_csv(prod.data(), prod.size()); h = fnv1a64_csv(&h, sizeof(h)) ^ fnv1a64_csv(ord.data(), ord.size());
      std::ostringstream ds; ds<<std::hex<<std::nouppercase<<h;
      std::filesystem::create_directories(neond, ec);
      std::filesystem::path logp = neond/"ledger.log";
      std::string prev="";
      { std::ifstream in(logp.string()); std::string line, last; while(std::getline(in,line)) if(!line.empty()) last=line;
        if(!last.empty()){ auto p=last.find("\"entry\":\""); if(p!=std::string::npos){ size_t q=last.find("\"", p+9); if(q!=std::string::npos) prev = last.substr(p+9, q-(p+9)); } } }
      if (prev.empty()) prev = std::string(64,'0');
      std::string ts = now_iso_csv();
      std::string entry = sha256_hex_csv(prev + ds.str() + ts);
      std::ofstream out((logp).string(), std::ios::app);
      out<<"{\"ts\":\"" << ts << "\",\"dataset\":\"" << ds.str() << "\",\"prev\":\"" << prev << "\",\"entry\":\"" << entry << "\"}\n";
    }
  } catch(...) { /* ignore ledger errors */ }

  return true;
}

static void write_sha256_sidecar(const std::string& path){
  std::ifstream f(path, std::ios::binary);
  std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  std::string h = Sha256::of_string(content);
  std::ofstream(path + ".sha256", std::ios::binary) << h << "\n";
}
static bool verify_sha256_sidecar(const std::string& path, std::string* reason){
  std::ifstream s(path + ".sha256", std::ios::binary);
  if (!s) cleanup();

  // Append tamper-evident ledger entry if enabled
  try{
    std::filesystem::path neond = std::filesystem::path(dir)/".neonstore";
    std::error_code ec;
    if (std::filesystem::exists(neond/"ledger.on", ec)){
      auto rd=[&](const std::string& f)->std::string{
        std::ifstream in((std::filesystem::path(dir)/f).string(), std::ios::binary); std::ostringstream ss; ss<<in.rdbuf();
        std::string s=ss.str(); std::string out; out.reserve(s.size());
        for(size_t i=0;i<s.size();++i){ char c=s[i]; if(c=='\r'){ if(i+1<s.size()&&s[i+1]=='\n'){ continue; } else continue; } else out.push_back(c); } return out;
      };
      auto prod = rd("products.csv"); auto ord = rd("orders.csv");
      uint64_t h=0; h=fnv1a64_csv(prod.data(), prod.size()); h = fnv1a64_csv(&h, sizeof(h)) ^ fnv1a64_csv(ord.data(), ord.size());
      std::ostringstream ds; ds<<std::hex<<std::nouppercase<<h;
      std::filesystem::create_directories(neond, ec);
      std::filesystem::path logp = neond/"ledger.log";
      std::string prev="";
      { std::ifstream in(logp.string()); std::string line, last; while(std::getline(in,line)) if(!line.empty()) last=line;
        if(!last.empty()){ auto p=last.find("\"entry\":\""); if(p!=std::string::npos){ size_t q=last.find("\"", p+9); if(q!=std::string::npos) prev = last.substr(p+9, q-(p+9)); } } }
      if (prev.empty()) prev = std::string(64,'0');
      std::string ts = now_iso_csv();
      std::string entry = sha256_hex_csv(prev + ds.str() + ts);
      std::ofstream out((logp).string(), std::ios::app);
      out<<"{\"ts\":\"" << ts << "\",\"dataset\":\"" << ds.str() << "\",\"prev\":\"" << prev << "\",\"entry\":\"" << entry << "\"}\n";
    }
  } catch(...) { /* ignore ledger errors */ }

  return true; // sidecar not present → skip
  std::string ref; std::getline(s, ref);
  std::ifstream f(path, std::ios::binary);
  if (!f) { if(reason) *reason="missing data file"; cleanup();
  return false; }
  std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  std::string h = Sha256::of_string(content);
  if (ref != h) { if(reason) *reason="sha256 mismatch"; cleanup();
  return false; }
  cleanup();

  // Append tamper-evident ledger entry if enabled
  try{
    std::filesystem::path neond = std::filesystem::path(dir)/".neonstore";
    std::error_code ec;
    if (std::filesystem::exists(neond/"ledger.on", ec)){
      auto rd=[&](const std::string& f)->std::string{
        std::ifstream in((std::filesystem::path(dir)/f).string(), std::ios::binary); std::ostringstream ss; ss<<in.rdbuf();
        std::string s=ss.str(); std::string out; out.reserve(s.size());
        for(size_t i=0;i<s.size();++i){ char c=s[i]; if(c=='\r'){ if(i+1<s.size()&&s[i+1]=='\n'){ continue; } else continue; } else out.push_back(c); } return out;
      };
      auto prod = rd("products.csv"); auto ord = rd("orders.csv");
      uint64_t h=0; h=fnv1a64_csv(prod.data(), prod.size()); h = fnv1a64_csv(&h, sizeof(h)) ^ fnv1a64_csv(ord.data(), ord.size());
      std::ostringstream ds; ds<<std::hex<<std::nouppercase<<h;
      std::filesystem::create_directories(neond, ec);
      std::filesystem::path logp = neond/"ledger.log";
      std::string prev="";
      { std::ifstream in(logp.string()); std::string line, last; while(std::getline(in,line)) if(!line.empty()) last=line;
        if(!last.empty()){ auto p=last.find("\"entry\":\""); if(p!=std::string::npos){ size_t q=last.find("\"", p+9); if(q!=std::string::npos) prev = last.substr(p+9, q-(p+9)); } } }
      if (prev.empty()) prev = std::string(64,'0');
      std::string ts = now_iso_csv();
      std::string entry = sha256_hex_csv(prev + ds.str() + ts);
      std::ofstream out((logp).string(), std::ios::app);
      out<<"{\"ts\":\"" << ts << "\",\"dataset\":\"" << ds.str() << "\",\"prev\":\"" << prev << "\",\"entry\":\"" << entry << "\"}\n";
    }
  } catch(...) { /* ignore ledger errors */ }

  return true;
}


bool atomic_write_dataset(const Inventory& inv, const std::vector<Order>& orders, const std::string& dir){

    try { std::error_code ec; auto ro = std::filesystem::path(dir)/".neonstore"/"readonly.on"; if (std::filesystem::exists(ro, ec)) { std::cerr<<"read_only
"; return false; } } catch(...) {}// Write lock directory to prevent concurrent writers
  std::filesystem::path lockdir = std::filesystem::path(dir) / ".neonstore" / "writelock";
  std::error_code lec;
  if (!std::filesystem::create_directories(lockdir, lec)) {
    // already exists -> refuse
    cleanup();
  return false;
  }
  auto cleanup = [&](){ std::error_code ec; std::filesystem::remove_all(lockdir, ec); };

  // Write products.csv.new and orders.csv.new, then rename with a small journal for recovery.
  std::error_code ec;
  auto d = std::filesystem::path(dir);
  auto pnew = (d/"products.csv.new").string();
  auto onew = (d/"orders.csv.new").string();
  // Emit products to .new
  {
    std::ofstream f(pnew, std::ios::binary|std::ios::trunc);
    f << "id,name,price,stock\n";
    for (auto &p : inv.list_products()){
      f << escape_csv(p.id) << "," << escape_csv(p.name) << "," << p.price << "," << p.stock << "\n";
    }
    f.flush(); f.close();
  }
  // Emit orders to .new
  {
    std::ofstream f(onew, std::ios::binary|std::ios::trunc);
    f << "order_id,product_id,quantity,unit_price\n";
    for (auto &o : orders){
      for (auto &it : o.items){
        f << escape_csv(o.id) << "," << escape_csv(it.product_id) << "," << it.quantity << "," << it.unit_price << "\n";
      }
    }
    f.flush(); f.close();
  }
  // Journal
  auto j = (d/"txn_journal.json");
  { std::ofstream jf(j.string(), std::ios::binary|std::ios::trunc);
    jf << "{\"state\":\"prepared\",\"products\":\"products.csv.new\",\"orders\":\"orders.csv.new\"}\n";
  }
  // Rename sequence
  std::filesystem::rename(d/"products.csv.new", d/"products.csv", ec); if (ec) cleanup();
  return false;
  std::filesystem::rename(d/"orders.csv.new", d/"orders.csv", ec); if (ec) cleanup();
  return false;
  std::filesystem::remove(j, ec);
  fsync_dir(dir);
  cleanup();

  // Append tamper-evident ledger entry if enabled
  try{
    std::filesystem::path neond = std::filesystem::path(dir)/".neonstore";
    std::error_code ec;
    if (std::filesystem::exists(neond/"ledger.on", ec)){
      auto rd=[&](const std::string& f)->std::string{
        std::ifstream in((std::filesystem::path(dir)/f).string(), std::ios::binary); std::ostringstream ss; ss<<in.rdbuf();
        std::string s=ss.str(); std::string out; out.reserve(s.size());
        for(size_t i=0;i<s.size();++i){ char c=s[i]; if(c=='\r'){ if(i+1<s.size()&&s[i+1]=='\n'){ continue; } else continue; } else out.push_back(c); } return out;
      };
      auto prod = rd("products.csv"); auto ord = rd("orders.csv");
      uint64_t h=0; h=fnv1a64_csv(prod.data(), prod.size()); h = fnv1a64_csv(&h, sizeof(h)) ^ fnv1a64_csv(ord.data(), ord.size());
      std::ostringstream ds; ds<<std::hex<<std::nouppercase<<h;
      std::filesystem::create_directories(neond, ec);
      std::filesystem::path logp = neond/"ledger.log";
      std::string prev="";
      { std::ifstream in(logp.string()); std::string line, last; while(std::getline(in,line)) if(!line.empty()) last=line;
        if(!last.empty()){ auto p=last.find("\"entry\":\""); if(p!=std::string::npos){ size_t q=last.find("\"", p+9); if(q!=std::string::npos) prev = last.substr(p+9, q-(p+9)); } } }
      if (prev.empty()) prev = std::string(64,'0');
      std::string ts = now_iso_csv();
      std::string entry = sha256_hex_csv(prev + ds.str() + ts);
      std::ofstream out((logp).string(), std::ios::app);
      out<<"{\"ts\":\"" << ts << "\",\"dataset\":\"" << ds.str() << "\",\"prev\":\"" << prev << "\",\"entry\":\"" << entry << "\"}\n";
    }
  } catch(...) { /* ignore ledger errors */ }

  return true;
}

} // namespace neonstore


