// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// The layout of a Switch 1 title's donor package: a flat PFS0 holding
// hbloader as rtld and the title's other ExeFS files, in the order and with
// the header that writeExeFsPackage (common/policy_package.cpp) uses for two
// files. Only the libnx types, so that host tests build it.
extern "C" {
#include <switch/types.h>
}

namespace ounce::game {

/// Most files a donor holds, and most files a title's ExeFS may list.
constexpr u32 DonorFileLimit = 24;
/// Longest ExeFS file name a donor takes.
constexpr size_t DonorNameLimit = 64;
/// Largest ExeFS file a donor takes.
constexpr u64 DonorFileSizeLimit = 0x10000000;
/// PFS0 header, one 24-byte entry per file, then the names.
constexpr size_t DonorHeaderCapacity =
    16 + DonorFileLimit * 24 + DonorFileLimit * (DonorNameLimit + 1);

struct DonorFile {
    char name[DonorNameLimit + 1];
    u64 size;
    /// The file is hbloader from SD, not one of the title's.
    bool hbloader;
};

struct DonorPlan {
    /// The package's files, in package order.
    DonorFile files[DonorFileLimit];
    /// Where each file's data starts, from the start of the package.
    u64 offsets[DonorFileLimit];
    u32 count;
    /// The PFS0 header, entries and names: header[0, header_size).
    u32 header_size;
    /// The whole package; the last file is not padded.
    u64 size;
    u8 header[DonorHeaderCapacity];
};

namespace donor_detail {

constexpr size_t length(const char* text) {
    size_t count = 0;
    while (text[count]) count++;
    return count;
}

constexpr int compare(const char* left, const char* right) {
    while (*left && *left == *right) {
        left++;
        right++;
    }
    return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

constexpr bool startsWith(const char* text, const char* prefix) {
    while (*prefix) {
        if (*text++ != *prefix++) return false;
    }
    return true;
}

/// The digits of "subsdk<digits>", or nullptr for any other name.
constexpr const char* subsdkDigits(const char* name) {
    if (!startsWith(name, "subsdk") || !name[6]) return nullptr;
    for (const char* digit = name + 6; *digit; digit++) {
        if (*digit < '0' || *digit > '9') return nullptr;
    }
    return name + 6;
}

/// rtld, main, subsdk<n>, sdk, any other file, main.npdm.
constexpr u32 rank(const char* name) {
    if (!compare(name, "rtld")) return 0;
    if (!compare(name, "main")) return 1;
    if (subsdkDigits(name)) return 2;
    if (!compare(name, "sdk")) return 3;
    if (!compare(name, "main.npdm")) return 5;
    return 4;
}

/// Compares two digit strings by value, of any length.
constexpr int compareNumbers(const char* left, const char* right) {
    while (*left == '0') left++;
    while (*right == '0') right++;
    const size_t left_length = length(left);
    const size_t right_length = length(right);
    if (left_length != right_length) return left_length < right_length ? -1 : 1;
    return compare(left, right);
}

inline void put32(u8* out, u32 value) {
    for (int index = 0; index < 4; index++) out[index] = static_cast<u8>(value >> (8 * index));
}

inline void put64(u8* out, u64 value) {
    for (int index = 0; index < 8; index++) out[index] = static_cast<u8>(value >> (8 * index));
}

constexpr u64 align32(u64 value) {
    return (value + 0x1f) & ~u64(0x1f);
}

} // namespace donor_detail

/// An ExeFS file name a donor takes: one path component of printable ASCII
/// without separators or drive marks, at most DonorNameLimit characters.
constexpr bool isDonorName(const char* name) {
    if (!name || !name[0]) return false;
    if (name[0] == '.' && (!name[1] || (name[1] == '.' && !name[2]))) return false;
    size_t count = 0;
    for (const char* character = name; *character; character++) {
        const unsigned char value = static_cast<unsigned char>(*character);
        if (value <= 0x20 || value >= 0x7f || value == '/' || value == '\\' || value == ':') {
            return false;
        }
        if (++count > DonorNameLimit) return false;
    }
    return true;
}

/// Whether `left` comes before `right` in a donor: rtld, main, subsdk<n> by
/// the value of n, sdk, any other file by name, then main.npdm. Equal values
/// of n, and other files, are ordered by name, byte by byte.
constexpr bool donorBefore(const char* left, const char* right) {
    const u32 left_rank = donor_detail::rank(left);
    const u32 right_rank = donor_detail::rank(right);
    if (left_rank != right_rank) return left_rank < right_rank;
    if (left_rank == 2) {
        const int numbers = donor_detail::compareNumbers(donor_detail::subsdkDigits(left),
                                                         donor_detail::subsdkDigits(right));
        if (numbers) return numbers < 0;
    }
    return donor_detail::compare(left, right) < 0;
}

/// The magic a donor file must start with: "NSO0" for rtld, main, sdk and
/// subsdk*, "META" for main.npdm, nullptr for any other file.
constexpr const char* donorMagic(const char* name) {
    if (!donor_detail::compare(name, "main.npdm")) return "META";
    if (!donor_detail::compare(name, "rtld") || !donor_detail::compare(name, "main") ||
        !donor_detail::compare(name, "sdk") || donor_detail::startsWith(name, "subsdk")) {
        return "NSO0";
    }
    return nullptr;
}

/// Where main.npdm's META header keeps its flags; bit 0 marks a 64-bit
/// process. hbloader is 64-bit code, so only a 64-bit title makes a donor.
constexpr size_t MetaFlagsOffset = 0xc;

/// Whether the start of a main.npdm is a META header for a 64-bit process.
constexpr bool isDonorPolicy(const u8* meta, size_t size) {
    return size > MetaFlagsOffset && meta[0] == 'M' && meta[1] == 'E' && meta[2] == 'T' &&
           meta[3] == 'A' && (meta[MetaFlagsOffset] & 1);
}

/// Plans a donor from a title's ExeFS listing and hbloader's size: hbloader
/// as rtld, then every file of the title except its own rtld, in donorBefore
/// order. False, with `plan` unusable, for a listing that cannot make a donor:
/// no main or main.npdm, a name isDonorName refuses or that repeats, a file
/// over DonorFileSizeLimit, or more than DonorFileLimit files.
inline bool planDonor(const DonorFile* title, u32 title_count, u64 hbloader_size,
                      DonorPlan& plan) {
    plan.count = 0;
    plan.header_size = 0;
    plan.size = 0;
    if (title_count > DonorFileLimit || hbloader_size > DonorFileSizeLimit) return false;

    DonorFile& rtld = plan.files[plan.count++];
    rtld = {};
    __builtin_memcpy(rtld.name, "rtld", sizeof("rtld"));
    rtld.size = hbloader_size;
    rtld.hbloader = true;
    bool has_main = false;
    bool has_policy = false;
    for (u32 index = 0; index < title_count; index++) {
        const DonorFile& file = title[index];
        if (!isDonorName(file.name) || file.size > DonorFileSizeLimit) return false;
        for (u32 other = index + 1; other < title_count; other++) {
            if (!donor_detail::compare(file.name, title[other].name)) return false;
        }
        has_main = has_main || !donor_detail::compare(file.name, "main");
        has_policy = has_policy || !donor_detail::compare(file.name, "main.npdm");
        if (!donor_detail::compare(file.name, "rtld")) continue;
        if (plan.count == DonorFileLimit) return false;
        DonorFile& entry = plan.files[plan.count++];
        entry = file;
        entry.hbloader = false;
    }
    if (!has_main || !has_policy) return false;

    // Insertion sort: at most DonorFileLimit files.
    for (u32 index = 1; index < plan.count; index++) {
        const DonorFile file = plan.files[index];
        u32 slot = index;
        while (slot && donorBefore(file.name, plan.files[slot - 1].name)) {
            plan.files[slot] = plan.files[slot - 1];
            slot--;
        }
        plan.files[slot] = file;
    }

    u32 names_size = 0;
    for (u32 index = 0; index < plan.count; index++) {
        names_size += static_cast<u32>(donor_detail::length(plan.files[index].name)) + 1;
    }
    plan.header_size = 16 + plan.count * 24 + names_size;
    __builtin_memset(plan.header, 0, sizeof(plan.header));
    __builtin_memcpy(plan.header, "PFS0", 4);
    donor_detail::put32(plan.header + 4, plan.count);
    donor_detail::put32(plan.header + 8, names_size);

    u8* names = plan.header + 16 + plan.count * 24;
    u32 name_offset = 0;
    u64 data_end = 0;
    for (u32 index = 0; index < plan.count; index++) {
        const DonorFile& file = plan.files[index];
        // Each file starts 32-byte aligned after the previous one.
        const u64 offset = index ? donor_detail::align32(data_end) : 0;
        u8* entry = plan.header + 16 + index * 24;
        donor_detail::put64(entry, offset);
        donor_detail::put64(entry + 8, file.size);
        donor_detail::put32(entry + 16, name_offset);
        const size_t name_size = donor_detail::length(file.name) + 1;
        __builtin_memcpy(names + name_offset, file.name, name_size);
        name_offset += static_cast<u32>(name_size);
        plan.offsets[index] = plan.header_size + offset;
        data_end = offset + file.size;
    }
    plan.size = plan.header_size + data_end;
    return true;
}

} // namespace ounce::game
