#ifndef PMM_H
#define PMM_H

#include <stdint.h>

/*
 * Physical Memory Manager
 *
 * Memory is divided into 4 KB frames.
 */
#define PMM_FRAME_SIZE 4096

/*
 * Maximum physical memory we will track for now:
 * 32 MB
 *
 * 32 MB / 4 KB = 8192 frames
 */
#define PMM_MAX_MEMORY   (32 * 1024 * 1024)
#define PMM_MAX_FRAMES   (PMM_MAX_MEMORY / PMM_FRAME_SIZE)

/*
 * Initialize the physical memory manager.
 */
void pmm_init(void);

/*
 * Mark a physical memory region as usable/free.
 *
 * base   = starting physical address
 * length = number of bytes
 */
void pmm_mark_region_free(uint32_t base, uint32_t length);

/*
 * Mark a physical memory region as reserved/used.
 */
void pmm_mark_region_used(uint32_t base, uint32_t length);

/*
 * Allocate one 4 KB physical frame.
 *
 * Returns the physical address of the frame.
 * Returns 0 if no frame is available.
 */
uint32_t pmm_alloc_frame(void);

/*
 * Free a previously allocated 4 KB frame.
 */
void pmm_free_frame(uint32_t frame_address);

/*
 * Statistics.
 */
uint32_t pmm_get_total_frames(void);
uint32_t pmm_get_used_frames(void);
uint32_t pmm_get_free_frames(void);

#endif