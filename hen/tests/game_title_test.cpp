// SPDX-License-Identifier: GPL-2.0-only
// Host test of the code mounts that game arming takes (source/game_title.hpp):
// both edges of each program ID range, and the content attributes. The title
// IDs are made up.

#include <cstdio>
#include "../source/game_title.hpp"

using namespace ounce::game;

namespace {

int g_failures = 0;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::printf("FAIL: %s\n", what);
        g_failures++;
    }
}

struct Mount {
    u64 program_id;
    u8 attributes;
    bool taken;
    const char* what;
};

constexpr Mount Mounts[] = {
    {0x0400000000100000ull, 1, true, "native application"},
    {0x0400000000010000ull, 1, true, "first native application"},
    {0x04ffffffffffffffull, 1, true, "last native application"},
    {0x040000000000ffffull, 1, false, "last native system program"},
    {0x0400000000001000ull, 1, false, "native system program"},
    {0x0500000000000000ull, 1, false, "above the native range"},
    {0x03ffffffffffffffull, 1, false, "below the native range"},
    {0x0400000000100000ull, 0, false, "native application without attributes"},
    {0x0400000000100000ull, 2, false, "native application, other attributes"},
    {0, 1, false, "program 0"},
};

void checkMounts() {
    for (const Mount& mount : Mounts) {
        if (isGameCodeMount(mount.program_id, mount.attributes) != mount.taken) {
            std::printf("FAIL: %s: program %#llx attributes %u\n", mount.what,
                        static_cast<unsigned long long>(mount.program_id), mount.attributes);
            g_failures++;
        }
    }
    expect(isNativeApplication(NativeApplicationIdMin), "native minimum");
    expect(!isNativeApplication(NativeApplicationIdMin - 1), "below the native minimum");
}

} // namespace

int main() {
    checkMounts();
    std::printf("%zu code mounts\n", sizeof(Mounts) / sizeof(Mounts[0]));
    if (g_failures) {
        std::printf("game title test: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::printf("game title test: PASS\n");
    return 0;
}
