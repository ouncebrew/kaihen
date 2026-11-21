// SPDX-License-Identifier: GPL-2.0-only
// RO's ldr:ro client contexts (session.cpp) and NRO mapping (mapping.cpp).
#pragma once

#include <switch/types.h>
#include "ro_diagnostics.h"
#include "results.h"
#include "wire.h"

constexpr u32 RoMaxNrr = 8;
constexpr u32 RoMaxNro = 8;
/// The JIT mappings that RO records for all its client contexts together.
/// Only homebrew sends the JIT commands, so the contexts share one table.
constexpr u32 RoMaxJit = 4096;

/// What RO has mapped for a loaded image, in RoLoadedImage::mapped.
enum RoImageState : u32 {
    RoImageState_CodeMapped = BIT(0),  ///< The image is mapped at base.
    RoImageState_BssMapped = BIT(1),   ///< The BSS is mapped after the image.
    RoImageState_AliasMapped = BIT(2), ///< RO's own view of the image is mapped at alias.
    RoImageState_Ready = BIT(3),       ///< Loaded: validated, published and with its permissions.
};

/// The arguments of LoadNro: the client's heap pages that hold the image
/// and its BSS.
struct RoImageRequest {
    u64 source;
    u64 size;
    u64 bss_source;
    u64 bss_size;
};

struct RoLoadedImage {
    u64 base;
    u64 source;
    u64 size;
    u64 bss_source;
    u64 bss_size;
    u64 alias;
    u8 module_id[32];
    u32 mapped; ///< RoImageState bits.
};

struct RoSession;

/// Generated code that RO mapped executable for a client (the JIT commands).
struct RoJitMapping {
    const RoSession* owner;
    u64 destination;
    u64 source;
    u64 size;    ///< Zero for a free entry.
    void* alias; ///< RO's writable view of destination, when the client asked for one.
};

/// One ldr:ro client context.
struct RoSession {
    u32 port_index;
    u64 process_id;
    Handle process_handle; ///< Copied by Initialize; owned.
    u64 nrr_source[RoMaxNrr];
    u64 nrr_size[RoMaxNrr];
    RoLoadedImage nro[RoMaxNro];
};

/**
 * @brief Maps an NRO into the process: finds a free address, maps the image
 *        and BSS there, checks the header through a view in RO, and sets
 *        the segment permissions. A failure unmaps what was mapped; when
 *        that also fails, the record keeps what is still mapped.
 * @param[in] existing The client's other records, checked for the same module.
 * @param[out] out_base The image's address in the process.
 */
Result roMapImage(Handle process, const RoImageRequest& request, RoLoadedImage* record,
                  const RoLoadedImage* existing, u32 existing_count, u64* out_base);

/// Unmaps an image that roMapImage mapped. A failure keeps the record of what is still mapped.
Result roUnmapImage(Handle process, RoLoadedImage* record);

/**
 * @brief Releases a client context: RO's JIT views and unfinished image
 *        mappings, then the process handle. Loaded NROs and JIT code stay
 *        mapped, as in stock RO's teardown, because the process may still
 *        run them.
 *        The process handle is closed and the context cleared even when an
 *        unmap fails, so that the process can be destroyed; the failure is
 *        counted in RoDiagnostics.
 * @return The last unmap failure, or success.
 */
Result roSessionClose(RoSession* s);

/// Answers one request on ldr:ro session @p index of @p sessions.
/// A returned handle was copied in the reply and must be closed after sending it.
Handle roDispatch(RoSession* sessions, u32 num_sessions, u32 index, void* base);

void roRecordAcceptedSession(u32 port_index);
void roRecordImageRejection(const void* header, u64 image_size, u64 bss_size);
RoDiagnostics roGetDiagnostics();
