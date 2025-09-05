
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace neonstore {
inline uint32_t crc32_update(uint32_t crc, const unsigned char* data, size_t len){
  static uint32_t table[256]; static bool init=false;
  if(!init){
    for(uint32_t i=0;i<256;++i){
      uint32_t c=i;
      for(int j=0;j<8;++j) c = (c&1)? (0xEDB88320u ^ (c>>1)) : (c>>1);
      table[i]=c;
    }
    init=true;
  }
  crc = ~crc;
  for(size_t i=0;i<len;++i) crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
  return ~crc;
}
inline uint32_t crc32_of_string(const std::string& s){
  return crc32_update(0u, reinterpret_cast<const unsigned char*>(s.data()), s.size());
}
} // namespace neonstore
