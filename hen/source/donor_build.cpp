// SPDX-License-Identifier: GPL-2.0-only
#include <cstring>
#include "donor_build.hpp"
#include "game_title.hpp"
#include "../../common/result.hpp"
#include "../../common/sd_writer.hpp"

namespace {

constexpr Result InvalidDonor = MAKERESULT(Module_Libnx, LibnxError_BadInput);
/// FS's result for a path that does not exist.
constexpr Result PathNotFound = MAKERESULT(2, 1);
/// FS's result for a path that already exists.
constexpr Result PathAlreadyExists = MAKERESULT(2, 2);
/// Most bytes one step() copies. HEN serves its IPC between steps.
constexpr size_t DonorChunk = 0x10000;
alignas(0x1000) u8 g_donor_chunk[DonorChunk];
/// The title's ExeFS listing, kept off HEN's stack; one build runs at a time.
ounce::game::DonorFile g_donor_title[ounce::game::DonorFileLimit];

/// Fails unless `path` names nothing on the SD card.
Result checkAbsent(FsFileSystem* sd, const char* path) {
    FsDirEntryType type = FsDirEntryType_File;
    const Result rc = fsFsGetEntryType(sd, path, &type);
    if (R_SUCCEEDED(rc)) return PathAlreadyExists;
    return rc == PathNotFound ? 0 : rc;
}

/// Opens a title's code file system: readSignedPolicy's fsp-ldr command 0,
/// OpenCodeFileSystem (common/policy_package.cpp), on the borrowed Loader root.
Result openCodeFileSystem(Handle loader_root, u64 program_id, u8 storage_id, u8 attributes,
                          FsFileSystem* out) {
    R_UNLESS(loader_root != INVALID_HANDLE, InvalidDonor);
    Service loader = {};
    serviceCreate(&loader, loader_root);
    loader.own_handle = false;
    const struct {
        u8 attributes;
        u8 storage_id;
        u64 program_id;
    } request = {attributes, storage_id, program_id};
    u8 code_info[0x1a4] = {};
    *out = {};
    return serviceDispatchIn(&loader, 0, request,
        .buffer_attrs = {SfBufferAttr_HipcMapAlias | SfBufferAttr_Out},
        .buffers = {{code_info, sizeof(code_info)}},
        .out_num_objects = 1,
        .out_objects = &out->s,
    );
}

/// "/<name>" in FS's path buffer, for a name that isDonorName takes.
void codePath(char (&out)[FS_MAX_PATH], const char* name) {
    static_assert(ounce::game::DonorNameLimit + 2 <= FS_MAX_PATH);
    out[0] = '/';
    std::memcpy(out + 1, name, std::strlen(name) + 1);
}

} // namespace

ounce::game::DonorPlan& donorPlanStorage() {
    static ounce::game::DonorPlan plan = {};
    return plan;
}

Switch1DonorBuild::~Switch1DonorBuild() {
    abandon();
}

void Switch1DonorBuild::abandon() {
    if (source_open_) {
        fsFileClose(&source_);
        source_open_ = false;
    }
    if (part_open_) {
        fsFileClose(&part_);
        part_open_ = false;
    }
    if (part_created_ && sd_open_) {
        fsFsDeleteFile(&sd_, ounce::game::Switch1DefaultDonorPart);
        fsFsCommit(&sd_);
    }
    part_created_ = false;
    if (sd_open_) {
        fsFsClose(&sd_);
        sd_open_ = false;
    }
    if (code_open_) {
        fsFsClose(&code_);
        code_open_ = false;
    }
}

Result Switch1DonorBuild::begin(Handle loader_root, Handle fs_root, u64 program_id,
                                u8 storage_id, const char* hbloader_path) {
    abandon();
    position_ = 0;
    copied_ = 0;
    written_ = 0;
    plan_.count = 0;
    stage_ = Stage_CodeFileSystem;
    const auto run = [&]() -> Result {
        R_UNLESS(hbloader_path && std::strlen(hbloader_path) < sizeof(hbloader_path_),
                 InvalidDonor);
        std::strcpy(hbloader_path_, hbloader_path);
        R_TRY(openCodeFileSystem(loader_root, program_id, storage_id,
                                 ounce::game::Switch1CodeAttributes, &code_));
        code_open_ = true;

        stage_ = Stage_List;
        ounce::game::DonorFile* const title = g_donor_title;
        std::memset(g_donor_title, 0, sizeof(g_donor_title));
        u32 title_count = 0;
        R_TRY(listTitle(title, title_count));
        stage_ = Stage_Policy;
        R_TRY(checkPolicy());

        stage_ = Stage_SdCard;
        R_TRY(openSdCardFileSystem(fs_root, &sd_));
        sd_open_ = true;
        stage_ = Stage_DonorAbsent;
        R_TRY(checkAbsent(&sd_, ounce::game::Switch1DefaultDonorFile));

        stage_ = Stage_Hbloader;
        FsFile hbloader = {};
        R_TRY(fsFsOpenFile(&sd_, hbloader_path_, FsOpenMode_Read, &hbloader));
        s64 hbloader_size = 0;
        const Result size_rc = fsFileGetSize(&hbloader, &hbloader_size);
        fsFileClose(&hbloader);
        R_TRY(size_rc);
        R_UNLESS(hbloader_size > 0, InvalidDonor);

        stage_ = Stage_Plan;
        R_UNLESS(ounce::game::planDonor(title, title_count, static_cast<u64>(hbloader_size),
                                        plan_),
                 InvalidDonor);

        // A part file left by an earlier HEN session that stopped mid-copy is
        // replaced; nothing opens it.
        stage_ = Stage_CreatePart;
        const char* part = ounce::game::Switch1DefaultDonorPart;
        R_TRY(createSdParents(&sd_, part));
        FsDirEntryType type = FsDirEntryType_Dir;
        if (R_SUCCEEDED(fsFsGetEntryType(&sd_, part, &type))) {
            R_UNLESS(type == FsDirEntryType_File, InvalidDonor);
            R_TRY(fsFsDeleteFile(&sd_, part));
        }
        R_TRY(fsFsCreateFile(&sd_, part, static_cast<s64>(plan_.size), 0));
        part_created_ = true;
        R_TRY(fsFsOpenFile(&sd_, part, FsOpenMode_Write, &part_));
        part_open_ = true;
        R_TRY(fsFileWrite(&part_, 0, plan_.header, plan_.header_size, FsWriteOption_None));
        written_ = plan_.header_size;
        stage_ = Stage_Copy;
        R_SUCCEED();
    };
    const Result rc = run();
    if (R_FAILED(rc)) abandon();
    return rc;
}

Result Switch1DonorBuild::listTitle(ounce::game::DonorFile* title, u32& count) {
    count = 0;
    char root[FS_MAX_PATH] = "/";
    FsDir directory = {};
    R_TRY(fsFsOpenDirectory(&code_, root, FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles,
                            &directory));
    Result rc = 0;
    for (;;) {
        FsDirectoryEntry entry = {};
        s64 read = 0;
        rc = fsDirRead(&directory, &read, 1, &entry);
        if (R_FAILED(rc) || read == 0) break;
        entry.name[sizeof(entry.name) - 1] = '\0';
        // A donor is flat: a directory, a name a donor does not take, or too
        // many files refuses the whole title.
        if (read != 1 || entry.type != FsDirEntryType_File || entry.file_size < 0 ||
            count == ounce::game::DonorFileLimit ||
            !ounce::game::isDonorName(entry.name)) {
            rc = InvalidDonor;
            break;
        }
        ounce::game::DonorFile& file = title[count++];
        std::strcpy(file.name, entry.name);
        file.size = static_cast<u64>(entry.file_size);
        file.hbloader = false;
    }
    fsDirClose(&directory);
    return rc;
}

Result Switch1DonorBuild::checkPolicy() {
    char path[FS_MAX_PATH] = {};
    codePath(path, "main.npdm");
    FsFile file = {};
    R_TRY(fsFsOpenFile(&code_, path, FsOpenMode_Read, &file));
    u8 meta[ounce::game::MetaFlagsOffset + 1] = {};
    u64 read = 0;
    Result rc = fsFileRead(&file, 0, meta, sizeof(meta), FsReadOption_None, &read);
    fsFileClose(&file);
    R_TRY(rc);
    R_UNLESS(ounce::game::isDonorPolicy(meta, read), InvalidDonor);
    R_SUCCEED();
}

Result Switch1DonorBuild::step(bool& complete) {
    complete = false;
    Result rc = InvalidDonor;
    if (part_open_) {
        rc = position_ < plan_.count ? copyChunk() : finish(complete);
    }
    if (R_FAILED(rc)) abandon();
    return rc;
}

Result Switch1DonorBuild::openSource() {
    const ounce::game::DonorFile& file = plan_.files[position_];
    char path[FS_MAX_PATH] = {};
    if (file.hbloader) {
        std::strcpy(path, hbloader_path_);
    } else {
        codePath(path, file.name);
    }
    R_TRY(fsFsOpenFile(file.hbloader ? &sd_ : &code_, path, FsOpenMode_Read, &source_));
    source_open_ = true;
    copied_ = 0;
    s64 size = 0;
    R_TRY(fsFileGetSize(&source_, &size));
    // The file must still be the size that the header gives it.
    R_UNLESS(size >= 0 && static_cast<u64>(size) == file.size, InvalidDonor);

    // Up to 31 zero bytes align the file's data.
    const u64 offset = plan_.offsets[position_];
    R_UNLESS(written_ <= offset && offset - written_ < 0x20, InvalidDonor);
    if (offset > written_) {
        const u8 padding[0x20] = {};
        R_TRY(fsFileWrite(&part_, static_cast<s64>(written_), padding, offset - written_,
                          FsWriteOption_None));
        written_ = offset;
    }
    R_SUCCEED();
}

Result Switch1DonorBuild::copyChunk() {
    if (!source_open_) R_TRY(openSource());
    const ounce::game::DonorFile& file = plan_.files[position_];
    const u64 left = file.size - copied_;
    const u64 wanted = left < DonorChunk ? left : DonorChunk;
    if (wanted) {
        u64 received = 0;
        R_TRY(fsFileRead(&source_, static_cast<s64>(copied_), g_donor_chunk, wanted,
                         FsReadOption_None, &received));
        R_UNLESS(received == wanted, InvalidDonor);
        if (!copied_) {
            // The module and policy files must be what Loader expects.
            const char* magic = ounce::game::donorMagic(file.name);
            R_UNLESS(!magic || (wanted >= 4 && !std::memcmp(g_donor_chunk, magic, 4)),
                     InvalidDonor);
        }
        R_TRY(fsFileWrite(&part_, static_cast<s64>(written_), g_donor_chunk, wanted,
                          FsWriteOption_None));
        copied_ += wanted;
        written_ += wanted;
    } else {
        // An empty file has no magic: only one without a required magic is taken.
        R_UNLESS(!ounce::game::donorMagic(file.name), InvalidDonor);
    }
    if (copied_ == file.size) {
        fsFileClose(&source_);
        source_open_ = false;
        position_++;
    }
    R_SUCCEED();
}

Result Switch1DonorBuild::finish(bool& complete) {
    stage_ = Stage_Finish;
    R_UNLESS(written_ == plan_.size, InvalidDonor);
    Result rc = fsFileFlush(&part_);
    fsFileClose(&part_);
    part_open_ = false;
    R_TRY(rc);
    R_TRY(fsFsCommit(&sd_));
    // A donor that appeared meanwhile is kept; this build is dropped.
    R_TRY(checkAbsent(&sd_, ounce::game::Switch1DefaultDonorFile));
    R_TRY(fsFsRenameFile(&sd_, ounce::game::Switch1DefaultDonorPart,
                         ounce::game::Switch1DefaultDonorFile));
    part_created_ = false;
    complete = true;
    rc = fsFsCommit(&sd_);
    abandon();
    return rc;
}
