// SPDX-License-Identifier: GPL-2.0-only
// Host test of the timer that ends an armed Switch 1 launch Loader refused
// (source/switch1_refusal.hpp): the mount seen closed for the timeout ends
// it, a mount seen open restarts that wait, a failed walk neither starts nor
// restarts it, and walks that keep failing end it at the deadline unless the
// last successful walk saw the mount open.

#include <cstdio>
#include "../source/switch1_refusal.hpp"

using ounce::game::MountSeen;
using ounce::game::Switch1RefusalTimer;

namespace {

int g_failures = 0;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::printf("FAIL: %s\n", what);
        g_failures++;
    }
}

constexpr u64 Timeout = 5000;
constexpr u64 Deadline = 30000;

bool observe(Switch1RefusalTimer& timer, MountSeen seen, u64 now) {
    return timer.observe(seen, now, Timeout, Deadline);
}

void closedForTheTimeout() {
    Switch1RefusalTimer timer;
    expect(!observe(timer, MountSeen::Closed, 100), "first closed walk");
    expect(!observe(timer, MountSeen::Closed, 5099), "closed, just short");
    expect(observe(timer, MountSeen::Closed, 5100), "closed for the timeout");
}

void closedAtTickZero() {
    Switch1RefusalTimer timer;
    expect(!observe(timer, MountSeen::Closed, 0), "closed at tick 0");
    expect(observe(timer, MountSeen::Closed, 5000), "tick 0 is a real start");
}

void openRestarts() {
    Switch1RefusalTimer timer;
    observe(timer, MountSeen::Closed, 0);
    expect(!observe(timer, MountSeen::Open, 4000), "open walk");
    expect(!observe(timer, MountSeen::Closed, 6000), "closed again restarts");
    expect(!observe(timer, MountSeen::Closed, 10999), "restarted, short");
    expect(observe(timer, MountSeen::Closed, 11000), "restarted, expired");
}

void failedWalksDoNotRestartIt() {
    Switch1RefusalTimer timer;
    observe(timer, MountSeen::Closed, 0);
    for (u64 now = 100; now < 5000; now += 100) {
        expect(!observe(timer, MountSeen::Unknown, now), "failed walk before the timeout");
    }
    expect(observe(timer, MountSeen::Closed, 5000), "closed walks around failed ones");
}

void failedAfterClosedEndAtTheDeadline() {
    Switch1RefusalTimer timer;
    observe(timer, MountSeen::Closed, 0);
    // A failed walk alone does not end the launch before the deadline...
    for (u64 now = 100; now < Deadline; now += 100) {
        expect(!observe(timer, MountSeen::Unknown, now), "failed walk before the deadline");
    }
    // ...but walks that never succeed again cannot hold it armed past it.
    expect(observe(timer, MountSeen::Unknown, Deadline), "failed walks at the deadline");
}

void onlyFailedWalksEndAtTheDeadline() {
    Switch1RefusalTimer timer;
    for (u64 now = 1000; now < 1000 + Deadline; now += 100) {
        expect(!observe(timer, MountSeen::Unknown, now), "only failed walks, before");
    }
    expect(observe(timer, MountSeen::Unknown, 1000 + Deadline), "only failed walks, deadline");
}

void openMountIsNeverDropped() {
    Switch1RefusalTimer timer;
    observe(timer, MountSeen::Open, 0);
    for (u64 now = 100; now <= 4 * Deadline; now += 100) {
        expect(!observe(timer, now % 300 ? MountSeen::Unknown : MountSeen::Open, now),
               "mount last seen open");
    }
}

void closedAfterOpenPastTheDeadline() {
    Switch1RefusalTimer timer;
    observe(timer, MountSeen::Open, 0);
    expect(!observe(timer, MountSeen::Open, Deadline + 100), "open past the deadline");
    expect(observe(timer, MountSeen::Closed, Deadline + 200),
           "closed past the deadline ends it");
}

void reset() {
    Switch1RefusalTimer timer;
    observe(timer, MountSeen::Closed, 0);
    timer.reset();
    expect(!observe(timer, MountSeen::Closed, 6000), "reset clears the closed wait");
    expect(!observe(timer, MountSeen::Unknown, 6000 + Deadline - 1), "reset clears the deadline");
    expect(observe(timer, MountSeen::Unknown, 6000 + Deadline), "deadline from the new start");
}

} // namespace

int main() {
    closedForTheTimeout();
    closedAtTickZero();
    openRestarts();
    failedWalksDoNotRestartIt();
    failedAfterClosedEndAtTheDeadline();
    onlyFailedWalksEndAtTheDeadline();
    openMountIsNeverDropped();
    closedAfterOpenPastTheDeadline();
    reset();
    if (g_failures) {
        std::printf("switch1 refusal test: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::printf("switch1 refusal test: PASS\n");
    return 0;
}
