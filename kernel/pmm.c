#include "pmm.h"

/*
 * One bit per physical frame:
 *
 * 0 = free
 * 1 = used
 *
 * 8192 frames / 32 bits = 256 uint32_t entries
 */
static uint32_t frame_bitmap[PMM_MAX_FRAMES / 32];

static uint32_t total_frames = PMM_MAX_FRAMES;
static uint32_t used_frames = PMM_MAX_FRAMES;


/*
 * Mark one frame as used.
 */
static void bitmap_set(uint32_t frame) {
    uint32_t index = frame / 32;
    uint32_t bit = frame % 32;

    frame_bitmap[index] |= (1u << bit);
}


/*
 * Mark one frame as free.
 */
static void bitmap_clear(uint32_t frame) {
    uint32_t index = frame / 32;
    uint32_t bit = frame % 32;

    frame_bitmap[index] &= ~(1u << bit);
}


/*
 * Check whether a frame is used.
 */
static int bitmap_test(uint32_t frame) {
    uint32_t index = frame / 32;
    uint32_t bit = frame % 32;

    return (frame_bitmap[index] & (1u << bit)) != 0;
}


/*
 * Initialize all physical memory as reserved.
 *
 * Later, E820 will tell us which regions are actually usable,
 * and those regions will be marked free.
 */

void pmm_init(void) {
    for (uint32_t i = 0; i < (PMM_MAX_FRAMES / 32); i++) {
        frame_bitmap[i] = 0xFFFFFFFFu;
    }

    total_frames = PMM_MAX_FRAMES;
    used_frames = PMM_MAX_FRAMES;
}


/*
 * Mark a region as free.
 */

void pmm_mark_region_free(uint32_t base, uint32_t length) {
    uint32_t region_end;

    if (length == 0) {
        return;
    }

    region_end = base + length;

    /*
     * Detect 32-bit overflow.
     */
    if (region_end < base) {
        region_end = PMM_MAX_MEMORY;
    }

    /*
     * Align the start UP to the next 4 KB boundary.
     *
     * Example:
     * 0x00100001 -> 0x00101000
     */
    uint32_t aligned_start =
        (base + PMM_FRAME_SIZE - 1) &
        ~(PMM_FRAME_SIZE - 1);

    /*
     * Align the end DOWN to a 4 KB boundary.
     *
     * This ensures we only free complete frames.
     */
    uint32_t aligned_end =
        region_end & ~(PMM_FRAME_SIZE - 1);

    if (aligned_start >= PMM_MAX_MEMORY) {
        return;
    }

    if (aligned_end > PMM_MAX_MEMORY) {
        aligned_end = PMM_MAX_MEMORY;
    }

    if (aligned_end <= aligned_start) {
        return;
    }

    uint32_t start_frame =
        aligned_start / PMM_FRAME_SIZE;

    uint32_t end_frame =
        aligned_end / PMM_FRAME_SIZE;

    for (uint32_t frame = start_frame;
         frame < end_frame;
         frame++) {

        if (bitmap_test(frame)) {
            bitmap_clear(frame);

            if (used_frames > 0) {
                used_frames--;
            }
        }
    }
}


/*
 * Mark a region as reserved/used.
 */

void pmm_mark_region_used(uint32_t base, uint32_t length) {
    uint32_t region_end;

    if (length == 0) {
        return;
    }

    region_end = base + length;

    if (region_end < base) {
        region_end = PMM_MAX_MEMORY;
    }

    

    uint32_t aligned_start =
    base & ~(PMM_FRAME_SIZE - 1);

    uint32_t aligned_end =
        (region_end + PMM_FRAME_SIZE - 1) &
        ~(PMM_FRAME_SIZE - 1);

    if (aligned_start >= PMM_MAX_MEMORY) {
        return;
    }

    if (aligned_end > PMM_MAX_MEMORY ||
        aligned_end < region_end) {
        aligned_end = PMM_MAX_MEMORY;
    }

    uint32_t start_frame =
        aligned_start / PMM_FRAME_SIZE;

    uint32_t end_frame =
        aligned_end / PMM_FRAME_SIZE;

    for (uint32_t frame = start_frame;
         frame < end_frame;
         frame++) {

        if (!bitmap_test(frame)) {
            bitmap_set(frame);
            used_frames++;
        }
    }
}



/*
 * Allocate the first free frame.
 */
uint32_t pmm_alloc_frame(void) {
    for (uint32_t frame = 0;
         frame < PMM_MAX_FRAMES;
         frame++) {

        if (!bitmap_test(frame)) {
            bitmap_set(frame);
            used_frames++;

            return frame * PMM_FRAME_SIZE;
        }
    }

    return 0;
}


/*
 * Free one frame.
 */
void pmm_free_frame(uint32_t frame_address) {
    uint32_t frame =
        frame_address / PMM_FRAME_SIZE;

    if (frame >= PMM_MAX_FRAMES) {
        return;
    }

    if (bitmap_test(frame)) {
        bitmap_clear(frame);

        if (used_frames > 0) {
            used_frames--;
        }
    }
}


uint32_t pmm_get_total_frames(void) {
    return total_frames;
}


uint32_t pmm_get_used_frames(void) {
    return used_frames;
}


uint32_t pmm_get_free_frames(void) {
    return total_frames - used_frames;
}