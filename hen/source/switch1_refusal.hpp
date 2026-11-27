// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// When an armed Switch 1 launch that Loader refused ends (game_arm.cpp
// pollSwitch1Refusal). Only the libnx types, so that host tests build it.
extern "C" {
#include <switch/types.h>
}

namespace ounce::game {

/// What one walk of Loader's FS domain showed of the launch's code mount.
enum class MountSeen {
    /// The walk failed: Loader changed its domain under it. Proves nothing.
    Unknown,
    Open,
    Closed,
};

/// Decides, walk by walk, when a launch with no process ends:
/// - once successful walks have seen the code mount closed for `timeout`;
///   a walk that sees it open restarts this wait, a failed walk does not;
/// - or once `deadline` has passed since the first walk, unless the last
///   walk that succeeded saw the mount open. Walks that keep failing, or
///   that fail between closed ones, therefore end the launch too, but only
///   after the deadline, and a mount Loader is seen to hold is never
///   dropped.
class Switch1RefusalTimer {
  public:
    /// Returns whether the launch is to end now. `now`, `timeout` and
    /// `deadline` are in the same unit (ticks).
    constexpr bool observe(MountSeen seen, u64 now, u64 timeout, u64 deadline) {
        if (!started_) {
            started_ = true;
            first_ = now;
        }
        if (seen == MountSeen::Open) {
            closed_ = false;
            last_open_ = true;
            return false;
        }
        if (seen == MountSeen::Closed) {
            last_open_ = false;
            if (!closed_) {
                closed_ = true;
                closed_since_ = now;
            } else if (now - closed_since_ >= timeout) {
                return true;
            }
        }
        return !last_open_ && now - first_ >= deadline;
    }

    constexpr void reset() { *this = {}; }

  private:
    bool started_ = false;
    bool closed_ = false;
    bool last_open_ = false;
    u64 first_ = 0;
    u64 closed_since_ = 0;
};

} // namespace ounce::game
