// SPDX-License-Identifier: Apache-2.0
#include "neonstore/config.hpp"
#include <fstream>
#include <sstream>

namespace neonstore {

Config load_config(const std::string& path){
  Config c;
  std::ifstream ifs(path);
  if (!ifs) return c;
  std::string line;
  while (std::getline(ifs, line)) {
    if (line.empty() || line[0]=='#') continue;
    auto pos = line.find('=');
    if (pos==std::string::npos) continue;
    std::string k = line.substr(0,pos);
    std::string v = line.substr(pos+1);
    auto trim=[](std::string& s){
      auto issp=[](unsigned char ch){ return ch==' '||ch=='\t'||ch=='\r'||ch=='\n'; };
      while(!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
      while(!s.empty() && issp((unsigned char)s.back())) s.pop_back();
    };
    trim(k); trim(v);
    if (k=="BACKEND") c.backend=v;
    else if (k=="DATA_DIR") c.data_dir=v;
    else if (k=="DSN") c.dsn=v;
  }
  return c;
}

} // namespace neonstore
