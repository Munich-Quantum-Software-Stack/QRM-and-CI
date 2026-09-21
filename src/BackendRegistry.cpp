/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "qrmci/BackendRegistry.h"

#include "qrmci/BackendWrapper.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace mqss::qrmci {

BackendRegistry::BackendRegistry(Clock::duration timeToLive)
    : entryTimeToLive(timeToLive) {}

bool BackendRegistry::insertOrRefresh(BackendWrapper backend,
                                      Clock::time_point now) {
  // Read before moving out of `backend`; getName()/getQueueName() read
  // members the move would leave empty.
  const std::string name = backend.getName();
  if (const auto it = entries.find(name);
      it != entries.end() && !it->second.backend.getQueueName().empty() &&
      !backend.getQueueName().empty() &&
      it->second.backend.getQueueName() != backend.getQueueName()) {
    return false;
  }

  entries.insert_or_assign(
      name, Entry{.backend = std::move(backend), .lastRefreshed = now});
  return true;
}

std::size_t BackendRegistry::expire(Clock::time_point now) {
  return std::erase_if(entries, [this, now](const auto &entry) {
    return now - entry.second.lastRefreshed > entryTimeToLive;
  });
}

const BackendWrapper *
BackendRegistry::find(std::string_view backendName) const {
  auto it = entries.find(backendName);
  return it == entries.end() ? nullptr : &it->second.backend;
}

bool BackendRegistry::contains(std::string_view backendName) const {
  return entries.contains(backendName);
}

} // namespace mqss::qrmci
