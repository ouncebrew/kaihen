// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <switch.h>
#include "donor_layout.hpp"

/// Builds the shared Switch 1 donor (game_title.hpp Switch1DefaultDonorFile)
/// from a running Switch 1 title's ExeFS, in bounded steps. The package is
/// written to Switch1DefaultDonorPart and renamed only once every byte is
/// written and committed; any failure, and abandon(), deletes the part file,
/// so the donor's name never holds a partial package.
/// The one DonorPlan a Switch1DonorBuild uses, kept off HEN's stack.
ounce::game::DonorPlan& donorPlanStorage();

class Switch1DonorBuild {
  public:
    /// Where a build stopped, for the trace: bits 8 and up of trace().
    enum Stage : u32 {
        Stage_CodeFileSystem = 1,
        Stage_List = 2,
        Stage_Policy = 3,
        Stage_SdCard = 4,
        Stage_DonorAbsent = 5,
        Stage_Hbloader = 6,
        Stage_Plan = 7,
        Stage_CreatePart = 8,
        Stage_Copy = 9,
        Stage_Finish = 10,
    };

    ~Switch1DonorBuild();

    /// Lists the title's code file system, plans the donor and writes its
    /// header to the part file. A failure has already abandoned the build.
    Result begin(Handle loader_root, Handle fs_root, u64 program_id, u8 storage_id,
                 const char* hbloader_path);
    /// Copies at most one chunk; once every file is copied, commits the part
    /// file and renames it, and sets `complete`. A failure has already
    /// abandoned the build.
    Result step(bool& complete);
    /// Closes every file and deletes the part file, if it exists.
    void abandon();
    /// The stage reached in bits 8 and up, the package file index in the low byte.
    u32 trace() const { return (stage_ << 8) | (position_ & 0xff); }
    u32 count() const { return plan_.count; }

  private:
    Result listTitle(ounce::game::DonorFile* title, u32& count);
    Result checkPolicy();
    Result openSource();
    Result copyChunk();
    Result finish(bool& complete);

    FsFileSystem code_ = {};
    FsFileSystem sd_ = {};
    FsFile source_ = {};
    FsFile part_ = {};
    bool code_open_ = false;
    bool sd_open_ = false;
    bool source_open_ = false;
    bool part_open_ = false;
    /// The part file exists on SD and is this build's to delete.
    bool part_created_ = false;
    u32 stage_ = 0;
    /// The package file being copied.
    u32 position_ = 0;
    /// Bytes of that file already copied.
    u64 copied_ = 0;
    /// Bytes of the part file already written.
    u64 written_ = 0;
    char hbloader_path_[FS_MAX_PATH] = {};
    /// File-static storage (donor_build.cpp): only one build runs at a time.
    ounce::game::DonorPlan& plan_ = donorPlanStorage();
};
