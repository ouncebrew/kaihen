// SPDX-License-Identifier: GPL-2.0-only
// Host test of the code mounts that game arming takes (source/game_title.hpp):
// both edges of each program ID range, and the content attributes, then the
// path of a Switch 1 title's donor package. The title IDs are made up.

#include <cstdio>
#include <cstring>
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
    {0x0100000000100000ull, 0, true, "Switch 1 application"},
    {0x0100000000010000ull, 0, true, "first Switch 1 application"},
    {0x01ffffffffffffffull, 0, true, "last Switch 1 application"},
    {0x010000000000ffffull, 0, false, "last Switch 1 system program"},
    {0x0100000000001000ull, 0, false, "Switch 1 system program"},
    {0x0200000000000000ull, 0, false, "above the Switch 1 range"},
    {0x00ffffffffffffffull, 0, false, "below the Switch 1 range"},
    {0x0100000000100000ull, 1, false, "Switch 1 application with native attributes"},
    {0x0100000000100000ull, 2, false, "Switch 1 application, other attributes"},
    {0x0400000000001000ull, 0, false, "native system program without attributes"},
    {0, 0, false, "program 0 without attributes"},
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
    expect(isSwitch1Application(Switch1ApplicationIdMin), "Switch 1 minimum");
    expect(!isSwitch1Application(Switch1ApplicationIdMin - 1), "below the Switch 1 minimum");
    expect(!isNativeApplication(Switch1ApplicationIdMax), "the ranges do not overlap");
}

void checkPackagePath() {
    const Switch1PackagePath path(0x0100000000100000ull);
    expect(std::strcmp(path.text, "@Sdcard:/kaihen/bin/s1/0100000000100000.nsp") == 0,
           "package path");
    expect(std::strlen(path.text) + 1 == sizeof(path.text), "package path fills its buffer");
    const Switch1PackagePath digits(0x01234567890abcdeull);
    expect(std::strcmp(digits.text, "@Sdcard:/kaihen/bin/s1/01234567890abcde.nsp") == 0,
           "lowercase hex digits");
    constexpr Switch1PackagePath last(Switch1ApplicationIdMax);
    static_assert(last.text[sizeof(last.text) - 2] == 'p' && last.text[sizeof(last.text) - 1] == 0);
    expect(std::strcmp(last.text, "@Sdcard:/kaihen/bin/s1/01ffffffffffffff.nsp") == 0,
           "last Switch 1 application");
}

} // namespace

int main() {
    checkMounts();
    checkPackagePath();
    std::printf("%zu code mounts\n", sizeof(Mounts) / sizeof(Mounts[0]));
    if (g_failures) {
        std::printf("game title test: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::printf("game title test: PASS\n");
    return 0;
}
