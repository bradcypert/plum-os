#include <stddef.h>
#include <stdint.h>
#include "limine.h"
#include "../support/kernel.h"

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER
__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3)
__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST, .revision = 0
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST, .revision = 0
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_paging_mode_request paging_request = {
    .id = LIMINE_PAGING_MODE_REQUEST, .revision = 1,
    .mode = LIMINE_PAGING_MODE_X86_64_4LVL,
    .max_mode = LIMINE_PAGING_MODE_X86_64_4LVL,
    .min_mode = LIMINE_PAGING_MODE_X86_64_4LVL
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0
};
__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER

struct boot_snapshot {
    uint64_t hhdm_offset;
    uint64_t memory_map_entries;
};

_Static_assert(sizeof(struct boot_snapshot) == 16, "BootSnapshot size");
_Static_assert(_Alignof(struct boot_snapshot) == 8, "BootSnapshot alignment");
_Static_assert(offsetof(struct boot_snapshot, memory_map_entries) == 8, "BootSnapshot entry count offset");

extern int boot_snapshot_matches(const struct boot_snapshot *snapshot,
                                 uint64_t hhdm_offset, uint64_t memory_map_entries);

int plum_boot_check_snapshot(void) {
    uint64_t offset = hhdm_request.response->offset;
    uint64_t entries = memmap_request.response->entry_count;
    const struct boot_snapshot snapshot = { .hhdm_offset = offset, .memory_map_entries = entries };
    return boot_snapshot_matches(&snapshot, offset, entries);
}

/* The LimineFramebufferPrefix in main.plum reads the pinned x86-64 layout. */
_Static_assert(sizeof(void *) == 8 && offsetof(struct limine_framebuffer, address) == 0 &&
               offsetof(struct limine_framebuffer, width) == 8 &&
               offsetof(struct limine_framebuffer, height) == 16 &&
               offsetof(struct limine_framebuffer, pitch) == 24 &&
               offsetof(struct limine_framebuffer, bpp) == 32 &&
               offsetof(struct limine_framebuffer, memory_model) == 34 &&
               offsetof(struct limine_framebuffer, green_mask_size) == 37 &&
               offsetof(struct limine_framebuffer, green_mask_shift) == 38,
               "Limine framebuffer prefix layout");

const uint8_t *plum_boot_framebuffer(void) {
    const struct limine_framebuffer_response *response = framebuffer_request.response;
    if (!response || !response->framebuffer_count || !response->framebuffers) return NULL;
    return (const uint8_t *)response->framebuffers[0];
}

_Static_assert(offsetof(struct limine_memmap_response, entry_count) == 8 &&
               offsetof(struct limine_memmap_response, entries) == 16 &&
               offsetof(struct limine_memmap_entry, base) == 0 &&
               offsetof(struct limine_memmap_entry, length) == 8 &&
               sizeof(struct limine_memmap_entry) == 24, "Limine memory map layout");

const uint8_t *plum_boot_memory_map(void) {
    return (const uint8_t *)memmap_request.response;
}

#define BOOT_FAIL(message) plum_kernel_panic((const uint8_t *)(message), sizeof(message) - 1)

_Noreturn void plum_boot_enter(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED || !LIMINE_LOADED_BASE_REV_VALID ||
        LIMINE_LOADED_BASE_REVISION != 3) {
        BOOT_FAIL("unsupported Limine base revision");
    }
    struct limine_paging_mode_response *paging = paging_request.response;
    if (!hhdm_request.response || !memmap_request.response || !paging) {
        BOOT_FAIL("missing required Limine response");
    }
    if (paging->mode != LIMINE_PAGING_MODE_X86_64_4LVL) BOOT_FAIL("expected four-level paging");
    /* Any early failure uses the same serial initializer; never reset COM1. */
    plum_kernel_runtime_init();
    plum_kernel_start();
}
