// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Which of Loader's OpenCodeFileSystem requests game arming (game_arm.cpp)
// takes as a game's code mount, and where a Switch 1 title's donor package
// is. Only the libnx types, so that host tests build it.
extern "C" {
#include <switch/types.h>
}

namespace ounce::game {

// Native titles observed here use the 0x04 namespace. This is the Switch 2
// analogue of Atmosphere's ncm::IsApplicationId range check; the exact native
// endpoints still need live qualification against more than one game title.
constexpr u64 NativeApplicationIdMin = 0x0400000000010000ull;
constexpr u64 NativeApplicationIdMax = 0x04ffffffffffffffull;
// Switch 1 titles keep their Switch 1 program IDs: Atmosphere's
// ncm::ApplicationId range. System programs (0x0100000000000000 to
// 0x010000000000ffff) stay outside it.
constexpr u64 Switch1ApplicationIdMin = 0x0100000000010000ull;
constexpr u64 Switch1ApplicationIdMax = 0x01ffffffffffffffull;
/// The content attributes of a native game's code file system, in Loader's
/// OpenCodeFileSystem request and in readSignedPolicy.
constexpr u8 NativeCodeAttributes = 1;
/// Loader opens a Switch 1 title's code file system with no content
/// attributes.
constexpr u8 Switch1CodeAttributes = 0;

constexpr bool isNativeApplication(u64 program_id) {
    return program_id >= NativeApplicationIdMin && program_id <= NativeApplicationIdMax;
}

constexpr bool isSwitch1Application(u64 program_id) {
    return program_id >= Switch1ApplicationIdMin && program_id <= Switch1ApplicationIdMax;
}

/// Whether Loader's OpenCodeFileSystem arguments mount a game's code.
constexpr bool isGameCodeMount(u64 program_id, u8 content_attributes) {
    return (content_attributes == NativeCodeAttributes && isNativeApplication(program_id)) ||
           (content_attributes == Switch1CodeAttributes && isSwitch1Application(program_id));
}

/// A Switch 1 title's donor package on SD:
/// "@Sdcard:/kaihen/bin/s1/<program ID, 16 lowercase hex digits>.nsp".
struct Switch1PackagePath {
    static constexpr char Prefix[] = "@Sdcard:/kaihen/bin/s1/";
    static constexpr char Suffix[] = ".nsp";
    char text[sizeof(Prefix) - 1 + 16 + sizeof(Suffix)] = {};

    constexpr explicit Switch1PackagePath(u64 program_id) {
        constexpr char Digits[] = "0123456789abcdef";
        size_t length = 0;
        for (size_t index = 0; index < sizeof(Prefix) - 1; index++) {
            text[length++] = Prefix[index];
        }
        for (int shift = 60; shift >= 0; shift -= 4) {
            text[length++] = Digits[(program_id >> shift) & 0xf];
        }
        for (size_t index = 0; index < sizeof(Suffix); index++) {
            text[length++] = Suffix[index];
        }
    }
};

} // namespace ounce::game
