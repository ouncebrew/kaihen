// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Which of Loader's OpenCodeFileSystem requests game arming (game_arm.cpp)
// takes as a game's code mount. Only the libnx types, so that host tests
// build it.
extern "C" {
#include <switch/types.h>
}

namespace ounce::game {

// Native titles observed here use the 0x04 namespace. This is the Switch 2
// analogue of Atmosphere's ncm::IsApplicationId range check; the exact native
// endpoints still need live qualification against more than one game title.
constexpr u64 NativeApplicationIdMin = 0x0400000000010000ull;
constexpr u64 NativeApplicationIdMax = 0x04ffffffffffffffull;
/// The content attributes of a native game's code file system, in Loader's
/// OpenCodeFileSystem request and in readSignedPolicy.
constexpr u8 NativeCodeAttributes = 1;

constexpr bool isNativeApplication(u64 program_id) {
    return program_id >= NativeApplicationIdMin && program_id <= NativeApplicationIdMax;
}

/// Whether Loader's OpenCodeFileSystem arguments mount a game's code.
constexpr bool isGameCodeMount(u64 program_id, u8 content_attributes) {
    return content_attributes == NativeCodeAttributes && isNativeApplication(program_id);
}

} // namespace ounce::game
