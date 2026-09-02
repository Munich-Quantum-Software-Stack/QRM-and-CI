/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file BackendRegistry.h
/// @brief The set of backends a QRM&CI process may schedule onto, and the
///        two clocks that keep it honest: a time-to-live after which an
///        entry nobody refreshed is dropped, and a publish interval that
///        paces status refresh/publication independently of how fast the
///        work loop spins.

#pragma once

#include "qrmci/BackendWrapper.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace mqss::qrmci {

/// @brief Named rule deciding which backend wins when several of them could
///        run the same task. Every policy is a total order over backend
///        names, so the same task and the same registry always yield the
///        same choice -- across runs, hosts and standard-library versions.
enum class BackendSelectionPolicy : std::uint8_t {
  /// @brief The compatible backend whose name sorts first.
  LowestName,
  /// @brief The compatible backend with the fewest qubits (best fit, leaving
  ///        the larger devices free for larger circuits), ties broken by
  ///        LowestName.
  SmallestSufficient,
};

/// @brief Owns the backends a process knows about, keyed by backend name.
///
/// Entries are inserted or refreshed with insertOrRefresh(), each stamped
/// with the time it was last heard from, and dropped by expire() once they
/// are older than the registry's time-to-live -- so a worker that dies stops
/// receiving task assignments instead of lingering forever. Lookup goes
/// through find(), which returns nullptr for an unknown name rather than
/// default-constructing an entry the way a raw map's operator[] does.
///
/// Iteration order is by ascending backend name, which is what makes
/// chooseBackend() reproducible.
class BackendRegistry {
public:
  /// @brief Monotonic clock used for entry freshness and publish-slot timing.
  using Clock = std::chrono::steady_clock;

  /// @brief How long an entry stays usable after it was last refreshed.
  static constexpr Clock::duration DefaultTimeToLive = std::chrono::seconds(30);
  /// @brief Minimum spacing between two publish slots.
  static constexpr Clock::duration DefaultPublishInterval =
      std::chrono::seconds(5);

  /// @brief Construct a registry with the default time-to-live and publish
  ///        interval.
  BackendRegistry() = default;

  /// @brief Construct a registry with explicit timings.
  /// @param timeToLive How long an entry stays usable after its last
  ///        refresh. Must be longer than @p publishInterval for a
  ///        self-refreshing process to keep its own entry alive.
  /// @param publishInterval Minimum spacing between two publish slots.
  BackendRegistry(Clock::duration timeToLive, Clock::duration publishInterval);

  /// @brief Insert a backend, or refresh the entry already registered under
  ///        the same name, and stamp it as last heard from at @p now.
  /// @param backend The backend to register.
  /// @param now The time to stamp the entry with.
  void insertOrRefresh(BackendWrapper backend,
                       Clock::time_point now = Clock::now());

  /// @brief Drop every entry last refreshed longer ago than the registry's
  ///        time-to-live.
  /// @param now The current time.
  /// @return The number of entries dropped.
  std::size_t expire(Clock::time_point now = Clock::now());

  /// @brief Look up a backend by name. This is the one lookup rule every
  ///        caller shares: a miss is a null return, never a silently
  ///        default-constructed entry.
  /// @param backendName The backend name to look up.
  /// @return A pointer to the registered backend, or nullptr if no backend
  ///         is registered under that name. The pointer is invalidated by
  ///         the next insertOrRefresh()/expire() touching that entry.
  [[nodiscard]] const BackendWrapper *find(std::string_view backendName) const;

  /// @brief Check whether a backend is registered under a given name.
  /// @param backendName The backend name to look up.
  /// @return True if the name is registered.
  [[nodiscard]] bool contains(std::string_view backendName) const;

  /// @brief Get the number of registered backends.
  /// @return The number of entries.
  [[nodiscard]] std::size_t size() const noexcept { return entries.size(); }

  /// @brief Check whether the registry holds no backends.
  /// @return True if there are no entries.
  [[nodiscard]] bool empty() const noexcept { return entries.empty(); }

  /// @brief Claim the next status-publication slot, if one is due.
  ///
  /// Returns true at most once per publish interval and stamps the slot as
  /// taken, so a caller can guard both its device interrogation and its
  /// status publication with it and stop doing either once per turn of a
  /// work loop that spins far faster than the status can meaningfully
  /// change. The first call always succeeds.
  /// @param now The current time.
  /// @return True if the caller should refresh and publish its status now.
  [[nodiscard]] bool claimPublishSlot(Clock::time_point now = Clock::now());

  /// @brief Get the registry's entry time-to-live.
  /// @return The configured time-to-live.
  [[nodiscard]] Clock::duration getTimeToLive() const noexcept {
    return entryTimeToLive;
  }

  /// @brief Get the registry's publish interval.
  /// @return The configured publish interval.
  [[nodiscard]] Clock::duration getPublishInterval() const noexcept {
    return publishInterval;
  }

  /// @brief Apply a function to every registered backend, in ascending
  ///        backend-name order.
  /// @tparam Fn Callable as fn(const std::string &name, const BackendWrapper
  ///         &backend).
  /// @param fn The function to apply.
  template <typename Fn> void forEachBackend(Fn &&fn) const {
    for (const auto &[backendName, entry] : entries) {
      std::invoke(fn, backendName, entry.backend);
    }
  }

private:
  struct Entry {
    BackendWrapper backend;
    Clock::time_point lastRefreshed;
  };

  // Ordered, so iteration -- and therefore selection -- is deterministic.
  // std::less<> gives heterogeneous lookup from std::string_view without
  // materialising a std::string per query.
  std::map<std::string, Entry, std::less<>> entries;
  Clock::duration entryTimeToLive{DefaultTimeToLive};
  Clock::duration publishInterval{DefaultPublishInterval};
  std::optional<Clock::time_point> lastPublished;
};

} // namespace mqss::qrmci
