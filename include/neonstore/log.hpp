// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdio>
#include <string>

#ifndef NEONSTORE_LOG_LEVEL
#define NEONSTORE_LOG_LEVEL 1
#endif

namespace neonstore {
inline bool ns_verbose=false;

inline void log_impl(int lvl, const char* tag, const std::string& msg){
  if (lvl > NEONSTORE_LOG_LEVEL) return;
  std::fputs(tag, stderr);
  std::fputs(": ", stderr);
  std::fputs(msg.c_str(), stderr);
  std::fputc('\n', stderr);
}

} // namespace neonstore

#define NS_LOG_INFO(msg)  do { if (::neonstore::ns_verbose) ::neonstore::log_impl(1, "[INFO]", (msg)); } while(0)
#define NS_LOG_WARN(msg)  do { ::neonstore::log_impl(1, "[WARN]", (msg)); } while(0)
#define NS_LOG_ERROR(msg) do { ::neonstore::log_impl(1, "[ERROR]", (msg)); } while(0)
