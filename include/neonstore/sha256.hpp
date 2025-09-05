
// SPDX-License-Identifier: CC0-1.0 or Public Domain
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
namespace neonstore {
struct Sha256 {
  std::array<uint32_t,8> s;
  std::vector<uint8_t> buf;
  uint64_t bits=0;
  Sha256(){ reset(); }
  void reset(){ s = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}; buf.clear(); bits=0; }
  static inline uint32_t R(uint32_t x,int n){ return (x>>n)|(x<<(32-n)); }
  static inline uint32_t ch(uint32_t x,uint32_t y,uint32_t z){ return (x & y) ^ (~x & z); }
  static inline uint32_t maj(uint32_t x,uint32_t y,uint32_t z){ return (x & y) ^ (x & z) ^ (y & z); }
  static inline uint32_t bsig0(uint32_t x){ return R(x,2) ^ R(x,13) ^ R(x,22); }
  static inline uint32_t bsig1(uint32_t x){ return R(x,6) ^ R(x,11) ^ R(x,25); }
  static inline uint32_t ssig0(uint32_t x){ return R(x,7) ^ R(x,18) ^ (x>>3); }
  static inline uint32_t ssig1(uint32_t x){ return R(x,17) ^ R(x,19) ^ (x>>10); }
  void process(const uint8_t* p){
    static const uint32_t K[64]={
      0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
      0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
      0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
      0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
      0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
      0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t w[64];
    for(int i=0;i<16;++i){ w[i]= (p[4*i]<<24)|(p[4*i+1]<<16)|(p[4*i+2]<<8)|p[4*i+3]; }
    for(int i=16;i<64;++i) w[i]= ssig1(w[i-2]) + w[i-7] + ssig0(w[i-15]) + w[i-16];
    uint32_t a=s[0],b=s[1],c=s[2],d=s[3],e=s[4],f=s[5],g=s[6],h=s[7];
    for(int i=0;i<64;++i){
      uint32_t t1 = h + bsig1(e) + ch(e,f,g) + K[i] + w[i];
      uint32_t t2 = bsig0(a) + maj(a,b,c);
      h=g; g=f; f=e; e=d + t1; d=c; c=b; b=a; a=t1 + t2;
    }
    s[0]+=a; s[1]+=b; s[2]+=c; s[3]+=d; s[4]+=e; s[5]+=f; s[6]+=g; s[7]+=h;
  }
  void update(const void* data, size_t len){
    const uint8_t* p=(const uint8_t*)data;
    bits += (uint64_t)len*8;
    while(len--){ buf.push_back(*p++); if(buf.size()==64){ process(buf.data()); buf.clear(); } }
  }
  std::array<uint8_t,32> finish(){
    std::vector<uint8_t> tmp = buf;
    tmp.push_back(0x80);
    while((tmp.size()%64)!=56) tmp.push_back(0x00);
    for(int i=7;i>=0;--i) tmp.push_back((uint8_t)((bits>>(8*i))&0xFF));
    for(size_t off=0; off<tmp.size(); off+=64) process(&tmp[off]);
    std::array<uint8_t,32> out{};
    for(int i=0;i<8;++i){ out[4*i+0]=(s[i]>>24)&0xFF; out[4*i+1]=(s[i]>>16)&0xFF; out[4*i+2]=(s[i]>>8)&0xFF; out[4*i+3]=s[i]&0xFF; }
    reset(); return out;
  }
  static std::string hex(const std::array<uint8_t,32>& a){
    static const char* H="0123456789abcdef";
    std::string s; s.reserve(64);
    for(uint8_t b: a){ s.push_back(H[b>>4]); s.push_back(H[b&0xF]); }
    return s;
  }
  static std::string of_string(const std::string& s){ Sha256 h; h.update(s.data(), s.size()); return hex(h.finish()); }
};
} // namespace neonstore
