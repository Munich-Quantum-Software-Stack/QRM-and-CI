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

BackendRegistry::BackendRegistry(Clock::duration timeToLive,
                                 Clock::duration publishIntervalArg)
    : entryTimeToLive(timeToLive), publishInterval(publishIntervalArg) {}

void BackendRegistry::insertOrRefresh(BackendWrapper backend,
                                      Clock::time_point now) {
  // Key the entry before moving out of `backend`; getName() reads a member
  // the move would leave empty.
  Entry &entry = entries[backend.getName()];
  entry.backend = std::move(backend);
  entry.lastRefreshed = now;
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

bool BackendRegistry::claimPublishSlot(Clock::time_point now) {
  if (lastPublished.has_value() && now - *lastPublished < publishInterval) {
    return false;
  }
  lastPublished = now;
  return true;
}

} // namespace mqss::qrmci
