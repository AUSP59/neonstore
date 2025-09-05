
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <string>
#include <fstream>
#include <cstdio>
#ifdef _WIN32
 #include <io.h>
 #define fsync _commit
#else
 #include <unistd.h>
#endif
#include <ctime>
#include <filesystem>
#include "neonstore/crc32.hpp"
#include "neonstore/json.hpp"
#include "neonstore/sha256.hpp"

namespace neonstore {

inline std::string iso8601_utc() {
  std::time_t t = std::time(nullptr);
  char buf[32];
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return std::string(buf);
}

inline std::string audit_log_path(const std::string& dir){
  return (std::filesystem::path(dir) / "audit.log").string();
}

inline std::string read_last_hash(const std::string& path){
  std::ifstream f(path, std::ios::binary);
  if (!f) return "00000000";
  std::string line, last;
  bool need_mac = (std::getenv("NEONSTORE_AUDIT_HMAC_KEY")!=nullptr);
  while (std::getline(f, line)) { if(!line.empty()) last = line; }
  if (last.empty()) return "00000000";
  auto pos = last.rfind("\"hash\":\"");
  if (pos == std::string::npos) return "00000000";
  pos += 8; // after "hash":"
  auto end = last.find('"', pos);
  if (end == std::string::npos) return "00000000";
  return last.substr(pos, end - pos);
}

inline std::string crc32_hex_of(const std::string& s){
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%08X", crc32_of_string(s));
  return std::string(buf);
}

inline std::string hmac_sha256_hex(const std::string& key, const std::string& data){
  const size_t B=64; std::string k=key; if(k.size()>B){ k = Sha256::of_string(k); }
  std::string ik(B,'\x36'), ok(B,'\x5c'); for(size_t i=0;i<k.size();++i){ ik[i]^=k[i]; ok[i]^=k[i]; }
  // simple two-step HMAC using Sha256
  Sha256 hi; hi.update(ik.data(), ik.size()); hi.update(data.data(), data.size()); auto ires = hi.finish();
  std::string ihex = Sha256::hex(ires);
  Sha256 ho; ho.update(ok.data(), ok.size()); ho.update(ihex.data(), ihex.size()); return Sha256::hex(ho.finish());
}
inline void audit_append(const std::string& dir, const std::string& op, const std::string& payload_json){
  std::error_code ec; std::filesystem::create_directories(dir, ec);
  const std::string path = audit_log_path(dir);
  const std::string prev = read_last_hash(path);
  const std::string entry_wo_hash = std::string("{\"ts\":\"") + iso8601_utc() + "\",\"op\":\"" + op + "\",\"prev\":\"" + prev + "\",\"data\":" + payload_json + "}";
  const char* kh = std::getenv("NEONSTORE_AUDIT_HMAC_KEY");
  const std::string mac = kh? hmac_sha256_hex(kh, prev + entry_wo_hash) : std::string();
  const std::string h = crc32_hex_of(prev + entry_wo_hash);
  std::ofstream out(path, std::ios::app | std::ios::binary);
  out << entry_wo_hash.substr(0, entry_wo_hash.size()-1) << ",\"hash\":\"" << h << "\""; if(!mac.empty()) out << ",\"hmac\":\"" << mac << "\""; out << "}\n";
  out.flush(); { FILE* fp = std::fopen(path.c_str(), "rb"); if(fp){ int fd = fileno(fp); if(fd>=0) fsync(fd); std::fclose(fp);} }
}

inline bool audit_verify(const std::string& dir, std::string* reason=nullptr){
  const std::string path = audit_log_path(dir);
  std::ifstream f(path, std::ios::binary);
  if (!f) return true; // no log => nothing to verify
  std::string prev = "00000000";
  std::string line;
  size_t ln = 0;
  bool need_mac = (std::getenv("NEONSTORE_AUDIT_HMAC_KEY")!=nullptr);
  while (std::getline(f, line)) {
    ++ln;
    if (line.empty()) continue;
    auto hpos = line.rfind("\"hash\":\"");
    if (hpos == std::string::npos) { if(reason) *reason = "missing hash at line " + std::to_string(ln); return false; }
    hpos += 8;
    auto hend = line.find('"', hpos);
    if (hend == std::string::npos) { if(reason) *reason = "malformed hash at line " + std::to_string(ln); return false; }
    std::string hash = line.substr(hpos, hend - hpos);
    std::string without_hash = line;
    // remove ,"hash":"...."} at end
    auto comma = without_hash.rfind(",\"hash\":\"");
    if (comma != std::string::npos) { without_hash = without_hash.substr(0, comma) + "}"; }
    std::string expect = crc32_hex_of(prev + without_hash);
    if (expect != hash) { if(reason) *reason = "hash mismatch at line " + std::to_string(ln); return false; }
    if (need_mac){ auto mpos=line.rfind("\"hmac\":\""); if (mpos==std::string::npos){ if(reason) *reason="missing hmac at line " + std::to_string(ln); return false;} }
    prev = hash;
  }
  return true;
}

} // namespace neonstore
