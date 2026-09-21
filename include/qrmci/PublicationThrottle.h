/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file PublicationThrottle.h
/// @brief Paces how often a device-owning process re-reads and publishes its
///        own status, independently of how fast its work loop spins.

#pragma once

#include <chrono>
#include <optional>

namespace mqss::qrmci {

/// @brief Claims at most one status-publication slot per configured
///        interval.
///
/// Carries no relationship to backend liveness: a process that never
/// publishes its own status, such as the distributed selector, has no use
/// for one and constructs none. A device-owning process constructs one
/// alongside its BackendRegistry and consults it before re-reading and
/// publishing its device's status.
class PublicationThrottle {
public:
  /// @brief Monotonic clock used for slot timing.
  using Clock = std::chrono::steady_clock;

  /// @brief Minimum spacing between two publish slots, absent an explicit
  ///        interval.
  static constexpr Clock::duration DefaultInterval = std::chrono::seconds(5);

  /// @brief Construct a throttle with the given interval.
  /// @param interval Minimum spacing between two publish slots.
  explicit PublicationThrottle(Clock::duration interval = DefaultInterval)
      : publishInterval(interval) {}

  /// @brief Claim the next publication slot, if one is due.
  ///
  /// Returns true at most once per publish interval and stamps the slot as
  /// taken, so a caller can guard both its device interrogation and its
  /// status publication with it and stop doing either once per turn of a
  /// work loop that spins far faster than the status can meaningfully
  /// change. The first call always succeeds.
  /// @param now The current time.
  /// @return True if the caller should refresh and publish its status now.
  [[nodiscard]] bool claim(Clock::time_point now = Clock::now()) {
    if (lastClaimed.has_value() && now - *lastClaimed < publishInterval) {
      return false;
    }
    lastClaimed = now;
    return true;
  }

  /// @brief Get the throttle's configured interval.
  /// @return The configured publish interval.
  [[nodiscard]] Clock::duration getInterval() const noexcept {
    return publishInterval;
  }

private:
  Clock::duration publishInterval;
  std::optional<Clock::time_point> lastClaimed;
};

} // namespace mqss::qrmci
