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

/* Borrowed boot data, reserved for later kernel facilities. Do not reclaim or
 * reinterpret these addresses as physical pointers. The demo owns no pages. */
volatile uint64_t plum_boot_hhdm_offset;
const struct limine_memmap_response *volatile plum_boot_memory_map;

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
    uint64_t offset = plum_boot_hhdm_offset;
    uint64_t entries = plum_boot_memory_map->entry_count;
    const struct boot_snapshot snapshot = { .hhdm_offset = offset, .memory_map_entries = entries };
    return boot_snapshot_matches(&snapshot, offset, entries);
}

struct boot_framebuffer {
    uint64_t address;
    int64_t width;
    int64_t height;
    int64_t pitch;
    uint32_t green;
};

_Static_assert(sizeof(struct boot_framebuffer) == 40, "BootFramebuffer size");
_Static_assert(offsetof(struct boot_framebuffer, green) == 32, "BootFramebuffer green offset");

static struct boot_framebuffer boot_framebuffer;

static void capture_framebuffer(void) {
    const struct limine_framebuffer_response *response = framebuffer_request.response;
    if (!response || !response->framebuffer_count || !response->framebuffers) return;

    const struct limine_framebuffer *framebuffer = response->framebuffers[0];
    if (!framebuffer || !framebuffer->address || framebuffer->bpp != 32 ||
        framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB ||
        framebuffer->green_mask_size != 8 || framebuffer->green_mask_shift > 24) return;

    uintptr_t address = (uintptr_t)framebuffer->address;
    uint64_t pitch = framebuffer->pitch;
    if (!framebuffer->width || !framebuffer->height || !pitch ||
        address % _Alignof(uint32_t) || pitch % sizeof(uint32_t) ||
        framebuffer->width > pitch / sizeof(uint32_t) ||
        framebuffer->height > INT64_MAX / pitch ||
        framebuffer->height * pitch > UINTPTR_MAX - address) return;

    // Limine already supplies a mapped virtual address for the pixels.
    boot_framebuffer = (struct boot_framebuffer){
        .address = address,
        .width = (int64_t)framebuffer->width,
        .height = (int64_t)framebuffer->height,
        .pitch = (int64_t)pitch,
        .green = UINT32_C(255) << framebuffer->green_mask_shift
    };
}

const struct boot_framebuffer *plum_boot_framebuffer(void) {
    return boot_framebuffer.address ? &boot_framebuffer : NULL;
}

#define BOOT_FAIL(message) plum_kernel_panic((const uint8_t *)(message), sizeof(message) - 1)

_Noreturn void plum_boot_enter(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED || !LIMINE_LOADED_BASE_REV_VALID ||
        LIMINE_LOADED_BASE_REVISION != 3) {
        BOOT_FAIL("unsupported Limine base revision");
    }
    struct limine_hhdm_response *hhdm = hhdm_request.response;
    struct limine_memmap_response *map = memmap_request.response;
    struct limine_paging_mode_response *paging = paging_request.response;
    if (!hhdm || !map || !paging) BOOT_FAIL("missing required Limine response");
    if (paging->mode != LIMINE_PAGING_MODE_X86_64_4LVL) BOOT_FAIL("expected four-level paging");
    if (!map->entry_count || !map->entries) BOOT_FAIL("empty Limine memory map");
    for (uint64_t i = 0; i < map->entry_count; ++i) {
        const struct limine_memmap_entry *entry = map->entries[i];
        if (!entry || entry->length > UINT64_MAX - entry->base) BOOT_FAIL("invalid Limine memory region");
    }
    plum_boot_hhdm_offset = hhdm->offset;
    plum_boot_memory_map = map;
    capture_framebuffer();
    /* Any early failure uses the same serial initializer; never reset COM1. */
    plum_kernel_runtime_init();
    plum_kernel_start();
}
